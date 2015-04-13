/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_PLATFORM_H
#define GFX_PLATFORM_H

#include "prlog.h"
#include "mozilla/gfx/Types.h"
#include "nsTArray.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"

#include "gfxPrefs.h"
#include "gfxTypes.h"
#include "gfxFontFamilyList.h"
#include "gfxBlur.h"
#include "nsRect.h"

#include "qcms.h"

#include "mozilla/RefPtr.h"
#include "GfxInfoCollector.h"

#include "mozilla/layers/CompositorTypes.h"

class gfxASurface;
class gfxFont;
class gfxFontGroup;
struct gfxFontStyle;
class gfxUserFontSet;
class gfxFontEntry;
class gfxPlatformFontList;
class gfxTextRun;
class nsIURI;
class nsIAtom;
class nsIObserver;
class SRGBOverrideObserver;
struct gfxRGBA;

namespace mozilla {
namespace gl {
class GLContext;
class SkiaGLGlue;
}
namespace gfx {
class DrawTarget;
class SourceSurface;
class DataSourceSurface;
class ScaledFont;
class DrawEventRecorder;
class VsyncSource;

inline uint32_t
BackendTypeBit(BackendType b)
{
  return 1 << uint8_t(b);
}
}
}

#define MOZ_PERFORMANCE_WARNING(module, ...) \
  do { \
    if (gfxPrefs::PerfWarnings()) { \
      printf_stderr("[" module "] " __VA_ARGS__); \
    } \
  } while (0)

extern cairo_user_data_key_t kDrawTarget;

// pref lang id's for font prefs
// !!! needs to match the list of pref font.default.xx entries listed in all.js !!!
// !!! don't use as bit mask, this may grow larger !!!

enum eFontPrefLang {
    eFontPrefLang_Western     =  0,
    eFontPrefLang_Japanese    =  1,
    eFontPrefLang_ChineseTW   =  2,
    eFontPrefLang_ChineseCN   =  3,
    eFontPrefLang_ChineseHK   =  4,
    eFontPrefLang_Korean      =  5,
    eFontPrefLang_Cyrillic    =  6,
    eFontPrefLang_Greek       =  7,
    eFontPrefLang_Thai        =  8,
    eFontPrefLang_Hebrew      =  9,
    eFontPrefLang_Arabic      = 10,
    eFontPrefLang_Devanagari  = 11,
    eFontPrefLang_Tamil       = 12,
    eFontPrefLang_Armenian    = 13,
    eFontPrefLang_Bengali     = 14,
    eFontPrefLang_Canadian    = 15,
    eFontPrefLang_Ethiopic    = 16,
    eFontPrefLang_Georgian    = 17,
    eFontPrefLang_Gujarati    = 18,
    eFontPrefLang_Gurmukhi    = 19,
    eFontPrefLang_Khmer       = 20,
    eFontPrefLang_Malayalam   = 21,
    eFontPrefLang_Oriya       = 22,
    eFontPrefLang_Telugu      = 23,
    eFontPrefLang_Kannada     = 24,
    eFontPrefLang_Sinhala     = 25,
    eFontPrefLang_Tibetan     = 26,

    eFontPrefLang_Others      = 27, // x-unicode

    eFontPrefLang_CJKSet      = 28  // special code for CJK set
};

enum eCMSMode {
    eCMSMode_Off          = 0,     // No color management
    eCMSMode_All          = 1,     // Color manage everything
    eCMSMode_TaggedOnly   = 2,     // Color manage tagged Images Only
    eCMSMode_AllCount     = 3
};

enum eGfxLog {
    // all font enumerations, localized names, fullname/psnames, cmap loads
    eGfxLog_fontlist         = 0,
    // timing info on font initialization
    eGfxLog_fontinit         = 1,
    // dump text runs, font matching, system fallback for content
    eGfxLog_textrun          = 2,
    // dump text runs, font matching, system fallback for chrome
    eGfxLog_textrunui        = 3,
    // dump cmap coverage data as they are loaded
    eGfxLog_cmapdata         = 4,
    // text perf data
    eGfxLog_textperf         = 5
};

// when searching through pref langs, max number of pref langs
const uint32_t kMaxLenPrefLangList = 32;

#define UNINITIALIZED_VALUE  (-1)

inline const char*
GetBackendName(mozilla::gfx::BackendType aBackend)
{
  switch (aBackend) {
      case mozilla::gfx::BackendType::DIRECT2D:
        return "direct2d";
      case mozilla::gfx::BackendType::COREGRAPHICS_ACCELERATED:
        return "quartz accelerated";
      case mozilla::gfx::BackendType::COREGRAPHICS:
        return "quartz";
      case mozilla::gfx::BackendType::CAIRO:
        return "cairo";
      case mozilla::gfx::BackendType::SKIA:
        return "skia";
      case mozilla::gfx::BackendType::RECORDING:
        return "recording";
      case mozilla::gfx::BackendType::DIRECT2D1_1:
        return "direct2d 1.1";
      case mozilla::gfx::BackendType::NONE:
        return "none";
  }
  MOZ_CRASH("Incomplete switch");
}

enum class DeviceResetReason
{
  OK = 0,
  HUNG,
  REMOVED,
  RESET,
  DRIVER_ERROR,
  INVALID_CALL,
  OUT_OF_MEMORY,
  UNKNOWN
};

class gfxPlatform {
    friend class SRGBOverrideObserver;

public:
    typedef mozilla::gfx::Color Color;
    typedef mozilla::gfx::DataSourceSurface DataSourceSurface;
    typedef mozilla::gfx::DrawTarget DrawTarget;
    typedef mozilla::gfx::IntSize IntSize;
    typedef mozilla::gfx::SourceSurface SourceSurface;

    /**
     * Return a pointer to the current active platform.
     * This is a singleton; it contains mostly convenience
     * functions to obtain platform-specific objects.
     */
    static gfxPlatform *GetPlatform();


    /**
     * Shut down Thebes.
     * Init() arranges for this to be called at an appropriate time.
     */
    static void Shutdown();

    static void InitLayersIPC();
    static void ShutdownLayersIPC();

    /**
     * Create an offscreen surface of the given dimensions
     * and image format.
     */
    virtual already_AddRefed<gfxASurface>
      CreateOffscreenSurface(const IntSize& size,
                             gfxContentType contentType) = 0;

    /**
     * Beware that these methods may return DrawTargets which are not fully supported
     * on the current platform and might fail silently in subtle ways. This is a massive
     * potential footgun. You should only use these methods for canvas drawing really.
     * Use extreme caution if you use them for content where you are not 100% sure we
     * support the DrawTarget we get back.
     * See SupportsAzureContentForDrawTarget.
     */
    virtual mozilla::TemporaryRef<DrawTarget>
      CreateDrawTargetForSurface(gfxASurface *aSurface, const mozilla::gfx::IntSize& aSize);

    virtual mozilla::TemporaryRef<DrawTarget>
      CreateDrawTargetForUpdateSurface(gfxASurface *aSurface, const mozilla::gfx::IntSize& aSize);

    /*
     * Creates a SourceSurface for a gfxASurface. This function does no caching,
     * so the caller should cache the gfxASurface if it will be used frequently.
     * The returned surface keeps a reference to aTarget, so it is OK to keep the
     * surface, even if aTarget changes.
     * aTarget should not keep a reference to the returned surface because that
     * will cause a cycle.
     *
     * This function is static so that it can be accessed from
     * PluginInstanceChild (where we can't call gfxPlatform::GetPlatform()
     * because the prefs service can only be accessed from the main process).
     */
    static mozilla::TemporaryRef<SourceSurface>
      GetSourceSurfaceForSurface(mozilla::gfx::DrawTarget *aTarget, gfxASurface *aSurface);

    static void ClearSourceSurfaceForSurface(gfxASurface *aSurface);

    static mozilla::TemporaryRef<DataSourceSurface>
        GetWrappedDataSourceSurface(gfxASurface *aSurface);

    virtual mozilla::TemporaryRef<mozilla::gfx::ScaledFont>
      GetScaledFontForFont(mozilla::gfx::DrawTarget* aTarget, gfxFont *aFont);

    mozilla::TemporaryRef<DrawTarget>
      CreateOffscreenContentDrawTarget(const mozilla::gfx::IntSize& aSize, mozilla::gfx::SurfaceFormat aFormat);

    mozilla::TemporaryRef<DrawTarget>
      CreateOffscreenCanvasDrawTarget(const mozilla::gfx::IntSize& aSize, mozilla::gfx::SurfaceFormat aFormat);

    virtual mozilla::TemporaryRef<DrawTarget>
      CreateDrawTargetForData(unsigned char* aData, const mozilla::gfx::IntSize& aSize, 
                              int32_t aStride, mozilla::gfx::SurfaceFormat aFormat);

    /**
     * Returns true if rendering to data surfaces produces the same results as
     * rendering to offscreen surfaces on this platform, making it safe to
     * render content to data surfaces. This is generally false on platforms
     * which use different backends for each type of DrawTarget.
     */
    virtual bool CanRenderContentToDataSurface() const {
      return false;
    }

    /**
     * Returns true if we should use Azure to render content with aTarget. For
     * example, it is possible that we are using Direct2D for rendering and thus
     * using Azure. But we want to render to a CairoDrawTarget, in which case
     * SupportsAzureContent will return true but SupportsAzureContentForDrawTarget
     * will return false.
     */
    bool SupportsAzureContentForDrawTarget(mozilla::gfx::DrawTarget* aTarget);

    bool SupportsAzureContentForType(mozilla::gfx::BackendType aType) {
      return BackendTypeBit(aType) & mContentBackendBitmask;
    }

    /// This function lets us know if the current preferences/platform
    /// combination allows for both accelerated and not accelerated canvas
    /// implementations.  If it does, and other relevant preferences are
    /// asking for it, we will examine the commands in the first few seconds
    /// of the canvas usage, and potentially change to accelerated or
    /// non-accelerated canvas.
    virtual bool HaveChoiceOfHWAndSWCanvas();

    virtual bool UseAcceleratedSkiaCanvas();
    virtual void InitializeSkiaCacheLimits();

    /// These should be used instead of directly accessing the preference,
    /// as different platforms may override the behaviour.
    virtual bool UseProgressivePaint() { return gfxPrefs::ProgressivePaintDoNotUseDirectly(); }

    void GetAzureBackendInfo(mozilla::widget::InfoObject &aObj) {
      aObj.DefineProperty("AzureCanvasBackend", GetBackendName(mPreferredCanvasBackend));
      aObj.DefineProperty("AzureSkiaAccelerated", UseAcceleratedSkiaCanvas());
      aObj.DefineProperty("AzureFallbackCanvasBackend", GetBackendName(mFallbackCanvasBackend));
      aObj.DefineProperty("AzureContentBackend", GetBackendName(mContentBackend));
    }
    void GetApzSupportInfo(mozilla::widget::InfoObject& aObj);

    mozilla::gfx::BackendType GetContentBackend() {
      return mContentBackend;
    }

    mozilla::gfx::BackendType GetPreferredCanvasBackend() {
      return mPreferredCanvasBackend;
    }

    /*
     * Font bits
     */

    virtual void SetupClusterBoundaries(gfxTextRun *aTextRun, const char16_t *aString);

    /**
     * Fill aListOfFonts with the results of querying the list of font names
     * that correspond to the given language group or generic font family
     * (or both, or neither).
     */
    virtual nsresult GetFontList(nsIAtom *aLangGroup,
                                 const nsACString& aGenericFamily,
                                 nsTArray<nsString>& aListOfFonts);

    int GetTileWidth();
    int GetTileHeight();
    void SetTileSize(int aWidth, int aHeight);
    /**
     * Calling this function will compute and set the ideal tile size for the
     * platform. This should only be called in the parent process; child processes
     * should be updated via SetTileSize to match the value computed in the parent.
     */
    void ComputeTileSize();

    /**
     * Rebuilds the any cached system font lists
     */
    virtual nsresult UpdateFontList();

    /**
     * Create the platform font-list object (gfxPlatformFontList concrete subclass).
     * This function is responsible to create the appropriate subclass of
     * gfxPlatformFontList *and* to call its InitFontList() method.
     */
    virtual gfxPlatformFontList *CreatePlatformFontList() {
        NS_NOTREACHED("oops, this platform doesn't have a gfxPlatformFontList implementation");
        return nullptr;
    }

    /**
     * Resolving a font name to family name. The result MUST be in the result of GetFontList().
     * If the name doesn't in the system, aFamilyName will be empty string, but not failed.
     */
    virtual nsresult GetStandardFamilyName(const nsAString& aFontName, nsAString& aFamilyName) = 0;

    /**
     * Create the appropriate platform font group
     */
    virtual gfxFontGroup
    *CreateFontGroup(const mozilla::FontFamilyList& aFontFamilyList,
                     const gfxFontStyle *aStyle,
                     gfxUserFontSet *aUserFontSet) = 0;
                                          
                                          
    /**
     * Look up a local platform font using the full font face name.
     * (Needed to support @font-face src local().)
     * Ownership of the returned gfxFontEntry is passed to the caller,
     * who must either AddRef() or delete.
     */
    virtual gfxFontEntry* LookupLocalFont(const nsAString& aFontName,
                                          uint16_t aWeight,
                                          int16_t aStretch,
                                          bool aItalic)
    { return nullptr; }

    /**
     * Activate a platform font.  (Needed to support @font-face src url().)
     * aFontData is a NS_Malloc'ed block that must be freed by this function
     * (or responsibility passed on) when it is no longer needed; the caller
     * will NOT free it.
     * Ownership of the returned gfxFontEntry is passed to the caller,
     * who must either AddRef() or delete.
     */
    virtual gfxFontEntry* MakePlatformFont(const nsAString& aFontName,
                                           uint16_t aWeight,
                                           int16_t aStretch,
                                           bool aItalic,
                                           const uint8_t* aFontData,
                                           uint32_t aLength);

    /**
     * Whether to allow downloadable fonts via @font-face rules
     */
    bool DownloadableFontsEnabled();

    /**
     * True when hinting should be enabled.  This setting shouldn't
     * change per gecko process, while the process is live.  If so the
     * results are not defined.
     *
     * NB: this bit is only honored by the FT2 backend, currently.
     */
    virtual bool FontHintingEnabled() { return true; }

    /**
     * True when zooming should not require reflow, so glyph metrics and
     * positioning should not be adjusted for device pixels.
     * If this is TRUE, then FontHintingEnabled() should be FALSE,
     * but the converse is not necessarily required; in particular,
     * B2G always has FontHintingEnabled FALSE, but RequiresLinearZoom
     * is only true for the browser process, not Gaia or other apps.
     *
     * Like FontHintingEnabled (above), this setting shouldn't
     * change per gecko process, while the process is live.  If so the
     * results are not defined.
     *
     * NB: this bit is only honored by the FT2 backend, currently.
     */
    virtual bool RequiresLinearZoom() { return false; }

    /**
     * Whether to check all font cmaps during system font fallback
     */
    bool UseCmapsDuringSystemFallback();

    /**
     * Whether to render SVG glyphs within an OpenType font wrapper
     */
    bool OpenTypeSVGEnabled();

    /**
     * Max character length of words in the word cache
     */
    uint32_t WordCacheCharLimit();

    /**
     * Max number of entries in word cache
     */
    uint32_t WordCacheMaxEntries();

    /**
     * Whether to use the SIL Graphite rendering engine
     * (for fonts that include Graphite tables)
     */
    bool UseGraphiteShaping();

    // check whether format is supported on a platform or not (if unclear, returns true)
    virtual bool IsFontFormatSupported(nsIURI *aFontURI, uint32_t aFormatFlags) { return false; }

    virtual bool DidRenderingDeviceReset(DeviceResetReason* aResetReason = nullptr) { return false; }

    void GetPrefFonts(nsIAtom *aLanguage, nsString& array, bool aAppendUnicode = true);

    // in some situations, need to make decisions about ambiguous characters, may need to look at multiple pref langs
    void GetLangPrefs(eFontPrefLang aPrefLangs[], uint32_t &aLen, eFontPrefLang aCharLang, eFontPrefLang aPageLang);
    
    /**
     * Iterate over pref fonts given a list of lang groups.  For a single lang
     * group, multiple pref fonts are possible.  If error occurs, returns false,
     * true otherwise.  Callback returns false to abort process.
     */
    typedef bool (*PrefFontCallback) (eFontPrefLang aLang, const nsAString& aName,
                                        void *aClosure);
    static bool ForEachPrefFont(eFontPrefLang aLangArray[], uint32_t aLangArrayLen,
                                  PrefFontCallback aCallback,
                                  void *aClosure);

    // convert a lang group to enum constant (i.e. "zh-TW" ==> eFontPrefLang_ChineseTW)
    static eFontPrefLang GetFontPrefLangFor(const char* aLang);

    // convert a lang group atom to enum constant
    static eFontPrefLang GetFontPrefLangFor(nsIAtom *aLang);

    // convert a enum constant to lang group string (i.e. eFontPrefLang_ChineseTW ==> "zh-TW")
    static const char* GetPrefLangName(eFontPrefLang aLang);
   
    // map a Unicode range (based on char code) to a font language for Preferences
    static eFontPrefLang GetFontPrefLangFor(uint8_t aUnicodeRange);

    // returns true if a pref lang is CJK
    static bool IsLangCJK(eFontPrefLang aLang);
    
    // helper method to add a pref lang to an array, if not already in array
    static void AppendPrefLang(eFontPrefLang aPrefLangs[], uint32_t& aLen, eFontPrefLang aAddLang);

    // returns a list of commonly used fonts for a given character
    // these are *possible* matches, no cmap-checking is done at this level
    virtual void GetCommonFallbackFonts(uint32_t /*aCh*/, uint32_t /*aNextCh*/,
                                        int32_t /*aRunScript*/,
                                        nsTArray<const char*>& /*aFontList*/)
    {
        // platform-specific override, by default do nothing
    }

    static bool OffMainThreadCompositingEnabled();

    static bool CanUseDirect3D9();
    static bool CanUseDirect3D11();
    static bool CanUseHardwareVideoDecoding();
    static bool CanUseDirect3D11ANGLE();

    /**
     * Is it possible to use buffer rotation.  Note that these
     * check the preference, but also allow for the override to
     * disable it using DisableBufferRotation.
     */
    static bool BufferRotationEnabled();
    static void DisableBufferRotation();

    /**
     * Are we going to try color management?
     */
    static eCMSMode GetCMSMode();

    /**
     * Determines the rendering intent for color management.
     *
     * If the value in the pref gfx.color_management.rendering_intent is a
     * valid rendering intent as defined in gfx/qcms/qcms.h, that
     * value is returned. Otherwise, -1 is returned and the embedded intent
     * should be used.
     *
     * See bug 444014 for details.
     */
    static int GetRenderingIntent();

    /**
     * Convert a pixel using a cms transform in an endian-aware manner.
     *
     * Sets 'out' to 'in' if transform is nullptr.
     */
    static void TransformPixel(const Color& in, Color& out, qcms_transform *transform);

    /**
     * Return the output device ICC profile.
     */
    static qcms_profile* GetCMSOutputProfile();

    /**
     * Return the sRGB ICC profile.
     */
    static qcms_profile* GetCMSsRGBProfile();

    /**
     * Return sRGB -> output device transform.
     */
    static qcms_transform* GetCMSRGBTransform();

    /**
     * Return output -> sRGB device transform.
     */
    static qcms_transform* GetCMSInverseRGBTransform();

    /**
     * Return sRGBA -> output device transform.
     */
    static qcms_transform* GetCMSRGBATransform();

    virtual void FontsPrefsChanged(const char *aPref);

    int32_t GetBidiNumeralOption();

    /**
     * Returns a 1x1 surface that can be used to create graphics contexts
     * for measuring text etc as if they will be rendered to the screen
     */
    gfxASurface* ScreenReferenceSurface() { return mScreenReferenceSurface; }
    mozilla::gfx::DrawTarget* ScreenReferenceDrawTarget() { return mScreenReferenceDrawTarget; }

    virtual mozilla::gfx::SurfaceFormat Optimal2DFormatForContent(gfxContentType aContent);

    virtual gfxImageFormat OptimalFormatForContent(gfxContentType aContent);

    virtual gfxImageFormat GetOffscreenFormat()
    { return gfxImageFormat::RGB24; }

    /**
     * Returns a logger if one is available and logging is enabled
     */
    static PRLogModuleInfo* GetLog(eGfxLog aWhichLog);

    virtual int GetScreenDepth() const;

    /**
     * Return the layer debugging options to use browser-wide.
     */
    mozilla::layers::DiagnosticTypes GetLayerDiagnosticTypes();

    static nsIntRect FrameCounterBounds() {
      int bits = 16;
      int sizeOfBit = 3;
      return nsIntRect(0, 0, bits * sizeOfBit, sizeOfBit);
    }

    mozilla::gl::SkiaGLGlue* GetSkiaGLGlue();
    void PurgeSkiaCache();

    virtual bool IsInGonkEmulator() const { return false; }

    static bool UsesOffMainThreadCompositing();

    bool HasEnoughTotalSystemMemoryForSkiaGL();

    /**
     * Get the hardware vsync source for each platform.
     * Should only exist and be valid on the parent process
     */
    virtual mozilla::gfx::VsyncSource* GetHardwareVsync() {
      MOZ_ASSERT(mVsyncSource != nullptr);
      MOZ_ASSERT(XRE_IsParentProcess());
      return mVsyncSource;
    }

    /**
     * True if layout rendering should use ASAP mode, which means
     * the refresh driver and compositor should render ASAP.
     * Used for talos testing purposes
     */
    static bool IsInLayoutAsapMode();

    /**
     * Used to test which input types are handled via APZ.
     */
    virtual bool SupportsApzWheelInput() {
      return false;
    }
    virtual bool SupportsApzTouchInput() {
      return false;
    }

protected:
    gfxPlatform();
    virtual ~gfxPlatform();

    void AppendCJKPrefLangs(eFontPrefLang aPrefLangs[], uint32_t &aLen,
                            eFontPrefLang aCharLang, eFontPrefLang aPageLang);

    /**
     * Initialized hardware vsync based on each platform.
     */
    virtual already_AddRefed<mozilla::gfx::VsyncSource> CreateHardwareVsyncSource();

    /**
     * Helper method, creates a draw target for a specific Azure backend.
     * Used by CreateOffscreenDrawTarget.
     */
    mozilla::TemporaryRef<DrawTarget>
      CreateDrawTargetForBackend(mozilla::gfx::BackendType aBackend,
                                 const mozilla::gfx::IntSize& aSize,
                                 mozilla::gfx::SurfaceFormat aFormat);

    /**
     * Initialise the preferred and fallback canvas backends
     * aBackendBitmask specifies the backends which are acceptable to the caller.
     * The backend used is determined by aBackendBitmask and the order specified
     * by the gfx.canvas.azure.backends pref.
     */
    void InitBackendPrefs(uint32_t aCanvasBitmask, mozilla::gfx::BackendType aCanvasDefault,
                          uint32_t aContentBitmask, mozilla::gfx::BackendType aContentDefault);

    /**
     * returns the first backend named in the pref gfx.canvas.azure.backends
     * which is a component of aBackendBitmask, a bitmask of backend types
     */
    static mozilla::gfx::BackendType GetCanvasBackendPref(uint32_t aBackendBitmask);

    /**
     * returns the first backend named in the pref gfx.content.azure.backend
     * which is a component of aBackendBitmask, a bitmask of backend types
     */
    static mozilla::gfx::BackendType GetContentBackendPref(uint32_t &aBackendBitmask);

    /**
     * Will return the first backend named in aBackendPrefName
     * allowed by aBackendBitmask, a bitmask of backend types.
     * It also modifies aBackendBitmask to only include backends that are
     * allowed given the prefs.
     */
    static mozilla::gfx::BackendType GetBackendPref(const char* aBackendPrefName,
                                                    uint32_t &aBackendBitmask);
    /**
     * Decode the backend enumberation from a string.
     */
    static mozilla::gfx::BackendType BackendTypeForName(const nsCString& aName);

    static mozilla::TemporaryRef<mozilla::gfx::ScaledFont>
      GetScaledFontForFontWithCairoSkia(mozilla::gfx::DrawTarget* aTarget, gfxFont* aFont);

    int8_t  mAllowDownloadableFonts;
    int8_t  mGraphiteShapingEnabled;
    int8_t  mOpenTypeSVGEnabled;

    int8_t  mBidiNumeralOption;

    // whether to always search font cmaps globally 
    // when doing system font fallback
    int8_t  mFallbackUsesCmaps;

    // max character limit for words in word cache
    int32_t mWordCacheCharLimit;

    // max number of entries in word cache
    int32_t mWordCacheMaxEntries;

    uint32_t mTotalSystemMemory;

    // Hardware vsync source. Only valid on parent process
    nsRefPtr<mozilla::gfx::VsyncSource> mVsyncSource;

    mozilla::RefPtr<mozilla::gfx::DrawTarget> mScreenReferenceDrawTarget;

private:
    /**
     * Start up Thebes.
     */
    static void Init();

    static void CreateCMSOutputProfile();

    static void GetCMSOutputProfileData(void *&mem, size_t &size);

    friend void RecordingPrefChanged(const char *aPrefName, void *aClosure);

    virtual void GetPlatformCMSOutputProfile(void *&mem, size_t &size);

    nsRefPtr<gfxASurface> mScreenReferenceSurface;
    nsTArray<uint32_t> mCJKPrefLangs;
    nsCOMPtr<nsIObserver> mSRGBOverrideObserver;
    nsCOMPtr<nsIObserver> mFontPrefsObserver;
    nsCOMPtr<nsIObserver> mMemoryPressureObserver;

    // The preferred draw target backend to use for canvas
    mozilla::gfx::BackendType mPreferredCanvasBackend;
    // The fallback draw target backend to use for canvas, if the preferred backend fails
    mozilla::gfx::BackendType mFallbackCanvasBackend;
    // The backend to use for content
    mozilla::gfx::BackendType mContentBackend;
    // Bitmask of backend types we can use to render content
    uint32_t mContentBackendBitmask;

    int mTileWidth;
    int mTileHeight;

    mozilla::widget::GfxInfoCollector<gfxPlatform> mAzureCanvasBackendCollector;
    mozilla::widget::GfxInfoCollector<gfxPlatform> mApzSupportCollector;

    mozilla::RefPtr<mozilla::gfx::DrawEventRecorder> mRecorder;
    mozilla::RefPtr<mozilla::gl::SkiaGLGlue> mSkiaGlue;
};

#endif /* GFX_PLATFORM_H */
