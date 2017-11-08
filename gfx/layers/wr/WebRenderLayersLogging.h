/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef GFX_WEBRENDERLAYERSLOGGING_H
#define GFX_WEBRENDERLAYERSLOGGING_H

#include "mozilla/webrender/WebRenderTypes.h"

namespace mozilla {
namespace layers {

void
AppendToString(std::stringstream& aStream, wr::MixBlendMode aMixBlendMode,
               const char* pfx="", const char* sfx="");

void
AppendToString(std::stringstream& aStream, wr::ImageRendering aTextureFilter,
               const char* pfx="", const char* sfx="");

void
AppendToString(std::stringstream& aStream, wr::LayoutVector2D aVector,
               const char* pfx="", const char* sfx="");

} // namespace layers
} // namespace mozilla

// this ensures that the WebRender AppendToString's are in scope for Stringify
#include "LayersLogging.h"

#endif // GFX_WEBRENDERLAYERSLOGGING_H
