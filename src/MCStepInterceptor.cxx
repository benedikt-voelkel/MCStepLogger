// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

//  @file   MCStepInterceptor.cxx
//  @author Sandro Wenzel
//  @since  2017-06-29
//  @brief  A LD_PRELOAD logger hooking into Stepping of TVirtualMCApplication's

class TVirtualMCApplication;
class TVirtualMCMultiApplication;
class TVirtualMagField;


extern "C" void dispatchOriginal(void*, char const* libname, char const*);
// Make it also working for TVirtualMCMultiApplication
extern "C" void dispatchOriginalMulti(TVirtualMCMultiApplication*, char const* libname, char const*);
extern "C" void dispatchOriginalField(TVirtualMagField*, char const* libname, char const*, const double x[3],
                                      double* B);

namespace helper
{
  void dispatch(TVirtualMCApplication* app, char const* libname, char const* symbol)
  {
    dispatchOriginal(app, libname, symbol);
  }
  void dispatch(TVirtualMCMultiApplication* app, char const* libname, char const* symbol)
  {
    dispatchOriginalMulti(app, libname, symbol);
  }
}

// (re)declare symbols to be able to hook into them
#define DECLARE_INTERCEPT_SYMBOLS(APP, STEPPING, FINISHEVENT, CONSTRUCTGEOMETRY) \
  class APP                            \
  {                                    \
   public:                             \
    void STEPPING();                   \
    void FINISHEVENT();                \
    void CONSTRUCTGEOMETRY();          \
  };

DECLARE_INTERCEPT_SYMBOLS(FairMCApplication, Stepping, FinishEvent, ConstructGeometry)
DECLARE_INTERCEPT_SYMBOLS(AliMC,Stepping, FinishEvent, ConstructGeometry)
DECLARE_INTERCEPT_SYMBOLS(CEMCSingleApplication, Stepping, FinishEvent, ConstructGeometry)
DECLARE_INTERCEPT_SYMBOLS(CEMCMultiApplication, SteppingMulti, FinishEventMulti, ConstructGeometryMulti)


// same for field
#define DECLARE_INTERCEPT_FIELD_SYMBOLS(FIELD)       \
  class FIELD                                        \
  {                                                  \
   public:                                           \
    void Field(const double* point, double* bField); \
  };

namespace o2
{
namespace field
{
DECLARE_INTERCEPT_FIELD_SYMBOLS(MagneticField);
}
}
DECLARE_INTERCEPT_FIELD_SYMBOLS(AliMagF);

//extern "C" void performLogging(TVirtualMCApplication*);
extern "C" void performLogging();
extern "C" void logField(const double*, const double*);
extern "C" void flushLog();
extern "C" void initLogger();

#define INTERCEPT_STEPPING(APP, BASEAPP, METHOD, LIB, SYMBOL)      \
  void APP::METHOD()                                               \
  {                                                                \
    auto baseptr = reinterpret_cast<BASEAPP*>(this);               \
    performLogging();                                              \
    helper::dispatch(baseptr, LIB, SYMBOL);                        \
  }

#define INTERCEPT_FINISHEVENT(APP, BASEAPP, METHOD, LIB, SYMBOL)   \
  void APP::METHOD()                                               \
  {                                                                \
    auto baseptr = reinterpret_cast<BASEAPP*>(this);               \
    flushLog();                                                    \
    helper::dispatch(baseptr, LIB, SYMBOL);                        \
  }

// we use the ConstructGeometry hook to setup the logger
#define INTERCEPT_GEOMETRYINIT(APP, BASEAPP, METHOD, LIB, SYMBOL)  \
  void APP::METHOD()                                               \
  {                                                                \
    auto baseptr = reinterpret_cast<BASEAPP*>(this);               \
    helper::dispatch(baseptr, LIB, SYMBOL);                        \
    initLogger();                                                  \
  }

// the runtime will now dispatch to these functions due to LD_PRELOAD
INTERCEPT_STEPPING(FairMCApplication, TVirtualMCApplication, Stepping, "libBase", "_ZN17FairMCApplication8SteppingEv")
INTERCEPT_STEPPING(AliMC, TVirtualMCApplication, Stepping, "libSTEER", "_ZN5AliMC8SteppingEv")
INTERCEPT_STEPPING(CEMCSingleApplication, TVirtualMCApplication, Stepping, "libvmc_CE", "_ZN21CEMCSingleApplication8SteppingEv")
INTERCEPT_STEPPING(CEMCMultiApplication, TVirtualMCMultiApplication, SteppingMulti, "libvmc_CE", "_ZN20CEMCMultiApplication13SteppingMultiEv")

INTERCEPT_FINISHEVENT(FairMCApplication, TVirtualMCApplication, FinishEvent, "libBase", "_ZN17FairMCApplication11FinishEventEv")
INTERCEPT_FINISHEVENT(AliMC, TVirtualMCApplication, FinishEvent, "libSTEER", "_ZN5AliMC11FinishEventEv")
INTERCEPT_FINISHEVENT(CEMCSingleApplication, TVirtualMCApplication, FinishEvent, "libvmc_CE", "_ZN21CEMCSingleApplication11FinishEventEv")
INTERCEPT_FINISHEVENT(CEMCMultiApplication, TVirtualMCMultiApplication, FinishEventMulti, "libvmc_CE", "_ZN20CEMCMultiApplication16FinishEventMultiEv")

INTERCEPT_GEOMETRYINIT(FairMCApplication, TVirtualMCApplication, ConstructGeometry, "libBase", "_ZN17FairMCApplication17ConstructGeometryEv")
INTERCEPT_GEOMETRYINIT(AliMC, TVirtualMCApplication, ConstructGeometry, "libSTEER", "_ZN5AliMC17ConstructGeometryEv")
INTERCEPT_GEOMETRYINIT(CEMCSingleApplication, TVirtualMCApplication, ConstructGeometry, "libvmc_CE", "_ZN21CEMCSingleApplication17ConstructGeometryEv")
INTERCEPT_GEOMETRYINIT(CEMCMultiApplication, TVirtualMCMultiApplication, ConstructGeometryMulti, "libvmc_CE", "_ZN20CEMCMultiApplication21ConstructGeometryMultiEv")

#define INTERCEPT_FIELD(FIELD, LIB, SYMBOL)                     \
  void FIELD::Field(const double* point, double* bField)        \
  {                                                             \
    auto baseptr = reinterpret_cast<TVirtualMagField*>(this);   \
    dispatchOriginalField(baseptr, LIB, SYMBOL, point, bField); \
    logField(point, bField);                                    \
  }

namespace o2
{
namespace field
{
INTERCEPT_FIELD(MagneticField, "libField", "_ZN2o25field13MagneticField5FieldEPKdPd");
}
}
// for AliRoot
INTERCEPT_FIELD(AliMagF, "libSTEERBase", "_ZN7AliMagF5FieldEPKdPd")
