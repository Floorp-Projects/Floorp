/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* keywords used within CSS property values */

/******

  This file contains the list of all parsed CSS keywords
  See nsCSSKeywords.h for access to the enum values for keywords

  It is designed to be used as inline input to nsCSSKeywords.cpp *only*
  through the magic of C preprocessing.

  All entries must be enclosed in the macro CSS_KEY which will have cruel
  and unusual things done to it

  It is recommended (but not strictly necessary) to keep all entries
  in alphabetical order

  Requirements:

  Entries are in the form: (name, id). 'id' must always be the same as 'name'
  except that all hyphens ('-') in 'name' are converted to underscores ('_')
  in 'id'. This lets us do nice things with the macros without having to
  copy/convert strings at runtime.

  'name' entries *must* use only lowercase characters.

  ** Break these invariants and bad things will happen. **

 ******/

// OUTPUT_CLASS=nsCSSKeywords
// MACRO_NAME=CSS_KEY

CSS_KEY(-moz-available, _moz_available)
CSS_KEY(-moz-center, _moz_center)
CSS_KEY(-moz-block-height, _moz_block_height)
CSS_KEY(-moz-fit-content, _moz_fit_content)
CSS_KEY(-moz-left, _moz_left)
CSS_KEY(-moz-none, _moz_none)
CSS_KEY(-moz-right, _moz_right)
CSS_KEY(auto, auto)
CSS_KEY(center, center)
CSS_KEY(dashed, dashed)
CSS_KEY(dotted, dotted)
CSS_KEY(double, double)
CSS_KEY(end, end)
CSS_KEY(grayscale, grayscale)
CSS_KEY(justify, justify)
CSS_KEY(left, left)
CSS_KEY(max-content, max_content)
CSS_KEY(min-content, min_content)
CSS_KEY(none, none)
CSS_KEY(normal, normal)
CSS_KEY(right, right)
CSS_KEY(solid, solid)
CSS_KEY(start, start)
CSS_KEY(subgrid, subgrid)
CSS_KEY(wavy, wavy)

// Appearance keywords for widget styles
//CSS_KEY(start, start)

