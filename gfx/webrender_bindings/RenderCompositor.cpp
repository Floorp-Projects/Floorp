/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderCompositor.h"

#include "GLContext.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/layers/SyncObject.h"
#include "mozilla/webrender/RenderCompositorOGL.h"
#include "mozilla/widget/CompositorWidget.h"

#ifdef XP_WIN
#  include "mozilla/webrender/RenderCompositorANGLE.h"
#endif

#if defined(MOZ_WAYLAND) || defined(MOZ_WIDGET_ANDROID)
#  include "mozilla/webrender/RenderCompositorEGL.h"
#endif

namespace mozilla {
namespace wr {

/* static */
UniquePtr<RenderCompositor> RenderCompositor::Create(
    RefPtr<widget::CompositorWidget>&& aWidget) {
#ifdef XP_WIN
  if (gfx::gfxVars::UseWebRenderANGLE()) {
    return RenderCompositorANGLE::Create(std::move(aWidget));
  }
#endif

#if defined(MOZ_WAYLAND) || defined(MOZ_WIDGET_ANDROID)
  UniquePtr<RenderCompositor> eglCompositor =
      RenderCompositorEGL::Create(aWidget);
  if (eglCompositor) {
    return eglCompositor;
  }
#endif

#if defined(MOZ_WIDGET_ANDROID)
  // RenderCompositorOGL is not used on android
  return nullptr;
#else
  return RenderCompositorOGL::Create(std::move(aWidget));
#endif
}

RenderCompositor::RenderCompositor(RefPtr<widget::CompositorWidget>&& aWidget)
    : mWidget(aWidget) {}

RenderCompositor::~RenderCompositor() {}

bool RenderCompositor::MakeCurrent() { return gl()->MakeCurrent(); }

}  // namespace wr
}  // namespace mozilla
