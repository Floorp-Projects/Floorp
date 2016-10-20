/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "2D.h"

#ifdef USE_CAIRO
#include "DrawTargetCairo.h"
#include "ScaledFontCairo.h"
#include "SourceSurfaceCairo.h"
#endif

#ifdef USE_SKIA
#include "DrawTargetSkia.h"
#include "ScaledFontBase.h"
#ifdef MOZ_ENABLE_FREETYPE
#define USE_SKIA_FREETYPE
#include "ScaledFontCairo.h"
#endif
#endif

#if defined(WIN32)
#include "ScaledFontWin.h"
#include "NativeFontResourceGDI.h"
#endif

#ifdef XP_DARWIN
#include "ScaledFontMac.h"
#include "NativeFontResourceMac.h"
#endif

#ifdef MOZ_WIDGET_GTK
#include "ScaledFontFontconfig.h"
#endif

#ifdef WIN32
#include "DrawTargetD2D1.h"
#include "ScaledFontDWrite.h"
#include "NativeFontResourceDWrite.h"
#include <d3d10_1.h>
#include "HelpersD2D.h"
#endif

#include "DrawTargetDual.h"
#include "DrawTargetTiled.h"
#include "DrawTargetRecording.h"

#include "SourceSurfaceRawData.h"

#include "DrawEventRecorder.h"

#include "Logging.h"

#include "mozilla/CheckedInt.h"

#if defined(MOZ_LOGGING)
GFX2D_API mozilla::LogModule*
GetGFX2DLog()
{
  static mozilla::LazyLogModule sLog("gfx2d");
  return sLog;
}
#endif

// The following code was largely taken from xpcom/glue/SSE.cpp and
// made a little simpler.
enum CPUIDRegister { eax = 0, ebx = 1, ecx = 2, edx = 3 };

#ifdef HAVE_CPUID_H

#if !(defined(__SSE2__) || defined(_M_X64) || \
     (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
// cpuid.h is available on gcc 4.3 and higher on i386 and x86_64
#include <cpuid.h>

static inline bool
HasCPUIDBit(unsigned int level, CPUIDRegister reg, unsigned int bit)
{
  unsigned int regs[4];
  return __get_cpuid(level, &regs[0], &regs[1], &regs[2], &regs[3]) &&
         (regs[reg] & bit);
}
#endif

#define HAVE_CPU_DETECTION
#else

#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_AMD64))
// MSVC 2005 or later supports __cpuid by intrin.h
#include <intrin.h>

#define HAVE_CPU_DETECTION
#elif defined(__SUNPRO_CC) && (defined(__i386) || defined(__x86_64__))

// Define a function identical to MSVC function.
#ifdef __i386
static void
__cpuid(int CPUInfo[4], int InfoType)
{
  asm (
    "xchg %esi, %ebx\n"
    "cpuid\n"
    "movl %eax, (%edi)\n"
    "movl %ebx, 4(%edi)\n"
    "movl %ecx, 8(%edi)\n"
    "movl %edx, 12(%edi)\n"
    "xchg %esi, %ebx\n"
    :
    : "a"(InfoType), // %eax
      "D"(CPUInfo) // %edi
    : "%ecx", "%edx", "%esi"
  );
}
#else
static void
__cpuid(int CPUInfo[4], int InfoType)
{
  asm (
    "xchg %rsi, %rbx\n"
    "cpuid\n"
    "movl %eax, (%rdi)\n"
    "movl %ebx, 4(%rdi)\n"
    "movl %ecx, 8(%rdi)\n"
    "movl %edx, 12(%rdi)\n"
    "xchg %rsi, %rbx\n"
    :
    : "a"(InfoType), // %eax
      "D"(CPUInfo) // %rdi
    : "%ecx", "%edx", "%rsi"
  );
}

#define HAVE_CPU_DETECTION
#endif
#endif

#ifdef HAVE_CPU_DETECTION
static inline bool
HasCPUIDBit(unsigned int level, CPUIDRegister reg, unsigned int bit)
{
  // Check that the level in question is supported.
  volatile int regs[4];
  __cpuid((int *)regs, level & 0x80000000u);
  if (unsigned(regs[0]) < level)
    return false;
  __cpuid((int *)regs, level);
  return !!(unsigned(regs[reg]) & bit);
}
#endif
#endif

namespace mozilla {
namespace gfx {

// In Gecko, this value is managed by gfx.logging.level in gfxPrefs.
int32_t LoggingPrefs::sGfxLogLevel = LOG_DEFAULT;

#ifdef WIN32
ID3D11Device *Factory::mD3D11Device;
ID2D1Device *Factory::mD2D1Device;
#endif

DrawEventRecorder *Factory::mRecorder;

mozilla::gfx::Config* Factory::sConfig = nullptr;

void
Factory::Init(const Config& aConfig)
{
  MOZ_ASSERT(!sConfig);
  sConfig = new Config(aConfig);

  // Make sure we don't completely break rendering because of a typo in the
  // pref or whatnot.
  const int32_t kMinAllocPref = 10000000;
  const int32_t kMinSizePref = 2048;
  if (sConfig->mMaxAllocSize < kMinAllocPref) {
    sConfig->mMaxAllocSize = kMinAllocPref;
  }
  if (sConfig->mMaxTextureSize < kMinSizePref) {
    sConfig->mMaxTextureSize = kMinSizePref;
  }
}

void
Factory::ShutDown()
{
  if (sConfig) {
    delete sConfig->mLogForwarder;
    delete sConfig;
    sConfig = nullptr;
  }
}

bool
Factory::HasSSE2()
{
#if defined(__SSE2__) || defined(_M_X64) || \
    (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
  // gcc with -msse2 (default on OSX and x86-64)
  // cl.exe with -arch:SSE2 (default on x64 compiler)
  return true;
#elif defined(HAVE_CPU_DETECTION)
  static enum {
    UNINITIALIZED,
    NO_SSE2,
    HAS_SSE2
  } sDetectionState = UNINITIALIZED;

  if (sDetectionState == UNINITIALIZED) {
    sDetectionState = HasCPUIDBit(1u, edx, (1u<<26)) ? HAS_SSE2 : NO_SSE2;
  }
  return sDetectionState == HAS_SSE2;
#else
  return false;
#endif
}

// If the size is "reasonable", we want gfxCriticalError to assert, so
// this is the option set up for it.
inline int LoggerOptionsBasedOnSize(const IntSize& aSize)
{
  return CriticalLog::DefaultOptions(Factory::ReasonableSurfaceSize(aSize));
}

bool
Factory::ReasonableSurfaceSize(const IntSize &aSize)
{
  return Factory::CheckSurfaceSize(aSize, 8192);
}

bool
Factory::AllowedSurfaceSize(const IntSize &aSize)
{
  if (sConfig) {
    return Factory::CheckSurfaceSize(aSize,
                                     sConfig->mMaxTextureSize,
                                     sConfig->mMaxAllocSize);
  }

  return CheckSurfaceSize(aSize);
}

bool
Factory::CheckBufferSize(int32_t bufSize)
{
  return !sConfig || bufSize < sConfig->mMaxAllocSize;
}

bool
Factory::CheckSurfaceSize(const IntSize &sz,
                          int32_t extentLimit,
                          int32_t allocLimit)
{
  if (sz.width <= 0 || sz.height <= 0) {
    gfxDebug() << "Surface width or height <= 0!";
    return false;
  }

  // reject images with sides bigger than limit
  if (extentLimit && (sz.width > extentLimit || sz.height > extentLimit)) {
    gfxDebug() << "Surface size too large (exceeds extent limit)!";
    return false;
  }

  // assuming 4 bytes per pixel, make sure the allocation size
  // doesn't overflow a int32_t either
  CheckedInt<int32_t> stride = GetAlignedStride<16>(sz.width, 4);
  if (!stride.isValid() || stride.value() == 0) {
    gfxDebug() << "Surface size too large (stride overflows int32_t)!";
    return false;
  }

  CheckedInt<int32_t> numBytes = stride * sz.height;
  if (!numBytes.isValid()) {
    gfxDebug() << "Surface size too large (allocation size would overflow int32_t)!";
    return false;
  }

  if (allocLimit && allocLimit < numBytes.value()) {
    gfxDebug() << "Surface size too large (exceeds allocation limit)!";
    return false;
  }

  return true;
}

already_AddRefed<DrawTarget>
Factory::CreateDrawTarget(BackendType aBackend, const IntSize &aSize, SurfaceFormat aFormat)
{
  if (!AllowedSurfaceSize(aSize)) {
    gfxCriticalError(LoggerOptionsBasedOnSize(aSize)) << "Failed to allocate a surface due to invalid size (CDT) " << aSize;
    return nullptr;
  }

  RefPtr<DrawTarget> retVal;
  switch (aBackend) {
#ifdef WIN32
  case BackendType::DIRECT2D1_1:
    {
      RefPtr<DrawTargetD2D1> newTarget;
      newTarget = new DrawTargetD2D1();
      if (newTarget->Init(aSize, aFormat)) {
        retVal = newTarget;
      }
      break;
    }
#endif
#ifdef USE_SKIA
  case BackendType::SKIA:
    {
      RefPtr<DrawTargetSkia> newTarget;
      newTarget = new DrawTargetSkia();
      if (newTarget->Init(aSize, aFormat)) {
        retVal = newTarget;
      }
      break;
    }
#endif
#ifdef USE_CAIRO
  case BackendType::CAIRO:
    {
      RefPtr<DrawTargetCairo> newTarget;
      newTarget = new DrawTargetCairo();
      if (newTarget->Init(aSize, aFormat)) {
        retVal = newTarget;
      }
      break;
    }
#endif
  default:
    return nullptr;
  }

  if (mRecorder && retVal) {
    return MakeAndAddRef<DrawTargetRecording>(mRecorder, retVal);
  }

  if (!retVal) {
    // Failed
    gfxCriticalError(LoggerOptionsBasedOnSize(aSize)) << "Failed to create DrawTarget, Type: " << int(aBackend) << " Size: " << aSize;
  }

  return retVal.forget();
}

already_AddRefed<DrawTarget>
Factory::CreateRecordingDrawTarget(DrawEventRecorder *aRecorder, DrawTarget *aDT)
{
  return MakeAndAddRef<DrawTargetRecording>(aRecorder, aDT);
}

already_AddRefed<DrawTarget>
Factory::CreateDrawTargetForData(BackendType aBackend,
                                 unsigned char *aData,
                                 const IntSize &aSize,
                                 int32_t aStride,
                                 SurfaceFormat aFormat,
                                 bool aUninitialized)
{
  MOZ_ASSERT(aData);
  if (!AllowedSurfaceSize(aSize)) {
    gfxCriticalError(LoggerOptionsBasedOnSize(aSize)) << "Failed to allocate a surface due to invalid size (DTD) " << aSize;
    return nullptr;
  }

  RefPtr<DrawTarget> retVal;

  switch (aBackend) {
#ifdef USE_SKIA
  case BackendType::SKIA:
    {
      RefPtr<DrawTargetSkia> newTarget;
      newTarget = new DrawTargetSkia();
      if (newTarget->Init(aData, aSize, aStride, aFormat, aUninitialized)) {
        retVal = newTarget;
      }
      break;
    }
#endif
#ifdef USE_CAIRO
  case BackendType::CAIRO:
    {
      RefPtr<DrawTargetCairo> newTarget;
      newTarget = new DrawTargetCairo();
      if (newTarget->Init(aData, aSize, aStride, aFormat)) {
        retVal = newTarget.forget();
      }
      break;
    }
#endif
  default:
    gfxCriticalNote << "Invalid draw target type specified: " << (int)aBackend;
    return nullptr;
  }

  if (mRecorder && retVal) {
    return MakeAndAddRef<DrawTargetRecording>(mRecorder, retVal, true);
  }

  if (!retVal) {
    gfxCriticalNote << "Failed to create DrawTarget, Type: " << int(aBackend) << " Size: " << aSize << ", Data: " << hexa(aData) << ", Stride: " << aStride;
  }

  return retVal.forget();
}

already_AddRefed<DrawTarget>
Factory::CreateTiledDrawTarget(const TileSet& aTileSet)
{
  RefPtr<DrawTargetTiled> dt = new DrawTargetTiled();

  if (!dt->Init(aTileSet)) {
    return nullptr;
  }

  return dt.forget();
}

bool
Factory::DoesBackendSupportDataDrawtarget(BackendType aType)
{
  switch (aType) {
  case BackendType::DIRECT2D:
  case BackendType::DIRECT2D1_1:
  case BackendType::RECORDING:
  case BackendType::NONE:
  case BackendType::BACKEND_LAST:
    return false;
  case BackendType::CAIRO:
  case BackendType::SKIA:
    return true;
  }

  return false;
}

uint32_t
Factory::GetMaxSurfaceSize(BackendType aType)
{
  switch (aType) {
  case BackendType::CAIRO:
    return DrawTargetCairo::GetMaxSurfaceSize();
#ifdef USE_SKIA
  case BackendType::SKIA:
    return DrawTargetSkia::GetMaxSurfaceSize();
#endif
#ifdef WIN32
  case BackendType::DIRECT2D1_1:
    return DrawTargetD2D1::GetMaxSurfaceSize();
#endif
  default:
    return 0;
  }
}

already_AddRefed<ScaledFont>
Factory::CreateScaledFontForNativeFont(const NativeFont &aNativeFont, Float aSize)
{
  switch (aNativeFont.mType) {
#ifdef WIN32
  case NativeFontType::DWRITE_FONT_FACE:
    {
      return MakeAndAddRef<ScaledFontDWrite>(static_cast<IDWriteFontFace*>(aNativeFont.mFont), aSize);
    }
#if defined(USE_CAIRO) || defined(USE_SKIA)
  case NativeFontType::GDI_FONT_FACE:
    {
      return MakeAndAddRef<ScaledFontWin>(static_cast<LOGFONT*>(aNativeFont.mFont), aSize);
    }
#endif
#endif
#ifdef XP_DARWIN
  case NativeFontType::MAC_FONT_FACE:
    {
      return MakeAndAddRef<ScaledFontMac>(static_cast<CGFontRef>(aNativeFont.mFont), aSize);
    }
#endif
#if defined(USE_CAIRO) || defined(USE_SKIA_FREETYPE)
  case NativeFontType::CAIRO_FONT_FACE:
    {
      return MakeAndAddRef<ScaledFontCairo>(static_cast<cairo_scaled_font_t*>(aNativeFont.mFont), aSize);
    }
#endif
  default:
    gfxWarning() << "Invalid native font type specified.";
    return nullptr;
  }
}

already_AddRefed<NativeFontResource>
Factory::CreateNativeFontResource(uint8_t *aData, uint32_t aSize,
                                  FontType aType)
{
  switch (aType) {
#ifdef WIN32
  case FontType::DWRITE:
    {
      return NativeFontResourceDWrite::Create(aData, aSize,
                                              /* aNeedsCairo = */ false);
    }
#endif
  case FontType::CAIRO:
#ifdef USE_SKIA
  case FontType::SKIA:
#endif
    {
#ifdef WIN32
      if (GetDirect3D11Device()) {
        return NativeFontResourceDWrite::Create(aData, aSize,
                                                /* aNeedsCairo = */ true);
      } else {
        return NativeFontResourceGDI::Create(aData, aSize,
                                             /* aNeedsCairo = */ true);
      }
#elif XP_DARWIN
      return NativeFontResourceMac::Create(aData, aSize);
#else
      gfxWarning() << "Unable to create cairo scaled font from truetype data";
      return nullptr;
#endif
    }
  default:
    gfxWarning() << "Unable to create requested font resource from truetype data";
    return nullptr;
  }
}

already_AddRefed<ScaledFont>
Factory::CreateScaledFontWithCairo(const NativeFont& aNativeFont, Float aSize, cairo_scaled_font_t* aScaledFont)
{
#ifdef USE_CAIRO
  // In theory, we could pull the NativeFont out of the cairo_scaled_font_t*,
  // but that would require a lot of code that would be otherwise repeated in
  // various backends.
  // Therefore, we just reuse CreateScaledFontForNativeFont's implementation.
  RefPtr<ScaledFont> font = CreateScaledFontForNativeFont(aNativeFont, aSize);
  static_cast<ScaledFontBase*>(font.get())->SetCairoScaledFont(aScaledFont);
  return font.forget();
#else
  return nullptr;
#endif
}

#ifdef MOZ_WIDGET_GTK
already_AddRefed<ScaledFont>
Factory::CreateScaledFontForFontconfigFont(cairo_scaled_font_t* aScaledFont, FcPattern* aPattern, Float aSize)
{
  return MakeAndAddRef<ScaledFontFontconfig>(aScaledFont, aPattern, aSize);
}
#endif

already_AddRefed<DrawTarget>
Factory::CreateDualDrawTarget(DrawTarget *targetA, DrawTarget *targetB)
{
  MOZ_ASSERT(targetA && targetB);

  RefPtr<DrawTarget> newTarget =
    new DrawTargetDual(targetA, targetB);

  RefPtr<DrawTarget> retVal = newTarget;

  if (mRecorder) {
    retVal = new DrawTargetRecording(mRecorder, retVal);
  }

  return retVal.forget();
}


#ifdef WIN32
already_AddRefed<DrawTarget>
Factory::CreateDrawTargetForD3D11Texture(ID3D11Texture2D *aTexture, SurfaceFormat aFormat)
{
  MOZ_ASSERT(aTexture);

  RefPtr<DrawTargetD2D1> newTarget;

  newTarget = new DrawTargetD2D1();
  if (newTarget->Init(aTexture, aFormat)) {
    RefPtr<DrawTarget> retVal = newTarget;

    if (mRecorder) {
      retVal = new DrawTargetRecording(mRecorder, retVal, true);
    }

    return retVal.forget();
  }

  gfxWarning() << "Failed to create draw target for D3D11 texture.";

  // Failed
  return nullptr;
}

bool
Factory::SetDirect3D11Device(ID3D11Device *aDevice)
{
  mD3D11Device = aDevice;

  if (mD2D1Device) {
    mD2D1Device->Release();
    mD2D1Device = nullptr;
  }

  if (!aDevice) {
    return true;
  }

  RefPtr<ID2D1Factory1> factory = D2DFactory1();

  RefPtr<IDXGIDevice> device;
  aDevice->QueryInterface((IDXGIDevice**)getter_AddRefs(device));
  HRESULT hr = factory->CreateDevice(device, &mD2D1Device);
  if (FAILED(hr)) {
    gfxCriticalError() << "[D2D1] Failed to create gfx factory's D2D1 device, code: " << hexa(hr);

    mD3D11Device = nullptr;
    return false;
  }

  return true;
}

ID3D11Device*
Factory::GetDirect3D11Device()
{
  return mD3D11Device;
}

ID2D1Device*
Factory::GetD2D1Device()
{
  return mD2D1Device;
}

bool
Factory::SupportsD2D1()
{
  return !!D2DFactory1();
}

already_AddRefed<GlyphRenderingOptions>
Factory::CreateDWriteGlyphRenderingOptions(IDWriteRenderingParams *aParams)
{
  return MakeAndAddRef<GlyphRenderingOptionsDWrite>(aParams);
}

uint64_t
Factory::GetD2DVRAMUsageDrawTarget()
{
  return DrawTargetD2D1::mVRAMUsageDT;
}

uint64_t
Factory::GetD2DVRAMUsageSourceSurface()
{
  return DrawTargetD2D1::mVRAMUsageSS;
}

void
Factory::D2DCleanup()
{
  if (mD2D1Device) {
    mD2D1Device->Release();
    mD2D1Device = nullptr;
  }
  DrawTargetD2D1::CleanupD2D();
}

already_AddRefed<ScaledFont>
Factory::CreateScaledFontForDWriteFont(IDWriteFontFace* aFontFace,
                                       const gfxFontStyle* aStyle,
                                       float aSize,
                                       bool aUseEmbeddedBitmap,
                                       bool aForceGDIMode)
{
  return MakeAndAddRef<ScaledFontDWrite>(aFontFace, aSize,
                                         aUseEmbeddedBitmap, aForceGDIMode,
                                         aStyle);
}

#endif // XP_WIN

#ifdef USE_SKIA_GPU
already_AddRefed<DrawTarget>
Factory::CreateDrawTargetSkiaWithGrContext(GrContext* aGrContext,
                                           const IntSize &aSize,
                                           SurfaceFormat aFormat)
{
  RefPtr<DrawTarget> newTarget = new DrawTargetSkia();
  if (!newTarget->InitWithGrContext(aGrContext, aSize, aFormat)) {
    return nullptr;
  }
  return newTarget.forget();
}

#endif // USE_SKIA_GPU

#ifdef USE_SKIA
already_AddRefed<DrawTarget>
Factory::CreateDrawTargetWithSkCanvas(SkCanvas* aCanvas)
{
  RefPtr<DrawTargetSkia> newTarget = new DrawTargetSkia();
  if (!newTarget->Init(aCanvas)) {
    return nullptr;
  }
  return newTarget.forget();
}
#endif

void
Factory::PurgeAllCaches()
{
}

already_AddRefed<DrawTarget>
Factory::CreateDrawTargetForCairoSurface(cairo_surface_t* aSurface, const IntSize& aSize, SurfaceFormat* aFormat)
{
  if (!AllowedSurfaceSize(aSize)) {
    gfxWarning() << "Allowing surface with invalid size (Cairo) " << aSize;
  }

  RefPtr<DrawTarget> retVal;

#ifdef USE_CAIRO
  RefPtr<DrawTargetCairo> newTarget = new DrawTargetCairo();

  if (newTarget->Init(aSurface, aSize, aFormat)) {
    retVal = newTarget;
  }

  if (mRecorder && retVal) {
    return MakeAndAddRef<DrawTargetRecording>(mRecorder, retVal, true);
  }
#endif
  return retVal.forget();
}

already_AddRefed<SourceSurface>
Factory::CreateSourceSurfaceForCairoSurface(cairo_surface_t* aSurface, const IntSize& aSize, SurfaceFormat aFormat)
{
  if (aSize.width <= 0 || aSize.height <= 0) {
    gfxWarning() << "Can't create a SourceSurface without a valid size";
    return nullptr;
  }

#ifdef USE_CAIRO
  return MakeAndAddRef<SourceSurfaceCairo>(aSurface, aSize, aFormat);
#else
  return nullptr;
#endif
}

already_AddRefed<DataSourceSurface>
Factory::CreateWrappingDataSourceSurface(uint8_t *aData,
                                         int32_t aStride,
                                         const IntSize &aSize,
                                         SurfaceFormat aFormat,
                                         SourceSurfaceDeallocator aDeallocator /* = nullptr */,
                                         void* aClosure /* = nullptr */)
{
  // Just check for negative/zero size instead of the full AllowedSurfaceSize() - since
  // the data is already allocated we do not need to check for a possible overflow - it
  // already worked.
  if (aSize.width <= 0 || aSize.height <= 0) {
    return nullptr;
  }
  if (!aDeallocator && aClosure) {
    return nullptr;
  }

  MOZ_ASSERT(aData);

  RefPtr<SourceSurfaceRawData> newSurf = new SourceSurfaceRawData();
  newSurf->InitWrappingData(aData, aSize, aStride, aFormat, aDeallocator, aClosure);

  return newSurf.forget();
}

#ifdef XP_DARWIN
already_AddRefed<GlyphRenderingOptions>
Factory::CreateCGGlyphRenderingOptions(const Color &aFontSmoothingBackgroundColor)
{
  return MakeAndAddRef<GlyphRenderingOptionsCG>(aFontSmoothingBackgroundColor);
}
#endif

already_AddRefed<DataSourceSurface>
Factory::CreateDataSourceSurface(const IntSize &aSize,
                                 SurfaceFormat aFormat,
                                 bool aZero)
{
  if (!AllowedSurfaceSize(aSize)) {
    gfxCriticalError(LoggerOptionsBasedOnSize(aSize)) << "Failed to allocate a surface due to invalid size (DSS) " << aSize;
    return nullptr;
  }

  // Skia doesn't support RGBX, so memset RGBX to 0xFF
  bool clearSurface = aZero || aFormat == SurfaceFormat::B8G8R8X8;
  uint8_t clearValue = aFormat == SurfaceFormat::B8G8R8X8 ? 0xFF : 0;

  RefPtr<SourceSurfaceAlignedRawData> newSurf = new SourceSurfaceAlignedRawData();
  if (newSurf->Init(aSize, aFormat, clearSurface, clearValue)) {
    return newSurf.forget();
  }

  gfxWarning() << "CreateDataSourceSurface failed in init";
  return nullptr;
}

already_AddRefed<DataSourceSurface>
Factory::CreateDataSourceSurfaceWithStride(const IntSize &aSize,
                                           SurfaceFormat aFormat,
                                           int32_t aStride,
                                           bool aZero)
{
  if (!AllowedSurfaceSize(aSize) ||
      aStride < aSize.width * BytesPerPixel(aFormat)) {
    gfxCriticalError(LoggerOptionsBasedOnSize(aSize)) << "CreateDataSourceSurfaceWithStride failed with bad stride " << aStride << ", " << aSize << ", " << aFormat;
    return nullptr;
  }

  // Skia doesn't support RGBX, so memset RGBX to 0xFF
  bool clearSurface = aZero || aFormat == SurfaceFormat::B8G8R8X8;
  uint8_t clearValue = aFormat == SurfaceFormat::B8G8R8X8 ? 0xFF : 0;

  RefPtr<SourceSurfaceAlignedRawData> newSurf = new SourceSurfaceAlignedRawData();
  if (newSurf->Init(aSize, aFormat, clearSurface, clearValue, aStride)) {
    return newSurf.forget();
  }

  gfxCriticalError(LoggerOptionsBasedOnSize(aSize)) << "CreateDataSourceSurfaceWithStride failed to initialize " << aSize << ", " << aFormat << ", " << aStride << ", " << aZero;
  return nullptr;
}

static uint16_t
PackRGB565(uint8_t r, uint8_t g, uint8_t b)
{
  uint16_t pixel = ((r << 11) & 0xf800) |
                   ((g <<  5) & 0x07e0) |
                   ((b      ) & 0x001f);

  return pixel;
}

void
Factory::CopyDataSourceSurface(DataSourceSurface* aSource,
                               DataSourceSurface* aDest)
{
  // Don't worry too much about speed.
  MOZ_ASSERT(aSource->GetSize() == aDest->GetSize());
  MOZ_ASSERT(aSource->GetFormat() == SurfaceFormat::R8G8B8A8 ||
             aSource->GetFormat() == SurfaceFormat::R8G8B8X8 ||
             aSource->GetFormat() == SurfaceFormat::B8G8R8A8 ||
             aSource->GetFormat() == SurfaceFormat::B8G8R8X8);
  MOZ_ASSERT(aDest->GetFormat() == SurfaceFormat::R8G8B8A8 ||
             aDest->GetFormat() == SurfaceFormat::R8G8B8X8 ||
             aDest->GetFormat() == SurfaceFormat::B8G8R8A8 ||
             aDest->GetFormat() == SurfaceFormat::B8G8R8X8 ||
             aDest->GetFormat() == SurfaceFormat::R5G6B5_UINT16);

  const bool isSrcBGR = aSource->GetFormat() == SurfaceFormat::B8G8R8A8 ||
                        aSource->GetFormat() == SurfaceFormat::B8G8R8X8;
  const bool isDestBGR = aDest->GetFormat() == SurfaceFormat::B8G8R8A8 ||
                         aDest->GetFormat() == SurfaceFormat::B8G8R8X8;
  const bool needsSwap02 = isSrcBGR != isDestBGR;

  const bool srcHasAlpha = aSource->GetFormat() == SurfaceFormat::R8G8B8A8 ||
                           aSource->GetFormat() == SurfaceFormat::B8G8R8A8;
  const bool destHasAlpha = aDest->GetFormat() == SurfaceFormat::R8G8B8A8 ||
                            aDest->GetFormat() == SurfaceFormat::B8G8R8A8;
  const bool needsAlphaMask = !srcHasAlpha && destHasAlpha;

  const bool needsConvertTo16Bits = aDest->GetFormat() == SurfaceFormat::R5G6B5_UINT16;

  DataSourceSurface::MappedSurface srcMap;
  DataSourceSurface::MappedSurface destMap;
  if (!aSource->Map(DataSourceSurface::MapType::READ, &srcMap) ||
    !aDest->Map(DataSourceSurface::MapType::WRITE, &destMap)) {
    MOZ_ASSERT(false, "CopyDataSourceSurface: Failed to map surface.");
    return;
  }

  MOZ_ASSERT(srcMap.mStride >= 0);
  MOZ_ASSERT(destMap.mStride >= 0);

  const size_t srcBPP = BytesPerPixel(aSource->GetFormat());
  const size_t srcRowBytes = aSource->GetSize().width * srcBPP;
  const size_t srcRowHole = srcMap.mStride - srcRowBytes;

  const size_t destBPP = BytesPerPixel(aDest->GetFormat());
  const size_t destRowBytes = aDest->GetSize().width * destBPP;
  const size_t destRowHole = destMap.mStride - destRowBytes;

  uint8_t* srcRow = srcMap.mData;
  uint8_t* destRow = destMap.mData;
  const size_t rows = aSource->GetSize().height;
  for (size_t i = 0; i < rows; i++) {
    const uint8_t* srcRowEnd = srcRow + srcRowBytes;

    while (srcRow != srcRowEnd) {
      uint8_t d0 = needsSwap02 ? srcRow[2] : srcRow[0];
      uint8_t d1 = srcRow[1];
      uint8_t d2 = needsSwap02 ? srcRow[0] : srcRow[2];
      uint8_t d3 = needsAlphaMask ? 0xff : srcRow[3];

      if (needsConvertTo16Bits) {
        *(uint16_t*)destRow = PackRGB565(d0, d1, d2);
      } else {
        destRow[0] = d0;
        destRow[1] = d1;
        destRow[2] = d2;
        destRow[3] = d3;
      }
      srcRow += srcBPP;
      destRow += destBPP;
    }

    srcRow += srcRowHole;
    destRow += destRowHole;
  }

  aSource->Unmap();
  aDest->Unmap();
}

already_AddRefed<DrawEventRecorder>
Factory::CreateEventRecorderForFile(const char *aFilename)
{
  return MakeAndAddRef<DrawEventRecorderFile>(aFilename);
}

void
Factory::SetGlobalEventRecorder(DrawEventRecorder *aRecorder)
{
  mRecorder = aRecorder;
}

// static
void
CriticalLogger::OutputMessage(const std::string &aString,
                              int aLevel, bool aNoNewline)
{
  if (Factory::GetLogForwarder()) {
    Factory::GetLogForwarder()->Log(aString);
  }

  BasicLogger::OutputMessage(aString, aLevel, aNoNewline);
}

void
CriticalLogger::CrashAction(LogReason aReason)
{
  if (Factory::GetLogForwarder()) {
    Factory::GetLogForwarder()->CrashAction(aReason);
  }
}

} // namespace gfx
} // namespace mozilla
