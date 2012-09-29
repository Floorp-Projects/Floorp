/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_FONT_FEATURES_H
#define GFX_FONT_FEATURES_H

// An OpenType feature tag and value pair
struct gfxFontFeature {
    uint32_t mTag; // see http://www.microsoft.com/typography/otspec/featuretags.htm
    uint32_t mValue; // 0 = off, 1 = on, larger values may be used as parameters
                     // to features that select among multiple alternatives
};

inline bool
operator<(const gfxFontFeature& a, const gfxFontFeature& b)
{
    return (a.mTag < b.mTag) || ((a.mTag == b.mTag) && (a.mValue < b.mValue));
}

inline bool
operator==(const gfxFontFeature& a, const gfxFontFeature& b)
{
    return (a.mTag == b.mTag) && (a.mValue == b.mValue);
}

#endif
