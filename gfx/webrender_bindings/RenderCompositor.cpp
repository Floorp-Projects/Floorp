/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderCompositor.h"

#include "GLContext.h"
#include "mozilla/layers/SyncObject.h"
#include "mozilla/webrender/RenderCompositorOGL.h"
#include "mozilla/widget/CompositorWidget.h"

namespace mozilla {
namespace wr {

/* static */ UniquePtr<RenderCompositor>
RenderCompositor::Create(RefPtr<widget::CompositorWidget>&& aWidget)
{
  return RenderCompositorOGL::Create(Move(aWidget));
}

RenderCompositor::RenderCompositor(RefPtr<widget::CompositorWidget>&& aWidget)
  : mWidget(aWidget)
{
}

RenderCompositor::~RenderCompositor()
{
}

} // namespace wr
} // namespace mozilla
