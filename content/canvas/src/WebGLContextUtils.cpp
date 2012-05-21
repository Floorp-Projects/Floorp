/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdarg.h>

#include "WebGLContext.h"

#include "prprf.h"

#include "nsIJSContextStack.h"
#include "jsapi.h"
#include "nsIScriptSecurityManager.h"
#include "nsServiceManagerUtils.h"
#include "nsIVariant.h"

#include "nsIDOMDocument.h"
#include "nsIDOMEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIDOMDataContainerEvent.h"

#include "nsContentUtils.h"
#include "mozilla/Preferences.h"

#if 0
#include "nsIContentURIGrouper.h"
#include "nsIContentPrefService.h"
#endif

using namespace mozilla;

void
WebGLContext::LogMessage(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    LogMessage(fmt, ap);

    va_end(ap);
}

void
WebGLContext::LogMessage(const char *fmt, va_list ap)
{
    if (!fmt) return;

    char buf[1024];
    PR_vsnprintf(buf, 1024, fmt, ap);

    // no need to print to stderr, as JS_ReportWarning takes care of this for us.

    nsCOMPtr<nsIJSContextStack> stack = do_GetService("@mozilla.org/js/xpc/ContextStack;1");
    JSContext* ccx = nsnull;
    if (stack && NS_SUCCEEDED(stack->Peek(&ccx)) && ccx)
        JS_ReportWarning(ccx, "WebGL: %s", buf);
}

void
WebGLContext::LogMessageIfVerbose(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    LogMessageIfVerbose(fmt, ap);

    va_end(ap);
}

void
WebGLContext::LogMessageIfVerbose(const char *fmt, va_list ap)
{
    static bool firstTime = true;

    if (mVerbose)
        LogMessage(fmt, ap);
    else if (firstTime)
        LogMessage("There are WebGL warnings or messages in this page, but they are hidden. To see them, "
                   "go to about:config, set the webgl.verbose preference, and reload this page.");

    firstTime = false;
}

CheckedUint32
WebGLContext::GetImageSize(WebGLsizei height, 
                           WebGLsizei width, 
                           PRUint32 pixelSize,
                           PRUint32 packOrUnpackAlignment)
{
    CheckedUint32 checked_plainRowSize = CheckedUint32(width) * pixelSize;

    // alignedRowSize = row size rounded up to next multiple of packAlignment
    CheckedUint32 checked_alignedRowSize = RoundedToNextMultipleOf(checked_plainRowSize, packOrUnpackAlignment);

    // if height is 0, we don't need any memory to store this; without this check, we'll get an overflow
    CheckedUint32 checked_neededByteLength
        = height <= 0 ? 0 : (height-1) * checked_alignedRowSize + checked_plainRowSize;

    return checked_neededByteLength;
}

void
WebGLContext::SynthesizeGLError(WebGLenum err)
{
    // If there is already a pending error, don't overwrite it;
    // but if there isn't, then we need to check for a gl error
    // that may have occurred before this one and use that code
    // instead.
    
    MakeContextCurrent();

    UpdateWebGLErrorAndClearGLError();

    if (!mWebGLError)
        mWebGLError = err;
}

void
WebGLContext::SynthesizeGLError(WebGLenum err, const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    LogMessageIfVerbose(fmt, va);
    va_end(va);

    return SynthesizeGLError(err);
}

void
WebGLContext::ErrorInvalidEnum(const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    LogMessageIfVerbose(fmt, va);
    va_end(va);

    return SynthesizeGLError(LOCAL_GL_INVALID_ENUM);
}

void
WebGLContext::ErrorInvalidOperation(const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    LogMessageIfVerbose(fmt, va);
    va_end(va);

    return SynthesizeGLError(LOCAL_GL_INVALID_OPERATION);
}

void
WebGLContext::ErrorInvalidValue(const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    LogMessageIfVerbose(fmt, va);
    va_end(va);

    return SynthesizeGLError(LOCAL_GL_INVALID_VALUE);
}

void
WebGLContext::ErrorInvalidFramebufferOperation(const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    LogMessageIfVerbose(fmt, va);
    va_end(va);

    return SynthesizeGLError(LOCAL_GL_INVALID_FRAMEBUFFER_OPERATION);
}

void
WebGLContext::ErrorOutOfMemory(const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    LogMessageIfVerbose(fmt, va);
    va_end(va);

    return SynthesizeGLError(LOCAL_GL_OUT_OF_MEMORY);
}

const char *
WebGLContext::ErrorName(GLenum error)
{
    switch(error) {
        case LOCAL_GL_INVALID_ENUM:
            return "INVALID_ENUM";
        case LOCAL_GL_INVALID_OPERATION:
            return "INVALID_OPERATION";
        case LOCAL_GL_INVALID_VALUE:
            return "INVALID_VALUE";
        case LOCAL_GL_OUT_OF_MEMORY:
            return "OUT_OF_MEMORY";
        case LOCAL_GL_INVALID_FRAMEBUFFER_OPERATION:
            return "INVALID_FRAMEBUFFER_OPERATION";
        case LOCAL_GL_NO_ERROR:
            return "NO_ERROR";
        default:
            NS_ABORT();
            return "[unknown WebGL error!]";
    }
}

bool
WebGLContext::IsTextureFormatCompressed(GLenum format)
{
    switch(format) {
        case LOCAL_GL_RGB:
        case LOCAL_GL_RGBA:
        case LOCAL_GL_ALPHA:
        case LOCAL_GL_LUMINANCE:
        case LOCAL_GL_LUMINANCE_ALPHA:
            return false;

        case LOCAL_GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
        case LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
        case LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
        case LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
            return true;
    }

    NS_NOTREACHED("Invalid WebGL texture format?");
    NS_ABORT();
    return false;
}
