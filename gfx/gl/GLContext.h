/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GLCONTEXT_H_
#define GLCONTEXT_H_

#include <bitset>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <map>
#include <queue>
#include <stack>

#ifdef DEBUG
#include <string.h>
#endif

#ifdef WIN32
#include <windows.h>
#endif

#ifdef GetClassName
#undef GetClassName
#endif

// Define MOZ_GL_DEBUG unconditionally to enable GL debugging in opt
// builds.
#ifdef DEBUG
#define MOZ_GL_DEBUG 1
#endif

#include "../../mfbt/Maybe.h"
#include "../../mfbt/RefPtr.h"
#include "../../mfbt/UniquePtr.h"

#include "GLDefs.h"
#include "GLLibraryLoader.h"
#include "nsISupportsImpl.h"
#include "plstr.h"
#include "nsAutoPtr.h"
#include "GLContextTypes.h"
//#include "GLTextureImage.h"
#include "SurfaceTypes.h"
#include "GLContextSymbols.h"
#include "base/platform_thread.h"       // for PlatformThreadId
#include "mozilla/GenericRefCounted.h"
#include "gfx2DGlue.h"
#include "GeckoProfiler.h"

class nsIWidget;

namespace android {
    class GraphicBuffer;
} // namespace android

namespace mozilla {
    namespace gfx {
        class DataSourceSurface;
        class SourceSurface;
    } // namespace gfx

    namespace gl {
        class GLBlitHelper;
        class GLBlitTextureImageHelper;
        class GLContext;
        class GLLibraryEGL;
        class GLReadTexImageHelper;
        class GLScreenBuffer;
        class SharedSurface;
        class TextureGarbageBin;
        struct SurfaceCaps;
    } // namespace gl

    namespace layers {
        class ColorTextureLayerProgram;
    } // namespace layers
} // namespace mozilla

namespace mozilla {
namespace gl {

enum class GLFeature {
    bind_buffer_offset,
    blend_minmax,
    clear_buffers,
    copy_buffer,
    depth_texture,
    draw_buffers,
    draw_instanced,
    draw_range_elements,
    element_index_uint,
    ES2_compatibility,
    ES3_compatibility,
    EXT_color_buffer_float,
    frag_color_float,
    frag_depth,
    framebuffer_blit,
    framebuffer_multisample,
    framebuffer_object,
    framebuffer_object_EXT_OES,
    get_integer_indexed,
    get_integer64_indexed,
    get_query_object_i64v,
    get_query_object_iv,
    get_string_indexed,
    gpu_shader4,
    instanced_arrays,
    instanced_non_arrays,
    internalformat_query,
    invalidate_framebuffer,
    map_buffer_range,
    occlusion_query,
    occlusion_query_boolean,
    occlusion_query2,
    packed_depth_stencil,
    query_counter,
    query_objects,
    query_time_elapsed,
    read_buffer,
    renderbuffer_color_float,
    renderbuffer_color_half_float,
    robustness,
    sRGB_framebuffer,
    sRGB_texture,
    sampler_objects,
    seamless_cube_map_opt_in,
    split_framebuffer,
    standard_derivatives,
    sync,
    texture_3D,
    texture_3D_compressed,
    texture_3D_copy,
    texture_float,
    texture_float_linear,
    texture_half_float,
    texture_half_float_linear,
    texture_non_power_of_two,
    texture_rg,
    texture_storage,
    texture_swizzle,
    transform_feedback2,
    uniform_buffer_object,
    uniform_matrix_nonsquare,
    vertex_array_object,
    EnumMax
};

enum class ContextProfile : uint8_t {
    Unknown = 0,
    OpenGL, // only for IsAtLeast's <profile> parameter
    OpenGLCore,
    OpenGLCompatibility,
    OpenGLES
};

enum class GLVendor {
    Intel,
    NVIDIA,
    ATI,
    Qualcomm,
    Imagination,
    Nouveau,
    Vivante,
    VMware,
    ARM,
    Other
};

enum class GLRenderer {
    Adreno200,
    Adreno205,
    AdrenoTM200,
    AdrenoTM205,
    AdrenoTM320,
    AdrenoTM420,
    SGX530,
    SGX540,
    Tegra,
    AndroidEmulator,
    GalliumLlvmpipe,
    IntelHD3000,
    MicrosoftBasicRenderDriver,
    Other
};

class GLContext
    : public GLLibraryLoader
    , public GenericAtomicRefCounted
{
// -----------------------------------------------------------------------------
// basic enums
public:

// -----------------------------------------------------------------------------
// basic getters
public:

    /**
     * Returns true if the context is using ANGLE. This should only be overridden
     * for an ANGLE implementation.
     */
    virtual bool IsANGLE() const {
        return false;
    }

    /**
    * Returns true if the context is using WARP. This should only be overridden
    * for an ANGLE implementation.
    */
    virtual bool IsWARP() const {
        return false;
    }

    /**
     * Return true if we are running on a OpenGL core profile context
     */
    inline bool IsCoreProfile() const {
        MOZ_ASSERT(mProfile != ContextProfile::Unknown, "unknown context profile");

        return mProfile == ContextProfile::OpenGLCore;
    }

    /**
     * Return true if we are running on a OpenGL compatibility profile context
     * (legacy profile 2.1 on Max OS X)
     */
    inline bool IsCompatibilityProfile() const {
        MOZ_ASSERT(mProfile != ContextProfile::Unknown, "unknown context profile");

        return mProfile == ContextProfile::OpenGLCompatibility;
    }

    /**
     * Return true if the context is a true OpenGL ES context or an ANGLE context
     */
    inline bool IsGLES() const {
        MOZ_ASSERT(mProfile != ContextProfile::Unknown, "unknown context profile");

        return mProfile == ContextProfile::OpenGLES;
    }

    static const char* GetProfileName(ContextProfile profile)
    {
        switch (profile)
        {
            case ContextProfile::OpenGL:
                return "OpenGL";
            case ContextProfile::OpenGLCore:
                return "OpenGL Core";
            case ContextProfile::OpenGLCompatibility:
                return "OpenGL Compatibility";
            case ContextProfile::OpenGLES:
                return "OpenGL ES";
            default:
                break;
        }

        MOZ_ASSERT(profile != ContextProfile::Unknown, "unknown context profile");
        return "OpenGL unknown profile";
    }

    /**
     * Return true if we are running on a OpenGL core profile context
     */
    const char* ProfileString() const {
        return GetProfileName(mProfile);
    }

    /**
     * Return true if the context is compatible with given parameters
     *
     * IsAtLeast(ContextProfile::OpenGL, N) is exactly same as
     * IsAtLeast(ContextProfile::OpenGLCore, N) || IsAtLeast(ContextProfile::OpenGLCompatibility, N)
     */
    inline bool IsAtLeast(ContextProfile profile, unsigned int version) const
    {
        MOZ_ASSERT(profile != ContextProfile::Unknown, "IsAtLeast: bad <profile> parameter");
        MOZ_ASSERT(mProfile != ContextProfile::Unknown, "unknown context profile");
        MOZ_ASSERT(mVersion != 0, "unknown context version");

        if (version > mVersion) {
            return false;
        }

        if (profile == ContextProfile::OpenGL) {
            return mProfile == ContextProfile::OpenGLCore ||
                   mProfile == ContextProfile::OpenGLCompatibility;
        }

        return profile == mProfile;
    }

    /**
     * Return the version of the context.
     * Example :
     *   If this a OpenGL 2.1, that will return 210
     */
    inline uint32_t Version() const {
        return mVersion;
    }

    const char* VersionString() const {
        return mVersionString.get();
    }

    inline uint32_t ShadingLanguageVersion() const {
        return mShadingLanguageVersion;
    }

    GLVendor Vendor() const {
        return mVendor;
    }

    GLRenderer Renderer() const {
        return mRenderer;
    }

    bool IsContextLost() const {
        return mContextLost;
    }

    /**
     * If this context is double-buffered, returns TRUE.
     */
    virtual bool IsDoubleBuffered() const {
        return false;
    }

    virtual GLContextType GetContextType() const = 0;

    virtual bool IsCurrent() = 0;

    /**
     * Get the default framebuffer for this context.
     */
    virtual GLuint GetDefaultFramebuffer() {
        return 0;
    }

protected:
    bool mIsOffscreen;
    bool mContextLost;

    /**
     * mVersion store the OpenGL's version, multiplied by 100. For example, if
     * the context is an OpenGL 2.1 context, mVersion value will be 210.
     */
    uint32_t mVersion;
    nsCString mVersionString;
    ContextProfile mProfile;

    uint32_t mShadingLanguageVersion;

    GLVendor mVendor;
    GLRenderer mRenderer;

    void SetProfileVersion(ContextProfile profile, uint32_t version) {
        MOZ_ASSERT(!mSymbols.fBindFramebuffer,
                   "SetProfileVersion can only be called before initialization!");
        MOZ_ASSERT(profile != ContextProfile::Unknown &&
                   profile != ContextProfile::OpenGL,
                   "Invalid `profile` for SetProfileVersion");
        MOZ_ASSERT(version >= 100, "Invalid `version` for SetProfileVersion");

        mVersion = version;
        mProfile = profile;
    }


// -----------------------------------------------------------------------------
// Extensions management
/**
 * This mechanism is designed to know if an extension is supported. In the long
 * term, we would like to only use the extension group queries XXX_* to have
 * full compatibility with context version and profiles (especialy the core that
 * officialy don't bring any extensions).
 */
public:

    /**
     * Known GL extensions that can be queried by
     * IsExtensionSupported.  The results of this are cached, and as
     * such it's safe to use this even in performance critical code.
     * If you add to this array, remember to add to the string names
     * in GLContext.cpp.
     */
    enum GLExtensions {
        Extension_None = 0,
        AMD_compressed_ATC_texture,
        ANGLE_depth_texture,
        ANGLE_framebuffer_blit,
        ANGLE_framebuffer_multisample,
        ANGLE_instanced_arrays,
        ANGLE_texture_compression_dxt3,
        ANGLE_texture_compression_dxt5,
        ANGLE_timer_query,
        APPLE_client_storage,
        APPLE_framebuffer_multisample,
        APPLE_sync,
        APPLE_texture_range,
        APPLE_vertex_array_object,
        ARB_ES2_compatibility,
        ARB_ES3_compatibility,
        ARB_color_buffer_float,
        ARB_copy_buffer,
        ARB_depth_texture,
        ARB_draw_buffers,
        ARB_draw_instanced,
        ARB_framebuffer_object,
        ARB_framebuffer_sRGB,
        ARB_geometry_shader4,
        ARB_half_float_pixel,
        ARB_instanced_arrays,
        ARB_internalformat_query,
        ARB_invalidate_subdata,
        ARB_map_buffer_range,
        ARB_occlusion_query2,
        ARB_pixel_buffer_object,
        ARB_robustness,
        ARB_sampler_objects,
        ARB_seamless_cube_map,
        ARB_sync,
        ARB_texture_compression,
        ARB_texture_float,
        ARB_texture_non_power_of_two,
        ARB_texture_rectangle,
        ARB_texture_rg,
        ARB_texture_storage,
        ARB_texture_swizzle,
        ARB_timer_query,
        ARB_transform_feedback2,
        ARB_uniform_buffer_object,
        ARB_vertex_array_object,
        EXT_bgra,
        EXT_blend_minmax,
        EXT_color_buffer_float,
        EXT_color_buffer_half_float,
        EXT_copy_texture,
        EXT_disjoint_timer_query,
        EXT_draw_buffers,
        EXT_draw_buffers2,
        EXT_draw_instanced,
        EXT_draw_range_elements,
        EXT_frag_depth,
        EXT_framebuffer_blit,
        EXT_framebuffer_multisample,
        EXT_framebuffer_object,
        EXT_framebuffer_sRGB,
        EXT_gpu_shader4,
        EXT_multisampled_render_to_texture,
        EXT_occlusion_query_boolean,
        EXT_packed_depth_stencil,
        EXT_read_format_bgra,
        EXT_robustness,
        EXT_sRGB,
        EXT_sRGB_write_control,
        EXT_shader_texture_lod,
        EXT_texture3D,
        EXT_texture_compression_dxt1,
        EXT_texture_compression_s3tc,
        EXT_texture_filter_anisotropic,
        EXT_texture_format_BGRA8888,
        EXT_texture_sRGB,
        EXT_texture_storage,
        EXT_timer_query,
        EXT_transform_feedback,
        EXT_unpack_subimage,
        IMG_read_format,
        IMG_texture_compression_pvrtc,
        IMG_texture_npot,
        KHR_debug,
        NV_draw_instanced,
        NV_fence,
        NV_framebuffer_blit,
        NV_geometry_program4,
        NV_half_float,
        NV_instanced_arrays,
        NV_texture_barrier,
        NV_transform_feedback,
        NV_transform_feedback2,
        OES_EGL_image,
        OES_EGL_image_external,
        OES_EGL_sync,
        OES_compressed_ETC1_RGB8_texture,
        OES_depth24,
        OES_depth32,
        OES_depth_texture,
        OES_element_index_uint,
        OES_framebuffer_object,
        OES_packed_depth_stencil,
        OES_rgb8_rgba8,
        OES_standard_derivatives,
        OES_stencil8,
        OES_texture_3D,
        OES_texture_float,
        OES_texture_float_linear,
        OES_texture_half_float,
        OES_texture_half_float_linear,
        OES_texture_npot,
        OES_vertex_array_object,
        Extensions_Max,
        Extensions_End
    };

    bool IsExtensionSupported(GLExtensions aKnownExtension) const {
        return mAvailableExtensions[aKnownExtension];
    }

protected:
    void MarkExtensionUnsupported(GLExtensions aKnownExtension) {
        mAvailableExtensions[aKnownExtension] = 0;
    }

    void MarkExtensionSupported(GLExtensions aKnownExtension) {
        mAvailableExtensions[aKnownExtension] = 1;
    }

    std::bitset<Extensions_Max> mAvailableExtensions;

// -----------------------------------------------------------------------------
// Feature queries
/*
 * This mecahnism introduces a new way to check if a OpenGL feature is
 * supported, regardless of whether it is supported by an extension or natively
 * by the context version/profile
 */
public:
    bool IsSupported(GLFeature feature) const {
        return mAvailableFeatures[size_t(feature)];
    }

    static const char* GetFeatureName(GLFeature feature);

private:
    std::bitset<size_t(GLFeature::EnumMax)> mAvailableFeatures;

    /**
     * Init features regarding OpenGL extension and context version and profile
     */
    void InitFeatures();

    /**
     * Mark the feature and associated extensions as unsupported
     */
    void MarkUnsupported(GLFeature feature);

    /**
     * Is this feature supported using the core (unsuffixed) symbols?
     */
    bool IsFeatureProvidedByCoreSymbols(GLFeature feature);

// -----------------------------------------------------------------------------
// Robustness handling
private:
    /**
     * The derived class is expected to provide information on whether or not it
     * supports robustness.
     */
    virtual bool SupportsRobustness() const = 0;

public:
// -----------------------------------------------------------------------------
// Error handling
    static const char* GLErrorToString(GLenum aError) {
        switch (aError) {
            case LOCAL_GL_INVALID_ENUM:
                return "GL_INVALID_ENUM";
            case LOCAL_GL_INVALID_VALUE:
                return "GL_INVALID_VALUE";
            case LOCAL_GL_INVALID_OPERATION:
                return "GL_INVALID_OPERATION";
            case LOCAL_GL_STACK_OVERFLOW:
                return "GL_STACK_OVERFLOW";
            case LOCAL_GL_STACK_UNDERFLOW:
                return "GL_STACK_UNDERFLOW";
            case LOCAL_GL_OUT_OF_MEMORY:
                return "GL_OUT_OF_MEMORY";
            case LOCAL_GL_TABLE_TOO_LARGE:
                return "GL_TABLE_TOO_LARGE";
            case LOCAL_GL_INVALID_FRAMEBUFFER_OPERATION:
                return "GL_INVALID_FRAMEBUFFER_OPERATION";
            default:
                return "";
        }
    }

private:
    GLenum mTopError;

    GLenum RawGetError() {
        return mSymbols.fGetError();
    }

    GLenum RawGetErrorAndClear() {
        GLenum err = RawGetError();

        if (err)
            while (RawGetError()) {}

        return err;
    }

public:
    GLenum FlushErrors() {
        GLenum err = RawGetErrorAndClear();
        if (!mTopError)
            mTopError = err;
        return err;
    }

    // We smash all errors together, so you never have to loop on this. We
    // guarantee that immediately after this call, there are no errors left.
    GLenum fGetError() {
        FlushErrors();

        GLenum err = mTopError;
        mTopError = LOCAL_GL_NO_ERROR;
        return err;
    }

    ////////////////////////////////////
    // Use this safer option.
    class LocalErrorScope;

private:
    std::stack<const LocalErrorScope*> mLocalErrorScopeStack;

public:
    class LocalErrorScope {
        GLContext& mGL;
        GLenum mOldTop;
        bool mHasBeenChecked;

    public:
        explicit LocalErrorScope(GLContext& gl)
            : mGL(gl)
            , mHasBeenChecked(false)
        {
            mGL.mLocalErrorScopeStack.push(this);

            mGL.FlushErrors();

            mOldTop = mGL.mTopError;
            mGL.mTopError = LOCAL_GL_NO_ERROR;
        }

        GLenum GetError() {
            MOZ_ASSERT(!mHasBeenChecked);
            mHasBeenChecked = true;

            const GLenum ret = mGL.fGetError();

            while (mGL.fGetError()) {}

            return ret;
        }

        ~LocalErrorScope() {
            MOZ_ASSERT(mHasBeenChecked);

            MOZ_ASSERT(mGL.fGetError() == LOCAL_GL_NO_ERROR);

            MOZ_ASSERT(mGL.mLocalErrorScopeStack.top() == this);
            mGL.mLocalErrorScopeStack.pop();

            mGL.mTopError = mOldTop;
        }
    };

    bool GetPotentialInteger(GLenum pname, GLint* param) {
        LocalErrorScope localError(*this);

        fGetIntegerv(pname, param);

        GLenum err = localError.GetError();
        MOZ_ASSERT_IF(err != LOCAL_GL_NO_ERROR, err == LOCAL_GL_INVALID_ENUM);
        return err == LOCAL_GL_NO_ERROR;
    }

private:
    static void GLAPIENTRY StaticDebugCallback(GLenum source,
                                               GLenum type,
                                               GLuint id,
                                               GLenum severity,
                                               GLsizei length,
                                               const GLchar* message,
                                               const GLvoid* userParam);
    void DebugCallback(GLenum source,
                       GLenum type,
                       GLuint id,
                       GLenum severity,
                       GLsizei length,
                       const GLchar* message);


// -----------------------------------------------------------------------------
// MOZ_GL_DEBUG implementation
private:

#undef BEFORE_GL_CALL
#undef AFTER_GL_CALL

#ifdef MOZ_GL_DEBUG

#ifndef MOZ_FUNCTION_NAME
# ifdef __GNUC__
#  define MOZ_FUNCTION_NAME __PRETTY_FUNCTION__
# elif defined(_MSC_VER)
#  define MOZ_FUNCTION_NAME __FUNCTION__
# else
#  define MOZ_FUNCTION_NAME __func__  // defined in C99, supported in various C++ compilers. Just raw function name.
# endif
#endif

    void BeforeGLCall(const char* funcName) {
        MOZ_ASSERT(IsCurrent());

        if (DebugMode()) {
            FlushErrors();

            if (DebugMode() & DebugTrace)
                printf_stderr("[gl:%p] > %s\n", this, funcName);

            GLContext* tlsContext = (GLContext*)PR_GetThreadPrivate(sCurrentGLContextTLS);
            if (this != tlsContext) {
                printf_stderr("Fatal: %s called on non-current context %p. The"
                              " current context for this thread is %p.\n",
                              funcName, this, tlsContext);
                MOZ_CRASH("GFX: GLContext is not current.");
            }
        }
    }

    void AfterGLCall(const char* funcName) {
        if (DebugMode()) {
            // calling fFinish() immediately after every GL call makes sure that if this GL command crashes,
            // the stack trace will actually point to it. Otherwise, OpenGL being an asynchronous API, stack traces
            // tend to be meaningless
            mSymbols.fFinish();
            GLenum err = FlushErrors();

            if (DebugMode() & DebugTrace) {
                printf_stderr("[gl:%p] < %s [%s (0x%04x)]\n", this, funcName,
                              GLErrorToString(err), err);
            }

            if (err != LOCAL_GL_NO_ERROR &&
                !mLocalErrorScopeStack.size())
            {
                printf_stderr("[gl:%p] %s: Generated unexpected %s error."
                              " (0x%04x)\n", this, funcName,
                              GLErrorToString(err), err);

                if (DebugMode() & DebugAbortOnError)
                    MOZ_CRASH("MOZ_GL_DEBUG_ABORT_ON_ERROR");
            }
        }
    }

    GLContext *TrackingContext()
    {
        GLContext *tip = this;
        while (tip->mSharedContext)
            tip = tip->mSharedContext;
        return tip;
    }

    static void AssertNotPassingStackBufferToTheGL(const void* ptr);

#ifdef MOZ_WIDGET_ANDROID
// Record the name of the GL call for better hang stacks on Android.
#define BEFORE_GL_CALL                              \
            PROFILER_LABEL_FUNC(                    \
              js::ProfileEntry::Category::GRAPHICS);\
            BeforeGLCall(MOZ_FUNCTION_NAME)
#else
#define BEFORE_GL_CALL                              \
            do {                                    \
                BeforeGLCall(MOZ_FUNCTION_NAME);    \
            } while (0)
#endif

#define AFTER_GL_CALL                               \
            do {                                    \
                AfterGLCall(MOZ_FUNCTION_NAME);     \
            } while (0)

#define TRACKING_CONTEXT(a)                         \
            do {                                    \
                TrackingContext()->a;               \
            } while (0)

#define ASSERT_NOT_PASSING_STACK_BUFFER_TO_GL(ptr) AssertNotPassingStackBufferToTheGL(ptr)

#else // ifdef MOZ_GL_DEBUG

#ifdef MOZ_WIDGET_ANDROID
// Record the name of the GL call for better hang stacks on Android.
#define BEFORE_GL_CALL PROFILER_LABEL_FUNC(js::ProfileEntry::Category::GRAPHICS)
#else
#define BEFORE_GL_CALL do { } while (0)
#endif
#define AFTER_GL_CALL do { } while (0)
#define TRACKING_CONTEXT(a) do {} while (0)
#define ASSERT_NOT_PASSING_STACK_BUFFER_TO_GL(ptr) do {} while (0)

#endif // ifdef MOZ_GL_DEBUG

#define ASSERT_SYMBOL_PRESENT(func) \
            do {\
                MOZ_ASSERT(strstr(MOZ_FUNCTION_NAME, #func) != nullptr, "Mismatched symbol check.");\
                if (MOZ_UNLIKELY(!mSymbols.func)) {\
                    printf_stderr("RUNTIME ASSERT: Uninitialized GL function: %s\n", #func);\
                    MOZ_CRASH("GFX: Uninitialized GL function");\
                }\
            } while (0)

    // Do whatever setup is necessary to draw to our offscreen FBO, if it's
    // bound.
    void BeforeGLDrawCall() { }

    // Do whatever tear-down is necessary after drawing to our offscreen FBO,
    // if it's bound.
    void AfterGLDrawCall();

    // Do whatever setup is necessary to read from our offscreen FBO, if it's
    // bound.
    void BeforeGLReadCall();

    // Do whatever tear-down is necessary after reading from our offscreen FBO,
    // if it's bound.
    void AfterGLReadCall() { }


// -----------------------------------------------------------------------------
// GL official entry points
public:

    void fActiveTexture(GLenum texture) {
        BEFORE_GL_CALL;
        mSymbols.fActiveTexture(texture);
        AFTER_GL_CALL;
    }

    void fAttachShader(GLuint program, GLuint shader) {
        BEFORE_GL_CALL;
        mSymbols.fAttachShader(program, shader);
        AFTER_GL_CALL;
    }

    void fBeginQuery(GLenum target, GLuint id) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fBeginQuery);
        mSymbols.fBeginQuery(target, id);
        AFTER_GL_CALL;
    }

    void fBindAttribLocation(GLuint program, GLuint index, const GLchar* name) {
        BEFORE_GL_CALL;
        mSymbols.fBindAttribLocation(program, index, name);
        AFTER_GL_CALL;
    }

    void fBindBuffer(GLenum target, GLuint buffer) {
        BEFORE_GL_CALL;
        mSymbols.fBindBuffer(target, buffer);
        AFTER_GL_CALL;
    }

    void fBindFramebuffer(GLenum target, GLuint framebuffer);

    void fInvalidateFramebuffer(GLenum target, GLsizei numAttachments, const GLenum* attachments) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fInvalidateFramebuffer);
        mSymbols.fInvalidateFramebuffer(target, numAttachments, attachments);
        AFTER_GL_CALL;
    }

    void fInvalidateSubFramebuffer(GLenum target, GLsizei numAttachments, const GLenum* attachments, GLint x, GLint y, GLsizei width, GLsizei height) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fInvalidateSubFramebuffer);
        mSymbols.fInvalidateSubFramebuffer(target, numAttachments, attachments, x, y, width, height);
        AFTER_GL_CALL;
    }

    void fBindTexture(GLenum target, GLuint texture) {
        BEFORE_GL_CALL;
        mSymbols.fBindTexture(target, texture);
        AFTER_GL_CALL;
    }

    void fBlendColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
        BEFORE_GL_CALL;
        mSymbols.fBlendColor(red, green, blue, alpha);
        AFTER_GL_CALL;
    }

    void fBlendEquation(GLenum mode) {
        BEFORE_GL_CALL;
        mSymbols.fBlendEquation(mode);
        AFTER_GL_CALL;
    }

    void fBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha) {
        BEFORE_GL_CALL;
        mSymbols.fBlendEquationSeparate(modeRGB, modeAlpha);
        AFTER_GL_CALL;
    }

    void fBlendFunc(GLenum sfactor, GLenum dfactor) {
        BEFORE_GL_CALL;
        mSymbols.fBlendFunc(sfactor, dfactor);
        AFTER_GL_CALL;
    }

    void fBlendFuncSeparate(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha) {
        BEFORE_GL_CALL;
        mSymbols.fBlendFuncSeparate(sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);
        AFTER_GL_CALL;
    }

private:
    void raw_fBufferData(GLenum target, GLsizeiptr size, const GLvoid* data, GLenum usage) {
        ASSERT_NOT_PASSING_STACK_BUFFER_TO_GL(data);
        BEFORE_GL_CALL;
        mSymbols.fBufferData(target, size, data, usage);
        AFTER_GL_CALL;
        mHeavyGLCallsSinceLastFlush = true;
    }

public:
    void fBufferData(GLenum target, GLsizeiptr size, const GLvoid* data, GLenum usage) {
        raw_fBufferData(target, size, data, usage);

        // bug 744888
        if (WorkAroundDriverBugs() &&
            !data &&
            Vendor() == GLVendor::NVIDIA)
        {
            UniquePtr<char[]> buf = MakeUnique<char[]>(1);
            buf[0] = 0;
            fBufferSubData(target, size-1, 1, buf.get());
        }
    }

    void fBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid* data) {
        ASSERT_NOT_PASSING_STACK_BUFFER_TO_GL(data);
        BEFORE_GL_CALL;
        mSymbols.fBufferSubData(target, offset, size, data);
        AFTER_GL_CALL;
        mHeavyGLCallsSinceLastFlush = true;
    }

private:
    void raw_fClear(GLbitfield mask) {
        BEFORE_GL_CALL;
        mSymbols.fClear(mask);
        AFTER_GL_CALL;
    }

public:
    void fClear(GLbitfield mask) {
        BeforeGLDrawCall();
        raw_fClear(mask);
        AfterGLDrawCall();
    }

    void fClearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil) {
        BEFORE_GL_CALL;
        mSymbols.fClearBufferfi(buffer, drawbuffer, depth, stencil);
        AFTER_GL_CALL;
    }

    void fClearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat* value) {
        BEFORE_GL_CALL;
        mSymbols.fClearBufferfv(buffer, drawbuffer, value);
        AFTER_GL_CALL;
    }

    void fClearBufferiv(GLenum buffer, GLint drawbuffer, const GLint* value) {
        BEFORE_GL_CALL;
        mSymbols.fClearBufferiv(buffer, drawbuffer, value);
        AFTER_GL_CALL;
    }

    void fClearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint* value) {
        BEFORE_GL_CALL;
        mSymbols.fClearBufferuiv(buffer, drawbuffer, value);
        AFTER_GL_CALL;
    }

    void fClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
        BEFORE_GL_CALL;
        mSymbols.fClearColor(r, g, b, a);
        AFTER_GL_CALL;
    }

    void fClearStencil(GLint s) {
        BEFORE_GL_CALL;
        mSymbols.fClearStencil(s);
        AFTER_GL_CALL;
    }

    void fClientActiveTexture(GLenum texture) {
        BEFORE_GL_CALL;
        mSymbols.fClientActiveTexture(texture);
        AFTER_GL_CALL;
    }

    void fColorMask(realGLboolean red, realGLboolean green, realGLboolean blue, realGLboolean alpha) {
        BEFORE_GL_CALL;
        mSymbols.fColorMask(red, green, blue, alpha);
        AFTER_GL_CALL;
    }

    void fCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const GLvoid *pixels) {
        ASSERT_NOT_PASSING_STACK_BUFFER_TO_GL(pixels);
        BEFORE_GL_CALL;
        mSymbols.fCompressedTexImage2D(target, level, internalformat, width, height, border, imageSize, pixels);
        AFTER_GL_CALL;
        mHeavyGLCallsSinceLastFlush = true;
    }

    void fCompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const GLvoid *pixels) {
        ASSERT_NOT_PASSING_STACK_BUFFER_TO_GL(pixels);
        BEFORE_GL_CALL;
        mSymbols.fCompressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, imageSize, pixels);
        AFTER_GL_CALL;
        mHeavyGLCallsSinceLastFlush = true;
    }

    void fCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x,
                         GLint y, GLsizei width, GLsizei height, GLint border);

    void fCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
        BeforeGLReadCall();
        raw_fCopyTexSubImage2D(target, level, xoffset, yoffset,
                               x, y, width, height);
        AfterGLReadCall();
    }

    void fCullFace(GLenum mode) {
        BEFORE_GL_CALL;
        mSymbols.fCullFace(mode);
        AFTER_GL_CALL;
    }

    void fDebugMessageCallback(GLDEBUGPROC callback, const GLvoid* userParam) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fDebugMessageCallback);
        mSymbols.fDebugMessageCallback(callback, userParam);
        AFTER_GL_CALL;
    }

    void fDebugMessageControl(GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint* ids, realGLboolean enabled) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fDebugMessageControl);
        mSymbols.fDebugMessageControl(source, type, severity, count, ids, enabled);
        AFTER_GL_CALL;
    }

    void fDebugMessageInsert(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* buf) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fDebugMessageInsert);
        mSymbols.fDebugMessageInsert(source, type, id, severity, length, buf);
        AFTER_GL_CALL;
    }

    void fDetachShader(GLuint program, GLuint shader) {
        BEFORE_GL_CALL;
        mSymbols.fDetachShader(program, shader);
        AFTER_GL_CALL;
    }

    void fDepthFunc(GLenum func) {
        BEFORE_GL_CALL;
        mSymbols.fDepthFunc(func);
        AFTER_GL_CALL;
    }

    void fDepthMask(realGLboolean flag) {
        BEFORE_GL_CALL;
        mSymbols.fDepthMask(flag);
        AFTER_GL_CALL;
    }

    void fDisable(GLenum capability) {
        BEFORE_GL_CALL;
        mSymbols.fDisable(capability);
        AFTER_GL_CALL;
    }

    void fDisableClientState(GLenum capability) {
        BEFORE_GL_CALL;
        mSymbols.fDisableClientState(capability);
        AFTER_GL_CALL;
    }

    void fDisableVertexAttribArray(GLuint index) {
        BEFORE_GL_CALL;
        mSymbols.fDisableVertexAttribArray(index);
        AFTER_GL_CALL;
    }

    void fDrawBuffer(GLenum mode) {
        BEFORE_GL_CALL;
        mSymbols.fDrawBuffer(mode);
        AFTER_GL_CALL;
    }

private:
    void raw_fDrawArrays(GLenum mode, GLint first, GLsizei count) {
        BEFORE_GL_CALL;
        mSymbols.fDrawArrays(mode, first, count);
        AFTER_GL_CALL;
    }

    void raw_fDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices) {
        BEFORE_GL_CALL;
        mSymbols.fDrawElements(mode, count, type, indices);
        AFTER_GL_CALL;
    }

public:
    void fDrawArrays(GLenum mode, GLint first, GLsizei count) {
        BeforeGLDrawCall();
        raw_fDrawArrays(mode, first, count);
        AfterGLDrawCall();
    }

    void fDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices) {
        BeforeGLDrawCall();
        raw_fDrawElements(mode, count, type, indices);
        AfterGLDrawCall();
    }

    void fEnable(GLenum capability) {
        BEFORE_GL_CALL;
        mSymbols.fEnable(capability);
        AFTER_GL_CALL;
    }

    void fEnableClientState(GLenum capability) {
        BEFORE_GL_CALL;
        mSymbols.fEnableClientState(capability);
        AFTER_GL_CALL;
    }

    void fEnableVertexAttribArray(GLuint index) {
        BEFORE_GL_CALL;
        mSymbols.fEnableVertexAttribArray(index);
        AFTER_GL_CALL;
    }

    void fEndQuery(GLenum target) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fEndQuery);
        mSymbols.fEndQuery(target);
        AFTER_GL_CALL;
    }

    void fFinish() {
        BEFORE_GL_CALL;
        mSymbols.fFinish();
        AFTER_GL_CALL;
        mHeavyGLCallsSinceLastFlush = false;
    }

    void fFlush() {
        BEFORE_GL_CALL;
        mSymbols.fFlush();
        AFTER_GL_CALL;
        mHeavyGLCallsSinceLastFlush = false;
    }

    void fFrontFace(GLenum face) {
        BEFORE_GL_CALL;
        mSymbols.fFrontFace(face);
        AFTER_GL_CALL;
    }

    void fGetActiveAttrib(GLuint program, GLuint index, GLsizei maxLength, GLsizei* length, GLint* size, GLenum* type, GLchar* name) {
        BEFORE_GL_CALL;
        mSymbols.fGetActiveAttrib(program, index, maxLength, length, size, type, name);
        AFTER_GL_CALL;
    }

    void fGetActiveUniform(GLuint program, GLuint index, GLsizei maxLength, GLsizei* length, GLint* size, GLenum* type, GLchar* name) {
        BEFORE_GL_CALL;
        mSymbols.fGetActiveUniform(program, index, maxLength, length, size, type, name);
        AFTER_GL_CALL;
    }

    void fGetAttachedShaders(GLuint program, GLsizei maxCount, GLsizei* count, GLuint* shaders) {
        BEFORE_GL_CALL;
        mSymbols.fGetAttachedShaders(program, maxCount, count, shaders);
        AFTER_GL_CALL;
    }

    GLint fGetAttribLocation(GLuint program, const GLchar* name) {
        BEFORE_GL_CALL;
        GLint retval = mSymbols.fGetAttribLocation(program, name);
        AFTER_GL_CALL;
        return retval;
    }

private:
    void raw_fGetIntegerv(GLenum pname, GLint* params) {
        BEFORE_GL_CALL;
        mSymbols.fGetIntegerv(pname, params);
        AFTER_GL_CALL;
    }

public:

    void fGetIntegerv(GLenum pname, GLint* params);

    void GetUIntegerv(GLenum pname, GLuint *params) {
        fGetIntegerv(pname, reinterpret_cast<GLint*>(params));
    }

    void fGetFloatv(GLenum pname, GLfloat* params) {
        BEFORE_GL_CALL;
        mSymbols.fGetFloatv(pname, params);
        AFTER_GL_CALL;
    }

    void fGetBooleanv(GLenum pname, realGLboolean* params) {
        BEFORE_GL_CALL;
        mSymbols.fGetBooleanv(pname, params);
        AFTER_GL_CALL;
    }

    void fGetBufferParameteriv(GLenum target, GLenum pname, GLint* params) {
        BEFORE_GL_CALL;
        mSymbols.fGetBufferParameteriv(target, pname, params);
        AFTER_GL_CALL;
    }

    GLuint fGetDebugMessageLog(GLuint count, GLsizei bufsize, GLenum* sources, GLenum* types, GLuint* ids, GLenum* severities, GLsizei* lengths, GLchar* messageLog) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fGetDebugMessageLog);
        GLuint ret = mSymbols.fGetDebugMessageLog(count, bufsize, sources, types, ids, severities, lengths, messageLog);
        AFTER_GL_CALL;
        return ret;
    }

    void fGetPointerv(GLenum pname, GLvoid** params) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fGetPointerv);
        mSymbols.fGetPointerv(pname, params);
        AFTER_GL_CALL;
    }

    void fGetObjectLabel(GLenum identifier, GLuint name, GLsizei bufSize, GLsizei* length, GLchar* label) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fGetObjectLabel);
        mSymbols.fGetObjectLabel(identifier, name, bufSize, length, label);
        AFTER_GL_CALL;
    }

    void fGetObjectPtrLabel(const GLvoid* ptr, GLsizei bufSize, GLsizei* length, GLchar* label) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fGetObjectPtrLabel);
        mSymbols.fGetObjectPtrLabel(ptr, bufSize, length, label);
        AFTER_GL_CALL;
    }

    void fGenerateMipmap(GLenum target) {
        BEFORE_GL_CALL;
        mSymbols.fGenerateMipmap(target);
        AFTER_GL_CALL;
    }

    void fGetProgramiv(GLuint program, GLenum pname, GLint* param) {
        BEFORE_GL_CALL;
        mSymbols.fGetProgramiv(program, pname, param);
        AFTER_GL_CALL;
    }

    void fGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog) {
        BEFORE_GL_CALL;
        mSymbols.fGetProgramInfoLog(program, bufSize, length, infoLog);
        AFTER_GL_CALL;
    }

    void fTexParameteri(GLenum target, GLenum pname, GLint param) {
        BEFORE_GL_CALL;
        mSymbols.fTexParameteri(target, pname, param);
        AFTER_GL_CALL;
    }

    void fTexParameteriv(GLenum target, GLenum pname, const GLint* params) {
        BEFORE_GL_CALL;
        mSymbols.fTexParameteriv(target, pname, params);
        AFTER_GL_CALL;
    }

    void fTexParameterf(GLenum target, GLenum pname, GLfloat param) {
        BEFORE_GL_CALL;
        mSymbols.fTexParameterf(target, pname, param);
        AFTER_GL_CALL;
    }

    const GLubyte* fGetString(GLenum name) {
        BEFORE_GL_CALL;
        const GLubyte *result = mSymbols.fGetString(name);
        AFTER_GL_CALL;
        return result;
    }

    void fGetTexImage(GLenum target, GLint level, GLenum format, GLenum type, GLvoid *img) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fGetTexImage);
        mSymbols.fGetTexImage(target, level, format, type, img);
        AFTER_GL_CALL;
    }

    void fGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint *params)
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fGetTexLevelParameteriv);
        mSymbols.fGetTexLevelParameteriv(target, level, pname, params);
        AFTER_GL_CALL;
    }

    void fGetTexParameterfv(GLenum target, GLenum pname, GLfloat* params) {
        BEFORE_GL_CALL;
        mSymbols.fGetTexParameterfv(target, pname, params);
        AFTER_GL_CALL;
    }

    void fGetTexParameteriv(GLenum target, GLenum pname, GLint* params) {
        BEFORE_GL_CALL;
        mSymbols.fGetTexParameteriv(target, pname, params);
        AFTER_GL_CALL;
    }

    void fGetUniformfv(GLuint program, GLint location, GLfloat* params) {
        BEFORE_GL_CALL;
        mSymbols.fGetUniformfv(program, location, params);
        AFTER_GL_CALL;
    }

    void fGetUniformiv(GLuint program, GLint location, GLint* params) {
        BEFORE_GL_CALL;
        mSymbols.fGetUniformiv(program, location, params);
        AFTER_GL_CALL;
    }

    void fGetUniformuiv(GLuint program, GLint location, GLuint* params) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fGetUniformuiv);
        mSymbols.fGetUniformuiv(program, location, params);
        AFTER_GL_CALL;
    }

    GLint fGetUniformLocation (GLint programObj, const GLchar* name) {
        BEFORE_GL_CALL;
        GLint retval = mSymbols.fGetUniformLocation(programObj, name);
        AFTER_GL_CALL;
        return retval;
    }

    void fGetVertexAttribfv(GLuint index, GLenum pname, GLfloat* retval) {
        BEFORE_GL_CALL;
        mSymbols.fGetVertexAttribfv(index, pname, retval);
        AFTER_GL_CALL;
    }

    void fGetVertexAttribiv(GLuint index, GLenum pname, GLint* retval) {
        BEFORE_GL_CALL;
        mSymbols.fGetVertexAttribiv(index, pname, retval);
        AFTER_GL_CALL;
    }

    void fGetVertexAttribPointerv(GLuint index, GLenum pname, GLvoid** retval) {
        BEFORE_GL_CALL;
        mSymbols.fGetVertexAttribPointerv(index, pname, retval);
        AFTER_GL_CALL;
    }

    void fHint(GLenum target, GLenum mode) {
        BEFORE_GL_CALL;
        mSymbols.fHint(target, mode);
        AFTER_GL_CALL;
    }

    realGLboolean fIsBuffer(GLuint buffer) {
        BEFORE_GL_CALL;
        realGLboolean retval = mSymbols.fIsBuffer(buffer);
        AFTER_GL_CALL;
        return retval;
    }

    realGLboolean fIsEnabled(GLenum capability) {
        BEFORE_GL_CALL;
        realGLboolean retval = mSymbols.fIsEnabled(capability);
        AFTER_GL_CALL;
        return retval;
    }

    realGLboolean fIsProgram(GLuint program) {
        BEFORE_GL_CALL;
        realGLboolean retval = mSymbols.fIsProgram(program);
        AFTER_GL_CALL;
        return retval;
    }

    realGLboolean fIsShader(GLuint shader) {
        BEFORE_GL_CALL;
        realGLboolean retval = mSymbols.fIsShader(shader);
        AFTER_GL_CALL;
        return retval;
    }

    realGLboolean fIsTexture(GLuint texture) {
        BEFORE_GL_CALL;
        realGLboolean retval = mSymbols.fIsTexture(texture);
        AFTER_GL_CALL;
        return retval;
    }

    void fLineWidth(GLfloat width) {
        BEFORE_GL_CALL;
        mSymbols.fLineWidth(width);
        AFTER_GL_CALL;
    }

    void fLinkProgram(GLuint program) {
        BEFORE_GL_CALL;
        mSymbols.fLinkProgram(program);
        AFTER_GL_CALL;
    }

    void fObjectLabel(GLenum identifier, GLuint name, GLsizei length, const GLchar* label) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fObjectLabel);
        mSymbols.fObjectLabel(identifier, name, length, label);
        AFTER_GL_CALL;
    }

    void fObjectPtrLabel(const GLvoid* ptr, GLsizei length, const GLchar* label) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fObjectPtrLabel);
        mSymbols.fObjectPtrLabel(ptr, length, label);
        AFTER_GL_CALL;
    }

    void fLoadIdentity() {
        BEFORE_GL_CALL;
        mSymbols.fLoadIdentity();
        AFTER_GL_CALL;
    }

    void fLoadMatrixf(const GLfloat *matrix) {
        BEFORE_GL_CALL;
        mSymbols.fLoadMatrixf(matrix);
        AFTER_GL_CALL;
    }

    void fMatrixMode(GLenum mode) {
        BEFORE_GL_CALL;
        mSymbols.fMatrixMode(mode);
        AFTER_GL_CALL;
    }

    void fPixelStorei(GLenum pname, GLint param) {
        BEFORE_GL_CALL;
        mSymbols.fPixelStorei(pname, param);
        AFTER_GL_CALL;
    }

    void fTextureRangeAPPLE(GLenum target, GLsizei length, GLvoid *pointer) {
        ASSERT_NOT_PASSING_STACK_BUFFER_TO_GL(pointer);
        BEFORE_GL_CALL;
        mSymbols.fTextureRangeAPPLE(target, length, pointer);
        AFTER_GL_CALL;
    }

    void fPointParameterf(GLenum pname, GLfloat param) {
        BEFORE_GL_CALL;
        mSymbols.fPointParameterf(pname, param);
        AFTER_GL_CALL;
    }

    void fPolygonOffset(GLfloat factor, GLfloat bias) {
        BEFORE_GL_CALL;
        mSymbols.fPolygonOffset(factor, bias);
        AFTER_GL_CALL;
    }

    void fPopDebugGroup() {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fPopDebugGroup);
        mSymbols.fPopDebugGroup();
        AFTER_GL_CALL;
    }

    void fPushDebugGroup(GLenum source, GLuint id, GLsizei length, const GLchar* message) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fPushDebugGroup);
        mSymbols.fPushDebugGroup(source, id, length, message);
        AFTER_GL_CALL;
    }

    void fReadBuffer(GLenum mode) {
        BEFORE_GL_CALL;
        mSymbols.fReadBuffer(mode);
        AFTER_GL_CALL;
    }

    void raw_fReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels) {
        ASSERT_NOT_PASSING_STACK_BUFFER_TO_GL(pixels);
        BEFORE_GL_CALL;
        mSymbols.fReadPixels(x, y, width, height, format, type, pixels);
        AFTER_GL_CALL;
        mHeavyGLCallsSinceLastFlush = true;
    }

    void fReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
                     GLenum type, GLvoid* pixels);

public:
    void fSampleCoverage(GLclampf value, realGLboolean invert) {
        BEFORE_GL_CALL;
        mSymbols.fSampleCoverage(value, invert);
        AFTER_GL_CALL;
    }

    void fScissor(GLint x, GLint y, GLsizei width, GLsizei height) {
        if (mScissorRect[0] == x &&
            mScissorRect[1] == y &&
            mScissorRect[2] == width &&
            mScissorRect[3] == height)
        {
            return;
        }
        mScissorRect[0] = x;
        mScissorRect[1] = y;
        mScissorRect[2] = width;
        mScissorRect[3] = height;
        BEFORE_GL_CALL;
        mSymbols.fScissor(x, y, width, height);
        AFTER_GL_CALL;
    }

    void fStencilFunc(GLenum func, GLint reference, GLuint mask) {
        BEFORE_GL_CALL;
        mSymbols.fStencilFunc(func, reference, mask);
        AFTER_GL_CALL;
    }

    void fStencilFuncSeparate(GLenum frontfunc, GLenum backfunc, GLint reference, GLuint mask) {
        BEFORE_GL_CALL;
        mSymbols.fStencilFuncSeparate(frontfunc, backfunc, reference, mask);
        AFTER_GL_CALL;
    }

    void fStencilMask(GLuint mask) {
        BEFORE_GL_CALL;
        mSymbols.fStencilMask(mask);
        AFTER_GL_CALL;
    }

    void fStencilMaskSeparate(GLenum face, GLuint mask) {
        BEFORE_GL_CALL;
        mSymbols.fStencilMaskSeparate(face, mask);
        AFTER_GL_CALL;
    }

    void fStencilOp(GLenum fail, GLenum zfail, GLenum zpass) {
        BEFORE_GL_CALL;
        mSymbols.fStencilOp(fail, zfail, zpass);
        AFTER_GL_CALL;
    }

    void fStencilOpSeparate(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass) {
        BEFORE_GL_CALL;
        mSymbols.fStencilOpSeparate(face, sfail, dpfail, dppass);
        AFTER_GL_CALL;
    }

    void fTexGeni(GLenum coord, GLenum pname, GLint param) {
        BEFORE_GL_CALL;
        mSymbols.fTexGeni(coord, pname, param);
        AFTER_GL_CALL;
    }

    void fTexGenf(GLenum coord, GLenum pname, GLfloat param) {
        BEFORE_GL_CALL;
        mSymbols.fTexGenf(coord, pname, param);
        AFTER_GL_CALL;
    }

    void fTexGenfv(GLenum coord, GLenum pname, const GLfloat *params) {
        BEFORE_GL_CALL;
        mSymbols.fTexGenfv(coord, pname, params);
        AFTER_GL_CALL;
    }

private:
    void raw_fTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels) {
        ASSERT_NOT_PASSING_STACK_BUFFER_TO_GL(pixels);
        BEFORE_GL_CALL;
        mSymbols.fTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
        AFTER_GL_CALL;
        mHeavyGLCallsSinceLastFlush = true;
    }

public:
    void fTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels) {
        if (!IsTextureSizeSafeToPassToDriver(target, width, height)) {
            // pass wrong values to cause the GL to generate GL_INVALID_VALUE.
            // See bug 737182 and the comment in IsTextureSizeSafeToPassToDriver.
            level = -1;
            width = -1;
            height = -1;
            border = -1;
        }
        raw_fTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
    }

    void fTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid* pixels) {
        ASSERT_NOT_PASSING_STACK_BUFFER_TO_GL(pixels);
        BEFORE_GL_CALL;
        mSymbols.fTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
        AFTER_GL_CALL;
        mHeavyGLCallsSinceLastFlush = true;
    }

    void fUniform1f(GLint location, GLfloat v0) {
        BEFORE_GL_CALL;
        mSymbols.fUniform1f(location, v0);
        AFTER_GL_CALL;
    }

    void fUniform1fv(GLint location, GLsizei count, const GLfloat* value) {
        BEFORE_GL_CALL;
        mSymbols.fUniform1fv(location, count, value);
        AFTER_GL_CALL;
    }

    void fUniform1i(GLint location, GLint v0) {
        BEFORE_GL_CALL;
        mSymbols.fUniform1i(location, v0);
        AFTER_GL_CALL;
    }

    void fUniform1iv(GLint location, GLsizei count, const GLint* value) {
        BEFORE_GL_CALL;
        mSymbols.fUniform1iv(location, count, value);
        AFTER_GL_CALL;
    }

    void fUniform2f(GLint location, GLfloat v0, GLfloat v1) {
        BEFORE_GL_CALL;
        mSymbols.fUniform2f(location, v0, v1);
        AFTER_GL_CALL;
    }

    void fUniform2fv(GLint location, GLsizei count, const GLfloat* value) {
        BEFORE_GL_CALL;
        mSymbols.fUniform2fv(location, count, value);
        AFTER_GL_CALL;
    }

    void fUniform2i(GLint location, GLint v0, GLint v1) {
        BEFORE_GL_CALL;
        mSymbols.fUniform2i(location, v0, v1);
        AFTER_GL_CALL;
    }

    void fUniform2iv(GLint location, GLsizei count, const GLint* value) {
        BEFORE_GL_CALL;
        mSymbols.fUniform2iv(location, count, value);
        AFTER_GL_CALL;
    }

    void fUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2) {
        BEFORE_GL_CALL;
        mSymbols.fUniform3f(location, v0, v1, v2);
        AFTER_GL_CALL;
    }

    void fUniform3fv(GLint location, GLsizei count, const GLfloat* value) {
        BEFORE_GL_CALL;
        mSymbols.fUniform3fv(location, count, value);
        AFTER_GL_CALL;
    }

    void fUniform3i(GLint location, GLint v0, GLint v1, GLint v2) {
        BEFORE_GL_CALL;
        mSymbols.fUniform3i(location, v0, v1, v2);
        AFTER_GL_CALL;
    }

    void fUniform3iv(GLint location, GLsizei count, const GLint* value) {
        BEFORE_GL_CALL;
        mSymbols.fUniform3iv(location, count, value);
        AFTER_GL_CALL;
    }

    void fUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {
        BEFORE_GL_CALL;
        mSymbols.fUniform4f(location, v0, v1, v2, v3);
        AFTER_GL_CALL;
    }

    void fUniform4fv(GLint location, GLsizei count, const GLfloat* value) {
        BEFORE_GL_CALL;
        mSymbols.fUniform4fv(location, count, value);
        AFTER_GL_CALL;
    }

    void fUniform4i(GLint location, GLint v0, GLint v1, GLint v2, GLint v3) {
        BEFORE_GL_CALL;
        mSymbols.fUniform4i(location, v0, v1, v2, v3);
        AFTER_GL_CALL;
    }

    void fUniform4iv(GLint location, GLsizei count, const GLint* value) {
        BEFORE_GL_CALL;
        mSymbols.fUniform4iv(location, count, value);
        AFTER_GL_CALL;
    }

    void fUniformMatrix2fv(GLint location, GLsizei count, realGLboolean transpose, const GLfloat* value) {
        BEFORE_GL_CALL;
        mSymbols.fUniformMatrix2fv(location, count, transpose, value);
        AFTER_GL_CALL;
    }

    void fUniformMatrix2x3fv(GLint location, GLsizei count, realGLboolean transpose, const GLfloat* value) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fUniformMatrix2x3fv);
        mSymbols.fUniformMatrix2x3fv(location, count, transpose, value);
        AFTER_GL_CALL;
    }

    void fUniformMatrix2x4fv(GLint location, GLsizei count, realGLboolean transpose, const GLfloat* value) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fUniformMatrix2x4fv);
        mSymbols.fUniformMatrix2x4fv(location, count, transpose, value);
        AFTER_GL_CALL;
    }

    void fUniformMatrix3fv(GLint location, GLsizei count, realGLboolean transpose, const GLfloat* value) {
        BEFORE_GL_CALL;
        mSymbols.fUniformMatrix3fv(location, count, transpose, value);
        AFTER_GL_CALL;
    }

    void fUniformMatrix3x2fv(GLint location, GLsizei count, realGLboolean transpose, const GLfloat* value) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fUniformMatrix3x2fv);
        mSymbols.fUniformMatrix3x2fv(location, count, transpose, value);
        AFTER_GL_CALL;
    }

    void fUniformMatrix3x4fv(GLint location, GLsizei count, realGLboolean transpose, const GLfloat* value) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fUniformMatrix3x4fv);
        mSymbols.fUniformMatrix3x4fv(location, count, transpose, value);
        AFTER_GL_CALL;
    }

    void fUniformMatrix4fv(GLint location, GLsizei count, realGLboolean transpose, const GLfloat* value) {
        BEFORE_GL_CALL;
        mSymbols.fUniformMatrix4fv(location, count, transpose, value);
        AFTER_GL_CALL;
    }

    void fUniformMatrix4x2fv(GLint location, GLsizei count, realGLboolean transpose, const GLfloat* value) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fUniformMatrix4x2fv);
        mSymbols.fUniformMatrix4x2fv(location, count, transpose, value);
        AFTER_GL_CALL;
    }

    void fUniformMatrix4x3fv(GLint location, GLsizei count, realGLboolean transpose, const GLfloat* value) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fUniformMatrix4x3fv);
        mSymbols.fUniformMatrix4x3fv(location, count, transpose, value);
        AFTER_GL_CALL;
    }

    void fUseProgram(GLuint program) {
        BEFORE_GL_CALL;
        mSymbols.fUseProgram(program);
        AFTER_GL_CALL;
    }

    void fValidateProgram(GLuint program) {
        BEFORE_GL_CALL;
        mSymbols.fValidateProgram(program);
        AFTER_GL_CALL;
    }

    void fVertexAttribPointer(GLuint index, GLint size, GLenum type, realGLboolean normalized, GLsizei stride, const GLvoid* pointer) {
        BEFORE_GL_CALL;
        mSymbols.fVertexAttribPointer(index, size, type, normalized, stride, pointer);
        AFTER_GL_CALL;
    }

    void fVertexAttrib1f(GLuint index, GLfloat x) {
        BEFORE_GL_CALL;
        mSymbols.fVertexAttrib1f(index, x);
        AFTER_GL_CALL;
    }

    void fVertexAttrib2f(GLuint index, GLfloat x, GLfloat y) {
        BEFORE_GL_CALL;
        mSymbols.fVertexAttrib2f(index, x, y);
        AFTER_GL_CALL;
    }

    void fVertexAttrib3f(GLuint index, GLfloat x, GLfloat y, GLfloat z) {
        BEFORE_GL_CALL;
        mSymbols.fVertexAttrib3f(index, x, y, z);
        AFTER_GL_CALL;
    }

    void fVertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
        BEFORE_GL_CALL;
        mSymbols.fVertexAttrib4f(index, x, y, z, w);
        AFTER_GL_CALL;
    }

    void fVertexAttrib1fv(GLuint index, const GLfloat* v) {
        BEFORE_GL_CALL;
        mSymbols.fVertexAttrib1fv(index, v);
        AFTER_GL_CALL;
    }

    void fVertexAttrib2fv(GLuint index, const GLfloat* v) {
        BEFORE_GL_CALL;
        mSymbols.fVertexAttrib2fv(index, v);
        AFTER_GL_CALL;
    }

    void fVertexAttrib3fv(GLuint index, const GLfloat* v) {
        BEFORE_GL_CALL;
        mSymbols.fVertexAttrib3fv(index, v);
        AFTER_GL_CALL;
    }

    void fVertexAttrib4fv(GLuint index, const GLfloat* v) {
        BEFORE_GL_CALL;
        mSymbols.fVertexAttrib4fv(index, v);
        AFTER_GL_CALL;
    }

    void fVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer) {
        BEFORE_GL_CALL;
        mSymbols.fVertexPointer(size, type, stride, pointer);
        AFTER_GL_CALL;
    }

    void fCompileShader(GLuint shader) {
        BEFORE_GL_CALL;
        mSymbols.fCompileShader(shader);
        AFTER_GL_CALL;
    }

private:

    friend class SharedSurface_IOSurface;

    void raw_fCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border)
    {
        BEFORE_GL_CALL;
        mSymbols.fCopyTexImage2D(target, level, internalformat, x, y, width, height, border);
        AFTER_GL_CALL;
    }

    void raw_fCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height)
    {
        BEFORE_GL_CALL;
        mSymbols.fCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
        AFTER_GL_CALL;
    }

public:
    void fGetShaderiv(GLuint shader, GLenum pname, GLint* param) {
        BEFORE_GL_CALL;
        mSymbols.fGetShaderiv(shader, pname, param);
        AFTER_GL_CALL;
    }

    void fGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog) {
        BEFORE_GL_CALL;
        mSymbols.fGetShaderInfoLog(shader, bufSize, length, infoLog);
        AFTER_GL_CALL;
    }

private:
    void raw_fGetShaderPrecisionFormat(GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision) {
        MOZ_ASSERT(IsGLES());

        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fGetShaderPrecisionFormat);
        mSymbols.fGetShaderPrecisionFormat(shadertype, precisiontype, range, precision);
        AFTER_GL_CALL;
    }

public:
    void fGetShaderPrecisionFormat(GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision) {
        if (IsGLES()) {
            raw_fGetShaderPrecisionFormat(shadertype, precisiontype, range, precision);
        } else {
            // Fall back to automatic values because almost all desktop hardware supports the OpenGL standard precisions.
            GetShaderPrecisionFormatNonES2(shadertype, precisiontype, range, precision);
        }
    }

    void fGetShaderSource(GLint obj, GLsizei maxLength, GLsizei* length, GLchar* source) {
        BEFORE_GL_CALL;
        mSymbols.fGetShaderSource(obj, maxLength, length, source);
        AFTER_GL_CALL;
    }

    void fShaderSource(GLuint shader, GLsizei count, const GLchar* const* strings, const GLint* lengths) {
        BEFORE_GL_CALL;
        mSymbols.fShaderSource(shader, count, strings, lengths);
        AFTER_GL_CALL;
    }

private:
    friend class SharedSurface;

    void raw_fBindFramebuffer(GLenum target, GLuint framebuffer) {
        BEFORE_GL_CALL;
        mSymbols.fBindFramebuffer(target, framebuffer);
        AFTER_GL_CALL;
    }

public:
    void fBindRenderbuffer(GLenum target, GLuint renderbuffer) {
        BEFORE_GL_CALL;
        mSymbols.fBindRenderbuffer(target, renderbuffer);
        AFTER_GL_CALL;
    }

    GLenum fCheckFramebufferStatus(GLenum target) {
        BEFORE_GL_CALL;
        GLenum retval = mSymbols.fCheckFramebufferStatus(target);
        AFTER_GL_CALL;
        return retval;
    }

    void fFramebufferRenderbuffer(GLenum target, GLenum attachmentPoint, GLenum renderbufferTarget, GLuint renderbuffer) {
        BEFORE_GL_CALL;
        mSymbols.fFramebufferRenderbuffer(target, attachmentPoint, renderbufferTarget, renderbuffer);
        AFTER_GL_CALL;
    }

    void fFramebufferTexture2D(GLenum target, GLenum attachmentPoint, GLenum textureTarget, GLuint texture, GLint level) {
        BEFORE_GL_CALL;
        mSymbols.fFramebufferTexture2D(target, attachmentPoint, textureTarget, texture, level);
        AFTER_GL_CALL;
    }

    void fFramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fFramebufferTextureLayer);
        mSymbols.fFramebufferTextureLayer(target, attachment, texture, level, layer);
        AFTER_GL_CALL;
    }

    void fGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname, GLint* value) {
        BEFORE_GL_CALL;
        mSymbols.fGetFramebufferAttachmentParameteriv(target, attachment, pname, value);
        AFTER_GL_CALL;
    }

    void fGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint* value) {
        BEFORE_GL_CALL;
        mSymbols.fGetRenderbufferParameteriv(target, pname, value);
        AFTER_GL_CALL;
    }

    realGLboolean fIsFramebuffer (GLuint framebuffer) {
        BEFORE_GL_CALL;
        realGLboolean retval = mSymbols.fIsFramebuffer(framebuffer);
        AFTER_GL_CALL;
        return retval;
    }

public:
    realGLboolean fIsRenderbuffer (GLuint renderbuffer) {
        BEFORE_GL_CALL;
        realGLboolean retval = mSymbols.fIsRenderbuffer(renderbuffer);
        AFTER_GL_CALL;
        return retval;
    }

    void fRenderbufferStorage(GLenum target, GLenum internalFormat, GLsizei width, GLsizei height) {
        BEFORE_GL_CALL;
        mSymbols.fRenderbufferStorage(target, internalFormat, width, height);
        AFTER_GL_CALL;
    }

private:
    void raw_fDepthRange(GLclampf a, GLclampf b) {
        MOZ_ASSERT(!IsGLES());

        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fDepthRange);
        mSymbols.fDepthRange(a, b);
        AFTER_GL_CALL;
    }

    void raw_fDepthRangef(GLclampf a, GLclampf b) {
        MOZ_ASSERT(IsGLES());

        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fDepthRangef);
        mSymbols.fDepthRangef(a, b);
        AFTER_GL_CALL;
    }

    void raw_fClearDepth(GLclampf v) {
        MOZ_ASSERT(!IsGLES());

        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fClearDepth);
        mSymbols.fClearDepth(v);
        AFTER_GL_CALL;
    }

    void raw_fClearDepthf(GLclampf v) {
        MOZ_ASSERT(IsGLES());

        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fClearDepthf);
        mSymbols.fClearDepthf(v);
        AFTER_GL_CALL;
    }

public:
    void fDepthRange(GLclampf a, GLclampf b) {
        if (IsGLES()) {
            raw_fDepthRangef(a, b);
        } else {
            raw_fDepthRange(a, b);
        }
    }

    void fClearDepth(GLclampf v) {
        if (IsGLES()) {
            raw_fClearDepthf(v);
        } else {
            raw_fClearDepth(v);
        }
    }

    void* fMapBuffer(GLenum target, GLenum access) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fMapBuffer);
        void *ret = mSymbols.fMapBuffer(target, access);
        AFTER_GL_CALL;
        return ret;
    }

    realGLboolean fUnmapBuffer(GLenum target) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fUnmapBuffer);
        realGLboolean ret = mSymbols.fUnmapBuffer(target);
        AFTER_GL_CALL;
        return ret;
    }


private:
    GLuint raw_fCreateProgram() {
        BEFORE_GL_CALL;
        GLuint ret = mSymbols.fCreateProgram();
        AFTER_GL_CALL;
        return ret;
    }

    GLuint raw_fCreateShader(GLenum t) {
        BEFORE_GL_CALL;
        GLuint ret = mSymbols.fCreateShader(t);
        AFTER_GL_CALL;
        return ret;
    }

    void raw_fGenBuffers(GLsizei n, GLuint* names) {
        BEFORE_GL_CALL;
        mSymbols.fGenBuffers(n, names);
        AFTER_GL_CALL;
    }

    void raw_fGenFramebuffers(GLsizei n, GLuint* names) {
        BEFORE_GL_CALL;
        mSymbols.fGenFramebuffers(n, names);
        AFTER_GL_CALL;
    }

    void raw_fGenRenderbuffers(GLsizei n, GLuint* names) {
        BEFORE_GL_CALL;
        mSymbols.fGenRenderbuffers(n, names);
        AFTER_GL_CALL;
    }

    void raw_fGenTextures(GLsizei n, GLuint* names) {
        BEFORE_GL_CALL;
        mSymbols.fGenTextures(n, names);
        AFTER_GL_CALL;
    }

public:
    GLuint fCreateProgram() {
        GLuint ret = raw_fCreateProgram();
        TRACKING_CONTEXT(CreatedProgram(this, ret));
        return ret;
    }

    GLuint fCreateShader(GLenum t) {
        GLuint ret = raw_fCreateShader(t);
        TRACKING_CONTEXT(CreatedShader(this, ret));
        return ret;
    }

    void fGenBuffers(GLsizei n, GLuint* names) {
        raw_fGenBuffers(n, names);
        TRACKING_CONTEXT(CreatedBuffers(this, n, names));
    }

    void fGenFramebuffers(GLsizei n, GLuint* names) {
        raw_fGenFramebuffers(n, names);
        TRACKING_CONTEXT(CreatedFramebuffers(this, n, names));
    }

    void fGenRenderbuffers(GLsizei n, GLuint* names) {
        raw_fGenRenderbuffers(n, names);
        TRACKING_CONTEXT(CreatedRenderbuffers(this, n, names));
    }

    void fGenTextures(GLsizei n, GLuint* names) {
        raw_fGenTextures(n, names);
        TRACKING_CONTEXT(CreatedTextures(this, n, names));
    }

private:
    void raw_fDeleteProgram(GLuint program) {
        BEFORE_GL_CALL;
        mSymbols.fDeleteProgram(program);
        AFTER_GL_CALL;
    }

    void raw_fDeleteShader(GLuint shader) {
        BEFORE_GL_CALL;
        mSymbols.fDeleteShader(shader);
        AFTER_GL_CALL;
    }

    void raw_fDeleteBuffers(GLsizei n, const GLuint* names) {
        BEFORE_GL_CALL;
        mSymbols.fDeleteBuffers(n, names);
        AFTER_GL_CALL;
    }

    void raw_fDeleteFramebuffers(GLsizei n, const GLuint* names) {
        BEFORE_GL_CALL;
        mSymbols.fDeleteFramebuffers(n, names);
        AFTER_GL_CALL;
    }

    void raw_fDeleteRenderbuffers(GLsizei n, const GLuint* names) {
        BEFORE_GL_CALL;
        mSymbols.fDeleteRenderbuffers(n, names);
        AFTER_GL_CALL;
    }

    void raw_fDeleteTextures(GLsizei n, const GLuint* names) {
        BEFORE_GL_CALL;
        mSymbols.fDeleteTextures(n, names);
        AFTER_GL_CALL;
    }

public:

    void fDeleteProgram(GLuint program) {
        raw_fDeleteProgram(program);
        TRACKING_CONTEXT(DeletedProgram(this, program));
    }

    void fDeleteShader(GLuint shader) {
        raw_fDeleteShader(shader);
        TRACKING_CONTEXT(DeletedShader(this, shader));
    }

    void fDeleteBuffers(GLsizei n, const GLuint* names) {
        raw_fDeleteBuffers(n, names);
        TRACKING_CONTEXT(DeletedBuffers(this, n, names));
    }

    void fDeleteFramebuffers(GLsizei n, const GLuint* names);

    void fDeleteRenderbuffers(GLsizei n, const GLuint* names) {
        raw_fDeleteRenderbuffers(n, names);
        TRACKING_CONTEXT(DeletedRenderbuffers(this, n, names));
    }

    void fDeleteTextures(GLsizei n, const GLuint* names) {
        raw_fDeleteTextures(n, names);
        TRACKING_CONTEXT(DeletedTextures(this, n, names));
    }

    GLenum fGetGraphicsResetStatus() {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fGetGraphicsResetStatus);
        GLenum ret = mSymbols.fGetGraphicsResetStatus();
        AFTER_GL_CALL;
        return ret;
    }


// -----------------------------------------------------------------------------
// Extension ARB_sync (GL)
public:
    GLsync fFenceSync(GLenum condition, GLbitfield flags) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fFenceSync);
        GLsync ret = mSymbols.fFenceSync(condition, flags);
        AFTER_GL_CALL;
        return ret;
    }

    realGLboolean fIsSync(GLsync sync) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fIsSync);
        realGLboolean ret = mSymbols.fIsSync(sync);
        AFTER_GL_CALL;
        return ret;
    }

    void fDeleteSync(GLsync sync) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fDeleteSync);
        mSymbols.fDeleteSync(sync);
        AFTER_GL_CALL;
    }

    GLenum fClientWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fClientWaitSync);
        GLenum ret = mSymbols.fClientWaitSync(sync, flags, timeout);
        AFTER_GL_CALL;
        return ret;
    }

    void fWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fWaitSync);
        mSymbols.fWaitSync(sync, flags, timeout);
        AFTER_GL_CALL;
    }

    void fGetInteger64v(GLenum pname, GLint64 *params) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fGetInteger64v);
        mSymbols.fGetInteger64v(pname, params);
        AFTER_GL_CALL;
    }

    void fGetSynciv(GLsync sync, GLenum pname, GLsizei bufSize, GLsizei *length, GLint *values) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fGetSynciv);
        mSymbols.fGetSynciv(sync, pname, bufSize, length, values);
        AFTER_GL_CALL;
    }


// -----------------------------------------------------------------------------
// Extension OES_EGL_image (GLES)
public:
    void fEGLImageTargetTexture2D(GLenum target, GLeglImage image) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fEGLImageTargetTexture2D);
        mSymbols.fEGLImageTargetTexture2D(target, image);
        AFTER_GL_CALL;
        mHeavyGLCallsSinceLastFlush = true;
    }

    void fEGLImageTargetRenderbufferStorage(GLenum target, GLeglImage image)
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fEGLImageTargetRenderbufferStorage);
        mSymbols.fEGLImageTargetRenderbufferStorage(target, image);
        AFTER_GL_CALL;
    }


// -----------------------------------------------------------------------------
// Package XXX_bind_buffer_offset
public:
    void fBindBufferOffset(GLenum target, GLuint index, GLuint buffer, GLintptr offset)
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fBindBufferOffset);
        mSymbols.fBindBufferOffset(target, index, buffer, offset);
        AFTER_GL_CALL;
    }


// -----------------------------------------------------------------------------
// Package XXX_draw_buffers
public:
    void fDrawBuffers(GLsizei n, const GLenum* bufs) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fDrawBuffers);
        mSymbols.fDrawBuffers(n, bufs);
        AFTER_GL_CALL;
    }


// -----------------------------------------------------------------------------
// Package XXX_draw_instanced
public:
    void fDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei primcount)
    {
        BeforeGLDrawCall();
        raw_fDrawArraysInstanced(mode, first, count, primcount);
        AfterGLDrawCall();
    }

    void fDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices, GLsizei primcount)
    {
        BeforeGLDrawCall();
        raw_fDrawElementsInstanced(mode, count, type, indices, primcount);
        AfterGLDrawCall();
    }

private:
    void raw_fDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei primcount)
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fDrawArraysInstanced);
        mSymbols.fDrawArraysInstanced(mode, first, count, primcount);
        AFTER_GL_CALL;
    }

    void raw_fDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices, GLsizei primcount)
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fDrawElementsInstanced);
        mSymbols.fDrawElementsInstanced(mode, count, type, indices, primcount);
        AFTER_GL_CALL;
    }

// -----------------------------------------------------------------------------
// Feature draw_range_elements
public:
    void fDrawRangeElements(GLenum mode, GLuint start, GLuint end,
                            GLsizei count, GLenum type, const GLvoid* indices)
    {
        BeforeGLDrawCall();
        raw_fDrawRangeElements(mode, start, end, count, type, indices);
        AfterGLDrawCall();
    }

private:
    void raw_fDrawRangeElements(GLenum mode, GLuint start, GLuint end,
                                GLsizei count, GLenum type, const GLvoid* indices)
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fDrawRangeElements);
        mSymbols.fDrawRangeElements(mode, start, end, count, type, indices);
        AFTER_GL_CALL;
    }

// -----------------------------------------------------------------------------
// Package XXX_framebuffer_blit
public:
    // Draw/Read
    void fBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter) {
        BeforeGLDrawCall();
        BeforeGLReadCall();
        raw_fBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
        AfterGLReadCall();
        AfterGLDrawCall();
    }


private:
    void raw_fBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fBlitFramebuffer);
        mSymbols.fBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
        AFTER_GL_CALL;
    }


// -----------------------------------------------------------------------------
// Package XXX_framebuffer_multisample
public:
    void fRenderbufferStorageMultisample(GLenum target, GLsizei samples, GLenum internalFormat, GLsizei width, GLsizei height) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fRenderbufferStorageMultisample);
        mSymbols.fRenderbufferStorageMultisample(target, samples, internalFormat, width, height);
        AFTER_GL_CALL;
    }

// -----------------------------------------------------------------------------
//  GL 3.0, GL ES 3.0 & EXT_gpu_shader4
public:
    void fGetVertexAttribIiv(GLuint index, GLenum pname, GLint* params)
    {
        ASSERT_SYMBOL_PRESENT(fGetVertexAttribIiv);
        BEFORE_GL_CALL;
        mSymbols.fGetVertexAttribIiv(index, pname, params);
        AFTER_GL_CALL;
    }

    void fGetVertexAttribIuiv(GLuint index, GLenum pname, GLuint* params)
    {
        ASSERT_SYMBOL_PRESENT(fGetVertexAttribIuiv);
        BEFORE_GL_CALL;
        mSymbols.fGetVertexAttribIuiv(index, pname, params);
        AFTER_GL_CALL;
    }

    void fVertexAttribI4i(GLuint index, GLint x, GLint y, GLint z, GLint w)
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fVertexAttribI4i);
        mSymbols.fVertexAttribI4i(index, x, y, z, w);
        AFTER_GL_CALL;
    }

    void fVertexAttribI4iv(GLuint index, const GLint* v)
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fVertexAttribI4iv);
        mSymbols.fVertexAttribI4iv(index, v);
        AFTER_GL_CALL;
    }

    void fVertexAttribI4ui(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w)
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fVertexAttribI4ui);
        mSymbols.fVertexAttribI4ui(index, x, y, z, w);
        AFTER_GL_CALL;
    }

    void fVertexAttribI4uiv(GLuint index, const GLuint* v)
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fVertexAttribI4uiv);
        mSymbols.fVertexAttribI4uiv(index, v);
        AFTER_GL_CALL;
    }

    void fVertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const GLvoid* offset)
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fVertexAttribIPointer);
        mSymbols.fVertexAttribIPointer(index, size, type, stride, offset);
        AFTER_GL_CALL;
    }

    void fUniform1ui(GLint location, GLuint v0) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fUniform1ui);
        mSymbols.fUniform1ui(location, v0);
        AFTER_GL_CALL;
    }

    void fUniform2ui(GLint location, GLuint v0, GLuint v1) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fUniform2ui);
        mSymbols.fUniform2ui(location, v0, v1);
        AFTER_GL_CALL;
    }

    void fUniform3ui(GLint location, GLuint v0, GLuint v1, GLuint v2) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fUniform3ui);
        mSymbols.fUniform3ui(location, v0, v1, v2);
        AFTER_GL_CALL;
    }

    void fUniform4ui(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fUniform4ui);
        mSymbols.fUniform4ui(location, v0, v1, v2, v3);
        AFTER_GL_CALL;
    }

    void fUniform1uiv(GLint location, GLsizei count, const GLuint* value) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fUniform1uiv);
        mSymbols.fUniform1uiv(location, count, value);
        AFTER_GL_CALL;
    }

    void fUniform2uiv(GLint location, GLsizei count, const GLuint* value) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fUniform2uiv);
        mSymbols.fUniform2uiv(location, count, value);
        AFTER_GL_CALL;
    }

    void fUniform3uiv(GLint location, GLsizei count, const GLuint* value) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fUniform3uiv);
        mSymbols.fUniform3uiv(location, count, value);
        AFTER_GL_CALL;
    }

    void fUniform4uiv(GLint location, GLsizei count, const GLuint* value) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fUniform4uiv);
        mSymbols.fUniform4uiv(location, count, value);
        AFTER_GL_CALL;
    }

    GLint fGetFragDataLocation(GLuint program, const GLchar* name)
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fGetFragDataLocation);
        GLint result = mSymbols.fGetFragDataLocation(program, name);
        AFTER_GL_CALL;
        return result;
    }


// -----------------------------------------------------------------------------
// Package XXX_instanced_arrays
public:
    void fVertexAttribDivisor(GLuint index, GLuint divisor)
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fVertexAttribDivisor);
        mSymbols.fVertexAttribDivisor(index, divisor);
        AFTER_GL_CALL;
    }

// -----------------------------------------------------------------------------
// Feature internalformat_query
public:
    void fGetInternalformativ(GLenum target, GLenum internalformat, GLenum pname, GLsizei bufSize, GLint* params) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fGetInternalformativ);
        mSymbols.fGetInternalformativ(target, internalformat, pname, bufSize, params);
        AFTER_GL_CALL;
    }


// -----------------------------------------------------------------------------
// Package XXX_query_counter
/**
 * XXX_query_counter:
 *  - depends on XXX_query_objects
 *  - provide all followed entry points
 *  - provide GL_TIMESTAMP
 */
public:
    void fQueryCounter(GLuint id, GLenum target) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fQueryCounter);
        mSymbols.fQueryCounter(id, target);
        AFTER_GL_CALL;
    }


// -----------------------------------------------------------------------------
// Package XXX_query_objects
/**
 * XXX_query_objects:
 *  - provide all followed entry points
 *
 * XXX_occlusion_query2:
 *  - depends on XXX_query_objects
 *  - provide ANY_SAMPLES_PASSED
 *
 * XXX_occlusion_query_boolean:
 *  - depends on XXX_occlusion_query2
 *  - provide ANY_SAMPLES_PASSED_CONSERVATIVE
 */
public:
    void fDeleteQueries(GLsizei n, const GLuint* names) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fDeleteQueries);
        mSymbols.fDeleteQueries(n, names);
        AFTER_GL_CALL;
        TRACKING_CONTEXT(DeletedQueries(this, n, names));
    }

    void fGenQueries(GLsizei n, GLuint* names) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fGenQueries);
        mSymbols.fGenQueries(n, names);
        AFTER_GL_CALL;
        TRACKING_CONTEXT(CreatedQueries(this, n, names));
    }

    void fGetQueryiv(GLenum target, GLenum pname, GLint* params) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fGetQueryiv);
        mSymbols.fGetQueryiv(target, pname, params);
        AFTER_GL_CALL;
    }

    void fGetQueryObjectuiv(GLuint id, GLenum pname, GLuint* params) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fGetQueryObjectuiv);
        mSymbols.fGetQueryObjectuiv(id, pname, params);
        AFTER_GL_CALL;
    }

    realGLboolean fIsQuery(GLuint query) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fIsQuery);
        realGLboolean retval = mSymbols.fIsQuery(query);
        AFTER_GL_CALL;
        return retval;
    }

// -----------------------------------------------------------------------------
// Package XXX_get_query_object_i64v
/**
 * XXX_get_query_object_i64v:
 *  - depends on XXX_query_objects
 *  - provide the followed entry point
 */
public:
    void fGetQueryObjecti64v(GLuint id, GLenum pname, GLint64* params) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fGetQueryObjecti64v);
        mSymbols.fGetQueryObjecti64v(id, pname, params);
        AFTER_GL_CALL;
    }

    void fGetQueryObjectui64v(GLuint id, GLenum pname, GLuint64* params) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fGetQueryObjectui64v);
        mSymbols.fGetQueryObjectui64v(id, pname, params);
        AFTER_GL_CALL;
    }


// -----------------------------------------------------------------------------
// Package XXX_get_query_object_iv
/**
 * XXX_get_query_object_iv:
 *  - depends on XXX_query_objects
 *  - provide the followed entry point
 *
 * XXX_occlusion_query:
 *  - depends on XXX_get_query_object_iv
 *  - provide LOCAL_GL_SAMPLES_PASSED
 */
public:
    void fGetQueryObjectiv(GLuint id, GLenum pname, GLint* params) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fGetQueryObjectiv);
        mSymbols.fGetQueryObjectiv(id, pname, params);
        AFTER_GL_CALL;
    }


// -----------------------------------------------------------------------------
// GL 4.0, GL ES 3.0, ARB_transform_feedback2, NV_transform_feedback2
public:
    void fBindBufferBase(GLenum target, GLuint index, GLuint buffer)
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fBindBufferBase);
        mSymbols.fBindBufferBase(target, index, buffer);
        AFTER_GL_CALL;
    }

    void fBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size)
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fBindBufferRange);
        mSymbols.fBindBufferRange(target, index, buffer, offset, size);
        AFTER_GL_CALL;
    }

    void fGenTransformFeedbacks(GLsizei n, GLuint* ids)
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fGenTransformFeedbacks);
        mSymbols.fGenTransformFeedbacks(n, ids);
        AFTER_GL_CALL;
    }

    void fDeleteTransformFeedbacks(GLsizei n, const GLuint* ids)
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fDeleteTransformFeedbacks);
        mSymbols.fDeleteTransformFeedbacks(n, ids);
        AFTER_GL_CALL;
    }

    realGLboolean fIsTransformFeedback(GLuint id)
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fIsTransformFeedback);
        realGLboolean result = mSymbols.fIsTransformFeedback(id);
        AFTER_GL_CALL;
        return result;
    }

    void fBindTransformFeedback(GLenum target, GLuint id)
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fBindTransformFeedback);
        mSymbols.fBindTransformFeedback(target, id);
        AFTER_GL_CALL;
    }

    void fBeginTransformFeedback(GLenum primitiveMode)
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fBeginTransformFeedback);
        mSymbols.fBeginTransformFeedback(primitiveMode);
        AFTER_GL_CALL;
    }

    void fEndTransformFeedback()
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fEndTransformFeedback);
        mSymbols.fEndTransformFeedback();
        AFTER_GL_CALL;
    }

    void fTransformFeedbackVaryings(GLuint program, GLsizei count, const GLchar* const* varyings, GLenum bufferMode)
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fTransformFeedbackVaryings);
        mSymbols.fTransformFeedbackVaryings(program, count, varyings, bufferMode);
        AFTER_GL_CALL;
    }

    void fGetTransformFeedbackVarying(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLsizei* size, GLenum* type, GLchar* name)
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fGetTransformFeedbackVarying);
        mSymbols.fGetTransformFeedbackVarying(program, index, bufSize, length, size, type, name);
        AFTER_GL_CALL;
    }

    void fPauseTransformFeedback()
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fPauseTransformFeedback);
        mSymbols.fPauseTransformFeedback();
        AFTER_GL_CALL;
    }

    void fResumeTransformFeedback()
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fResumeTransformFeedback);
        mSymbols.fResumeTransformFeedback();
        AFTER_GL_CALL;
    }

    void fGetIntegeri_v(GLenum param, GLuint index, GLint* values)
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fGetIntegeri_v);
        mSymbols.fGetIntegeri_v(param, index, values);
        AFTER_GL_CALL;
    }

    void fGetInteger64i_v(GLenum target, GLuint index, GLint64* data) {
        ASSERT_SYMBOL_PRESENT(fGetInteger64i_v);
        BEFORE_GL_CALL;
        mSymbols.fGetInteger64i_v(target, index, data);
        AFTER_GL_CALL;
    }

// -----------------------------------------------------------------------------
// Package XXX_vertex_array_object
public:
    void fBindVertexArray(GLuint array)
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fBindVertexArray);
        mSymbols.fBindVertexArray(array);
        AFTER_GL_CALL;
    }

    void fDeleteVertexArrays(GLsizei n, const GLuint *arrays)
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fDeleteVertexArrays);
        mSymbols.fDeleteVertexArrays(n, arrays);
        AFTER_GL_CALL;
    }

    void fGenVertexArrays(GLsizei n, GLuint *arrays)
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fGenVertexArrays);
        mSymbols.fGenVertexArrays(n, arrays);
        AFTER_GL_CALL;
    }

    realGLboolean fIsVertexArray(GLuint array)
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fIsVertexArray);
        realGLboolean ret = mSymbols.fIsVertexArray(array);
        AFTER_GL_CALL;
        return ret;
    }

// -----------------------------------------------------------------------------
// Extension NV_fence
public:
    void fGenFences(GLsizei n, GLuint* fences)
    {
        ASSERT_SYMBOL_PRESENT(fGenFences);
        BEFORE_GL_CALL;
        mSymbols.fGenFences(n, fences);
        AFTER_GL_CALL;
    }

    void fDeleteFences(GLsizei n, const GLuint* fences)
    {
        ASSERT_SYMBOL_PRESENT(fDeleteFences);
        BEFORE_GL_CALL;
        mSymbols.fDeleteFences(n, fences);
        AFTER_GL_CALL;
    }

    void fSetFence(GLuint fence, GLenum condition)
    {
        ASSERT_SYMBOL_PRESENT(fSetFence);
        BEFORE_GL_CALL;
        mSymbols.fSetFence(fence, condition);
        AFTER_GL_CALL;
    }

    realGLboolean fTestFence(GLuint fence)
    {
        ASSERT_SYMBOL_PRESENT(fTestFence);
        BEFORE_GL_CALL;
        realGLboolean ret = mSymbols.fTestFence(fence);
        AFTER_GL_CALL;
        return ret;
    }

    void fFinishFence(GLuint fence)
    {
        ASSERT_SYMBOL_PRESENT(fFinishFence);
        BEFORE_GL_CALL;
        mSymbols.fFinishFence(fence);
        AFTER_GL_CALL;
    }

    realGLboolean fIsFence(GLuint fence)
    {
        ASSERT_SYMBOL_PRESENT(fIsFence);
        BEFORE_GL_CALL;
        realGLboolean ret = mSymbols.fIsFence(fence);
        AFTER_GL_CALL;
        return ret;
    }

    void fGetFenceiv(GLuint fence, GLenum pname, GLint* params)
    {
        ASSERT_SYMBOL_PRESENT(fGetFenceiv);
        BEFORE_GL_CALL;
        mSymbols.fGetFenceiv(fence, pname, params);
        AFTER_GL_CALL;
    }

// -----------------------------------------------------------------------------
// Extension NV_texture_barrier
public:
    void fTextureBarrier()
    {
        ASSERT_SYMBOL_PRESENT(fTextureBarrier);
        BEFORE_GL_CALL;
        mSymbols.fTextureBarrier();
        AFTER_GL_CALL;
    }

// Core GL & Extension ARB_copy_buffer
public:
    void fCopyBufferSubData(GLenum readtarget, GLenum writetarget,
                            GLintptr readoffset, GLintptr writeoffset,
                            GLsizeiptr size)
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fCopyBufferSubData);
        mSymbols.fCopyBufferSubData(readtarget, writetarget, readoffset, writeoffset, size);
        AFTER_GL_CALL;
    }


// -----------------------------------------------------------------------------
// Core GL & Extension ARB_map_buffer_range
public:
    void* fMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length,
                          GLbitfield access)
    {
        ASSERT_SYMBOL_PRESENT(fMapBufferRange);
        BEFORE_GL_CALL;
        void* data = mSymbols.fMapBufferRange(target, offset, length, access);
        AFTER_GL_CALL;
        return data;
    }

    void fFlushMappedBufferRange(GLenum target, GLintptr offset, GLsizeiptr length) {
        ASSERT_SYMBOL_PRESENT(fFlushMappedBufferRange);
        BEFORE_GL_CALL;
        mSymbols.fFlushMappedBufferRange(target, offset, length);
        AFTER_GL_CALL;
    }


// -----------------------------------------------------------------------------
// Core GL & Extension ARB_sampler_objects
public:
    void fGenSamplers(GLsizei count, GLuint *samplers)
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fGenSamplers);
        mSymbols.fGenSamplers(count, samplers);
        AFTER_GL_CALL;
    }

    void fDeleteSamplers(GLsizei count, const GLuint *samplers)
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fDeleteSamplers);
        mSymbols.fDeleteSamplers(count, samplers);
        AFTER_GL_CALL;
    }

    realGLboolean fIsSampler(GLuint sampler)
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fIsSampler);
        realGLboolean result = mSymbols.fIsSampler(sampler);
        AFTER_GL_CALL;
        return result;
    }

    void fBindSampler(GLuint unit, GLuint sampler)
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fBindSampler);
        mSymbols.fBindSampler(unit, sampler);
        AFTER_GL_CALL;
    }

    void fSamplerParameteri(GLuint sampler, GLenum pname, GLint param)
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fSamplerParameteri);
        mSymbols.fSamplerParameteri(sampler, pname, param);
        AFTER_GL_CALL;
    }

    void fSamplerParameteriv(GLuint sampler, GLenum pname, const GLint *param)
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fSamplerParameteriv);
        mSymbols.fSamplerParameteriv(sampler, pname, param);
        AFTER_GL_CALL;
    }

    void fSamplerParameterf(GLuint sampler, GLenum pname, GLfloat param)
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fSamplerParameterf);
        mSymbols.fSamplerParameterf(sampler, pname, param);
        AFTER_GL_CALL;
    }

    void fSamplerParameterfv(GLuint sampler, GLenum pname, const GLfloat *param)
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fSamplerParameterfv);
        mSymbols.fSamplerParameterfv(sampler, pname, param);
        AFTER_GL_CALL;
    }

    void fGetSamplerParameteriv(GLuint sampler, GLenum pname, GLint *params)
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fGetSamplerParameteriv);
        mSymbols.fGetSamplerParameteriv(sampler, pname, params);
        AFTER_GL_CALL;
    }

    void fGetSamplerParameterfv(GLuint sampler, GLenum pname, GLfloat *params)
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fGetSamplerParameterfv);
        mSymbols.fGetSamplerParameterfv(sampler, pname, params);
        AFTER_GL_CALL;
    }


// -----------------------------------------------------------------------------
// Core GL & Extension ARB_uniform_buffer_object
public:
    void fGetUniformIndices(GLuint program, GLsizei uniformCount,
                            const GLchar* const* uniformNames, GLuint* uniformIndices)
    {
        ASSERT_SYMBOL_PRESENT(fGetUniformIndices);
        BEFORE_GL_CALL;
        mSymbols.fGetUniformIndices(program, uniformCount, uniformNames, uniformIndices);
        AFTER_GL_CALL;
    }

    void fGetActiveUniformsiv(GLuint program, GLsizei uniformCount, const GLuint* uniformIndices,
                              GLenum pname, GLint* params)
    {
        ASSERT_SYMBOL_PRESENT(fGetActiveUniformsiv);
        BEFORE_GL_CALL;
        mSymbols.fGetActiveUniformsiv(program, uniformCount, uniformIndices, pname, params);
        AFTER_GL_CALL;
    }

    GLuint fGetUniformBlockIndex(GLuint program, const GLchar* uniformBlockName) {
        ASSERT_SYMBOL_PRESENT(fGetUniformBlockIndex);
        BEFORE_GL_CALL;
        GLuint result = mSymbols.fGetUniformBlockIndex(program, uniformBlockName);
        AFTER_GL_CALL;
        return result;
    }

    void fGetActiveUniformBlockiv(GLuint program, GLuint uniformBlockIndex,
                                  GLenum pname, GLint* params)
    {
        ASSERT_SYMBOL_PRESENT(fGetActiveUniformBlockiv);
        BEFORE_GL_CALL;
        mSymbols.fGetActiveUniformBlockiv(program, uniformBlockIndex, pname, params);
        AFTER_GL_CALL;
    }

    void fGetActiveUniformBlockName(GLuint program, GLuint uniformBlockIndex, GLsizei bufSize,
                                    GLsizei* length, GLchar* uniformBlockName)
    {
        ASSERT_SYMBOL_PRESENT(fGetActiveUniformBlockName);
        BEFORE_GL_CALL;
        mSymbols.fGetActiveUniformBlockName(program, uniformBlockIndex, bufSize, length, uniformBlockName);
        AFTER_GL_CALL;
    }

    void fUniformBlockBinding(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding) {
        ASSERT_SYMBOL_PRESENT(fUniformBlockBinding);
        BEFORE_GL_CALL;
        mSymbols.fUniformBlockBinding(program, uniformBlockIndex, uniformBlockBinding);
        AFTER_GL_CALL;
    }

// -----------------------------------------------------------------------------
// Core GL 4.2, GL ES 3.0 & Extension ARB_texture_storage/EXT_texture_storage
    void fTexStorage2D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height)
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fTexStorage2D);
        mSymbols.fTexStorage2D(target, levels, internalformat, width, height);
        AFTER_GL_CALL;
    }

    void fTexStorage3D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth)
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fTexStorage3D);
        mSymbols.fTexStorage3D(target, levels, internalformat, width, height, depth);
        AFTER_GL_CALL;
    }

// -----------------------------------------------------------------------------
// 3D Textures
    void fTexImage3D(GLenum target, GLint level,
                     GLint internalFormat,
                     GLsizei width, GLsizei height, GLsizei depth,
                     GLint border, GLenum format, GLenum type,
                     const GLvoid * data)
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fTexImage3D);
        mSymbols.fTexImage3D(target, level, internalFormat,
                             width, height, depth,
                             border, format, type,
                             data);
        AFTER_GL_CALL;
    }

    void fTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                        GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                        GLenum format, GLenum type, const GLvoid* pixels)
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fTexSubImage3D);
        mSymbols.fTexSubImage3D(target, level, xoffset, yoffset, zoffset,
                                width, height, depth, format, type,
                                pixels);
        AFTER_GL_CALL;
    }

    void fCopyTexSubImage3D(GLenum target, GLint level, GLint xoffset,
                            GLint yoffset, GLint zoffset, GLint x,
                            GLint y, GLsizei width, GLsizei height)
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fCopyTexSubImage3D);
        mSymbols.fCopyTexSubImage3D(target, level, xoffset, yoffset, zoffset,
                                    x, y, width, height);
        AFTER_GL_CALL;
    }

    void fCompressedTexImage3D(GLenum target, GLint level, GLenum internalformat,
                               GLsizei width, GLsizei height, GLsizei depth,
                               GLint border, GLsizei imageSize, const GLvoid* data)
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fCompressedTexImage3D);
        mSymbols.fCompressedTexImage3D(target, level, internalformat,
                                       width, height, depth,
                                       border, imageSize, data);
        AFTER_GL_CALL;
    }

    void fCompressedTexSubImage3D(GLenum target, GLint level,
                                  GLint xoffset, GLint yoffset, GLint zoffset,
                                  GLsizei width, GLsizei height, GLsizei depth,
                                  GLenum format, GLsizei imageSize, const GLvoid* data)
    {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fCompressedTexSubImage3D);
        mSymbols.fCompressedTexSubImage3D(target, level,
                                          xoffset, yoffset, zoffset,
                                          width, height, depth,
                                          format, imageSize, data);
        AFTER_GL_CALL;
    }

// -----------------------------------------------------------------------------
// get_string_indexed

    const GLubyte* fGetStringi(GLenum name, GLuint index) {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fGetStringi);
        const GLubyte* ret = mSymbols.fGetStringi(name, index);
        AFTER_GL_CALL;
        return ret;
    }

// -----------------------------------------------------------------------------
// APPLE_framebuffer_multisample

    void fResolveMultisampleFramebufferAPPLE() {
        BEFORE_GL_CALL;
        ASSERT_SYMBOL_PRESENT(fResolveMultisampleFramebufferAPPLE);
        mSymbols.fResolveMultisampleFramebufferAPPLE();
        AFTER_GL_CALL;
    }

// -----------------------------------------------------------------------------
// Constructor
protected:
    explicit GLContext(const SurfaceCaps& caps,
                       GLContext* sharedContext = nullptr,
                       bool isOffscreen = false);


// -----------------------------------------------------------------------------
// Destructor
public:
    virtual ~GLContext();

    // Mark this context as destroyed.  This will nullptr out all
    // the GL function pointers!
    void MarkDestroyed();

// -----------------------------------------------------------------------------
// Everything that isn't standard GL APIs
protected:
    typedef gfx::SurfaceFormat SurfaceFormat;

    virtual bool MakeCurrentImpl(bool aForce) = 0;

public:
#ifdef MOZ_GL_DEBUG
    static void StaticInit() {
        PR_NewThreadPrivateIndex(&sCurrentGLContextTLS, nullptr);
    }
#endif

    bool MakeCurrent(bool aForce = false) {
        if (IsDestroyed()) {
            return false;
        }
#ifdef MOZ_GL_DEBUG
    PR_SetThreadPrivate(sCurrentGLContextTLS, this);

    // XXX this assertion is disabled because it's triggering on Mac;
    // we need to figure out why and reenable it.
#if 0
        // IsOwningThreadCurrent is a bit of a misnomer;
        // the "owning thread" is the creation thread,
        // and the only thread that can own this.  We don't
        // support contexts used on multiple threads.
        NS_ASSERTION(IsOwningThreadCurrent(),
                     "MakeCurrent() called on different thread than this context was created on!");
#endif
#endif
        return MakeCurrentImpl(aForce);
    }

    virtual bool Init() = 0;

    virtual bool SetupLookupFunction() = 0;

    virtual void ReleaseSurface() {}

    bool IsDestroyed() {
        // MarkDestroyed will mark all these as null.
        return mSymbols.fUseProgram == nullptr;
    }

    GLContext *GetSharedContext() { return mSharedContext; }

    /**
     * Returns true if the thread on which this context was created is the currently
     * executing thread.
     */
    bool IsOwningThreadCurrent();

    static void PlatformStartup();

public:
    /**
     * If this context wraps a double-buffered target, swap the back
     * and front buffers.  It should be assumed that after a swap, the
     * contents of the new back buffer are undefined.
     */
    virtual bool SwapBuffers() { return false; }

    /**
     * Defines a two-dimensional texture image for context target surface
     */
    virtual bool BindTexImage() { return false; }
    /*
     * Releases a color buffer that is being used as a texture
     */
    virtual bool ReleaseTexImage() { return false; }

    // Before reads from offscreen texture
    void GuaranteeResolve();

    /*
     * Resize the current offscreen buffer.  Returns true on success.
     * If it returns false, the context should be treated as unusable
     * and should be recreated.  After the resize, the viewport is not
     * changed; glViewport should be called as appropriate.
     *
     * Only valid if IsOffscreen() returns true.
     */
    bool ResizeOffscreen(const gfx::IntSize& size) {
        return ResizeScreenBuffer(size);
    }

    /*
     * Return size of this offscreen context.
     *
     * Only valid if IsOffscreen() returns true.
     */
    const gfx::IntSize& OffscreenSize() const;

    void BindFB(GLuint fb) {
        fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, fb);
        MOZ_ASSERT(!fb || fIsFramebuffer(fb));
    }

    void BindDrawFB(GLuint fb) {
        fBindFramebuffer(LOCAL_GL_DRAW_FRAMEBUFFER_EXT, fb);
    }

    void BindReadFB(GLuint fb) {
        fBindFramebuffer(LOCAL_GL_READ_FRAMEBUFFER_EXT, fb);
    }

    GLuint GetDrawFB();

    GLuint GetReadFB();

    GLuint GetFB();

    /*
     * Retrieves the size of the native windowing system drawable.
     */
    virtual Maybe<gfx::IntSize> GetTargetSize() {
        return Maybe<gfx::IntSize>();
    };

private:
    void GetShaderPrecisionFormatNonES2(GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision) {
        switch (precisiontype) {
            case LOCAL_GL_LOW_FLOAT:
            case LOCAL_GL_MEDIUM_FLOAT:
            case LOCAL_GL_HIGH_FLOAT:
                // Assume IEEE 754 precision
                range[0] = 127;
                range[1] = 127;
                *precision = 23;
                break;
            case LOCAL_GL_LOW_INT:
            case LOCAL_GL_MEDIUM_INT:
            case LOCAL_GL_HIGH_INT:
                // Some (most) hardware only supports single-precision floating-point numbers,
                // which can accurately represent integers up to +/-16777216
                range[0] = 24;
                range[1] = 24;
                *precision = 0;
                break;
        }
    }

public:

    void ForceDirtyScreen();
    void CleanDirtyScreen();

    virtual GLenum GetPreferredARGB32Format() const { return LOCAL_GL_RGBA; }

    virtual bool RenewSurface(nsIWidget* aWidget) { return false; }

    // Shared code for GL extensions and GLX extensions.
    static bool ListHasExtension(const GLubyte *extensions,
                                 const char *extension);

    GLint GetMaxTextureImageSize() { return mMaxTextureImageSize; }

public:
    std::map<GLuint, SharedSurface*> mFBOMapping;

    enum {
        DebugEnabled = 1 << 0,
        DebugTrace = 1 << 1,
        DebugAbortOnError = 1 << 2
    };

    static uint32_t sDebugMode;

    static uint32_t DebugMode() {
#ifdef MOZ_GL_DEBUG
        return sDebugMode;
#else
        return 0;
#endif
    }

protected:
    RefPtr<GLContext> mSharedContext;

    // The thread id which this context was created.
    PlatformThreadId mOwningThreadId;

    GLContextSymbols mSymbols;

#ifdef MOZ_GL_DEBUG
    // GLDebugMode will check that we don't send call
    // to a GLContext that isn't current on the current
    // thread.
    // Store the current context when binding to thread local
    // storage to support DebugMode on an arbitrary thread.
    static unsigned sCurrentGLContextTLS;
#endif

    UniquePtr<GLBlitHelper> mBlitHelper;
    UniquePtr<GLReadTexImageHelper> mReadTexImageHelper;

public:
    GLBlitHelper* BlitHelper();
    GLBlitTextureImageHelper* BlitTextureImageHelper();
    GLReadTexImageHelper* ReadTexImageHelper();

    // Assumes shares are created by all sharing with the same global context.
    bool SharesWith(const GLContext* other) const {
        MOZ_ASSERT(!this->mSharedContext || !this->mSharedContext->mSharedContext);
        MOZ_ASSERT(!other->mSharedContext || !other->mSharedContext->mSharedContext);
        MOZ_ASSERT(!this->mSharedContext ||
                   !other->mSharedContext ||
                   this->mSharedContext == other->mSharedContext);

        const GLContext* thisShared = this->mSharedContext ? this->mSharedContext
                                                           : this;
        const GLContext* otherShared = other->mSharedContext ? other->mSharedContext
                                                             : other;

        return thisShared == otherShared;
    }

    bool InitOffscreen(const gfx::IntSize& size, const SurfaceCaps& caps);

protected:
    // Note that it does -not- clear the resized buffers.
    bool CreateScreenBuffer(const gfx::IntSize& size, const SurfaceCaps& caps) {
        if (!IsOffscreenSizeAllowed(size))
            return false;

       return CreateScreenBufferImpl(size, caps);
    }

    bool CreateScreenBufferImpl(const gfx::IntSize& size,
                                const SurfaceCaps& caps);

public:
    bool ResizeScreenBuffer(const gfx::IntSize& size);

protected:
    SurfaceCaps mCaps;

public:
    const SurfaceCaps& Caps() const {
        return mCaps;
    }

    // Only varies based on bpp16 and alpha.
    GLFormats ChooseGLFormats(const SurfaceCaps& caps) const;

    bool IsFramebufferComplete(GLuint fb, GLenum* status = nullptr);

    // Does not check completeness.
    void AttachBuffersToFB(GLuint colorTex, GLuint colorRB,
                           GLuint depthRB, GLuint stencilRB,
                           GLuint fb, GLenum target = LOCAL_GL_TEXTURE_2D);

    // Passing null is fine if the value you'd get is 0.
    bool AssembleOffscreenFBs(const GLuint colorMSRB,
                              const GLuint depthRB,
                              const GLuint stencilRB,
                              const GLuint texture,
                              GLuint* drawFB,
                              GLuint* readFB);

protected:
    friend class GLScreenBuffer;
    UniquePtr<GLScreenBuffer> mScreen;

    SharedSurface* mLockedSurface;

public:
    void LockSurface(SharedSurface* surf) {
        MOZ_ASSERT(!mLockedSurface);
        mLockedSurface = surf;
    }

    void UnlockSurface(SharedSurface* surf) {
        MOZ_ASSERT(mLockedSurface == surf);
        mLockedSurface = nullptr;
    }

    SharedSurface* GetLockedSurface() const {
        return mLockedSurface;
    }

    bool IsOffscreen() const {
        return mIsOffscreen;
    }

    GLScreenBuffer* Screen() const {
        return mScreen.get();
    }

    /* Clear to transparent black, with 0 depth and stencil,
     * while preserving current ClearColor etc. values.
     * Useful for resizing offscreen buffers.
     */
    void ClearSafely();

    bool WorkAroundDriverBugs() const { return mWorkAroundDriverBugs; }

    bool IsDrawingToDefaultFramebuffer();

protected:
    RefPtr<TextureGarbageBin> mTexGarbageBin;

public:
    TextureGarbageBin* TexGarbageBin() {
        MOZ_ASSERT(mTexGarbageBin);
        return mTexGarbageBin;
    }

    void EmptyTexGarbageBin();

    bool IsOffscreenSizeAllowed(const gfx::IntSize& aSize) const;

protected:
    bool InitWithPrefix(const char* prefix, bool trygl);

private:
    bool InitWithPrefixImpl(const char* prefix, bool trygl);
    void LoadMoreSymbols(const char* prefix, bool trygl);
    bool LoadExtSymbols(const char* prefix, bool trygl, const SymLoadStruct* list,
                        GLExtensions ext);
    bool LoadFeatureSymbols(const char* prefix, bool trygl, const SymLoadStruct* list,
                            GLFeature feature);

protected:
    void InitExtensions();

    GLint mViewportRect[4];
    GLint mScissorRect[4];

    GLint mMaxTextureSize;
    GLint mMaxCubeMapTextureSize;
    GLint mMaxTextureImageSize;
    GLint mMaxRenderbufferSize;
    GLint mMaxViewportDims[2];
    GLsizei mMaxSamples;
    bool mNeedsTextureSizeChecks;
    bool mNeedsFlushBeforeDeleteFB;
    bool mWorkAroundDriverBugs;

    bool IsTextureSizeSafeToPassToDriver(GLenum target, GLsizei width, GLsizei height) const {
        if (mNeedsTextureSizeChecks) {
            // some drivers incorrectly handle some large texture sizes that are below the
            // max texture size that they report. So we check ourselves against our own values
            // (mMax[CubeMap]TextureSize).
            // see bug 737182 for Mac Intel 2D textures
            // see bug 684882 for Mac Intel cube map textures
            // see bug 814716 for Mesa Nouveau
            GLsizei maxSize = target == LOCAL_GL_TEXTURE_CUBE_MAP ||
                                (target >= LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X &&
                                target <= LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z)
                              ? mMaxCubeMapTextureSize
                              : mMaxTextureSize;
            return width <= maxSize && height <= maxSize;
        }
        return true;
    }


public:
    GLsizei MaxSamples() const {
        return mMaxSamples;
    }

    void fViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
        if (mViewportRect[0] == x &&
            mViewportRect[1] == y &&
            mViewportRect[2] == width &&
            mViewportRect[3] == height)
        {
            return;
        }
        mViewportRect[0] = x;
        mViewportRect[1] = y;
        mViewportRect[2] = width;
        mViewportRect[3] = height;
        BEFORE_GL_CALL;
        mSymbols.fViewport(x, y, width, height);
        AFTER_GL_CALL;
    }

#undef ASSERT_SYMBOL_PRESENT

#ifdef MOZ_GL_DEBUG
    void CreatedProgram(GLContext *aOrigin, GLuint aName);
    void CreatedShader(GLContext *aOrigin, GLuint aName);
    void CreatedBuffers(GLContext *aOrigin, GLsizei aCount, GLuint *aNames);
    void CreatedQueries(GLContext *aOrigin, GLsizei aCount, GLuint *aNames);
    void CreatedTextures(GLContext *aOrigin, GLsizei aCount, GLuint *aNames);
    void CreatedFramebuffers(GLContext *aOrigin, GLsizei aCount, GLuint *aNames);
    void CreatedRenderbuffers(GLContext *aOrigin, GLsizei aCount, GLuint *aNames);
    void DeletedProgram(GLContext *aOrigin, GLuint aName);
    void DeletedShader(GLContext *aOrigin, GLuint aName);
    void DeletedBuffers(GLContext *aOrigin, GLsizei aCount, const GLuint *aNames);
    void DeletedQueries(GLContext *aOrigin, GLsizei aCount, const GLuint *aNames);
    void DeletedTextures(GLContext *aOrigin, GLsizei aCount, const GLuint *aNames);
    void DeletedFramebuffers(GLContext *aOrigin, GLsizei aCount, const GLuint *aNames);
    void DeletedRenderbuffers(GLContext *aOrigin, GLsizei aCount, const GLuint *aNames);

    void SharedContextDestroyed(GLContext *aChild);
    void ReportOutstandingNames();

    struct NamedResource {
        NamedResource()
            : origin(nullptr), name(0), originDeleted(false)
        { }

        NamedResource(GLContext *aOrigin, GLuint aName)
            : origin(aOrigin), name(aName), originDeleted(false)
        { }

        GLContext *origin;
        GLuint name;
        bool originDeleted;

        // for sorting
        bool operator<(const NamedResource& aOther) const {
            if (intptr_t(origin) < intptr_t(aOther.origin))
                return true;
            if (name < aOther.name)
                return true;
            return false;
        }
        bool operator==(const NamedResource& aOther) const {
            return origin == aOther.origin &&
                name == aOther.name &&
                originDeleted == aOther.originDeleted;
        }
    };

    nsTArray<NamedResource> mTrackedPrograms;
    nsTArray<NamedResource> mTrackedShaders;
    nsTArray<NamedResource> mTrackedTextures;
    nsTArray<NamedResource> mTrackedFramebuffers;
    nsTArray<NamedResource> mTrackedRenderbuffers;
    nsTArray<NamedResource> mTrackedBuffers;
    nsTArray<NamedResource> mTrackedQueries;
#endif


protected:
    bool mHeavyGLCallsSinceLastFlush;

public:
    void FlushIfHeavyGLCallsSinceLastFlush();
    static bool ShouldSpew();
    static bool ShouldDumpExts();
    void Readback(SharedSurface* src, gfx::DataSourceSurface* dest);
};

bool DoesStringMatch(const char* aString, const char *aWantedString);

void SplitByChar(const nsACString& str, const char delim,
                 std::vector<nsCString>* const out);

template<size_t N>
bool
MarkBitfieldByString(const nsACString& str, const char* const (&markStrList)[N],
                     std::bitset<N>* const out_markList)
{
    for (size_t i = 0; i < N; i++) {
        if (str.Equals(markStrList[i])) {
            (*out_markList)[i] = 1;
            return true;
        }
    }
    return false;
}

template<size_t N>
void
MarkBitfieldByStrings(const std::vector<nsCString>& strList,
                      bool dumpStrings, const char* const (&markStrList)[N],
                      std::bitset<N>* const out_markList)
{
    for (auto itr = strList.begin(); itr != strList.end(); ++itr) {
        const nsACString& str = *itr;
        const bool wasMarked = MarkBitfieldByString(str, markStrList,
                                                    out_markList);
        if (dumpStrings)
            printf_stderr("  %s%s\n", str.BeginReading(), wasMarked ? "(*)" : "");
    }
}

/**
 * Helper function that creates a 2D texture aSize.width x aSize.height with
 * storage type specified by aFormats. Returns GL texture object id.
 *
 * See mozilla::gl::CreateTexture.
 */
GLuint CreateTextureForOffscreen(GLContext* aGL, const GLFormats& aFormats,
                                 const gfx::IntSize& aSize);

/**
 * Helper function that creates a 2D texture aSize.width x aSize.height with
 * storage type aInternalFormat. Returns GL texture object id.
 *
 * Initialize textyre parameters to:
 *    GL_TEXTURE_MIN_FILTER = GL_LINEAR
 *    GL_TEXTURE_MAG_FILTER = GL_LINEAR
 *    GL_TEXTURE_WRAP_S = GL_CLAMP_TO_EDGE
 *    GL_TEXTURE_WRAP_T = GL_CLAMP_TO_EDGE
 */
GLuint CreateTexture(GLContext* aGL, GLenum aInternalFormat, GLenum aFormat,
                     GLenum aType, const gfx::IntSize& aSize, bool linear = true);

} /* namespace gl */
} /* namespace mozilla */

#endif /* GLCONTEXT_H_ */
