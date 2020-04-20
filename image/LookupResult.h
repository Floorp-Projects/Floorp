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

#include <utility>

#include "ISurfaceProvider.h"
#include "mozilla/Attributes.h"
#include "mozilla/gfx/Point.h"  // for IntSize

namespace mozilla {
namespace image {

enum class MatchType : uint8_t {
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
class MOZ_STACK_CLASS LookupResult {
 public:
  explicit LookupResult(MatchType aMatchType)
      : mMatchType(aMatchType), mFailedToRequestDecode(false) {
    MOZ_ASSERT(
        mMatchType == MatchType::NOT_FOUND || mMatchType == MatchType::PENDING,
        "Only NOT_FOUND or PENDING make sense with no surface");
  }

  LookupResult(LookupResult&& aOther)
      : mSurface(std::move(aOther.mSurface)),
        mMatchType(aOther.mMatchType),
        mSuggestedSize(aOther.mSuggestedSize),
        mFailedToRequestDecode(aOther.mFailedToRequestDecode) {}

  LookupResult(DrawableSurface&& aSurface, MatchType aMatchType)
      : mSurface(std::move(aSurface)),
        mMatchType(aMatchType),
        mFailedToRequestDecode(false) {
    MOZ_ASSERT(!mSurface || !(mMatchType == MatchType::NOT_FOUND ||
                              mMatchType == MatchType::PENDING),
               "Only NOT_FOUND or PENDING make sense with no surface");
    MOZ_ASSERT(mSurface || mMatchType == MatchType::NOT_FOUND ||
                   mMatchType == MatchType::PENDING,
               "NOT_FOUND or PENDING do not make sense with a surface");
  }

  LookupResult(MatchType aMatchType, const gfx::IntSize& aSuggestedSize)
      : mMatchType(aMatchType),
        mSuggestedSize(aSuggestedSize),
        mFailedToRequestDecode(false) {
    MOZ_ASSERT(
        mMatchType == MatchType::NOT_FOUND || mMatchType == MatchType::PENDING,
        "Only NOT_FOUND or PENDING make sense with no surface");
  }

  LookupResult(DrawableSurface&& aSurface, MatchType aMatchType,
               const gfx::IntSize& aSuggestedSize)
      : mSurface(std::move(aSurface)),
        mMatchType(aMatchType),
        mSuggestedSize(aSuggestedSize),
        mFailedToRequestDecode(false) {
    MOZ_ASSERT(!mSurface || !(mMatchType == MatchType::NOT_FOUND ||
                              mMatchType == MatchType::PENDING),
               "Only NOT_FOUND or PENDING make sense with no surface");
    MOZ_ASSERT(mSurface || mMatchType == MatchType::NOT_FOUND ||
                   mMatchType == MatchType::PENDING,
               "NOT_FOUND or PENDING do not make sense with a surface");
  }

  LookupResult& operator=(LookupResult&& aOther) {
    MOZ_ASSERT(&aOther != this, "Self-move-assignment is not supported");
    mSurface = std::move(aOther.mSurface);
    mMatchType = aOther.mMatchType;
    mSuggestedSize = aOther.mSuggestedSize;
    mFailedToRequestDecode = aOther.mFailedToRequestDecode;
    return *this;
  }

  DrawableSurface& Surface() { return mSurface; }
  const DrawableSurface& Surface() const { return mSurface; }
  const gfx::IntSize& SuggestedSize() const { return mSuggestedSize; }

  /// @return true if this LookupResult contains a surface.
  explicit operator bool() const { return bool(mSurface); }

  /// @return what kind of match this is (exact, substitute, etc.)
  MatchType Type() const { return mMatchType; }

  void SetFailedToRequestDecode() { mFailedToRequestDecode = true; }
  bool GetFailedToRequestDecode() { return mFailedToRequestDecode; }

 private:
  LookupResult(const LookupResult&) = delete;
  LookupResult& operator=(const LookupResult& aOther) = delete;

  DrawableSurface mSurface;
  MatchType mMatchType;

  /// mSuggestedSize will be the size of the returned surface if the result is
  /// SUBSTITUTE_BECAUSE_BEST. It will be empty for EXACT, and can contain a
  /// non-empty size possibly different from the returned surface (if any) for
  /// all other results. If non-empty, it will always be the size the caller
  /// should request any decodes at.
  gfx::IntSize mSuggestedSize;

  // True if we tried to start a decode but failed, likely because the image was
  // too big to fit into the surface cache.
  bool mFailedToRequestDecode;
};

}  // namespace image
}  // namespace mozilla

#endif  // mozilla_image_LookupResult_h
