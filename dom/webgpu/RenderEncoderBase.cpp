/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderEncoderBase.h"

#include "RenderPipeline.h"

namespace mozilla {
namespace webgpu {

NS_IMPL_CYCLE_COLLECTION_INHERITED(RenderEncoderBase, ProgrammablePassEncoder)
NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(RenderEncoderBase,
                                               ProgrammablePassEncoder)
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(RenderEncoderBase,
                                               ProgrammablePassEncoder)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

RenderEncoderBase::~RenderEncoderBase() = default;

}  // namespace webgpu
}  // namespace mozilla
