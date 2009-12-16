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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org>, Mozilla Corporation
 *   Mats Palmgren <mats.palmgren@bredband.net>
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

/*
 * a list of all CSS properties with considerable data about them, for
 * preprocessing
 */

/******

  This file contains the list of all parsed CSS properties.  It is
  designed to be used as inline input through the magic of C
  preprocessing.  All entries must be enclosed in the appropriate
  CSS_PROP_* macro which will have cruel and unusual things done to it.
  It is recommended (but not strictly necessary) to keep all entries in
  alphabetical order.

  The arguments to CSS_PROP and CSS_PROP_* are:

  1. 'name' entries represent a CSS property name and *must* use only
  lowercase characters.

  2. 'id' should be the same as 'name' except that all hyphens ('-')
  in 'name' are converted to underscores ('_') in 'id'. This lets us
  do nice things with the macros without having to copy/convert strings
  at runtime.  These are the names used for the enum values of the
  nsCSSProperty enumeration defined in nsCSSProps.h.

  3. 'method' is designed to be as input for CSS2Properties and similar
  callers.  It must always be the same as 'name' except it must use
  InterCaps and all hyphens ('-') must be removed.

  4. 'flags', a bitfield containing CSS_PROPERTY_* flags.

  5. 'datastruct' says which nsRuleData* struct this property goes in.

  6. 'member' gives the name of the member variable in the nsRuleData
  struct.

  7. 'type' gives the |nsCSSType| of the data in the nsRuleData struct
  and in the nsCSSDeclaration backend.

  8. 'kwtable', which is either nsnull or the name of the appropriate
  keyword table member of class nsCSSProps, for use in
  nsCSSProps::LookupPropertyValue.

  9. 'stylestruct_' [used only for CSS_PROP, not CSS_PROP_*] gives the
  name of the style struct.  Can be used to make nsStyle##stylestruct_
  and eStyleStruct_##stylestruct_

  10. 'stylestructoffset_' [not used for CSS_PROP_BACKENDONLY] gives the
  result of offsetof(nsStyle*, member).  Ignored (and generally
  CSS_PROP_NO_OFFSET, or -1) for properties whose animtype_ is
  eStyleAnimType_None.

  11. 'animtype_' [not used for CSS_PROP_BACKENDONLY] gives the
  animation type (see nsStyleAnimType) of this property.

  CSS_PROP_SHORTHAND only takes 1-4.

 ******/


/*************************************************************************/


// All includers must explicitly define CSS_PROP_SHORTHAND if they
// want it.
#ifndef CSS_PROP_SHORTHAND
#define CSS_PROP_SHORTHAND(name_, id_, method_, flags_) /* nothing */
#define DEFINED_CSS_PROP_SHORTHAND
#endif

#define CSS_PROP_NO_OFFSET (-1)

// Callers may define CSS_PROP_LIST_EXCLUDE_INTERNAL if they want to
// exclude internal properties that are not represented in the DOM (only
// the DOM style code defines this).

// A caller who wants all the properties can define the |CSS_PROP|
// macro.
#ifdef CSS_PROP

#define USED_CSS_PROP
#define CSS_PROP_FONT(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, stylestructoffset_, animtype_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, Font, stylestructoffset_, animtype_)
#define CSS_PROP_COLOR(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, stylestructoffset_, animtype_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, Color, stylestructoffset_, animtype_)
#define CSS_PROP_BACKGROUND(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, stylestructoffset_, animtype_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, Background, stylestructoffset_, animtype_)
#define CSS_PROP_LIST(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, stylestructoffset_, animtype_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, List, stylestructoffset_, animtype_)
#define CSS_PROP_POSITION(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, stylestructoffset_, animtype_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, Position, stylestructoffset_, animtype_)
#define CSS_PROP_TEXT(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, stylestructoffset_, animtype_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, Text, stylestructoffset_, animtype_)
#define CSS_PROP_TEXTRESET(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, stylestructoffset_, animtype_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, TextReset, stylestructoffset_, animtype_)
#define CSS_PROP_DISPLAY(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, stylestructoffset_, animtype_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, Display, stylestructoffset_, animtype_)
#define CSS_PROP_VISIBILITY(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, stylestructoffset_, animtype_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, Visibility, stylestructoffset_, animtype_)
#define CSS_PROP_CONTENT(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, stylestructoffset_, animtype_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, Content, stylestructoffset_, animtype_)
#define CSS_PROP_QUOTES(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, stylestructoffset_, animtype_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, Quotes, stylestructoffset_, animtype_)
#define CSS_PROP_USERINTERFACE(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, stylestructoffset_, animtype_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, UserInterface, stylestructoffset_, animtype_)
#define CSS_PROP_UIRESET(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, stylestructoffset_, animtype_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, UIReset, stylestructoffset_, animtype_)
#define CSS_PROP_TABLE(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, stylestructoffset_, animtype_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, Table, stylestructoffset_, animtype_)
#define CSS_PROP_TABLEBORDER(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, stylestructoffset_, animtype_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, TableBorder, stylestructoffset_, animtype_)
#define CSS_PROP_MARGIN(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, stylestructoffset_, animtype_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, Margin, stylestructoffset_, animtype_)
#define CSS_PROP_PADDING(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, stylestructoffset_, animtype_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, Padding, stylestructoffset_, animtype_)
#define CSS_PROP_BORDER(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, stylestructoffset_, animtype_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, Border, stylestructoffset_, animtype_)
#define CSS_PROP_OUTLINE(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, stylestructoffset_, animtype_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, Outline, stylestructoffset_, animtype_)
#define CSS_PROP_XUL(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, stylestructoffset_, animtype_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, XUL, stylestructoffset_, animtype_)
#define CSS_PROP_COLUMN(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, stylestructoffset_, animtype_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, Column, stylestructoffset_, animtype_)
#define CSS_PROP_SVG(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, stylestructoffset_, animtype_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, SVG, stylestructoffset_, animtype_)
#define CSS_PROP_SVGRESET(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, stylestructoffset_, animtype_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, SVGReset, stylestructoffset_, animtype_)

// For properties that are stored in the CSS backend but are not
// computed.  An includer may define this in addition to CSS_PROP, but
// otherwise we treat it as the same.
#ifndef CSS_PROP_BACKENDONLY
#define CSS_PROP_BACKENDONLY(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, BackendOnly, CSS_PROP_NO_OFFSET, eStyleAnimType_None)
#define DEFINED_CSS_PROP_BACKENDONLY
#endif

#else /* !defined(CSS_PROP) */

// An includer who does not define CSS_PROP can define any or all of the
// per-struct macros that are equivalent to it, and the rest will be
// ignored.

#ifndef CSS_PROP_FONT
#define CSS_PROP_FONT(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, stylestructoffset_, animtype_) /* nothing */
#define DEFINED_CSS_PROP_FONT
#endif
#ifndef CSS_PROP_COLOR
#define CSS_PROP_COLOR(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, stylestructoffset_, animtype_) /* nothing */
#define DEFINED_CSS_PROP_COLOR
#endif
#ifndef CSS_PROP_BACKGROUND
#define CSS_PROP_BACKGROUND(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, stylestructoffset_, animtype_) /* nothing */
#define DEFINED_CSS_PROP_BACKGROUND
#endif
#ifndef CSS_PROP_LIST
#define CSS_PROP_LIST(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, stylestructoffset_, animtype_) /* nothing */
#define DEFINED_CSS_PROP_LIST
#endif
#ifndef CSS_PROP_POSITION
#define CSS_PROP_POSITION(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, stylestructoffset_, animtype_) /* nothing */
#define DEFINED_CSS_PROP_POSITION
#endif
#ifndef CSS_PROP_TEXT
#define CSS_PROP_TEXT(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, stylestructoffset_, animtype_) /* nothing */
#define DEFINED_CSS_PROP_TEXT
#endif
#ifndef CSS_PROP_TEXTRESET
#define CSS_PROP_TEXTRESET(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, stylestructoffset_, animtype_) /* nothing */
#define DEFINED_CSS_PROP_TEXTRESET
#endif
#ifndef CSS_PROP_DISPLAY
#define CSS_PROP_DISPLAY(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, stylestructoffset_, animtype_) /* nothing */
#define DEFINED_CSS_PROP_DISPLAY
#endif
#ifndef CSS_PROP_VISIBILITY
#define CSS_PROP_VISIBILITY(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, stylestructoffset_, animtype_) /* nothing */
#define DEFINED_CSS_PROP_VISIBILITY
#endif
#ifndef CSS_PROP_CONTENT
#define CSS_PROP_CONTENT(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, stylestructoffset_, animtype_) /* nothing */
#define DEFINED_CSS_PROP_CONTENT
#endif
#ifndef CSS_PROP_QUOTES
#define CSS_PROP_QUOTES(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, stylestructoffset_, animtype_) /* nothing */
#define DEFINED_CSS_PROP_QUOTES
#endif
#ifndef CSS_PROP_USERINTERFACE
#define CSS_PROP_USERINTERFACE(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, stylestructoffset_, animtype_) /* nothing */
#define DEFINED_CSS_PROP_USERINTERFACE
#endif
#ifndef CSS_PROP_UIRESET
#define CSS_PROP_UIRESET(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, stylestructoffset_, animtype_) /* nothing */
#define DEFINED_CSS_PROP_UIRESET
#endif
#ifndef CSS_PROP_TABLE
#define CSS_PROP_TABLE(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, stylestructoffset_, animtype_) /* nothing */
#define DEFINED_CSS_PROP_TABLE
#endif
#ifndef CSS_PROP_TABLEBORDER
#define CSS_PROP_TABLEBORDER(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, stylestructoffset_, animtype_) /* nothing */
#define DEFINED_CSS_PROP_TABLEBORDER
#endif
#ifndef CSS_PROP_MARGIN
#define CSS_PROP_MARGIN(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, stylestructoffset_, animtype_) /* nothing */
#define DEFINED_CSS_PROP_MARGIN
#endif
#ifndef CSS_PROP_PADDING
#define CSS_PROP_PADDING(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, stylestructoffset_, animtype_) /* nothing */
#define DEFINED_CSS_PROP_PADDING
#endif
#ifndef CSS_PROP_BORDER
#define CSS_PROP_BORDER(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, stylestructoffset_, animtype_) /* nothing */
#define DEFINED_CSS_PROP_BORDER
#endif
#ifndef CSS_PROP_OUTLINE
#define CSS_PROP_OUTLINE(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, stylestructoffset_, animtype_) /* nothing */
#define DEFINED_CSS_PROP_OUTLINE
#endif
#ifndef CSS_PROP_XUL
#define CSS_PROP_XUL(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, stylestructoffset_, animtype_) /* nothing */
#define DEFINED_CSS_PROP_XUL
#endif
#ifndef CSS_PROP_COLUMN
#define CSS_PROP_COLUMN(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, stylestructoffset_, animtype_) /* nothing */
#define DEFINED_CSS_PROP_COLUMN
#endif
#ifndef CSS_PROP_SVG
#define CSS_PROP_SVG(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, stylestructoffset_, animtype_) /* nothing */
#define DEFINED_CSS_PROP_SVG
#endif
#ifndef CSS_PROP_SVGRESET
#define CSS_PROP_SVGRESET(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_, stylestructoffset_, animtype_) /* nothing */
#define DEFINED_CSS_PROP_SVGRESET
#endif

#ifndef CSS_PROP_BACKENDONLY
#define CSS_PROP_BACKENDONLY(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_) /* nothing */
#define DEFINED_CSS_PROP_BACKENDONLY
#endif

#endif /* !defined(CSS_PROP) */

/*************************************************************************/

// For notes XXX bug 3935 below, the names being parsed do not correspond
// to the constants used internally.  It would be nice to bring the
// constants into line sometime.

// The parser will refuse to parse properties marked with -x-.

// Those marked XXX bug 48973 are CSS2 properties that we support
// differently from the spec for UI requirements.  If we ever
// support them correctly the old constants need to be renamed and
// new ones should be entered.

// CSS2.1 section 5.12.1 says that the properties that apply to
// :first-line are: font properties, color properties, background
// properties, 'word-spacing', 'letter-spacing', 'text-decoration',
// 'vertical-align', 'text-transform', and 'line-height'.
//
// We also allow 'text-shadow', which was listed in CSS2 (where the
// property existed).

// CSS2.1 section 5.12.2 says that the properties that apply to
// :first-letter are: font properties, 'text-decoration',
// 'text-transform', 'letter-spacing', 'word-spacing' (when
// appropriate), 'line-height', 'float', 'vertical-align' (only if
// 'float' is 'none'), margin properties, padding properties, border
// properties, 'color', and background properties.  We also allow
// 'text-shadow' (see above) and '-moz-box-shadow' (which is like the
// border properties).

// We include '-moz-background-inline-policy' (css3-background's
// 'background-break') in both as a background property, although this
// is somewhat questionable.

CSS_PROP_DISPLAY(
    -moz-appearance,
    appearance,
    MozAppearance,
    0,
    Display,
    mAppearance,
    eCSSType_Value,
    kAppearanceKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_SHORTHAND(
    -moz-border-radius,
    _moz_border_radius,
    MozBorderRadius,
    0)
CSS_PROP_BORDER(
    -moz-border-radius-topleft,
    _moz_border_radius_topLeft,
    MozBorderRadiusTopleft,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER,
    Margin,
    mBorderRadius.mTopLeft,
    eCSSType_ValuePair,
    nsnull,
    offsetof(nsStyleBorder, mBorderRadius),
    eStyleAnimType_Corner_TopLeft)
CSS_PROP_BORDER(
    -moz-border-radius-topright,
    _moz_border_radius_topRight,
    MozBorderRadiusTopright,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER,
    Margin,
    mBorderRadius.mTopRight,
    eCSSType_ValuePair,
    nsnull,
    offsetof(nsStyleBorder, mBorderRadius),
    eStyleAnimType_Corner_TopRight)
CSS_PROP_BORDER(
    -moz-border-radius-bottomright,
    _moz_border_radius_bottomRight,
    MozBorderRadiusBottomright,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER,
    Margin,
    mBorderRadius.mBottomRight,
    eCSSType_ValuePair,
    nsnull,
    offsetof(nsStyleBorder, mBorderRadius),
    eStyleAnimType_Corner_BottomRight)
CSS_PROP_BORDER(
    -moz-border-radius-bottomleft,
    _moz_border_radius_bottomLeft,
    MozBorderRadiusBottomleft,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER,
    Margin,
    mBorderRadius.mBottomLeft,
    eCSSType_ValuePair,
    nsnull,
    offsetof(nsStyleBorder, mBorderRadius),
    eStyleAnimType_Corner_BottomLeft)
CSS_PROP_SHORTHAND(
    -moz-outline-radius,
    _moz_outline_radius,
    MozOutlineRadius,
    0)
CSS_PROP_OUTLINE(
    -moz-outline-radius-topleft,
    _moz_outline_radius_topLeft,
    MozOutlineRadiusTopleft,
    0,
    Margin,
    mOutlineRadius.mTopLeft,
    eCSSType_ValuePair,
    nsnull,
    offsetof(nsStyleOutline, mOutlineRadius),
    eStyleAnimType_Corner_TopLeft)
CSS_PROP_OUTLINE(
    -moz-outline-radius-topright,
    _moz_outline_radius_topRight,
    MozOutlineRadiusTopright,
    0,
    Margin,
    mOutlineRadius.mTopRight,
    eCSSType_ValuePair,
    nsnull,
    offsetof(nsStyleOutline, mOutlineRadius),
    eStyleAnimType_Corner_TopRight)
CSS_PROP_OUTLINE(
    -moz-outline-radius-bottomright,
    _moz_outline_radius_bottomRight,
    MozOutlineRadiusBottomright,
    0,
    Margin,
    mOutlineRadius.mBottomRight,
    eCSSType_ValuePair,
    nsnull,
    offsetof(nsStyleOutline, mOutlineRadius),
    eStyleAnimType_Corner_BottomRight)
CSS_PROP_OUTLINE(
    -moz-outline-radius-bottomleft,
    _moz_outline_radius_bottomLeft,
    MozOutlineRadiusBottomleft,
    0,
    Margin,
    mOutlineRadius.mBottomLeft,
    eCSSType_ValuePair,
    nsnull,
    offsetof(nsStyleOutline, mOutlineRadius),
    eStyleAnimType_Corner_BottomLeft)
CSS_PROP_TEXT(
    -moz-tab-size,
    _moz_tab_size,
    MozTabSize,
    0,
    Text,
    mTabSize,
    eCSSType_Value,
    nsnull,
    offsetof(nsStyleText, mTabSize),
    eStyleAnimType_None)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_FONT(
    -x-system-font,
    _x_system_font,
    X,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE,
    Font,
    mSystemFont,
    eCSSType_Value,
    kFontKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
#endif
CSS_PROP_BACKENDONLY(
    azimuth,
    azimuth,
    Azimuth,
    0,
    Aural,
    mAzimuth,
    eCSSType_Value,
    kAzimuthKTable)
CSS_PROP_SHORTHAND(
    background,
    background,
    Background,
    0)
CSS_PROP_BACKGROUND(
    background-attachment,
    background_attachment,
    BackgroundAttachment,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE |
        CSS_PROPERTY_VALUE_LIST_USES_COMMAS,
    Color,
    mBackAttachment,
    eCSSType_ValueList,
    kBackgroundAttachmentKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_BACKGROUND(
    -moz-background-clip,
    _moz_background_clip,
    MozBackgroundClip,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE |
        CSS_PROPERTY_VALUE_LIST_USES_COMMAS,
    Color,
    mBackClip,
    eCSSType_ValueList,
    kBackgroundClipKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_BACKGROUND(
    background-color,
    background_color,
    BackgroundColor,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE |
        CSS_PROPERTY_IGNORED_WHEN_COLORS_DISABLED,
    Color,
    mBackColor,
    eCSSType_Value,
    nsnull,
    offsetof(nsStyleBackground, mBackgroundColor),
    eStyleAnimType_Color)
CSS_PROP_BACKGROUND(
    background-image,
    background_image,
    BackgroundImage,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE |
        CSS_PROPERTY_VALUE_LIST_USES_COMMAS |
        CSS_PROPERTY_IGNORED_WHEN_COLORS_DISABLED |
        CSS_PROPERTY_START_IMAGE_LOADS,
    Color,
    mBackImage,
    eCSSType_ValueList,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_BACKGROUND(
    -moz-background-inline-policy,
    _moz_background_inline_policy,
    MozBackgroundInlinePolicy,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE,
    Color,
    mBackInlinePolicy,
    eCSSType_Value,
    kBackgroundInlinePolicyKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_BACKGROUND(
    -moz-background-origin,
    _moz_background_origin,
    MozBackgroundOrigin,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE |
        CSS_PROPERTY_VALUE_LIST_USES_COMMAS,
    Color,
    mBackOrigin,
    eCSSType_ValueList,
    kBackgroundOriginKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_BACKGROUND(
    background-position,
    background_position,
    BackgroundPosition,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE |
        CSS_PROPERTY_VALUE_LIST_USES_COMMAS,
    Color,
    mBackPosition,
    eCSSType_ValuePairList,
    kBackgroundPositionKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_BACKGROUND(
    background-repeat,
    background_repeat,
    BackgroundRepeat,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE |
        CSS_PROPERTY_VALUE_LIST_USES_COMMAS,
    Color,
    mBackRepeat,
    eCSSType_ValueList,
    kBackgroundRepeatKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_BACKGROUND(
    -moz-background-size,
    _moz_background_size,
    MozBackgroundSize,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE |
        CSS_PROPERTY_VALUE_LIST_USES_COMMAS,
    Color,
    mBackSize,
    eCSSType_ValuePairList,
    kBackgroundSizeKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_DISPLAY(
    -moz-binding,
    binding,
    MozBinding,
    0,
    Display,
    mBinding,
    eCSSType_Value,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None) // XXX bug 3935
CSS_PROP_SHORTHAND(
    border,
    border,
    Border,
    0)
CSS_PROP_SHORTHAND(
    border-bottom,
    border_bottom,
    BorderBottom,
    0)
CSS_PROP_BORDER(
    border-bottom-color,
    border_bottom_color,
    BorderBottomColor,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_IGNORED_WHEN_COLORS_DISABLED,
    Margin,
    mBorderColor.mBottom,
    eCSSType_Value,
    kBorderColorKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_Custom)
CSS_PROP_BORDER(
    -moz-border-bottom-colors,
    border_bottom_colors,
    MozBorderBottomColors,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_IGNORED_WHEN_COLORS_DISABLED,
    Margin,
    mBorderColors.mBottom,
    eCSSType_ValueList,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_BORDER(
    border-bottom-style,
    border_bottom_style,
    BorderBottomStyle,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER,
    Margin,
    mBorderStyle.mBottom,
    eCSSType_Value,
    kBorderStyleKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)  // on/off will need reflow
CSS_PROP_BORDER(
    border-bottom-width,
    border_bottom_width,
    BorderBottomWidth,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER,
    Margin,
    mBorderWidth.mBottom,
    eCSSType_Value,
    kBorderWidthKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_Custom)
CSS_PROP_TABLEBORDER(
    border-collapse,
    border_collapse,
    BorderCollapse,
    0,
    Table,
    mBorderCollapse,
    eCSSType_Value,
    kBorderCollapseKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_SHORTHAND(
    border-color,
    border_color,
    BorderColor,
    0)
CSS_PROP_SHORTHAND(
    -moz-border-end,
    border_end,
    MozBorderEnd,
    0)
CSS_PROP_SHORTHAND(
    -moz-border-end-color,
    border_end_color,
    MozBorderEndColor,
    0)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_BORDER(
    border-end-color-value,
    border_end_color_value,
    X,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER,
    Margin,
    mBorderEndColor,
    eCSSType_Value,
    kBorderColorKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
#endif
CSS_PROP_SHORTHAND(
    -moz-border-end-style,
    border_end_style,
    MozBorderEndStyle,
    0)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_BORDER(
    border-end-style-value,
    border_end_style_value,
    X,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER,
    Margin,
    mBorderEndStyle,
    eCSSType_Value,
    kBorderStyleKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
#endif
CSS_PROP_SHORTHAND(
    -moz-border-end-width,
    border_end_width,
    MozBorderEndWidth,
    0)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_BORDER(
    border-end-width-value,
    border_end_width_value,
    X,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER,
    Margin,
    mBorderEndWidth,
    eCSSType_Value,
    kBorderWidthKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
#endif
CSS_PROP_BORDER(
    -moz-border-image,
    border_image,
    MozBorderImage,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_START_IMAGE_LOADS |
        CSS_PROPERTY_IMAGE_IS_IN_ARRAY_0,
    Margin,
    mBorderImage,
    eCSSType_Value,
    kBorderImageKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_SHORTHAND(
    border-left,
    border_left,
    BorderLeft,
    0)
CSS_PROP_SHORTHAND(
    border-left-color,
    border_left_color,
    BorderLeftColor,
    0)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_BORDER(
    border-left-color-value,
    border_left_color_value,
    X,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_IGNORED_WHEN_COLORS_DISABLED,
    Margin,
    mBorderColor.mLeft,
    eCSSType_Value,
    kBorderColorKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_Custom)
CSS_PROP_BORDER(
    border-left-color-ltr-source,
    border_left_color_ltr_source,
    X,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_DIRECTIONAL_SOURCE |
        CSS_PROPERTY_IGNORED_WHEN_COLORS_DISABLED,
    Margin,
    mBorderLeftColorLTRSource,
    eCSSType_Value,
    kBoxPropSourceKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_BORDER(
    border-left-color-rtl-source,
    border_left_color_rtl_source,
    X,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_DIRECTIONAL_SOURCE |
        CSS_PROPERTY_IGNORED_WHEN_COLORS_DISABLED,
    Margin,
    mBorderLeftColorRTLSource,
    eCSSType_Value,
    kBoxPropSourceKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
#endif
CSS_PROP_BORDER(
    -moz-border-left-colors,
    border_left_colors,
    MozBorderLeftColors,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_IGNORED_WHEN_COLORS_DISABLED,
    Margin,
    mBorderColors.mLeft,
    eCSSType_ValueList,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_SHORTHAND(
    border-left-style,
    border_left_style,
    BorderLeftStyle,
    0) // on/off will need reflow
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_BORDER(
    border-left-style-value,
    border_left_style_value,
    X,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER,
    Margin,
    mBorderStyle.mLeft,
    eCSSType_Value,
    kBorderStyleKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_BORDER(
    border-left-style-ltr-source,
    border_left_style_ltr_source,
    X,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_DIRECTIONAL_SOURCE,
    Margin,
    mBorderLeftStyleLTRSource,
    eCSSType_Value,
    kBoxPropSourceKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_BORDER(
    border-left-style-rtl-source,
    border_left_style_rtl_source,
    X,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_DIRECTIONAL_SOURCE,
    Margin,
    mBorderLeftStyleRTLSource,
    eCSSType_Value,
    kBoxPropSourceKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
#endif
CSS_PROP_SHORTHAND(
    border-left-width,
    border_left_width,
    BorderLeftWidth,
    0)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_BORDER(
    border-left-width-value,
    border_left_width_value,
    X,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER,
    Margin,
    mBorderWidth.mLeft,
    eCSSType_Value,
    kBorderWidthKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_Custom)
CSS_PROP_BORDER(
    border-left-width-ltr-source,
    border_left_width_ltr_source,
    X,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_DIRECTIONAL_SOURCE,
    Margin,
    mBorderLeftWidthLTRSource,
    eCSSType_Value,
    kBoxPropSourceKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_BORDER(
    border-left-width-rtl-source,
    border_left_width_rtl_source,
    X,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_DIRECTIONAL_SOURCE,
    Margin,
    mBorderLeftWidthRTLSource,
    eCSSType_Value,
    kBoxPropSourceKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
#endif
CSS_PROP_SHORTHAND(
    border-right,
    border_right,
    BorderRight,
    0)
CSS_PROP_SHORTHAND(
    border-right-color,
    border_right_color,
    BorderRightColor,
    0)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_BORDER(
    border-right-color-value,
    border_right_color_value,
    X,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_IGNORED_WHEN_COLORS_DISABLED,
    Margin,
    mBorderColor.mRight,
    eCSSType_Value,
    kBorderColorKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_Custom)
CSS_PROP_BORDER(
    border-right-color-ltr-source,
    border_right_color_ltr_source,
    X,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_DIRECTIONAL_SOURCE |
        CSS_PROPERTY_IGNORED_WHEN_COLORS_DISABLED,
    Margin,
    mBorderRightColorLTRSource,
    eCSSType_Value,
    kBoxPropSourceKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_BORDER(
    border-right-color-rtl-source,
    border_right_color_rtl_source,
    X,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_DIRECTIONAL_SOURCE |
        CSS_PROPERTY_IGNORED_WHEN_COLORS_DISABLED,
    Margin,
    mBorderRightColorRTLSource,
    eCSSType_Value,
    kBoxPropSourceKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
#endif
CSS_PROP_BORDER(
    -moz-border-right-colors,
    border_right_colors,
    MozBorderRightColors,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_IGNORED_WHEN_COLORS_DISABLED,
    Margin,
    mBorderColors.mRight,
    eCSSType_ValueList,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_SHORTHAND(
    border-right-style,
    border_right_style,
    BorderRightStyle,
    0) // on/off will need reflow
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_BORDER(
    border-right-style-value,
    border_right_style_value,
    X,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER,
    Margin,
    mBorderStyle.mRight,
    eCSSType_Value,
    kBorderStyleKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_BORDER(
    border-right-style-ltr-source,
    border_right_style_ltr_source,
    X,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_DIRECTIONAL_SOURCE,
    Margin,
    mBorderRightStyleLTRSource,
    eCSSType_Value,
    kBoxPropSourceKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_BORDER(
    border-right-style-rtl-source,
    border_right_style_rtl_source,
    X,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_DIRECTIONAL_SOURCE,
    Margin,
    mBorderRightStyleRTLSource,
    eCSSType_Value,
    kBoxPropSourceKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
#endif
CSS_PROP_SHORTHAND(
    border-right-width,
    border_right_width,
    BorderRightWidth,
    0)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_BORDER(
    border-right-width-value,
    border_right_width_value,
    X,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER,
    Margin,
    mBorderWidth.mRight,
    eCSSType_Value,
    kBorderWidthKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_Custom)
CSS_PROP_BORDER(
    border-right-width-ltr-source,
    border_right_width_ltr_source,
    X,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_DIRECTIONAL_SOURCE,
    Margin,
    mBorderRightWidthLTRSource,
    eCSSType_Value,
    kBoxPropSourceKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_BORDER(
    border-right-width-rtl-source,
    border_right_width_rtl_source,
    X,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_DIRECTIONAL_SOURCE,
    Margin,
    mBorderRightWidthRTLSource,
    eCSSType_Value,
    kBoxPropSourceKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
#endif
CSS_PROP_TABLEBORDER(
    border-spacing,
    border_spacing,
    BorderSpacing,
    0,
    Table,
    mBorderSpacing,
    eCSSType_ValuePair,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_Custom) // XXX bug 3935
CSS_PROP_SHORTHAND(
    -moz-border-start,
    border_start,
    MozBorderStart,
    0)
CSS_PROP_SHORTHAND(
    -moz-border-start-color,
    border_start_color,
    MozBorderStartColor,
    0)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_BORDER(
    border-start-color-value,
    border_start_color_value,
    X,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER,
    Margin,
    mBorderStartColor,
    eCSSType_Value,
    kBorderColorKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
#endif
CSS_PROP_SHORTHAND(
    -moz-border-start-style,
    border_start_style,
    MozBorderStartStyle,
    0)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_BORDER(
    border-start-style-value,
    border_start_style_value,
    X,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER,
    Margin,
    mBorderStartStyle,
    eCSSType_Value,
    kBorderStyleKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
#endif
CSS_PROP_SHORTHAND(
    -moz-border-start-width,
    border_start_width,
    MozBorderStartWidth,
    0)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_BORDER(
    border-start-width-value,
    border_start_width_value,
    X,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER,
    Margin,
    mBorderStartWidth,
    eCSSType_Value,
    kBorderWidthKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
#endif
CSS_PROP_SHORTHAND(
    border-style,
    border_style,
    BorderStyle,
    0)  // on/off will need reflow
CSS_PROP_SHORTHAND(
    border-top,
    border_top,
    BorderTop,
    0)
CSS_PROP_BORDER(
    border-top-color,
    border_top_color,
    BorderTopColor,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_IGNORED_WHEN_COLORS_DISABLED,
    Margin,
    mBorderColor.mTop,
    eCSSType_Value,
    kBorderColorKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_Custom)
CSS_PROP_BORDER(
    -moz-border-top-colors,
    border_top_colors,
    MozBorderTopColors,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_IGNORED_WHEN_COLORS_DISABLED,
    Margin,
    mBorderColors.mTop,
    eCSSType_ValueList,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_BORDER(
    border-top-style,
    border_top_style,
    BorderTopStyle,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER,
    Margin,
    mBorderStyle.mTop,
    eCSSType_Value,
    kBorderStyleKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)  // on/off will need reflow
CSS_PROP_BORDER(
    border-top-width,
    border_top_width,
    BorderTopWidth,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER,
    Margin,
    mBorderWidth.mTop,
    eCSSType_Value,
    kBorderWidthKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_Custom)
CSS_PROP_SHORTHAND(
    border-width,
    border_width,
    BorderWidth,
    0)
CSS_PROP_POSITION(
    bottom,
    bottom,
    Bottom,
    0,
    Position,
    mOffset.mBottom,
    eCSSType_Value,
    nsnull,
    offsetof(nsStylePosition, mOffset),
    eStyleAnimType_Sides_Bottom)
CSS_PROP_BORDER(
    -moz-box-shadow,
    box_shadow,
    MozBoxShadow,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_VALUE_LIST_USES_COMMAS |
        CSS_PROPERTY_IGNORED_WHEN_COLORS_DISABLED,
    Margin,
    mBoxShadow,
    eCSSType_ValueList,
    kBoxShadowTypeKTable,
    offsetof(nsStyleBorder, mBoxShadow),
    eStyleAnimType_Shadow)
CSS_PROP_POSITION(
    -moz-box-sizing,
    box_sizing,
    MozBoxSizing,
    0,
    Position,
    mBoxSizing,
    eCSSType_Value,
    kBoxSizingKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None) // XXX bug 3935
CSS_PROP_TABLEBORDER(
    caption-side,
    caption_side,
    CaptionSide,
    0,
    Table,
    mCaptionSide,
    eCSSType_Value,
    kCaptionSideKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_DISPLAY(
    clear,
    clear,
    Clear,
    0,
    Display,
    mClear,
    eCSSType_Value,
    kClearKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_DISPLAY(
    clip,
    clip,
    Clip,
    0,
    Display,
    mClip,
    eCSSType_Rect,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_COLOR(
    color,
    color,
    Color,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE |
        CSS_PROPERTY_IGNORED_WHEN_COLORS_DISABLED,
    Color,
    mColor,
    eCSSType_Value,
    nsnull,
    offsetof(nsStyleColor, mColor),
    eStyleAnimType_Color)
CSS_PROP_COLUMN(
    -moz-column-count,
    _moz_column_count,
    MozColumnCount,
    0,
    Column,
    mColumnCount,
    eCSSType_Value,
    nsnull,
    offsetof(nsStyleColumn, mColumnCount),
    eStyleAnimType_Custom)
CSS_PROP_COLUMN(
    -moz-column-width,
    _moz_column_width,
    MozColumnWidth,
    0,
    Column,
    mColumnWidth,
    eCSSType_Value,
    nsnull,
    offsetof(nsStyleColumn, mColumnWidth),
    eStyleAnimType_Coord)
CSS_PROP_COLUMN(
    -moz-column-gap,
    _moz_column_gap,
    MozColumnGap,
    0,
    Column,
    mColumnGap,
    eCSSType_Value,
    nsnull,
    offsetof(nsStyleColumn, mColumnGap),
    eStyleAnimType_Coord)
CSS_PROP_SHORTHAND(
    -moz-column-rule,
    _moz_column_rule,
    MozColumnRule,
    0)
CSS_PROP_COLUMN(
    -moz-column-rule-color,
    _moz_column_rule_color,
    MozColumnRuleColor,
    CSS_PROPERTY_IGNORED_WHEN_COLORS_DISABLED,
    Column,
    mColumnRuleColor,
    eCSSType_Value,
    nsnull,
    offsetof(nsStyleColumn, mColumnRuleColor),
    eStyleAnimType_Color)
CSS_PROP_COLUMN(
    -moz-column-rule-style,
    _moz_column_rule_style,
    MozColumnRuleStyle,
    0,
    Column,
    mColumnRuleStyle,
    eCSSType_Value,
    kBorderStyleKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_COLUMN(
    -moz-column-rule-width,
    _moz_column_rule_width,
    MozColumnRuleWidth,
    0,
    Column,
    mColumnRuleWidth,
    eCSSType_Value,
    kBorderWidthKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_Custom)
CSS_PROP_CONTENT(
    content,
    content,
    Content,
    CSS_PROPERTY_START_IMAGE_LOADS,
    Content,
    mContent,
    eCSSType_ValueList,
    kContentKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_CONTENT(
    counter-increment,
    counter_increment,
    CounterIncrement,
    0,
    Content,
    mCounterIncrement,
    eCSSType_ValuePairList,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None) // XXX bug 137285
CSS_PROP_CONTENT(
    counter-reset,
    counter_reset,
    CounterReset,
    0,
    Content,
    mCounterReset,
    eCSSType_ValuePairList,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None) // XXX bug 137285
CSS_PROP_SHORTHAND(
    cue,
    cue,
    Cue,
    0)
CSS_PROP_BACKENDONLY(
    cue-after,
    cue_after,
    CueAfter,
    0,
    Aural,
    mCueAfter,
    eCSSType_Value,
    nsnull)
CSS_PROP_BACKENDONLY(
    cue-before,
    cue_before,
    CueBefore,
    0,
    Aural,
    mCueBefore,
    eCSSType_Value,
    nsnull)
CSS_PROP_USERINTERFACE(
    cursor,
    cursor,
    Cursor,
    CSS_PROPERTY_VALUE_LIST_USES_COMMAS |
        CSS_PROPERTY_START_IMAGE_LOADS |
        CSS_PROPERTY_IMAGE_IS_IN_ARRAY_0,
    UserInterface,
    mCursor,
    eCSSType_ValueList,
    kCursorKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_VISIBILITY(
    direction,
    direction,
    Direction,
    0,
    Display,
    mDirection,
    eCSSType_Value,
    kDirectionKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_DISPLAY(
    display,
    display,
    Display,
    0,
    Display,
    mDisplay,
    eCSSType_Value,
    kDisplayKTable,
    offsetof(nsStyleDisplay, mDisplay),
    eStyleAnimType_EnumU8)
CSS_PROP_BACKENDONLY(
    elevation,
    elevation,
    Elevation,
    0,
    Aural,
    mElevation,
    eCSSType_Value,
    kElevationKTable)
CSS_PROP_TABLEBORDER(
    empty-cells,
    empty_cells,
    EmptyCells,
    0,
    Table,
    mEmptyCells,
    eCSSType_Value,
    kEmptyCellsKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_DISPLAY(
    float,
    float,
    CssFloat,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER,
    Display,
    mFloat,
    eCSSType_Value,
    kFloatKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_BORDER(
    -moz-float-edge,
    float_edge,
    MozFloatEdge,
    0,
    Margin,
    mFloatEdge,
    eCSSType_Value,
    kFloatEdgeKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None) // XXX bug 3935
CSS_PROP_SHORTHAND(
    font,
    font,
    Font,
    0)
CSS_PROP_FONT(
    font-family,
    font_family,
    FontFamily,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE,
    Font,
    mFamily,
    eCSSType_Value,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_FONT(
    font-size,
    font_size,
    FontSize,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE,
    Font,
    mSize,
    eCSSType_Value,
    kFontSizeKTable,
    // Note that mSize is the correct place for *reading* the computed value,
    // but setting it requires setting mFont.size as well.
    offsetof(nsStyleFont, mSize),
    eStyleAnimType_nscoord)
CSS_PROP_FONT(
    font-size-adjust,
    font_size_adjust,
    FontSizeAdjust,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE,
    Font,
    mSizeAdjust,
    eCSSType_Value,
    nsnull,
    offsetof(nsStyleFont, mFont.sizeAdjust),
    eStyleAnimType_float)
CSS_PROP_FONT(
    font-stretch,
    font_stretch,
    FontStretch,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE,
    Font,
    mStretch,
    eCSSType_Value,
    kFontStretchKTable,
    offsetof(nsStyleFont, mFont.stretch),
    eStyleAnimType_Custom)
CSS_PROP_FONT(
    font-style,
    font_style,
    FontStyle,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE,
    Font,
    mStyle,
    eCSSType_Value,
    kFontStyleKTable,
    offsetof(nsStyleFont, mFont.style),
    eStyleAnimType_EnumU8)
CSS_PROP_FONT(
    font-variant,
    font_variant,
    FontVariant,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE,
    Font,
    mVariant,
    eCSSType_Value,
    kFontVariantKTable,
    offsetof(nsStyleFont, mFont.variant),
    eStyleAnimType_EnumU8)
CSS_PROP_FONT(
    font-weight,
    font_weight,
    FontWeight,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE,
    Font,
    mWeight,
    eCSSType_Value,
    kFontWeightKTable,
    offsetof(nsStyleFont, mFont.weight),
    eStyleAnimType_Custom)
CSS_PROP_UIRESET(
    -moz-force-broken-image-icon,
    force_broken_image_icon,
    MozForceBrokenImageIcon,
    0,
    UserInterface,
    mForceBrokenImageIcon,
    eCSSType_Value,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None) // bug 58646
CSS_PROP_POSITION(
    height,
    height,
    Height,
    0,
    Position,
    mHeight,
    eCSSType_Value,
    nsnull,
    offsetof(nsStylePosition, mHeight),
    eStyleAnimType_Coord)
CSS_PROP_LIST(
    -moz-image-region,
    image_region,
    MozImageRegion,
    0,
    List,
    mImageRegion,
    eCSSType_Rect,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_UIRESET(
    ime-mode,
    ime_mode,
    ImeMode,
    0,
    UserInterface,
    mIMEMode,
    eCSSType_Value,
    kIMEModeKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_POSITION(
    left,
    left,
    Left,
    0,
    Position,
    mOffset.mLeft,
    eCSSType_Value,
    nsnull,
    offsetof(nsStylePosition, mOffset),
    eStyleAnimType_Sides_Left)
CSS_PROP_TEXT(
    letter-spacing,
    letter_spacing,
    LetterSpacing,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE,
    Text,
    mLetterSpacing,
    eCSSType_Value,
    nsnull,
    offsetof(nsStyleText, mLetterSpacing),
    eStyleAnimType_Coord)
CSS_PROP_TEXT(
    line-height,
    line_height,
    LineHeight,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE,
    Text,
    mLineHeight,
    eCSSType_Value,
    kLineHeightKTable,
    offsetof(nsStyleText, mLineHeight),
    eStyleAnimType_Coord)
CSS_PROP_SHORTHAND(
    list-style,
    list_style,
    ListStyle,
    0)
CSS_PROP_LIST(
    list-style-image,
    list_style_image,
    ListStyleImage,
    CSS_PROPERTY_START_IMAGE_LOADS,
    List,
    mImage,
    eCSSType_Value,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_LIST(
    list-style-position,
    list_style_position,
    ListStylePosition,
    0,
    List,
    mPosition,
    eCSSType_Value,
    kListStylePositionKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_LIST(
    list-style-type,
    list_style_type,
    ListStyleType,
    0,
    List,
    mType,
    eCSSType_Value,
    kListStyleKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_SHORTHAND(
    margin,
    margin,
    Margin,
    0)
CSS_PROP_MARGIN(
    margin-bottom,
    margin_bottom,
    MarginBottom,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER,
    Margin,
    mMargin.mBottom,
    eCSSType_Value,
    nsnull,
    offsetof(nsStyleMargin, mMargin),
    eStyleAnimType_Sides_Bottom)
CSS_PROP_SHORTHAND(
    -moz-margin-end,
    margin_end,
    MozMarginEnd,
    0)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_MARGIN(
    margin-end-value,
    margin_end_value,
    X,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER,
    Margin,
    mMarginEnd,
    eCSSType_Value,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
#endif
CSS_PROP_SHORTHAND(
    margin-left,
    margin_left,
    MarginLeft,
    0)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_MARGIN(
    margin-left-value,
    margin_left_value,
    X,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER,
    Margin,
    mMargin.mLeft,
    eCSSType_Value,
    nsnull,
    offsetof(nsStyleMargin, mMargin),
    eStyleAnimType_Sides_Left)
CSS_PROP_MARGIN(
    margin-left-ltr-source,
    margin_left_ltr_source,
    X,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_DIRECTIONAL_SOURCE,
    Margin,
    mMarginLeftLTRSource,
    eCSSType_Value,
    kBoxPropSourceKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_MARGIN(
    margin-left-rtl-source,
    margin_left_rtl_source,
    X,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_DIRECTIONAL_SOURCE,
    Margin,
    mMarginLeftRTLSource,
    eCSSType_Value,
    kBoxPropSourceKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
#endif
CSS_PROP_SHORTHAND(
    margin-right,
    margin_right,
    MarginRight,
    0)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_MARGIN(
    margin-right-value,
    margin_right_value,
    X,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER,
    Margin,
    mMargin.mRight,
    eCSSType_Value,
    nsnull,
    offsetof(nsStyleMargin, mMargin),
    eStyleAnimType_Sides_Right)
CSS_PROP_MARGIN(
    margin-right-ltr-source,
    margin_right_ltr_source,
    X,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_DIRECTIONAL_SOURCE,
    Margin,
    mMarginRightLTRSource,
    eCSSType_Value,
    kBoxPropSourceKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_MARGIN(
    margin-right-rtl-source,
    margin_right_rtl_source,
    X,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_DIRECTIONAL_SOURCE,
    Margin,
    mMarginRightRTLSource,
    eCSSType_Value,
    kBoxPropSourceKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
#endif
CSS_PROP_SHORTHAND(
    -moz-margin-start,
    margin_start,
    MozMarginStart,
    0)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_MARGIN(
    margin-start-value,
    margin_start_value,
    X,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER,
    Margin,
    mMarginStart,
    eCSSType_Value,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
#endif
CSS_PROP_MARGIN(
    margin-top,
    margin_top,
    MarginTop,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER,
    Margin,
    mMargin.mTop,
    eCSSType_Value,
    nsnull,
    offsetof(nsStyleMargin, mMargin),
    eStyleAnimType_Sides_Top)
CSS_PROP_CONTENT(
    marker-offset,
    marker_offset,
    MarkerOffset,
    0,
    Content,
    mMarkerOffset,
    eCSSType_Value,
    nsnull,
    offsetof(nsStyleContent, mMarkerOffset),
    eStyleAnimType_Coord)
CSS_PROP_BACKENDONLY(
    marks,
    marks,
    Marks,
    0,
    Page,
    mMarks,
    eCSSType_Value,
    kPageMarksKTable)
CSS_PROP_POSITION(
    max-height,
    max_height,
    MaxHeight,
    0,
    Position,
    mMaxHeight,
    eCSSType_Value,
    nsnull,
    offsetof(nsStylePosition, mMaxHeight),
    eStyleAnimType_Coord)
CSS_PROP_POSITION(
    max-width,
    max_width,
    MaxWidth,
    0,
    Position,
    mMaxWidth,
    eCSSType_Value,
    kWidthKTable,
    offsetof(nsStylePosition, mMaxWidth),
    eStyleAnimType_Coord)
CSS_PROP_POSITION(
    min-height,
    min_height,
    MinHeight,
    0,
    Position,
    mMinHeight,
    eCSSType_Value,
    nsnull,
    offsetof(nsStylePosition, mMinHeight),
    eStyleAnimType_Coord)
CSS_PROP_POSITION(
    min-width,
    min_width,
    MinWidth,
    0,
    Position,
    mMinWidth,
    eCSSType_Value,
    kWidthKTable,
    offsetof(nsStylePosition, mMinWidth),
    eStyleAnimType_Coord)
CSS_PROP_DISPLAY(
    opacity,
    opacity,
    Opacity,
    0,
    Display,
    mOpacity,
    eCSSType_Value,
    nsnull,
    offsetof(nsStyleDisplay, mOpacity),
    eStyleAnimType_float) // XXX bug 3935
CSS_PROP_BACKENDONLY(
    orphans,
    orphans,
    Orphans,
    0,
    Breaks,
    mOrphans,
    eCSSType_Value,
    nsnull)
CSS_PROP_SHORTHAND(
    outline,
    outline,
    Outline,
    0)
CSS_PROP_OUTLINE(
    outline-color,
    outline_color,
    OutlineColor,
    CSS_PROPERTY_IGNORED_WHEN_COLORS_DISABLED,
    Margin,
    mOutlineColor,
    eCSSType_Value,
    kOutlineColorKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_Custom)
CSS_PROP_OUTLINE(
    outline-style,
    outline_style,
    OutlineStyle,
    0,
    Margin,
    mOutlineStyle,
    eCSSType_Value,
    kBorderStyleKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_OUTLINE(
    outline-width,
    outline_width,
    OutlineWidth,
    0,
    Margin,
    mOutlineWidth,
    eCSSType_Value,
    kBorderWidthKTable,
    offsetof(nsStyleOutline, mOutlineWidth),
    eStyleAnimType_Coord)
CSS_PROP_OUTLINE(
    outline-offset,
    outline_offset,
    OutlineOffset,
    0,
    Margin,
    mOutlineOffset,
    eCSSType_Value,
    nsnull,
    offsetof(nsStyleOutline, mOutlineOffset),
    eStyleAnimType_nscoord)
CSS_PROP_SHORTHAND(
    overflow,
    overflow,
    Overflow,
    0)
CSS_PROP_DISPLAY(
    overflow-x,
    overflow_x,
    OverflowX,
    0,
    Display,
    mOverflowX,
    eCSSType_Value,
    kOverflowSubKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_DISPLAY(
    overflow-y,
    overflow_y,
    OverflowY,
    0,
    Display,
    mOverflowY,
    eCSSType_Value,
    kOverflowSubKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_SHORTHAND(
    padding,
    padding,
    Padding,
    0)
CSS_PROP_PADDING(
    padding-bottom,
    padding_bottom,
    PaddingBottom,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER,
    Margin,
    mPadding.mBottom,
    eCSSType_Value,
    nsnull,
    offsetof(nsStylePadding, mPadding),
    eStyleAnimType_Sides_Bottom)
CSS_PROP_SHORTHAND(
    -moz-padding-end,
    padding_end,
    MozPaddingEnd,
    0)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_PADDING(
    padding-end-value,
    padding_end_value,
    X,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER,
    Margin,
    mPaddingEnd,
    eCSSType_Value,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
#endif
CSS_PROP_SHORTHAND(
    padding-left,
    padding_left,
    PaddingLeft,
    0)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_PADDING(
    padding-left-value,
    padding_left_value,
    X,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER,
    Margin,
    mPadding.mLeft,
    eCSSType_Value,
    nsnull,
    offsetof(nsStylePadding, mPadding),
    eStyleAnimType_Sides_Left)
CSS_PROP_PADDING(
    padding-left-ltr-source,
    padding_left_ltr_source,
    X,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_DIRECTIONAL_SOURCE,
    Margin,
    mPaddingLeftLTRSource,
    eCSSType_Value,
    kBoxPropSourceKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_PADDING(
    padding-left-rtl-source,
    padding_left_rtl_source,
    X,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_DIRECTIONAL_SOURCE,
    Margin,
    mPaddingLeftRTLSource,
    eCSSType_Value,
    kBoxPropSourceKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
#endif
CSS_PROP_SHORTHAND(
    padding-right,
    padding_right,
    PaddingRight,
    0)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_PADDING(
    padding-right-value,
    padding_right_value,
    X,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER,
    Margin,
    mPadding.mRight,
    eCSSType_Value,
    nsnull,
    offsetof(nsStylePadding, mPadding),
    eStyleAnimType_Sides_Right)
CSS_PROP_PADDING(
    padding-right-ltr-source,
    padding_right_ltr_source,
    X,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_DIRECTIONAL_SOURCE,
    Margin,
    mPaddingRightLTRSource,
    eCSSType_Value,
    kBoxPropSourceKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_PADDING(
    padding-right-rtl-source,
    padding_right_rtl_source,
    X,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_DIRECTIONAL_SOURCE,
    Margin,
    mPaddingRightRTLSource,
    eCSSType_Value,
    kBoxPropSourceKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
#endif
CSS_PROP_SHORTHAND(
    -moz-padding-start,
    padding_start,
    MozPaddingStart,
    0)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_PADDING(
    padding-start-value,
    padding_start_value,
    X,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER,
    Margin,
    mPaddingStart,
    eCSSType_Value,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
#endif
CSS_PROP_PADDING(
    padding-top,
    padding_top,
    PaddingTop,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER,
    Margin,
    mPadding.mTop,
    eCSSType_Value,
    nsnull,
    offsetof(nsStylePadding, mPadding),
    eStyleAnimType_Sides_Top)
CSS_PROP_BACKENDONLY(
    page,
    page,
    Page,
    0,
    Breaks,
    mPage,
    eCSSType_Value,
    nsnull)
CSS_PROP_DISPLAY(
    page-break-after,
    page_break_after,
    PageBreakAfter,
    0,
    Display,
    mBreakAfter,
    eCSSType_Value,
    kPageBreakKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None) // temp fix for bug 24000
CSS_PROP_DISPLAY(
    page-break-before,
    page_break_before,
    PageBreakBefore,
    0,
    Display,
    mBreakBefore,
    eCSSType_Value,
    kPageBreakKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None) // temp fix for bug 24000
CSS_PROP_BACKENDONLY(
    page-break-inside,
    page_break_inside,
    PageBreakInside,
    0,
    Breaks,
    mPageBreakInside,
    eCSSType_Value,
    kPageBreakInsideKTable)
CSS_PROP_SHORTHAND(
    pause,
    pause,
    Pause,
    0)
CSS_PROP_BACKENDONLY(
    pause-after,
    pause_after,
    PauseAfter,
    0,
    Aural,
    mPauseAfter,
    eCSSType_Value,
    nsnull)
CSS_PROP_BACKENDONLY(
    pause-before,
    pause_before,
    PauseBefore,
    0,
    Aural,
    mPauseBefore,
    eCSSType_Value,
    nsnull)
CSS_PROP_BACKENDONLY(
    pitch,
    pitch,
    Pitch,
    0,
    Aural,
    mPitch,
    eCSSType_Value,
    kPitchKTable)
CSS_PROP_BACKENDONLY(
    pitch-range,
    pitch_range,
    PitchRange,
    0,
    Aural,
    mPitchRange,
    eCSSType_Value,
    nsnull)
CSS_PROP_VISIBILITY(
    pointer-events,
    pointer_events,
    PointerEvents,
    0,
    Display,
    mPointerEvents,
    eCSSType_Value,
    kPointerEventsKTable,
    offsetof(nsStyleVisibility, mPointerEvents),
    eStyleAnimType_EnumU8)
CSS_PROP_DISPLAY(
    position,
    position,
    Position,
    0,
    Display,
    mPosition,
    eCSSType_Value,
    kPositionKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_QUOTES(
    quotes,
    quotes,
    Quotes,
    0,
    Content,
    mQuotes,
    eCSSType_ValuePairList,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_BACKENDONLY(
    richness,
    richness,
    Richness,
    0,
    Aural,
    mRichness,
    eCSSType_Value,
    nsnull)
CSS_PROP_POSITION(
    right,
    right,
    Right,
    0,
    Position,
    mOffset.mRight,
    eCSSType_Value,
    nsnull,
    offsetof(nsStylePosition, mOffset),
    eStyleAnimType_Sides_Right)
CSS_PROP_BACKENDONLY(
    size,
    size,
    Size,
    0,
    Page,
    mSize,
    eCSSType_ValuePair,
    kPageSizeKTable)
CSS_PROP_BACKENDONLY(
    speak,
    speak,
    Speak,
    0,
    Aural,
    mSpeak,
    eCSSType_Value,
    kSpeakKTable)
CSS_PROP_BACKENDONLY(
    speak-header,
    speak_header,
    SpeakHeader,
    0,
    Aural,
    mSpeakHeader,
    eCSSType_Value,
    kSpeakHeaderKTable)
CSS_PROP_BACKENDONLY(
    speak-numeral,
    speak_numeral,
    SpeakNumeral,
    0,
    Aural,
    mSpeakNumeral,
    eCSSType_Value,
    kSpeakNumeralKTable)
CSS_PROP_BACKENDONLY(
    speak-punctuation,
    speak_punctuation,
    SpeakPunctuation,
    0,
    Aural,
    mSpeakPunctuation,
    eCSSType_Value,
    kSpeakPunctuationKTable)
CSS_PROP_BACKENDONLY(
    speech-rate,
    speech_rate,
    SpeechRate,
    0,
    Aural,
    mSpeechRate,
    eCSSType_Value,
    kSpeechRateKTable)
CSS_PROP_BACKENDONLY(
    stress,
    stress,
    Stress,
    0,
    Aural,
    mStress,
    eCSSType_Value,
    nsnull)
CSS_PROP_TABLE(
    table-layout,
    table_layout,
    TableLayout,
    0,
    Table,
    mLayout,
    eCSSType_Value,
    kTableLayoutKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_TEXT(
    text-align,
    text_align,
    TextAlign,
    0,
    Text,
    mTextAlign,
    eCSSType_Value,
    kTextAlignKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_TEXTRESET(
    text-decoration,
    text_decoration,
    TextDecoration,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE,
    Text,
    mDecoration,
    eCSSType_Value,
    kTextDecorationKTable,
    offsetof(nsStyleTextReset, mTextDecoration),
    eStyleAnimType_EnumU8)
CSS_PROP_TEXT(
    text-indent,
    text_indent,
    TextIndent,
    0,
    Text,
    mTextIndent,
    eCSSType_Value,
    nsnull,
    offsetof(nsStyleText, mTextIndent),
    eStyleAnimType_Coord)
CSS_PROP_TEXT(
    text-shadow,
    text_shadow,
    TextShadow,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE |
        CSS_PROPERTY_VALUE_LIST_USES_COMMAS |
        CSS_PROPERTY_IGNORED_WHEN_COLORS_DISABLED,
    Text,
    mTextShadow,
    eCSSType_ValueList,
    nsnull,
    offsetof(nsStyleText, mTextShadow),
    eStyleAnimType_Shadow)
CSS_PROP_TEXT(
    text-transform,
    text_transform,
    TextTransform,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE,
    Text,
    mTextTransform,
    eCSSType_Value,
    kTextTransformKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_DISPLAY(
    -moz-transform,
    _moz_transform,
    MozTransform,
    0,
    Display,
    mTransform,
    eCSSType_ValueList,
    kDisplayKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_DISPLAY(
    -moz-transform-origin,
    _moz_transform_origin,
    MozTransformOrigin,
    0,
    Display,
    mTransformOrigin,
    eCSSType_ValuePair,
    kBackgroundPositionKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_Custom)
CSS_PROP_POSITION(
    top,
    top,
    Top,
    0,
    Position,
    mOffset.mTop,
    eCSSType_Value,
    nsnull,
    offsetof(nsStylePosition, mOffset),
    eStyleAnimType_Sides_Top)
CSS_PROP_SHORTHAND(
    -moz-transition,
    transition,
    MozTransition,
    0)
CSS_PROP_DISPLAY(
    -moz-transition-delay,
    transition_delay,
    MozTransitionDelay,
    CSS_PROPERTY_VALUE_LIST_USES_COMMAS,
    Display,
    mTransitionDelay,
    eCSSType_ValueList,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_DISPLAY(
    -moz-transition-duration,
    transition_duration,
    MozTransitionDuration,
    CSS_PROPERTY_VALUE_LIST_USES_COMMAS,
    Display,
    mTransitionDuration,
    eCSSType_ValueList,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_DISPLAY(
    -moz-transition-property,
    transition_property,
    MozTransitionProperty,
    CSS_PROPERTY_VALUE_LIST_USES_COMMAS,
    Display,
    mTransitionProperty,
    eCSSType_ValueList /* list of CSS properties that have transitions ? */,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_DISPLAY(
    -moz-transition-timing-function,
    transition_timing_function,
    MozTransitionTimingFunction,
    CSS_PROPERTY_VALUE_LIST_USES_COMMAS,
    Display,
    mTransitionTimingFunction,
    eCSSType_ValueList,
    kTransitionTimingFunctionKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_TEXTRESET(
    unicode-bidi,
    unicode_bidi,
    UnicodeBidi,
    0,
    Text,
    mUnicodeBidi,
    eCSSType_Value,
    kUnicodeBidiKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_USERINTERFACE(
    -moz-user-focus,
    user_focus,
    MozUserFocus,
    0,
    UserInterface,
    mUserFocus,
    eCSSType_Value,
    kUserFocusKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None) // XXX bug 3935
CSS_PROP_USERINTERFACE(
    -moz-user-input,
    user_input,
    MozUserInput,
    0,
    UserInterface,
    mUserInput,
    eCSSType_Value,
    kUserInputKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None) // XXX ??? // XXX bug 3935
CSS_PROP_USERINTERFACE(
    -moz-user-modify,
    user_modify,
    MozUserModify,
    0,
    UserInterface,
    mUserModify,
    eCSSType_Value,
    kUserModifyKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None) // XXX bug 3935
CSS_PROP_UIRESET(
    -moz-user-select,
    user_select,
    MozUserSelect,
    0,
    UserInterface,
    mUserSelect,
    eCSSType_Value,
    kUserSelectKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None) // XXX bug 3935
// NOTE: vertical-align is only supposed to apply to :first-letter when
// 'float' is 'none', but we don't worry about that since it has no
// effect otherwise
CSS_PROP_TEXTRESET(
    vertical-align,
    vertical_align,
    VerticalAlign,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE,
    Text,
    mVerticalAlign,
    eCSSType_Value,
    kVerticalAlignKTable,
    offsetof(nsStyleTextReset, mVerticalAlign),
    eStyleAnimType_Coord)
CSS_PROP_VISIBILITY(
    visibility,
    visibility,
    Visibility,
    0,
    Display,
    mVisibility,
    eCSSType_Value,
    kVisibilityKTable,
    offsetof(nsStyleVisibility, mVisible),
    eStyleAnimType_EnumU8)  // reflow for collapse
CSS_PROP_BACKENDONLY(
    voice-family,
    voice_family,
    VoiceFamily,
    0,
    Aural,
    mVoiceFamily,
    eCSSType_Value,
    nsnull)
CSS_PROP_BACKENDONLY(
    volume,
    volume,
    Volume,
    0,
    Aural,
    mVolume,
    eCSSType_Value,
    kVolumeKTable)
CSS_PROP_TEXT(
    white-space,
    white_space,
    WhiteSpace,
    0,
    Text,
    mWhiteSpace,
    eCSSType_Value,
    kWhitespaceKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_BACKENDONLY(
    widows,
    widows,
    Widows,
    0,
    Breaks,
    mWidows,
    eCSSType_Value,
    nsnull)
CSS_PROP_POSITION(
    width,
    width,
    Width,
    0,
    Position,
    mWidth,
    eCSSType_Value,
    kWidthKTable,
    offsetof(nsStylePosition, mWidth),
    eStyleAnimType_Coord)
CSS_PROP_UIRESET(
    -moz-window-shadow,
    _moz_window_shadow,
    MozWindowShadow,
    0,
    UserInterface,
    mWindowShadow,
    eCSSType_Value,
    kWindowShadowKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_TEXT(
    word-spacing,
    word_spacing,
    WordSpacing,
    CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE,
    Text,
    mWordSpacing,
    eCSSType_Value,
    nsnull,
    offsetof(nsStyleText, mWordSpacing),
    eStyleAnimType_nscoord)
CSS_PROP_TEXT(
    word-wrap,
    word_wrap,
    WordWrap,
    0,
    Text,
    mWordWrap,
    eCSSType_Value,
    kWordwrapKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_POSITION(
    z-index,
    z_index,
    ZIndex,
    0,
    Position,
    mZIndex,
    eCSSType_Value,
    nsnull,
    offsetof(nsStylePosition, mZIndex),
    eStyleAnimType_Coord)
CSS_PROP_XUL(
    -moz-box-align,
    box_align,
    MozBoxAlign,
    0,
    XUL,
    mBoxAlign,
    eCSSType_Value,
    kBoxAlignKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None) // XXX bug 3935
CSS_PROP_XUL(
    -moz-box-direction,
    box_direction,
    MozBoxDirection,
    0,
    XUL,
    mBoxDirection,
    eCSSType_Value,
    kBoxDirectionKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None) // XXX bug 3935
CSS_PROP_XUL(
    -moz-box-flex,
    box_flex,
    MozBoxFlex,
    0,
    XUL,
    mBoxFlex,
    eCSSType_Value,
    nsnull,
    offsetof(nsStyleXUL, mBoxFlex),
    eStyleAnimType_float) // XXX bug 3935
CSS_PROP_XUL(
    -moz-box-orient,
    box_orient,
    MozBoxOrient,
    0,
    XUL,
    mBoxOrient,
    eCSSType_Value,
    kBoxOrientKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None) // XXX bug 3935
CSS_PROP_XUL(
    -moz-box-pack,
    box_pack,
    MozBoxPack,
    0,
    XUL,
    mBoxPack,
    eCSSType_Value,
    kBoxPackKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None) // XXX bug 3935
CSS_PROP_XUL(
    -moz-box-ordinal-group,
    box_ordinal_group,
    MozBoxOrdinalGroup,
    0,
    XUL,
    mBoxOrdinal,
    eCSSType_Value,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_XUL(
    -moz-stack-sizing,
    stack_sizing,
    MozStackSizing,
    0,
    XUL,
    mStackSizing,
    eCSSType_Value,
    kStackSizingKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)

#ifdef MOZ_MATHML
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_FONT(
    -moz-script-level,
    script_level,
    ScriptLevel,
    0,
    Font,
    mScriptLevel,
    eCSSType_Value,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_FONT(
    -moz-script-size-multiplier,
    script_size_multiplier,
    ScriptSizeMultiplier,
    0,
    Font,
    mScriptSizeMultiplier,
    eCSSType_Value,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_FONT(
    -moz-script-min-size,
    script_min_size,
    ScriptMinSize,
    0,
    Font,
    mScriptMinSize,
    eCSSType_Value,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
#endif
#endif

// XXX treat SVG's CSS Properties as internal for now.
// Do we want to create an nsIDOMSVGCSS2Properties interface?
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_SVGRESET(
    clip-path,
    clip_path,
    ClipPath,
    0,
    SVG,
    mClipPath,
    eCSSType_Value,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_SVG(
    clip-rule,
    clip_rule,
    ClipRule,
    0,
    SVG,
    mClipRule,
    eCSSType_Value,
    kFillRuleKTable,
    offsetof(nsStyleSVG, mClipRule),
    eStyleAnimType_EnumU8)
CSS_PROP_SVG(
    color-interpolation,
    color_interpolation,
    ColorInterpolation,
    0,
    SVG,
    mColorInterpolation,
    eCSSType_Value,
    kColorInterpolationKTable,
    offsetof(nsStyleSVG, mColorInterpolation),
    eStyleAnimType_EnumU8)
CSS_PROP_SVG(
    color-interpolation-filters,
    color_interpolation_filters,
    ColorInterpolationFilters,
    0,
    SVG,
    mColorInterpolationFilters,
    eCSSType_Value,
    kColorInterpolationKTable,
    offsetof(nsStyleSVG, mColorInterpolationFilters),
    eStyleAnimType_EnumU8)
CSS_PROP_SVGRESET(
    dominant-baseline,
    dominant_baseline,
    DominantBaseline,
    0,
    SVG,
    mDominantBaseline,
    eCSSType_Value,
    kDominantBaselineKTable,
    offsetof(nsStyleSVGReset, mDominantBaseline),
    eStyleAnimType_EnumU8)
CSS_PROP_SVG(
    fill,
    fill,
    Fill,
    0,
    SVG,
    mFill,
    eCSSType_ValuePair,
    nsnull,
    offsetof(nsStyleSVG, mFill),
    eStyleAnimType_PaintServer)
CSS_PROP_SVG(
    fill-opacity,
    fill_opacity,
    FillOpacity,
    0,
    SVG,
    mFillOpacity,
    eCSSType_Value,
    nsnull,
    offsetof(nsStyleSVG, mFillOpacity),
    eStyleAnimType_float)
CSS_PROP_SVG(
    fill-rule,
    fill_rule,
    FillRule,
    0,
    SVG,
    mFillRule,
    eCSSType_Value,
    kFillRuleKTable,
    offsetof(nsStyleSVG, mFillRule),
    eStyleAnimType_EnumU8)
CSS_PROP_SVGRESET(
    filter,
    filter,
    Filter,
    0,
    SVG,
    mFilter,
    eCSSType_Value,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_SVGRESET(
    flood-color,
    flood_color,
    FloodColor,
    0,
    SVG,
    mFloodColor,
    eCSSType_Value,
    nsnull,
    offsetof(nsStyleSVGReset, mFloodColor),
    eStyleAnimType_Color)
CSS_PROP_SVGRESET(
    flood-opacity,
    flood_opacity,
    FloodOpacity,
    0,
    SVG,
    mFloodOpacity,
    eCSSType_Value,
    nsnull,
    offsetof(nsStyleSVGReset, mFloodOpacity),
    eStyleAnimType_float)
CSS_PROP_SVG(
    image-rendering,
    image_rendering,
    ImageRendering,
    0,
    SVG,
    mImageRendering,
    eCSSType_Value,
    kImageRenderingKTable,
    offsetof(nsStyleSVG, mImageRendering),
    eStyleAnimType_EnumU8)
CSS_PROP_SVGRESET(
    lighting-color,
    lighting_color,
    LightingColor,
    0,
    SVG,
    mLightingColor,
    eCSSType_Value,
    nsnull,
    offsetof(nsStyleSVGReset, mLightingColor),
    eStyleAnimType_Color)
CSS_PROP_SHORTHAND(
    marker,
    marker,
    Marker,
    0)
CSS_PROP_SVG(
    marker-end,
    marker_end,
    MarkerEnd,
    0,
    SVG,
    mMarkerEnd,
    eCSSType_Value,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_SVG(
    marker-mid,
    marker_mid,
    MarkerMid,
    0,
    SVG,
    mMarkerMid,
    eCSSType_Value,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_SVG(
    marker-start,
    marker_start,
    MarkerStart,
    0,
    SVG,
    mMarkerStart,
    eCSSType_Value,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_SVGRESET(
    mask,
    mask,
    Mask,
    0,
    SVG,
    mMask,
    eCSSType_Value,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_SVG(
    shape-rendering,
    shape_rendering,
    ShapeRendering,
    0,
    SVG,
    mShapeRendering,
    eCSSType_Value,
    kShapeRenderingKTable,
    offsetof(nsStyleSVG, mShapeRendering),
    eStyleAnimType_EnumU8)
CSS_PROP_SVGRESET(
    stop-color,
    stop_color,
    StopColor,
    0,
    SVG,
    mStopColor,
    eCSSType_Value,
    nsnull,
    offsetof(nsStyleSVGReset, mStopColor),
    eStyleAnimType_Color)
CSS_PROP_SVGRESET(
    stop-opacity,
    stop_opacity,
    StopOpacity,
    0,
    SVG,
    mStopOpacity,
    eCSSType_Value,
    nsnull,
    offsetof(nsStyleSVGReset, mStopOpacity),
    eStyleAnimType_float)
CSS_PROP_SVG(
    stroke,
    stroke,
    Stroke,
    0,
    SVG,
    mStroke,
    eCSSType_ValuePair,
    nsnull,
    offsetof(nsStyleSVG, mStroke),
    eStyleAnimType_PaintServer)
CSS_PROP_SVG(
    stroke-dasharray,
    stroke_dasharray,
    StrokeDasharray,
    CSS_PROPERTY_VALUE_LIST_USES_COMMAS,
    SVG,
    mStrokeDasharray,
    eCSSType_ValueList,
    nsnull,
    CSS_PROP_NO_OFFSET, /* property stored in 2 separate members */
    eStyleAnimType_Custom)
CSS_PROP_SVG(
    stroke-dashoffset,
    stroke_dashoffset,
    StrokeDashoffset,
    0,
    SVG,
    mStrokeDashoffset,
    eCSSType_Value,
    nsnull,
    offsetof(nsStyleSVG, mStrokeDashoffset),
    eStyleAnimType_Coord)
CSS_PROP_SVG(
    stroke-linecap,
    stroke_linecap,
    StrokeLinecap,
    0,
    SVG,
    mStrokeLinecap,
    eCSSType_Value,
    kStrokeLinecapKTable,
    offsetof(nsStyleSVG, mStrokeLinecap),
    eStyleAnimType_EnumU8)
CSS_PROP_SVG(
    stroke-linejoin,
    stroke_linejoin,
    StrokeLinejoin,
    0,
    SVG,
    mStrokeLinejoin,
    eCSSType_Value,
    kStrokeLinejoinKTable,
    offsetof(nsStyleSVG, mStrokeLinejoin),
    eStyleAnimType_EnumU8)
CSS_PROP_SVG(
    stroke-miterlimit,
    stroke_miterlimit,
    StrokeMiterlimit,
    0,
    SVG,
    mStrokeMiterlimit,
    eCSSType_Value,
    nsnull,
    offsetof(nsStyleSVG, mStrokeMiterlimit),
    eStyleAnimType_float)
CSS_PROP_SVG(
    stroke-opacity,
    stroke_opacity,
    StrokeOpacity,
    0,
    SVG,
    mStrokeOpacity,
    eCSSType_Value,
    nsnull,
    offsetof(nsStyleSVG, mStrokeOpacity),
    eStyleAnimType_float)
CSS_PROP_SVG(
    stroke-width,
    stroke_width,
    StrokeWidth,
    0,
    SVG,
    mStrokeWidth,
    eCSSType_Value,
    nsnull,
    offsetof(nsStyleSVG, mStrokeWidth),
    eStyleAnimType_Coord)
CSS_PROP_SVG(
    text-anchor,
    text_anchor,
    TextAnchor,
    0,
    SVG,
    mTextAnchor,
    eCSSType_Value,
    kTextAnchorKTable,
    offsetof(nsStyleSVG, mTextAnchor),
    eStyleAnimType_EnumU8)
CSS_PROP_SVG(
    text-rendering,
    text_rendering,
    TextRendering,
    0,
    SVG,
    mTextRendering,
    eCSSType_Value,
    kTextRenderingKTable,
    offsetof(nsStyleSVG, mTextRendering),
    eStyleAnimType_EnumU8)
#endif /* !defined (CSS_PROP_LIST_EXCLUDE_INTERNAL) */

// Callers that want information on the properties that are in
// the style structs but not in the nsCSS* structs should define
// |CSS_PROP_INCLUDE_NOT_CSS|.  (Some of these are also in nsRuleData*,
// and a distinction might be needed at some point.)
// The first 3 parameters don't matter, but some compilers don't like
// empty arguments to macros.
#ifdef CSS_PROP_INCLUDE_NOT_CSS
CSS_PROP_VISIBILITY(
    X,
    X,
    X,
    0,
    Display,
    mLang,
    eCSSType_Value,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_TABLE(
    X,
    X,
    X,
    0,
    Table,
    mFrame,
    eCSSType_Value,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_TABLE(
    X,
    X,
    X,
    0,
    Table,
    mRules,
    eCSSType_Value,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_TABLE(
    X,
    X,
    X,
    0,
    Table,
    mCols,
    eCSSType_Value,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_TABLE(
    X,
    X,
    X,
    0,
    Table,
    mSpan,
    eCSSType_Value,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
#endif /* defined(CSS_PROP_INCLUDE_NOT_CSS) */

#ifdef USED_CSS_PROP

#undef USED_CSS_PROP
#undef CSS_PROP_FONT
#undef CSS_PROP_COLOR
#undef CSS_PROP_BACKGROUND
#undef CSS_PROP_LIST
#undef CSS_PROP_POSITION
#undef CSS_PROP_TEXT
#undef CSS_PROP_TEXTRESET
#undef CSS_PROP_DISPLAY
#undef CSS_PROP_VISIBILITY
#undef CSS_PROP_CONTENT
#undef CSS_PROP_QUOTES
#undef CSS_PROP_USERINTERFACE
#undef CSS_PROP_UIRESET
#undef CSS_PROP_TABLE
#undef CSS_PROP_TABLEBORDER
#undef CSS_PROP_MARGIN
#undef CSS_PROP_PADDING
#undef CSS_PROP_BORDER
#undef CSS_PROP_OUTLINE
#undef CSS_PROP_XUL
#undef CSS_PROP_COLUMN
#undef CSS_PROP_SVG
#undef CSS_PROP_SVGRESET
#ifdef DEFINED_CSS_PROP_BACKENDONLY
#undef CSS_PROP_BACKENDONLY
#undef DEFINED_CSS_PROP_BACKENDONLY
#endif

#else /* !defined(USED_CSS_PROP) */

#ifdef DEFINED_CSS_PROP_FONT
#undef CSS_PROP_FONT
#undef DEFINED_CSS_PROP_FONT
#endif
#ifdef DEFINED_CSS_PROP_COLOR
#undef CSS_PROP_COLOR
#undef DEFINED_CSS_PROP_COLOR
#endif
#ifdef DEFINED_CSS_PROP_BACKGROUND
#undef CSS_PROP_BACKGROUND
#undef DEFINED_CSS_PROP_BACKGROUND
#endif
#ifdef DEFINED_CSS_PROP_LIST
#undef CSS_PROP_LIST
#undef DEFINED_CSS_PROP_LIST
#endif
#ifdef DEFINED_CSS_PROP_POSITION
#undef CSS_PROP_POSITION
#undef DEFINED_CSS_PROP_POSITION
#endif
#ifdef DEFINED_CSS_PROP_TEXT
#undef CSS_PROP_TEXT
#undef DEFINED_CSS_PROP_TETEXTRESETT
#endif
#ifdef DEFINED_CSS_PROP_TEXTRESET
#undef CSS_PROP_TEXTRESET
#undef DEFINED_CSS_PROP_TEDISPLAYTRESET
#endif
#ifdef DEFINED_CSS_PROP_DISPLAY
#undef CSS_PROP_DISPLAY
#undef DEFINED_CSS_PROP_DISPLAY
#endif
#ifdef DEFINED_CSS_PROP_VISIBILITY
#undef CSS_PROP_VISIBILITY
#undef DEFINED_CSS_PROP_VISIBILITY
#endif
#ifdef DEFINED_CSS_PROP_CONTENT
#undef CSS_PROP_CONTENT
#undef DEFINED_CSS_PROP_CONTENT
#endif
#ifdef DEFINED_CSS_PROP_QUOTES
#undef CSS_PROP_QUOTES
#undef DEFINED_CSS_PROP_QUOTES
#endif
#ifdef DEFINED_CSS_PROP_USERINTERFACE
#undef CSS_PROP_USERINTERFACE
#undef DEFINED_CSS_PROP_USERINTERFACE
#endif
#ifdef DEFINED_CSS_PROP_UIRESET
#undef CSS_PROP_UIRESET
#undef DEFINED_CSS_PROP_UIRESET
#endif
#ifdef DEFINED_CSS_PROP_TABLE
#undef CSS_PROP_TABLE
#undef DEFINED_CSS_PROP_TABLE
#endif
#ifdef DEFINED_CSS_PROP_TABLEBORDER
#undef CSS_PROP_TABLEBORDER
#undef DEFINED_CSS_PROP_TABLEBORDER
#endif
#ifdef DEFINED_CSS_PROP_MARGIN
#undef CSS_PROP_MARGIN
#undef DEFINED_CSS_PROP_MARGIN
#endif
#ifdef DEFINED_CSS_PROP_PADDING
#undef CSS_PROP_PADDING
#undef DEFINED_CSS_PROP_PADDING
#endif
#ifdef DEFINED_CSS_PROP_BORDER
#undef CSS_PROP_BORDER
#undef DEFINED_CSS_PROP_BORDER
#endif
#ifdef DEFINED_CSS_PROP_OUTLINE
#undef CSS_PROP_OUTLINE
#undef DEFINED_CSS_PROP_OUTLINE
#endif
#ifdef DEFINED_CSS_PROP_XUL
#undef CSS_PROP_XUL
#undef DEFINED_CSS_PROP_XUL
#endif
#ifdef DEFINED_CSS_PROP_COLUMN
#undef CSS_PROP_COLUMN
#undef DEFINED_CSS_PROP_COLUMN
#endif
#ifdef DEFINED_CSS_PROP_SVG
#undef CSS_PROP_SVG
#undef DEFINED_CSS_PROP_SVG
#endif
#ifdef DEFINED_CSS_PROP_SVGRESET
#undef CSS_PROP_SVGRESET
#undef DEFINED_CSS_PROP_SVGRESET
#endif
#ifdef DEFINED_CSS_PROP_BACKENDONLY
#undef CSS_PROP_BACKENDONLY
#undef DEFINED_CSS_PROP_BACKENDONLY
#endif

#endif /* !defined(USED_CSS_PROP) */

#ifdef DEFINED_CSS_PROP_SHORTHAND
#undef CSS_PROP_SHORTHAND
#undef DEFINED_CSS_PROP_SHORTHAND
#endif
