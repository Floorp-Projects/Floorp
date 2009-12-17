// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMMON_THUMBNAIL_SCORE_H__
#define CHROME_BROWSER_COMMON_THUMBNAIL_SCORE_H__

#include "base/time.h"

// A set of metadata about a Thumbnail.
struct ThumbnailScore {
  // Initializes the ThumbnailScore to the absolute worst possible
  // values except for time, which is set to Now().
  ThumbnailScore();

  // Builds a ThumbnailScore with the passed in values, and sets the
  // thumbnail generation time to Now().
  ThumbnailScore(double score, bool clipping, bool top);

  // Builds a ThumbnailScore with the passed in values.
  ThumbnailScore(double score, bool clipping, bool top,
                 const base::Time& time);
  ~ThumbnailScore();

  // Tests for equivalence between two ThumbnailScore objects.
  bool Equals(const ThumbnailScore& rhs) const;

  // How "boring" a thumbnail is. The boring score is the 0,1 ranged
  // percentage of pixels that are the most common luma. Higher boring
  // scores indicate that a higher percentage of a bitmap are all the
  // same brightness (most likely the same color).
  double boring_score;

  // Whether the thumbnail was taken with height greater then
  // width. In cases where we don't have |good_clipping|, the
  // thumbnails are either clipped from the horizontal center of the
  // window, or are otherwise weirdly stretched.
  bool good_clipping;

  // Whether this thumbnail was taken while the renderer was
  // displaying the top of the page. Most pages are more recognizable
  // by their headers then by a set of random text half way down the
  // page; i.e. most MediaWiki sites would be indistinguishable by
  // thumbnails with |at_top| set to false.
  bool at_top;

  // Record the time when a thumbnail was taken. This is used to make
  // sure thumbnails are kept fresh.
  base::Time time_at_snapshot;

  // How bad a thumbnail needs to be before we completely ignore it.
  static const double kThumbnailMaximumBoringness;

  // Time before we take a worse thumbnail (subject to
  // kThumbnailMaximumBoringness) over what's currently in the database
  // for freshness.
  static const base::TimeDelta kUpdateThumbnailTime;

  // Penalty of how much more boring a thumbnail should be per hour.
  static const double kThumbnailDegradePerHour;
};

// Checks whether we should replace one thumbnail with another.
bool ShouldReplaceThumbnailWith(const ThumbnailScore& current,
                                const ThumbnailScore& replacement);

#endif  // CHROME_BROWSER_COMMON_THUMBNAIL_SCORE_H__
