/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_DRAWCOMMAND_H_
#define MOZILLA_GFX_DRAWCOMMAND_H_

#include <math.h>

#include "2D.h"
#include "Blur.h"
#include "Filters.h"
#include <vector>
#include "FilterNodeCapture.h"
#include "Logging.h"

namespace mozilla {
namespace gfx {

class CaptureCommandList;

enum class CommandType : int8_t {
  DRAWSURFACE = 0,
  DRAWFILTER,
  DRAWSURFACEWITHSHADOW,
  CLEARRECT,
  COPYSURFACE,
  COPYRECT,
  FILLRECT,
  FILLROUNDEDRECT,
  STROKERECT,
  STROKELINE,
  STROKE,
  FILL,
  FILLGLYPHS,
  STROKEGLYPHS,
  MASK,
  MASKSURFACE,
  PUSHCLIP,
  PUSHCLIPRECT,
  PUSHLAYER,
  POPCLIP,
  POPLAYER,
  SETTRANSFORM,
  SETPERMITSUBPIXELAA,
  FLUSH,
  BLUR,
  PADEDGES,
};

class DrawingCommand {
 public:
  virtual ~DrawingCommand() = default;

  virtual CommandType GetType() const = 0;
  virtual void ExecuteOnDT(DrawTarget* aDT,
                           const Matrix* aTransform = nullptr) const = 0;
  virtual void CloneInto(CaptureCommandList* aList) = 0;
  virtual void Log(TreeLog<>& aLog) const = 0;
};

}  // namespace gfx
}  // namespace mozilla

#endif /* MOZILLA_GFX_DRAWCOMMAND_H_ */
