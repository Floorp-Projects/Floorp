/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef GFX_WEBRENDERLAYERSLOGGING_H
#define GFX_WEBRENDERLAYERSLOGGING_H

#include "mozilla/webrender/WebRenderTypes.h"

namespace mozilla {
namespace layers {

void
AppendToString(std::stringstream& aStream, WrMixBlendMode aMixBlendMode,
               const char* pfx="", const char* sfx="");

void
AppendToString(std::stringstream& aStream, WrTextureFilter aTextureFilter,
               const char* pfx="", const char* sfx="");

} // namespace layers
} // namespace mozilla

// this ensures that the WebRender AppendToString's are in scope for Stringify
#include "LayersLogging.h"

#endif // GFX_WEBRENDERLAYERSLOGGING_H
