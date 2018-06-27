/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "2D.h"
#include "Swizzle.h"

#ifdef USE_CAIRO
#include "DrawTargetCairo.h"
#include "SourceSurfaceCairo.h"
#endif

#ifdef USE_SKIA
#include "DrawTargetSkia.h"
#include "ScaledFontBase.h"
#endif

#if defined(WIN32)
#include "ScaledFontWin.h"
#include "NativeFontResourceGDI.h"
#include "UnscaledFontGDI.h"
#endif

#ifdef XP_DARWIN
#include "ScaledFontMac.h"
#include "NativeFontResourceMac.h"
#endif

#ifdef MOZ_WIDGET_GTK
#include "ScaledFontFontconfig.h"
#include "NativeFontResourceFreeType.h"
#include "UnscaledFontFreeType.h"
#endif

#ifdef MOZ_WIDGET_ANDROID
#include "ScaledFontFreeType.h"
#include "NativeFontResourceFreeType.h"
#endif

#ifdef WIN32
#include "DrawTargetD2D1.h"
#include "ScaledFontDWrite.h"
#include "NativeFontResourceDWrite.h"
#include <d3d10_1.h>
#include "HelpersD2D.h"
#include "HelpersWinFonts.h"
#include "mozilla/Mutex.h"
#endif

#include "DrawTargetCapture.h"
#include "DrawTargetDual.h"
#include "DrawTargetTiled.h"
#include "DrawTargetWrapAndRecord.h"
#include "DrawTargetRecording.h"

#include "SourceSurfaceRawData.h"

#include "DrawEventRecorder.h"

#include "Logging.h"

#include "mozilla/CheckedInt.h"

#ifdef MOZ_ENABLE_FREETYPE
#include "ft2build.h"
#include FT_FREETYPE_H

#include "mozilla/Mutex.h"
#endif
#include "MainThreadUtils.h"

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
     (defined(_M_IX86_FP) && _M_IX86_FP >= 2)) || \
    !defined(__SSE4__)
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

#ifdef MOZ_ENABLE_FREETYPE
extern "C" {

FT_Face
mozilla_NewFTFace(FT_Library aFTLibrary, const char* aFileName, int aFaceIndex)
{
  return mozilla::gfx::Factory::NewFTFace(aFTLibrary, aFileName, aFaceIndex);
}

FT_Face
mozilla_NewFTFaceFromData(FT_Library aFTLibrary, const uint8_t* aData, size_t aDataSize, int aFaceIndex)
{
  return mozilla::gfx::Factory::NewFTFaceFromData(aFTLibrary, aData, aDataSize, aFaceIndex);
}

void
mozilla_ReleaseFTFace(FT_Face aFace)
{
  mozilla::gfx::Factory::ReleaseFTFace(aFace);
}

FT_Error
mozilla_LoadFTGlyph(FT_Face aFace, uint32_t aGlyphIndex, int32_t aFlags)
{
  return mozilla::gfx::Factory::LoadFTGlyph(aFace, aGlyphIndex, aFlags);
}

void
mozilla_LockFTLibrary(FT_Library aFTLibrary)
{
  mozilla::gfx::Factory::LockFTLibrary(aFTLibrary);
}

void
mozilla_UnlockFTLibrary(FT_Library aFTLibrary)
{
  mozilla::gfx::Factory::UnlockFTLibrary(aFTLibrary);
}

}
#endif

namespace mozilla {
namespace gfx {

// In Gecko, this value is managed by gfx.logging.level in gfxPrefs.
int32_t LoggingPrefs::sGfxLogLevel = LOG_DEFAULT;

#ifdef MOZ_ENABLE_FREETYPE
FT_Library Factory::mFTLibrary = nullptr;
Mutex* Factory::mFTLock = nullptr;
#endif

#ifdef WIN32
// Note: mDeviceLock must be held when mutating these values.
static uint32_t mDeviceSeq = 0;
StaticRefPtr<ID3D11Device> Factory::mD3D11Device;
StaticRefPtr<ID2D1Device> Factory::mD2D1Device;
StaticRefPtr<IDWriteFactory> Factory::mDWriteFactory;
bool Factory::mDWriteFactoryInitialized = false;
StaticMutex Factory::mDeviceLock;
StaticMutex Factory::mDTDependencyLock;
#endif

DrawEventRecorder *Factory::mRecorder;

mozilla::gfx::Config* Factory::sConfig = nullptr;

void
Factory::Init(const Config& aConfig)
{
  MOZ_ASSERT(!sConfig);
  sConfig = new Config(aConfig);

#ifdef MOZ_ENABLE_FREETYPE
  mFTLock = new Mutex("Factory::mFTLock");
#endif
}

void
Factory::ShutDown()
{
  if (sConfig) {
    delete sConfig->mLogForwarder;
    delete sConfig;
    sConfig = nullptr;
  }

#ifdef MOZ_ENABLE_FREETYPE
  mFTLibrary = nullptr;
  if (mFTLock) {
    delete mFTLock;
    mFTLock = nullptr;
  }
#endif
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

bool
Factory::HasSSE4()
{
#if defined(__SSE4__)
  // gcc with -msse2 (default on OSX and x86-64)
  // cl.exe with -arch:SSE2 (default on x64 compiler)
  return true;
#elif defined(HAVE_CPU_DETECTION)
  static enum {
    UNINITIALIZED,
    NO_SSE4,
    HAS_SSE4
  } sDetectionState = UNINITIALIZED;

  if (sDetectionState == UNINITIALIZED) {
    sDetectionState = HasCPUIDBit(1u, ecx, (1u << 19)) ? HAS_SSE4 : NO_SSE4;
  }
  return sDetectionState == HAS_SSE4;
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
    return MakeAndAddRef<DrawTargetWrapAndRecord>(mRecorder, retVal);
  }

  if (!retVal) {
    // Failed
    gfxCriticalError(LoggerOptionsBasedOnSize(aSize)) << "Failed to create DrawTarget, Type: " << int(aBackend) << " Size: " << aSize;
  }

  return retVal.forget();
}

already_AddRefed<DrawTarget>
Factory::CreateWrapAndRecordDrawTarget(DrawEventRecorder *aRecorder, DrawTarget *aDT)
{
  return MakeAndAddRef<DrawTargetWrapAndRecord>(aRecorder, aDT);
}

already_AddRefed<DrawTarget>
Factory::CreateRecordingDrawTarget(DrawEventRecorder *aRecorder, DrawTarget *aDT, IntSize aSize)
{
  return MakeAndAddRef<DrawTargetRecording>(aRecorder, aDT, aSize);
}

already_AddRefed<DrawTargetCapture>
Factory::CreateCaptureDrawTarget(BackendType aBackend, const IntSize& aSize, SurfaceFormat aFormat)
{
  return MakeAndAddRef<DrawTargetCaptureImpl>(aBackend, aSize, aFormat);
}

already_AddRefed<DrawTargetCapture>
Factory::CreateCaptureDrawTargetForData(BackendType aBackend,
                                        const IntSize &aSize,
                                        SurfaceFormat aFormat,
                                        int32_t aStride,
                                        size_t aSurfaceAllocationSize)
{
  MOZ_ASSERT(aSurfaceAllocationSize && aStride);

  BackendType type = aBackend;
  if (!Factory::DoesBackendSupportDataDrawtarget(aBackend)) {
    type = BackendType::SKIA;
  }

  RefPtr<DrawTargetCaptureImpl> dt = new DrawTargetCaptureImpl(type, aSize, aFormat);
  dt->InitForData(aStride, aSurfaceAllocationSize);
  return dt.forget();
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
    return MakeAndAddRef<DrawTargetWrapAndRecord>(mRecorder, retVal, true);
  }

  if (!retVal) {
    gfxCriticalNote << "Failed to create DrawTarget, Type: " << int(aBackend) << " Size: " << aSize << ", Data: " << hexa((void *)aData) << ", Stride: " << aStride;
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
  case BackendType::WEBRENDER_TEXT:
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
Factory::CreateScaledFontForNativeFont(const NativeFont &aNativeFont,
                                       const RefPtr<UnscaledFont>& aUnscaledFont,
                                       Float aSize,
                                       cairo_scaled_font_t* aScaledFont)
{
  switch (aNativeFont.mType) {
#ifdef WIN32
  case NativeFontType::GDI_LOGFONT:
    {
      RefPtr<ScaledFontWin> font = MakeAndAddRef<ScaledFontWin>(static_cast<LOGFONT*>(aNativeFont.mFont), aUnscaledFont, aSize);
#ifdef USE_CAIRO
      if (aScaledFont) {
        font->SetCairoScaledFont(aScaledFont);
      } else {
        font->PopulateCairoScaledFont();
      }
#endif
      return font.forget();
    }
#elif defined(MOZ_WIDGET_GTK)
  case NativeFontType::FONTCONFIG_PATTERN:
    return MakeAndAddRef<ScaledFontFontconfig>(aScaledFont, static_cast<FcPattern*>(aNativeFont.mFont), aUnscaledFont, aSize);
#elif defined(MOZ_WIDGET_ANDROID)
  case NativeFontType::FREETYPE_FACE:
    return MakeAndAddRef<ScaledFontFreeType>(aScaledFont, static_cast<FT_Face>(aNativeFont.mFont), aUnscaledFont, aSize);
#endif
  default:
    gfxWarning() << "Invalid native font type specified.";
    return nullptr;
  }
}

already_AddRefed<NativeFontResource>
Factory::CreateNativeFontResource(uint8_t *aData, uint32_t aSize, BackendType aBackendType, FontType aFontType, void* aFontContext)
{
  switch (aFontType) {
#ifdef WIN32
  case FontType::DWRITE:
    {
      bool needsCairo = aBackendType == BackendType::CAIRO ||
                        aBackendType == BackendType::SKIA;
      return NativeFontResourceDWrite::Create(aData, aSize, needsCairo);
    }
  case FontType::GDI:
    return NativeFontResourceGDI::Create(aData, aSize);
#elif defined(XP_DARWIN)
  case FontType::MAC:
    return NativeFontResourceMac::Create(aData, aSize);
#elif defined(MOZ_WIDGET_GTK)
  case FontType::FONTCONFIG:
    return NativeFontResourceFontconfig::Create(aData, aSize,
                                                static_cast<FT_Library>(aFontContext));
#elif defined(MOZ_WIDGET_ANDROID)
  case FontType::FREETYPE:
    return NativeFontResourceFreeType::Create(aData, aSize,
                                              static_cast<FT_Library>(aFontContext));
#endif
  default:
    gfxWarning() << "Unable to create requested font resource from truetype data";
    return nullptr;
  }
}

already_AddRefed<UnscaledFont>
Factory::CreateUnscaledFontFromFontDescriptor(FontType aType, const uint8_t* aData, uint32_t aDataLength, uint32_t aIndex)
{
  switch (aType) {
#ifdef WIN32
  case FontType::GDI:
    return UnscaledFontGDI::CreateFromFontDescriptor(aData, aDataLength, aIndex);
#endif
#ifdef MOZ_WIDGET_GTK
  case FontType::FONTCONFIG:
    return UnscaledFontFontconfig::CreateFromFontDescriptor(aData, aDataLength, aIndex);
#endif
  default:
    gfxWarning() << "Invalid type specified for UnscaledFont font descriptor";
    return nullptr;
  }
}

#ifdef XP_DARWIN
already_AddRefed<ScaledFont>
Factory::CreateScaledFontForMacFont(CGFontRef aCGFont,
                                    const RefPtr<UnscaledFont>& aUnscaledFont,
                                    Float aSize,
                                    const Color& aFontSmoothingBackgroundColor,
                                    bool aUseFontSmoothing,
                                    bool aApplySyntheticBold)
{
  return MakeAndAddRef<ScaledFontMac>(
    aCGFont, aUnscaledFont, aSize, false,
    aFontSmoothingBackgroundColor, aUseFontSmoothing,
    aApplySyntheticBold);
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
    retVal = new DrawTargetWrapAndRecord(mRecorder, retVal);
  }

  return retVal.forget();
}


#ifdef MOZ_ENABLE_FREETYPE
void
Factory::SetFTLibrary(FT_Library aFTLibrary)
{
  mFTLibrary = aFTLibrary;
}

FT_Library
Factory::GetFTLibrary()
{
  MOZ_ASSERT(mFTLibrary);
  return mFTLibrary;
}

FT_Library
Factory::NewFTLibrary()
{
  FT_Library library;
  if (FT_Init_FreeType(&library) != FT_Err_Ok) {
    return nullptr;
  }
  return library;
}

void
Factory::ReleaseFTLibrary(FT_Library aFTLibrary)
{
  FT_Done_FreeType(aFTLibrary);
}

void
Factory::LockFTLibrary(FT_Library aFTLibrary)
{
  MOZ_ASSERT(mFTLock);
  mFTLock->Lock();
}

void
Factory::UnlockFTLibrary(FT_Library aFTLibrary)
{
  MOZ_ASSERT(mFTLock);
  mFTLock->Unlock();
}

FT_Face
Factory::NewFTFace(FT_Library aFTLibrary, const char* aFileName, int aFaceIndex)
{
  MOZ_ASSERT(mFTLock);
  MutexAutoLock lock(*mFTLock);
  if (!aFTLibrary) {
    aFTLibrary = mFTLibrary;
  }
  FT_Face face;
  if (FT_New_Face(aFTLibrary, aFileName, aFaceIndex, &face) != FT_Err_Ok) {
    return nullptr;
  }
  return face;
}

FT_Face
Factory::NewFTFaceFromData(FT_Library aFTLibrary, const uint8_t* aData, size_t aDataSize, int aFaceIndex)
{
  MOZ_ASSERT(mFTLock);
  MutexAutoLock lock(*mFTLock);
  if (!aFTLibrary) {
    aFTLibrary = mFTLibrary;
  }
  FT_Face face;
  if (FT_New_Memory_Face(aFTLibrary, aData, aDataSize, aFaceIndex, &face) != FT_Err_Ok) {
    return nullptr;
  }
  return face;
}

void
Factory::ReleaseFTFace(FT_Face aFace)
{
  // May be called during shutdown when the lock is already destroyed.
  // However, there are no other threads using the face by this point,
  // so it is safe to skip locking if the lock is not around.
  if (mFTLock) {
    mFTLock->Lock();
  }
  FT_Done_Face(aFace);
  if (mFTLock) {
    mFTLock->Unlock();
  }
}

FT_Error
Factory::LoadFTGlyph(FT_Face aFace, uint32_t aGlyphIndex, int32_t aFlags)
{
  MOZ_ASSERT(mFTLock);
  MutexAutoLock lock(*mFTLock);
  return FT_Load_Glyph(aFace, aGlyphIndex, aFlags);
}
#endif

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
      retVal = new DrawTargetWrapAndRecord(mRecorder, retVal, true);
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
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  // D2DFactory already takes the device lock, so we get the factory before
  // entering the lock scope.
  RefPtr<ID2D1Factory1> factory = D2DFactory();

  StaticMutexAutoLock lock(mDeviceLock);

  mD3D11Device = aDevice;

  if (mD2D1Device) {
    mD2D1Device = nullptr;
  }

  if (!aDevice) {
    return true;
  }

  RefPtr<IDXGIDevice> device;
  aDevice->QueryInterface((IDXGIDevice**)getter_AddRefs(device));

  RefPtr<ID2D1Device> d2dDevice;
  HRESULT hr = factory->CreateDevice(device, getter_AddRefs(d2dDevice));
  if (FAILED(hr)) {
    gfxCriticalError() << "[D2D1] Failed to create gfx factory's D2D1 device, code: " << hexa(hr);

    mD3D11Device = nullptr;
    return false;
  }

  mDeviceSeq++;
  mD2D1Device = d2dDevice;
  return true;
}

RefPtr<ID3D11Device>
Factory::GetDirect3D11Device()
{
  StaticMutexAutoLock lock(mDeviceLock);
  return mD3D11Device;
}

RefPtr<ID2D1Device>
Factory::GetD2D1Device(uint32_t* aOutSeqNo)
{
  StaticMutexAutoLock lock(mDeviceLock);
  if (aOutSeqNo) {
    *aOutSeqNo = mDeviceSeq;
  }
  return mD2D1Device.get();
}

bool
Factory::HasD2D1Device()
{
  return !!GetD2D1Device();
}

RefPtr<IDWriteFactory>
Factory::GetDWriteFactory()
{
  StaticMutexAutoLock lock(mDeviceLock);
  return mDWriteFactory;
}

RefPtr<IDWriteFactory>
Factory::EnsureDWriteFactory()
{
  StaticMutexAutoLock lock(mDeviceLock);

  if (mDWriteFactoryInitialized) {
    return mDWriteFactory;
  }

  mDWriteFactoryInitialized = true;

  HMODULE dwriteModule = LoadLibraryW(L"dwrite.dll");
  decltype(DWriteCreateFactory)* createDWriteFactory = (decltype(DWriteCreateFactory)*)
    GetProcAddress(dwriteModule, "DWriteCreateFactory");

  if (!createDWriteFactory) {
    gfxWarning() << "Failed to locate DWriteCreateFactory function.";
    return nullptr;
  }

  HRESULT hr = createDWriteFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                   reinterpret_cast<IUnknown**>(&mDWriteFactory));

  if (FAILED(hr)) {
    gfxWarning() << "Failed to create DWrite Factory.";
  }

  return mDWriteFactory;
}

bool
Factory::SupportsD2D1()
{
  return !!D2DFactory();
}

BYTE sSystemTextQuality = CLEARTYPE_QUALITY;
void
Factory::UpdateSystemTextQuality()
{
#ifdef WIN32
  gfx::UpdateSystemTextQuality();
#endif
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
  StaticMutexAutoLock lock(mDeviceLock);
  if (mD2D1Device) {
    mD2D1Device = nullptr;
  }
  DrawTargetD2D1::CleanupD2D();
}

already_AddRefed<ScaledFont>
Factory::CreateScaledFontForDWriteFont(IDWriteFontFace* aFontFace,
                                       const gfxFontStyle* aStyle,
                                       const RefPtr<UnscaledFont>& aUnscaledFont,
                                       float aSize,
                                       bool aUseEmbeddedBitmap,
                                       bool aForceGDIMode,
                                       IDWriteRenderingParams* aParams,
                                       Float aGamma,
                                       Float aContrast)
{
  return MakeAndAddRef<ScaledFontDWrite>(aFontFace, aUnscaledFont, aSize,
                                         aUseEmbeddedBitmap, aForceGDIMode,
                                         aParams, aGamma, aContrast,
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
    return MakeAndAddRef<DrawTargetWrapAndRecord>(mRecorder, retVal, true);
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

  DataSourceSurface::MappedSurface srcMap;
  DataSourceSurface::MappedSurface destMap;
  if (!aSource->Map(DataSourceSurface::MapType::READ, &srcMap) ||
    !aDest->Map(DataSourceSurface::MapType::WRITE, &destMap)) {
    MOZ_ASSERT(false, "CopyDataSourceSurface: Failed to map surface.");
    return;
  }

  SwizzleData(srcMap.mData, srcMap.mStride, aSource->GetFormat(),
              destMap.mData, destMap.mStride, aDest->GetFormat(),
              aSource->GetSize());

  aSource->Unmap();
  aDest->Unmap();
}

already_AddRefed<DrawEventRecorder>
Factory::CreateEventRecorderForFile(const char_type* aFilename)
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
