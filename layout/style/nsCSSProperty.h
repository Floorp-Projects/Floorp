/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

  #define CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_) eCSSProperty_##id_,
  #include "nsCSSPropList.h"
  #undef CSS_PROP

  eCSSProperty_COUNT_no_shorthands,
  // Make the count continue where it left off:
  eCSSProperty_COUNT_DUMMY = eCSSProperty_COUNT_no_shorthands - 1,

  #define CSS_PROP_SHORTHAND(name_, id_, method_, flags_) eCSSProperty_##id_,
  #include "nsCSSPropList.h"
  #undef CSS_PROP_SHORTHAND

  eCSSProperty_COUNT,

  // Extra dummy values for nsCSSParser internal use.
  eCSSPropertyExtra_x_none_value
};

// The types of values that can be in the nsCSS*/nsRuleData* structs.
// See nsCSSPropList.h for uses.
enum nsCSSType {
  eCSSType_Value,
  eCSSType_Rect,
  eCSSType_ValuePair,
  eCSSType_ValueList,
  eCSSType_ValuePairList
};

// The "descriptors" that can appear in a @font-face rule.
// They have the syntax of properties but different value rules.
// Keep in sync with kCSSRawFontDescs in nsCSSProps.cpp and
// nsCSSFontFaceStyleDecl::Fields in nsCSSRules.cpp.
enum nsCSSFontDesc {
  eCSSFontDesc_UNKNOWN = -1,
  eCSSFontDesc_Family,
  eCSSFontDesc_Style,
  eCSSFontDesc_Weight,
  eCSSFontDesc_Stretch,
  eCSSFontDesc_Src,
  eCSSFontDesc_UnicodeRange,
  eCSSFontDesc_COUNT
};

#endif /* nsCSSProperty_h___ */
