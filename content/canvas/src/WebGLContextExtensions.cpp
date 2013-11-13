/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
#include "WebGLContextUtils.h"
#include "WebGLExtensions.h"
#include "GLContext.h"

#include "nsString.h"
#include "mozilla/Preferences.h"
#include "AccessCheck.h"

using namespace mozilla;
using namespace mozilla::gl;

// must match WebGLContext::WebGLExtensionID
static const char *sExtensionNames[] = {
    "EXT_sRGB",
    "EXT_texture_filter_anisotropic",
    "OES_element_index_uint",
    "OES_standard_derivatives",
    "OES_texture_float",
    "OES_texture_float_linear",
    "OES_vertex_array_object",
    "WEBGL_compressed_texture_atc",
    "WEBGL_compressed_texture_pvrtc",
    "WEBGL_compressed_texture_s3tc",
    "WEBGL_debug_renderer_info",
    "WEBGL_depth_texture",
    "WEBGL_lose_context",
    "WEBGL_draw_buffers",
    "ANGLE_instanced_arrays"
};

/* static */ const char*
WebGLContext::GetExtensionString(WebGLExtensionID ext)
{
    static_assert(MOZ_ARRAY_LENGTH(sExtensionNames) == size_t(WebGLExtensionID_max),
                  "Mismatched lengths for sFeatureInfoInfos and GLFeature enums");

    MOZ_ASSERT(ext < WebGLExtensionID_max, "unknown extension!");

    return sExtensionNames[ext];
}

bool
WebGLContext::IsExtensionEnabled(WebGLExtensionID ext) const {
    return mExtensions.SafeElementAt(ext);
}

bool WebGLContext::IsExtensionSupported(JSContext *cx, WebGLExtensionID ext) const
{
    // Chrome contexts need access to debug information even when
    // webgl.disable-extensions is set. This is used in the graphics
    // section of about:support.
    if (xpc::AccessCheck::isChrome(js::GetContextCompartment(cx))) {
        switch (ext) {
            case WEBGL_debug_renderer_info:
                return true;
            default:
                // For warnings-as-errors.
                break;
        }
    }

    return IsExtensionSupported(ext);
}

bool WebGLContext::IsExtensionSupported(WebGLExtensionID ext) const
{
    if (mDisableExtensions) {
        return false;
    }

    switch (ext) {
        case OES_element_index_uint:
            return gl->IsSupported(GLFeature::element_index_uint);
        case OES_standard_derivatives:
            return gl->IsSupported(GLFeature::standard_derivatives);
        case WEBGL_lose_context:
            // We always support this extension.
            return true;
        case OES_texture_float:
            return gl->IsSupported(GLFeature::texture_float);
        case OES_texture_float_linear:
            return gl->IsSupported(GLFeature::texture_float_linear);
        case OES_vertex_array_object:
            return WebGLExtensionVertexArray::IsSupported(this);
        case EXT_texture_filter_anisotropic:
            return gl->IsExtensionSupported(GLContext::EXT_texture_filter_anisotropic);
        case WEBGL_compressed_texture_s3tc:
            if (gl->IsExtensionSupported(GLContext::EXT_texture_compression_s3tc)) {
                return true;
            }
            else if (gl->IsExtensionSupported(GLContext::EXT_texture_compression_dxt1) &&
                     gl->IsExtensionSupported(GLContext::ANGLE_texture_compression_dxt3) &&
                     gl->IsExtensionSupported(GLContext::ANGLE_texture_compression_dxt5))
            {
                return true;
            }
            return false;
        case WEBGL_compressed_texture_atc:
            return gl->IsExtensionSupported(GLContext::AMD_compressed_ATC_texture);
        case WEBGL_compressed_texture_pvrtc:
            return gl->IsExtensionSupported(GLContext::IMG_texture_compression_pvrtc);
        case WEBGL_depth_texture:
            // WEBGL_depth_texture supports DEPTH_STENCIL textures
            if (!gl->IsSupported(GLFeature::packed_depth_stencil)) {
                return false;
            }
            return gl->IsSupported(GLFeature::depth_texture) ||
                   gl->IsExtensionSupported(GLContext::ANGLE_depth_texture);
        case ANGLE_instanced_arrays:
            return WebGLExtensionInstancedArrays::IsSupported(this);
        case EXT_sRGB:
            return WebGLExtensionSRGB::IsSupported(this);
        case WEBGL_draw_buffers:
            return WebGLExtensionDrawBuffers::IsSupported(this);
        default:
            // For warnings-as-errors.
            break;
    }
// Uncomment this switch for any new extensions
#if 0
    if (Preferences::GetBool("webgl.enable-draft-extensions", false) || IsWebGL2()) {
        switch (ext) {
            default:
                // For warnings-as-errors.
                break;
        }
    }
#endif

    return false;
}

static bool
CompareWebGLExtensionName(const nsACString& name, const char *other)
{
    return name.Equals(other, nsCaseInsensitiveCStringComparator());
}

JSObject*
WebGLContext::GetExtension(JSContext *cx, const nsAString& aName, ErrorResult& rv)
{
    if (IsContextLost())
        return nullptr;

    NS_LossyConvertUTF16toASCII name(aName);

    WebGLExtensionID ext = WebGLExtensionID_unknown_extension;

    // step 1: figure what extension is wanted
    for (size_t i = 0; i < size_t(WebGLExtensionID_max); i++)
    {
        WebGLExtensionID extension = WebGLExtensionID(i);
        
        if (CompareWebGLExtensionName(name, GetExtensionString(extension))) {
            ext = extension;
            break;
        }
    }

    if (ext == WebGLExtensionID_unknown_extension)
    {
        /**
         * We keep backward compatibility for these deprecated vendor-prefixed
         * alias. Do not add new ones anymore. Hide it behind the
         * webgl.enable-draft-extensions flag instead.
         */
        if (CompareWebGLExtensionName(name, "MOZ_WEBGL_lose_context")) {
            ext = WEBGL_lose_context;
        }
        else if (CompareWebGLExtensionName(name, "MOZ_WEBGL_compressed_texture_s3tc")) {
            ext = WEBGL_compressed_texture_s3tc;
        }
        else if (CompareWebGLExtensionName(name, "MOZ_WEBGL_compressed_texture_atc")) {
            ext = WEBGL_compressed_texture_atc;
        }
        else if (CompareWebGLExtensionName(name, "MOZ_WEBGL_compressed_texture_pvrtc")) {
            ext = WEBGL_compressed_texture_pvrtc;
        }
        else if (CompareWebGLExtensionName(name, "MOZ_WEBGL_depth_texture")) {
            ext = WEBGL_depth_texture;
        }

        if (ext != WebGLExtensionID_unknown_extension) {
            GenerateWarning("getExtension('%s'): MOZ_ prefixed WebGL extension strings are deprecated. "
                            "Support for them will be removed in the future. Use unprefixed extension strings. "
                            "To get draft extensions, set the webgl.enable-draft-extensions preference.",
                            name.get());
        }
    }

    if (ext == WebGLExtensionID_unknown_extension) {
        return nullptr;
    }

    // step 2: check if the extension is supported
    if (!IsExtensionSupported(cx, ext)) {
        return nullptr;
    }

    // step 3: if the extension hadn't been previously been created, create it now, thus enabling it
    if (!IsExtensionEnabled(ext)) {
        EnableExtension(ext);
    }

    return WebGLObjectAsJSObject(cx, mExtensions[ext].get(), rv);
}

void
WebGLContext::EnableExtension(WebGLExtensionID ext)
{
    mExtensions.EnsureLengthAtLeast(ext + 1);

    MOZ_ASSERT(IsExtensionEnabled(ext) == false);

    WebGLExtensionBase* obj = nullptr;
    switch (ext) {
        case OES_element_index_uint:
            obj = new WebGLExtensionElementIndexUint(this);
            break;
        case OES_standard_derivatives:
            obj = new WebGLExtensionStandardDerivatives(this);
            break;
        case EXT_texture_filter_anisotropic:
            obj = new WebGLExtensionTextureFilterAnisotropic(this);
            break;
        case WEBGL_lose_context:
            obj = new WebGLExtensionLoseContext(this);
            break;
        case WEBGL_compressed_texture_s3tc:
            obj = new WebGLExtensionCompressedTextureS3TC(this);
            break;
        case WEBGL_compressed_texture_atc:
            obj = new WebGLExtensionCompressedTextureATC(this);
            break;
        case WEBGL_compressed_texture_pvrtc:
            obj = new WebGLExtensionCompressedTexturePVRTC(this);
            break;
        case WEBGL_debug_renderer_info:
            obj = new WebGLExtensionDebugRendererInfo(this);
            break;
        case WEBGL_depth_texture:
            obj = new WebGLExtensionDepthTexture(this);
            break;
        case OES_texture_float:
            obj = new WebGLExtensionTextureFloat(this);
            break;
        case OES_texture_float_linear:
            obj = new WebGLExtensionTextureFloatLinear(this);
            break;
        case WEBGL_draw_buffers:
            obj = new WebGLExtensionDrawBuffers(this);
            break;
        case OES_vertex_array_object:
            obj = new WebGLExtensionVertexArray(this);
            break;
        case ANGLE_instanced_arrays:
            obj = new WebGLExtensionInstancedArrays(this);
            break;
        case EXT_sRGB:
            obj = new WebGLExtensionSRGB(this);
            break;
        default:
            MOZ_ASSERT(false, "should not get there.");
    }

    mExtensions[ext] = obj;
}

void
WebGLContext::GetSupportedExtensions(JSContext *cx, Nullable< nsTArray<nsString> > &retval)
{
    retval.SetNull();
    if (IsContextLost())
        return;

    nsTArray<nsString>& arr = retval.SetValue();

    for (size_t i = 0; i < size_t(WebGLExtensionID_max); i++)
    {
        WebGLExtensionID extension = WebGLExtensionID(i);

        if (IsExtensionSupported(cx, extension)) {
            arr.AppendElement(NS_ConvertUTF8toUTF16(GetExtensionString(extension)));
        }
    }

    /**
     * We keep backward compatibility for these deprecated vendor-prefixed
     * alias. Do not add new ones anymore. Hide it behind the
     * webgl.enable-draft-extensions flag instead.
     */
    if (IsExtensionSupported(cx, WEBGL_lose_context))
        arr.AppendElement(NS_LITERAL_STRING("MOZ_WEBGL_lose_context"));
    if (IsExtensionSupported(cx, WEBGL_compressed_texture_s3tc))
        arr.AppendElement(NS_LITERAL_STRING("MOZ_WEBGL_compressed_texture_s3tc"));
    if (IsExtensionSupported(cx, WEBGL_compressed_texture_atc))
        arr.AppendElement(NS_LITERAL_STRING("MOZ_WEBGL_compressed_texture_atc"));
    if (IsExtensionSupported(cx, WEBGL_compressed_texture_pvrtc))
        arr.AppendElement(NS_LITERAL_STRING("MOZ_WEBGL_compressed_texture_pvrtc"));
    if (IsExtensionSupported(cx, WEBGL_depth_texture))
        arr.AppendElement(NS_LITERAL_STRING("MOZ_WEBGL_depth_texture"));
}

