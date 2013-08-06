/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_PLATFORM_H
#define GFX_PLATFORM_H

#include "prlog.h"
#include "nsTArray.h"
#include "nsStringGlue.h"
#include "nsIObserver.h"

#include "gfxTypes.h"
#include "gfxASurface.h"
#include "gfxColor.h"

#include "qcms.h"

#include "gfx2DGlue.h"
#include "mozilla/RefPtr.h"
#include "GfxInfoCollector.h"

#include "mozilla/layers/CompositorTypes.h"

#ifdef XP_OS2
#undef OS2EMX_PLAIN_CHAR
#endif

class gfxImageSurface;
class gfxFont;
class gfxFontGroup;
struct gfxFontStyle;
class gfxUserFontSet;
class gfxFontEntry;
class gfxProxyFontEntry;
class gfxPlatformFontList;
class gfxTextRun;
class nsIURI;
class nsIAtom;

namespace mozilla {
namespace gl {
class GLContext;
}
}

extern cairo_user_data_key_t kDrawTarget;

// pref lang id's for font prefs
// !!! needs to match the list of pref font.default.xx entries listed in all.js !!!
// !!! don't use as bit mask, this may grow larger !!!

enum eFontPrefLang {
    eFontPrefLang_Western     =  0,
    eFontPrefLang_CentEuro    =  1,
    eFontPrefLang_Japanese    =  2,
    eFontPrefLang_ChineseTW   =  3,
    eFontPrefLang_ChineseCN   =  4,
    eFontPrefLang_ChineseHK   =  5,
    eFontPrefLang_Korean      =  6,
    eFontPrefLang_Cyrillic    =  7,
    eFontPrefLang_Baltic      =  8,
    eFontPrefLang_Greek       =  9,
    eFontPrefLang_Turkish     = 10,
    eFontPrefLang_Thai        = 11,
    eFontPrefLang_Hebrew      = 12,
    eFontPrefLang_Arabic      = 13,
    eFontPrefLang_Devanagari  = 14,
    eFontPrefLang_Tamil       = 15,
    eFontPrefLang_Armenian    = 16,
    eFontPrefLang_Bengali     = 17,
    eFontPrefLang_Canadian    = 18,
    eFontPrefLang_Ethiopic    = 19,
    eFontPrefLang_Georgian    = 20,
    eFontPrefLang_Gujarati    = 21,
    eFontPrefLang_Gurmukhi    = 22,
    eFontPrefLang_Khmer       = 23,
    eFontPrefLang_Malayalam   = 24,
    eFontPrefLang_Oriya       = 25,
    eFontPrefLang_Telugu      = 26,
    eFontPrefLang_Kannada     = 27,
    eFontPrefLang_Sinhala     = 28,
    eFontPrefLang_Tibetan     = 29,

    eFontPrefLang_LangCount   = 30, // except Others and UserDefined.

    eFontPrefLang_Others      = 30, // x-unicode
    eFontPrefLang_UserDefined = 31,

    eFontPrefLang_CJKSet      = 32, // special code for CJK set
    eFontPrefLang_AllCount    = 33
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
    eGfxLog_cmapdata         = 4
};

// when searching through pref langs, max number of pref langs
const uint32_t kMaxLenPrefLangList = 32;

#define UNINITIALIZED_VALUE  (-1)

typedef gfxASurface::gfxImageFormat gfxImageFormat;

inline const char*
GetBackendName(mozilla::gfx::BackendType aBackend)
{
  switch (aBackend) {
      case mozilla::gfx::BACKEND_DIRECT2D:
        return "direct2d";
      case mozilla::gfx::BACKEND_COREGRAPHICS_ACCELERATED:
        return "quartz accelerated";
      case mozilla::gfx::BACKEND_COREGRAPHICS:
        return "quartz";
      case mozilla::gfx::BACKEND_CAIRO:
        return "cairo";
      case mozilla::gfx::BACKEND_SKIA:
        return "skia";
      case mozilla::gfx::BACKEND_RECORDING:
        return "recording";
      case mozilla::gfx::BACKEND_DIRECT2D1_1:
        return "direct2d 1.1";
      case mozilla::gfx::BACKEND_NONE:
        return "none";
  }
  MOZ_CRASH("Incomplete switch");
}

class gfxPlatform {
public:
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

    /**
     * Create an offscreen surface of the given dimensions
     * and image format.
     */
    virtual already_AddRefed<gfxASurface> CreateOffscreenSurface(const gfxIntSize& size,
                                                                 gfxASurface::gfxContentType contentType) = 0;

    /**
     * Create an offscreen surface of the given dimensions and image format which
     * can be converted to a gfxImageSurface without copying. If we can provide
     * a platform-hosted surface, then we will return that instead of an actual
     * gfxImageSurface.
     * Sub-classes should override this method if CreateOffscreenSurface returns a
     * surface which implements GetAsImageSurface
     */
    virtual already_AddRefed<gfxASurface>
      CreateOffscreenImageSurface(const gfxIntSize& aSize,
                                  gfxASurface::gfxContentType aContentType);

    virtual already_AddRefed<gfxASurface> OptimizeImage(gfxImageSurface *aSurface,
                                                        gfxASurface::gfxImageFormat format);

    virtual mozilla::RefPtr<mozilla::gfx::DrawTarget>
      CreateDrawTargetForSurface(gfxASurface *aSurface, const mozilla::gfx::IntSize& aSize);

    virtual mozilla::RefPtr<mozilla::gfx::DrawTarget>
      CreateDrawTargetForUpdateSurface(gfxASurface *aSurface, const mozilla::gfx::IntSize& aSize);

    /*
     * Creates a SourceSurface for a gfxASurface. This function does no caching,
     * so the caller should cache the gfxASurface if it will be used frequently.
     * The returned surface keeps a reference to aTarget, so it is OK to keep the
     * surface, even if aTarget changes.
     * aTarget should not keep a reference to the returned surface because that
     * will cause a cycle.
     */
    virtual mozilla::RefPtr<mozilla::gfx::SourceSurface>
      GetSourceSurfaceForSurface(mozilla::gfx::DrawTarget *aTarget, gfxASurface *aSurface);

    static void ClearSourceSurfaceForSurface(gfxASurface *aSurface);

    virtual mozilla::TemporaryRef<mozilla::gfx::ScaledFont>
      GetScaledFontForFont(mozilla::gfx::DrawTarget* aTarget, gfxFont *aFont);

    /*
     * Cairo doesn't give us a way to create a surface pointing to a context
     * without marking it as copy on write. For canvas we want to create
     * a surface that points to what is currently being drawn by a canvas
     * without a copy thus we need to create a special case. This works on
     * most platforms with GetThebesSurfaceForDrawTarget but fails on Mac
     * because when we create the surface we vm_copy the memory and never
     * notify the context that the canvas has drawn to it thus we end up
     * with a static snapshot.
     *
     * This function guarantes that the gfxASurface reflects the DrawTarget.
     */
    virtual already_AddRefed<gfxASurface>
      CreateThebesSurfaceAliasForDrawTarget_hack(mozilla::gfx::DrawTarget *aTarget) {
      // Overwrite me on platform where GetThebesSurfaceForDrawTarget returns
      // a snapshot of the draw target.
      return GetThebesSurfaceForDrawTarget(aTarget);
    }

    virtual already_AddRefed<gfxASurface>
      GetThebesSurfaceForDrawTarget(mozilla::gfx::DrawTarget *aTarget);

    virtual mozilla::RefPtr<mozilla::gfx::DrawTarget>
      CreateOffscreenDrawTarget(const mozilla::gfx::IntSize& aSize, mozilla::gfx::SurfaceFormat aFormat);

    virtual mozilla::RefPtr<mozilla::gfx::DrawTarget>
      CreateDrawTargetForData(unsigned char* aData, const mozilla::gfx::IntSize& aSize, 
                              int32_t aStride, mozilla::gfx::SurfaceFormat aFormat);

    /**
     * Returns true if we will render content using Azure using a gfxPlatform
     * provided DrawTarget.
     */
    bool SupportsAzureContent() {
      return GetContentBackend() != mozilla::gfx::BACKEND_NONE;
    }

    /**
     * Returns true if we should use Azure to render content with aTarget. For
     * example, it is possible that we are using Direct2D for rendering and thus
     * using Azure. But we want to render to a CairoDrawTarget, in which case
     * SupportsAzureContent will return true but SupportsAzureContentForDrawTarget
     * will return false.
     */
    bool SupportsAzureContentForDrawTarget(mozilla::gfx::DrawTarget* aTarget);

    virtual bool UseAcceleratedSkiaCanvas();

    void GetAzureBackendInfo(mozilla::widget::InfoObject &aObj) {
      aObj.DefineProperty("AzureCanvasBackend", GetBackendName(mPreferredCanvasBackend));
      aObj.DefineProperty("AzureSkiaAccelerated", UseAcceleratedSkiaCanvas());
      aObj.DefineProperty("AzureFallbackCanvasBackend", GetBackendName(mFallbackCanvasBackend));
      aObj.DefineProperty("AzureContentBackend", GetBackendName(mContentBackend));
    }

    mozilla::gfx::BackendType GetPreferredCanvasBackend() {
      return mPreferredCanvasBackend;
    }

    /*
     * Font bits
     */

    virtual void SetupClusterBoundaries(gfxTextRun *aTextRun, const PRUnichar *aString);

    /**
     * Fill aListOfFonts with the results of querying the list of font names
     * that correspond to the given language group or generic font family
     * (or both, or neither).
     */
    virtual nsresult GetFontList(nsIAtom *aLangGroup,
                                 const nsACString& aGenericFamily,
                                 nsTArray<nsString>& aListOfFonts);

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
     * Font name resolver, this returns actual font name(s) by the callback
     * function. If the font doesn't exist, the callback function is not called.
     * If the callback function returns false, the aAborted value is set to
     * true, otherwise, false.
     */
    typedef bool (*FontResolverCallback) (const nsAString& aName,
                                            void *aClosure);
    virtual nsresult ResolveFontName(const nsAString& aFontName,
                                     FontResolverCallback aCallback,
                                     void *aClosure,
                                     bool& aAborted) = 0;

    /**
     * Resolving a font name to family name. The result MUST be in the result of GetFontList().
     * If the name doesn't in the system, aFamilyName will be empty string, but not failed.
     */
    virtual nsresult GetStandardFamilyName(const nsAString& aFontName, nsAString& aFamilyName) = 0;

    /**
     * Create the appropriate platform font group
     */
    virtual gfxFontGroup *CreateFontGroup(const nsAString& aFamilies,
                                          const gfxFontStyle *aStyle,
                                          gfxUserFontSet *aUserFontSet) = 0;
                                          
                                          
    /**
     * Look up a local platform font using the full font face name.
     * (Needed to support @font-face src local().)
     * Ownership of the returned gfxFontEntry is passed to the caller,
     * who must either AddRef() or delete.
     */
    virtual gfxFontEntry* LookupLocalFont(const gfxProxyFontEntry *aProxyEntry,
                                          const nsAString& aFontName)
    { return nullptr; }

    /**
     * Activate a platform font.  (Needed to support @font-face src url().)
     * aFontData is a NS_Malloc'ed block that must be freed by this function
     * (or responsibility passed on) when it is no longer needed; the caller
     * will NOT free it.
     * Ownership of the returned gfxFontEntry is passed to the caller,
     * who must either AddRef() or delete.
     */
    virtual gfxFontEntry* MakePlatformFont(const gfxProxyFontEntry *aProxyEntry,
                                           const uint8_t *aFontData,
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

    bool UsesSubpixelAATextRendering() {
#ifdef MOZ_GFX_OPTIMIZE_MOBILE
	return false;
#endif
	return true;
    }

    /**
     * Whether to check all font cmaps during system font fallback
     */
    bool UseCmapsDuringSystemFallback();

    /**
     * Whether to render SVG glyphs within an OpenType font wrapper
     */
    bool OpenTypeSVGEnabled();

    /**
     * Whether to use the SIL Graphite rendering engine
     * (for fonts that include Graphite tables)
     */
    bool UseGraphiteShaping();

    /**
     * Whether to use the harfbuzz shaper (depending on script complexity).
     *
     * This allows harfbuzz to be enabled selectively via the preferences.
     */
    bool UseHarfBuzzForScript(int32_t aScriptCode);

    // check whether format is supported on a platform or not (if unclear, returns true)
    virtual bool IsFontFormatSupported(nsIURI *aFontURI, uint32_t aFormatFlags) { return false; }

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
    virtual void GetCommonFallbackFonts(const uint32_t /*aCh*/,
                                        int32_t /*aRunScript*/,
                                        nsTArray<const char*>& /*aFontList*/)
    {
        // platform-specific override, by default do nothing
    }

    // Break large OMTC tiled thebes layer painting into small paints.
    static bool UseProgressiveTilePainting();

    // When a critical display-port is set, render the visible area outside of
    // it into a buffer at a lower precision. Requires tiled buffers.
    static bool UseLowPrecisionBuffer();

    // Retrieve the resolution that a low precision buffer should render at.
    static float GetLowPrecisionResolution();

    // Retain some invalid tiles when the valid region of a layer changes and
    // excludes previously valid tiles.
    static bool UseReusableTileStore();

    static bool OffMainThreadCompositingEnabled();

    /** Use gfxPlatform::GetPref* methods instead of direct calls to Preferences
     * to get the values for layers preferences.  These will only be evaluated
     * only once, and remain the same until restart.
     */
    static bool GetPrefLayersOffMainThreadCompositionEnabled();
    static bool GetPrefLayersOffMainThreadCompositionForceEnabled();
    static bool GetPrefLayersAccelerationForceEnabled();
    static bool GetPrefLayersAccelerationDisabled();
    static bool GetPrefLayersPreferOpenGL();
    static bool GetPrefLayersPreferD3D9();
    static bool CanUseDirect3D9();
    static int  GetPrefLayoutFrameRate();

    /**
     * Is it possible to use buffer rotation
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
    static void TransformPixel(const gfxRGBA& in, gfxRGBA& out, qcms_transform *transform);

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

    void OrientationSyncPrefsObserverChanged();

    int32_t GetBidiNumeralOption();

    /**
     * Returns a 1x1 surface that can be used to create graphics contexts
     * for measuring text etc as if they will be rendered to the screen
     */
    gfxASurface* ScreenReferenceSurface() { return mScreenReferenceSurface; }

    virtual mozilla::gfx::SurfaceFormat Optimal2DFormatForContent(gfxASurface::gfxContentType aContent);

    virtual gfxImageFormat OptimalFormatForContent(gfxASurface::gfxContentType aContent);

    virtual gfxImageFormat GetOffscreenFormat()
    { return gfxASurface::ImageFormatRGB24; }

    /**
     * Returns a logger if one is available and logging is enabled
     */
    static PRLogModuleInfo* GetLog(eGfxLog aWhichLog);

    bool WorkAroundDriverBugs() const { return mWorkAroundDriverBugs; }

    virtual int GetScreenDepth() const;

    bool WidgetUpdateFlashing() const { return mWidgetUpdateFlashing; }

    uint32_t GetOrientationSyncMillis() const;

    /**
     * Return the layer debugging options to use browser-wide.
     */
    mozilla::layers::DiagnosticTypes GetLayerDiagnosticTypes();

    static bool DrawFrameCounter();
    /**
     * Returns true if we should use raw memory to send data to the compositor
     * rather than using shmems.
     *
     * This method should not be called from the compositor thread.
     */
    bool PreferMemoryOverShmem() const;
    bool UseDeprecatedTextures() const { return mLayersUseDeprecated; }

protected:
    gfxPlatform();
    virtual ~gfxPlatform();

    void AppendCJKPrefLangs(eFontPrefLang aPrefLangs[], uint32_t &aLen, 
                            eFontPrefLang aCharLang, eFontPrefLang aPageLang);

    /**
     * Helper method, creates a draw target for a specific Azure backend.
     * Used by CreateOffscreenDrawTarget.
     */
    mozilla::RefPtr<mozilla::gfx::DrawTarget>
      CreateDrawTargetForBackend(mozilla::gfx::BackendType aBackend,
                                 const mozilla::gfx::IntSize& aSize,
                                 mozilla::gfx::SurfaceFormat aFormat);

    /**
     * Initialise the preferred and fallback canvas backends
     * aBackendBitmask specifies the backends which are acceptable to the caller.
     * The backend used is determined by aBackendBitmask and the order specified
     * by the gfx.canvas.azure.backends pref.
     */
    void InitBackendPrefs(uint32_t aCanvasBitmask, uint32_t aContentBitmask);

    /**
     * returns the first backend named in the pref gfx.canvas.azure.backends
     * which is a component of aBackendBitmask, a bitmask of backend types
     */
    static mozilla::gfx::BackendType GetCanvasBackendPref(uint32_t aBackendBitmask);

    /**
     * returns the first backend named in the pref gfx.content.azure.backend
     * which is a component of aBackendBitmask, a bitmask of backend types
     */
    static mozilla::gfx::BackendType GetContentBackendPref(uint32_t aBackendBitmask);

    /**
     * If aEnabledPrefName is non-null, checks the aEnabledPrefName pref and
     * returns BACKEND_NONE if the pref is not enabled.
     * Otherwise it will return the first backend named in aBackendPrefName
     * allowed by aBackendBitmask, a bitmask of backend types.
     */
    static mozilla::gfx::BackendType GetBackendPref(const char* aEnabledPrefName,
                                                    const char* aBackendPrefName,
                                                    uint32_t aBackendBitmask);
    /**
     * Decode the backend enumberation from a string.
     */
    static mozilla::gfx::BackendType BackendTypeForName(const nsCString& aName);

    mozilla::gfx::BackendType GetContentBackend() {
      return mContentBackend;
    }

    int8_t  mAllowDownloadableFonts;
    int8_t  mGraphiteShapingEnabled;
    int8_t  mOpenTypeSVGEnabled;

    int8_t  mBidiNumeralOption;

    // whether to always search font cmaps globally 
    // when doing system font fallback
    int8_t  mFallbackUsesCmaps;

    // which scripts should be shaped with harfbuzz
    int32_t mUseHarfBuzzScripts;

private:
    /**
     * Start up Thebes.
     */
    static void Init();

    static void CreateCMSOutputProfile();

    friend int RecordingPrefChanged(const char *aPrefName, void *aClosure);

    virtual qcms_profile* GetPlatformCMSOutputProfile();

    virtual bool SupportsOffMainThreadCompositing() { return true; }

    nsRefPtr<gfxASurface> mScreenReferenceSurface;
    nsTArray<uint32_t> mCJKPrefLangs;
    nsCOMPtr<nsIObserver> mSRGBOverrideObserver;
    nsCOMPtr<nsIObserver> mFontPrefsObserver;
    nsCOMPtr<nsIObserver> mOrientationSyncPrefsObserver;

    // The preferred draw target backend to use for canvas
    mozilla::gfx::BackendType mPreferredCanvasBackend;
    // The fallback draw target backend to use for canvas, if the preferred backend fails
    mozilla::gfx::BackendType mFallbackCanvasBackend;
    // The backend to use for content
    mozilla::gfx::BackendType mContentBackend;
    // Bitmask of backend types we can use to render content
    uint32_t mContentBackendBitmask;

    mozilla::widget::GfxInfoCollector<gfxPlatform> mAzureCanvasBackendCollector;
    bool mWorkAroundDriverBugs;

    mozilla::RefPtr<mozilla::gfx::DrawEventRecorder> mRecorder;
    bool mWidgetUpdateFlashing;
    uint32_t mOrientationSyncMillis;
    bool mLayersPreferMemoryOverShmem;
    bool mLayersUseDeprecated;
    bool mDrawLayerBorders;
    bool mDrawTileBorders;
    bool mDrawBigImageBorders;
};

#endif /* GFX_PLATFORM_H */
