/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_LAYERSLOGGING_H
#define GFX_LAYERSLOGGING_H

#include "FrameMetrics.h"             // for FrameMetrics
#include "mozilla/gfx/Matrix.h"       // for Matrix4x4
#include "mozilla/gfx/Point.h"        // for IntSize, etc
#include "mozilla/gfx/TiledRegion.h"  // for TiledRegion
#include "mozilla/gfx/Types.h"        // for SurfaceFormat
#include "mozilla/layers/CompositorTypes.h"  // for TextureFlags
#include "nsAString.h"
#include "nsPrintfCString.h"  // for nsPrintfCString
#include "nsRegion.h"         // for nsRegion, nsIntRegion
#include "nscore.h"           // for nsACString, etc

// versions of printf_stderr and fprintf_stderr that deal with line
// truncation on android by printing individual lines out of the
// stringstream as separate calls to logcat.
void print_stderr(std::stringstream& aStr);
void fprint_stderr(FILE* aFile, std::stringstream& aStr);

#endif /* GFX_LAYERSLOGGING_H */
