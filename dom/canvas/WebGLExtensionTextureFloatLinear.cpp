/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLExtensions.h"

#include "GLContext.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "WebGLContext.h"
#include "WebGLFormats.h"

namespace mozilla {

WebGLExtensionTextureFloatLinear::WebGLExtensionTextureFloatLinear(
    WebGLContext* webgl)
    : WebGLExtensionBase(webgl) {
  auto& fua = webgl->mFormatUsage;

  fua->EditUsage(webgl::EffectiveFormat::RGBA32F)->isFilterable = true;
  fua->EditUsage(webgl::EffectiveFormat::RGB32F)->isFilterable = true;

  if (webgl->IsWebGL2()) {
    fua->EditUsage(webgl::EffectiveFormat::R32F)->isFilterable = true;
    fua->EditUsage(webgl::EffectiveFormat::RG32F)->isFilterable = true;
  } else {
    fua->EditUsage(webgl::EffectiveFormat::Luminance32FAlpha32F)->isFilterable =
        true;
    fua->EditUsage(webgl::EffectiveFormat::Luminance32F)->isFilterable = true;
    fua->EditUsage(webgl::EffectiveFormat::Alpha32F)->isFilterable = true;
  }
}

WebGLExtensionTextureFloatLinear::~WebGLExtensionTextureFloatLinear() {}

}  // namespace mozilla
