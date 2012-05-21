/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* font constants shared by both thebes and layout */

#ifndef GFX_FONT_CONSTANTS_H
#define GFX_FONT_CONSTANTS_H

/*
 * This file is separate from gfxFont.h so that layout can include it
 * without bringing in gfxFont.h and everything it includes.
 */

#define NS_FONT_STYLE_NORMAL            0
#define NS_FONT_STYLE_ITALIC            1
#define NS_FONT_STYLE_OBLIQUE           2

#define NS_FONT_WEIGHT_NORMAL           400
#define NS_FONT_WEIGHT_BOLD             700

#define NS_FONT_STRETCH_ULTRA_CONDENSED (-4)
#define NS_FONT_STRETCH_EXTRA_CONDENSED (-3)
#define NS_FONT_STRETCH_CONDENSED       (-2)
#define NS_FONT_STRETCH_SEMI_CONDENSED  (-1)
#define NS_FONT_STRETCH_NORMAL          0
#define NS_FONT_STRETCH_SEMI_EXPANDED   1
#define NS_FONT_STRETCH_EXPANDED        2
#define NS_FONT_STRETCH_EXTRA_EXPANDED  3
#define NS_FONT_STRETCH_ULTRA_EXPANDED  4

#endif
