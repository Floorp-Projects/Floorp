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
#include "mozilla/gfx/Point.h"  // for IntSize
#include "mozilla/Move.h"
#include "ISurfaceProvider.h"

namespace mozilla {
namespace image {

enum class MatchType : uint8_t
{
  NOT_FOUND,  // No matching surface and no placeholder.
  PENDING,    // Found a matching placeholder, but no surface.
  EXACT,      // Found a surface that matches exactly.
  SUBSTITUTE_BECAUSE_NOT_FOUND,  // No exact match, but found a similar one.
  SUBSTITUTE_BECAUSE_PENDING,    // Found a similar surface and a placeholder
                                 // for an exact match.

  /* No exact match, but this should be considered an exact match for purposes
   * of deciding whether or not to request a new decode. This is because the
   * cache has determined that callers require too many size variants of this
   * image. It determines the set of sizes which best represent the image, and
   * will only suggest decoding of unavailable sizes from that set. */
  SUBSTITUTE_BECAUSE_BEST
};

/**
 * LookupResult is the return type of SurfaceCache's Lookup*() functions. It
 * combines a surface with relevant metadata tracked by SurfaceCache.
 */
class MOZ_STACK_CLASS LookupResult
{
public:
  explicit LookupResult(MatchType aMatchType)
    : mMatchType(aMatchType)
  {
    MOZ_ASSERT(mMatchType == MatchType::NOT_FOUND ||
               mMatchType == MatchType::PENDING,
               "Only NOT_FOUND or PENDING make sense with no surface");
  }

  LookupResult(LookupResult&& aOther)
    : mSurface(std::move(aOther.mSurface))
    , mMatchType(aOther.mMatchType)
    , mSuggestedSize(aOther.mSuggestedSize)
  { }

  LookupResult(DrawableSurface&& aSurface, MatchType aMatchType)
    : mSurface(std::move(aSurface))
    , mMatchType(aMatchType)
  {
    MOZ_ASSERT(!mSurface || !(mMatchType == MatchType::NOT_FOUND ||
                              mMatchType == MatchType::PENDING),
               "Only NOT_FOUND or PENDING make sense with no surface");
    MOZ_ASSERT(mSurface || mMatchType == MatchType::NOT_FOUND ||
                           mMatchType == MatchType::PENDING,
               "NOT_FOUND or PENDING do not make sense with a surface");
  }

  LookupResult(DrawableSurface&& aSurface, MatchType aMatchType,
               const gfx::IntSize& aSuggestedSize)
    : mSurface(std::move(aSurface))
    , mMatchType(aMatchType)
    , mSuggestedSize(aSuggestedSize)
  {
    MOZ_ASSERT(!mSurface || !(mMatchType == MatchType::NOT_FOUND ||
                              mMatchType == MatchType::PENDING),
               "Only NOT_FOUND or PENDING make sense with no surface");
    MOZ_ASSERT(mSurface || mMatchType == MatchType::NOT_FOUND ||
                           mMatchType == MatchType::PENDING,
               "NOT_FOUND or PENDING do not make sense with a surface");
  }

  LookupResult& operator=(LookupResult&& aOther)
  {
    MOZ_ASSERT(&aOther != this, "Self-move-assignment is not supported");
    mSurface = std::move(aOther.mSurface);
    mMatchType = aOther.mMatchType;
    mSuggestedSize = aOther.mSuggestedSize;
    return *this;
  }

  DrawableSurface& Surface() { return mSurface; }
  const DrawableSurface& Surface() const { return mSurface; }
  const gfx::IntSize& SuggestedSize() const { return mSuggestedSize; }

  /// @return true if this LookupResult contains a surface.
  explicit operator bool() const { return bool(mSurface); }

  /// @return what kind of match this is (exact, substitute, etc.)
  MatchType Type() const { return mMatchType; }

private:
  LookupResult(const LookupResult&) = delete;
  LookupResult& operator=(const LookupResult& aOther) = delete;

  DrawableSurface mSurface;
  MatchType mMatchType;

  /// If given, the size the caller should request a decode at. This may or may
  /// not match the size the caller requested from the cache.
  gfx::IntSize mSuggestedSize;
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_LookupResult_h
