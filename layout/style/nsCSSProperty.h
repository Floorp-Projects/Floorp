/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* enum types for CSS properties and their values */
 
#ifndef nsCSSProperty_h___
#define nsCSSProperty_h___

/*
   Declare the enum list using the magic of preprocessing
   enum values are "eCSSProperty_foo" (where foo is the property)

   To change the list of properties, see nsCSSPropList.h

 */
enum nsCSSProperty {
  eCSSProperty_UNKNOWN = -1,

  #define CSS_PROP(name_, id_, method_, flags_, pref_, parsevariant_, \
                   kwtable_, stylestruct_, stylestructoffset_, animtype_) \
    eCSSProperty_##id_,
  #include "nsCSSPropList.h"
  #undef CSS_PROP

  eCSSProperty_COUNT_no_shorthands,
  // Make the count continue where it left off:
  eCSSProperty_COUNT_DUMMY = eCSSProperty_COUNT_no_shorthands - 1,

  #define CSS_PROP_SHORTHAND(name_, id_, method_, flags_, pref_) \
    eCSSProperty_##id_,
  #include "nsCSSPropList.h"
  #undef CSS_PROP_SHORTHAND

  eCSSProperty_COUNT,
  // Make the count continue where it left off:
  eCSSProperty_COUNT_DUMMY2 = eCSSProperty_COUNT - 1,

  #define CSS_PROP_ALIAS(aliasname_, id_, method_, pref_) \
    eCSSPropertyAlias_##method_,
  #include "nsCSSPropAliasList.h"
  #undef CSS_PROP_ALIAS

  eCSSProperty_COUNT_with_aliases,
  // Make the count continue where it left off:
  eCSSProperty_COUNT_DUMMY3 = eCSSProperty_COUNT_with_aliases - 1,

  // Some of the values below could probably overlap with each other
  // if we had a need for them to do so.

  // Extra values for use in the values of the 'transition-property'
  // property.
  eCSSPropertyExtra_no_properties,
  eCSSPropertyExtra_all_properties,

  // Extra dummy values for nsCSSParser internal use.
  eCSSPropertyExtra_x_none_value,
  eCSSPropertyExtra_x_auto_value,

  // Extra value to represent custom properties (var-*).
  eCSSPropertyExtra_variable
};

// The "descriptors" that can appear in a @font-face rule.
// They have the syntax of properties but different value rules.
enum nsCSSFontDesc {
  eCSSFontDesc_UNKNOWN = -1,
#define CSS_FONT_DESC(name_, method_) eCSSFontDesc_##method_,
#include "nsCSSFontDescList.h"
#undef CSS_FONT_DESC
  eCSSFontDesc_COUNT
};

#endif /* nsCSSProperty_h___ */
