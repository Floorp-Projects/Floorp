/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * LookupResult is the return type of SurfaceCache's Lookup*() functions. It
 * combines a surface with relevant metadata tracked by SurfaceCache.
 */

#ifndef mozilla_image_LookupResult_h
#define mozilla_image_LookupResult_h

#include "mozilla/Attributes.h"
#include "mozilla/Move.h"
#include "imgFrame.h"

namespace mozilla {
namespace image {

/**
 * LookupResult is the return type of SurfaceCache's Lookup*() functions. It
 * combines a surface with relevant metadata tracked by SurfaceCache.
 */
class MOZ_STACK_CLASS LookupResult
{
public:
  LookupResult()
    : mIsExactMatch(false)
  { }

  LookupResult(LookupResult&& aOther)
    : mDrawableRef(Move(aOther.mDrawableRef))
    , mIsExactMatch(aOther.mIsExactMatch)
  { }

  LookupResult(DrawableFrameRef&& aDrawableRef, bool aIsExactMatch)
    : mDrawableRef(Move(aDrawableRef))
    , mIsExactMatch(aIsExactMatch)
  { }

  LookupResult& operator=(LookupResult&& aOther)
  {
    MOZ_ASSERT(&aOther != this, "Self-move-assignment is not supported");
    mDrawableRef = Move(aOther.mDrawableRef);
    mIsExactMatch = aOther.mIsExactMatch;
    return *this;
  }

  DrawableFrameRef& DrawableRef() { return mDrawableRef; }
  const DrawableFrameRef& DrawableRef() const { return mDrawableRef; }

  /// @return true if this LookupResult contains a surface.
  explicit operator bool() const { return bool(mDrawableRef); }

  /// @return true if the surface is an exact match for the Lookup*() arguments.
  bool IsExactMatch() const { return mIsExactMatch; }

private:
  LookupResult(const LookupResult&) = delete;

  DrawableFrameRef mDrawableRef;
  bool mIsExactMatch;
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_LookupResult_h
