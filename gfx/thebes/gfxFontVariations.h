/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_FONT_VARIATIONS_H
#define GFX_FONT_VARIATIONS_H

// An OpenType variation tag and value pair
struct gfxFontVariation {
    uint32_t mTag;
    float mValue;
};

inline bool
operator==(const gfxFontVariation& a, const gfxFontVariation& b)
{
    return (a.mTag == b.mTag) && (a.mValue == b.mValue);
}

#endif
