/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLExtensions.h"

#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "GLContext.h"
#include "WebGLContext.h"
#include "WebGLContextUtils.h"

namespace mozilla {

WebGLExtensionMOZDebug::WebGLExtensionMOZDebug(WebGLContext* webgl)
    : WebGLExtensionBase(webgl) {}

WebGLExtensionMOZDebug::~WebGLExtensionMOZDebug() {}

void WebGLExtensionMOZDebug::GetParameter(JSContext* cx, GLenum pname,
                                          JS::MutableHandle<JS::Value> retval,
                                          ErrorResult& er) const {
  if (mIsLost || !mContext) return;
  const WebGLContext::FuncScope funcScope(*mContext, "MOZ_debug.getParameter");
  MOZ_ASSERT(!mContext->IsContextLost());

  const auto& gl = mContext->gl;

  switch (pname) {
    case LOCAL_GL_EXTENSIONS: {
      nsString ret;
      if (!gl->IsCoreProfile()) {
        const auto rawExts = (const char*)gl->fGetString(LOCAL_GL_EXTENSIONS);
        ret = NS_ConvertUTF8toUTF16(rawExts);
      } else {
        const auto& numExts = gl->GetIntAs<GLuint>(LOCAL_GL_NUM_EXTENSIONS);
        for (GLuint i = 0; i < numExts; i++) {
          const auto rawExt =
              (const char*)gl->fGetStringi(LOCAL_GL_EXTENSIONS, i);
          if (i > 0) {
            ret.AppendLiteral(" ");
          }
          ret.Append(NS_ConvertUTF8toUTF16(rawExt));
        }
      }
      retval.set(StringValue(cx, ret, er));
      return;
    }

    case LOCAL_GL_RENDERER:
    case LOCAL_GL_VENDOR:
    case LOCAL_GL_VERSION: {
      const auto raw = (const char*)gl->fGetString(pname);
      retval.set(StringValue(cx, NS_ConvertUTF8toUTF16(raw), er));
      return;
    }

    case dom::MOZ_debug_Binding::WSI_INFO: {
      nsCString info;
      gl->GetWSIInfo(&info);
      retval.set(StringValue(cx, NS_ConvertUTF8toUTF16(info), er));
      return;
    }

    case dom::MOZ_debug_Binding::DOES_INDEX_VALIDATION:
      retval.set(JS::BooleanValue(mContext->mNeedsIndexValidation));
      return;

    default:
      mContext->ErrorInvalidEnumInfo("pname", pname);
      retval.set(JS::NullValue());
      return;
  }
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionMOZDebug, MOZ_debug)

}  // namespace mozilla
