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

  -. 'name' entries represent a CSS property name and *must* use only
  lowercase characters.

  -. 'id' should be the same as 'name' except that all hyphens ('-')
  in 'name' are converted to underscores ('_') in 'id'. This lets us
  do nice things with the macros without having to copy/convert strings
  at runtime.  These are the names used for the enum values of the
  nsCSSProperty enumeration defined in nsCSSProps.h.

  -. 'method' is designed to be as input for CSS2Properties and similar
  callers.  It must always be the same as 'name' except it must use
  InterCaps and all hyphens ('-') must be removed.

  -. 'flags', a bitfield containing CSS_PROPERTY_* flags.

  -. 'datastruct' says which nsRuleData* struct this property goes in.

  -. 'member' gives the name of the member variable in the nsRuleData
  struct.

  -. 'parsevariant', to be passed to ParseVariant in the parser.

  -. 'kwtable', which is either nsnull or the name of the appropriate
  keyword table member of class nsCSSProps, for use in
  nsCSSProps::LookupPropertyValue.

  -. 'stylestruct_' [used only for CSS_PROP, not CSS_PROP_*] gives the
  name of the style struct.  Can be used to make nsStyle##stylestruct_
  and eStyleStruct_##stylestruct_

  -. 'stylestructoffset_' [not used for CSS_PROP_BACKENDONLY] gives the
  result of offsetof(nsStyle*, member).  Ignored (and generally
  CSS_PROP_NO_OFFSET, or -1) for properties whose animtype_ is
  eStyleAnimType_None.

  -. 'animtype_' [not used for CSS_PROP_BACKENDONLY] gives the
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
#define CSS_PROP_FONT(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, stylestructoffset_, animtype_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, Font, stylestructoffset_, animtype_)
#define CSS_PROP_COLOR(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, stylestructoffset_, animtype_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, Color, stylestructoffset_, animtype_)
#define CSS_PROP_BACKGROUND(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, stylestructoffset_, animtype_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, Background, stylestructoffset_, animtype_)
#define CSS_PROP_LIST(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, stylestructoffset_, animtype_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, List, stylestructoffset_, animtype_)
#define CSS_PROP_POSITION(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, stylestructoffset_, animtype_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, Position, stylestructoffset_, animtype_)
#define CSS_PROP_TEXT(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, stylestructoffset_, animtype_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, Text, stylestructoffset_, animtype_)
#define CSS_PROP_TEXTRESET(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, stylestructoffset_, animtype_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, TextReset, stylestructoffset_, animtype_)
#define CSS_PROP_DISPLAY(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, stylestructoffset_, animtype_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, Display, stylestructoffset_, animtype_)
#define CSS_PROP_VISIBILITY(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, stylestructoffset_, animtype_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, Visibility, stylestructoffset_, animtype_)
#define CSS_PROP_CONTENT(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, stylestructoffset_, animtype_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, Content, stylestructoffset_, animtype_)
#define CSS_PROP_QUOTES(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, stylestructoffset_, animtype_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, Quotes, stylestructoffset_, animtype_)
#define CSS_PROP_USERINTERFACE(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, stylestructoffset_, animtype_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, UserInterface, stylestructoffset_, animtype_)
#define CSS_PROP_UIRESET(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, stylestructoffset_, animtype_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, UIReset, stylestructoffset_, animtype_)
#define CSS_PROP_TABLE(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, stylestructoffset_, animtype_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, Table, stylestructoffset_, animtype_)
#define CSS_PROP_TABLEBORDER(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, stylestructoffset_, animtype_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, TableBorder, stylestructoffset_, animtype_)
#define CSS_PROP_MARGIN(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, stylestructoffset_, animtype_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, Margin, stylestructoffset_, animtype_)
#define CSS_PROP_PADDING(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, stylestructoffset_, animtype_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, Padding, stylestructoffset_, animtype_)
#define CSS_PROP_BORDER(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, stylestructoffset_, animtype_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, Border, stylestructoffset_, animtype_)
#define CSS_PROP_OUTLINE(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, stylestructoffset_, animtype_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, Outline, stylestructoffset_, animtype_)
#define CSS_PROP_XUL(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, stylestructoffset_, animtype_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, XUL, stylestructoffset_, animtype_)
#define CSS_PROP_COLUMN(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, stylestructoffset_, animtype_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, Column, stylestructoffset_, animtype_)
#define CSS_PROP_SVG(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, stylestructoffset_, animtype_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, SVG, stylestructoffset_, animtype_)
#define CSS_PROP_SVGRESET(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, stylestructoffset_, animtype_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, SVGReset, stylestructoffset_, animtype_)

// For properties that are stored in the CSS backend but are not
// computed.  An includer may define this in addition to CSS_PROP, but
// otherwise we treat it as the same.
#ifndef CSS_PROP_BACKENDONLY
#define CSS_PROP_BACKENDONLY(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_) CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, BackendOnly, CSS_PROP_NO_OFFSET, eStyleAnimType_None)
#define DEFINED_CSS_PROP_BACKENDONLY
#endif

#else /* !defined(CSS_PROP) */

// An includer who does not define CSS_PROP can define any or all of the
// per-struct macros that are equivalent to it, and the rest will be
// ignored.

#ifndef CSS_PROP_FONT
#define CSS_PROP_FONT(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, stylestructoffset_, animtype_) /* nothing */
#define DEFINED_CSS_PROP_FONT
#endif
#ifndef CSS_PROP_COLOR
#define CSS_PROP_COLOR(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, stylestructoffset_, animtype_) /* nothing */
#define DEFINED_CSS_PROP_COLOR
#endif
#ifndef CSS_PROP_BACKGROUND
#define CSS_PROP_BACKGROUND(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, stylestructoffset_, animtype_) /* nothing */
#define DEFINED_CSS_PROP_BACKGROUND
#endif
#ifndef CSS_PROP_LIST
#define CSS_PROP_LIST(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, stylestructoffset_, animtype_) /* nothing */
#define DEFINED_CSS_PROP_LIST
#endif
#ifndef CSS_PROP_POSITION
#define CSS_PROP_POSITION(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, stylestructoffset_, animtype_) /* nothing */
#define DEFINED_CSS_PROP_POSITION
#endif
#ifndef CSS_PROP_TEXT
#define CSS_PROP_TEXT(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, stylestructoffset_, animtype_) /* nothing */
#define DEFINED_CSS_PROP_TEXT
#endif
#ifndef CSS_PROP_TEXTRESET
#define CSS_PROP_TEXTRESET(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, stylestructoffset_, animtype_) /* nothing */
#define DEFINED_CSS_PROP_TEXTRESET
#endif
#ifndef CSS_PROP_DISPLAY
#define CSS_PROP_DISPLAY(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, stylestructoffset_, animtype_) /* nothing */
#define DEFINED_CSS_PROP_DISPLAY
#endif
#ifndef CSS_PROP_VISIBILITY
#define CSS_PROP_VISIBILITY(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, stylestructoffset_, animtype_) /* nothing */
#define DEFINED_CSS_PROP_VISIBILITY
#endif
#ifndef CSS_PROP_CONTENT
#define CSS_PROP_CONTENT(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, stylestructoffset_, animtype_) /* nothing */
#define DEFINED_CSS_PROP_CONTENT
#endif
#ifndef CSS_PROP_QUOTES
#define CSS_PROP_QUOTES(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, stylestructoffset_, animtype_) /* nothing */
#define DEFINED_CSS_PROP_QUOTES
#endif
#ifndef CSS_PROP_USERINTERFACE
#define CSS_PROP_USERINTERFACE(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, stylestructoffset_, animtype_) /* nothing */
#define DEFINED_CSS_PROP_USERINTERFACE
#endif
#ifndef CSS_PROP_UIRESET
#define CSS_PROP_UIRESET(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, stylestructoffset_, animtype_) /* nothing */
#define DEFINED_CSS_PROP_UIRESET
#endif
#ifndef CSS_PROP_TABLE
#define CSS_PROP_TABLE(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, stylestructoffset_, animtype_) /* nothing */
#define DEFINED_CSS_PROP_TABLE
#endif
#ifndef CSS_PROP_TABLEBORDER
#define CSS_PROP_TABLEBORDER(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, stylestructoffset_, animtype_) /* nothing */
#define DEFINED_CSS_PROP_TABLEBORDER
#endif
#ifndef CSS_PROP_MARGIN
#define CSS_PROP_MARGIN(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, stylestructoffset_, animtype_) /* nothing */
#define DEFINED_CSS_PROP_MARGIN
#endif
#ifndef CSS_PROP_PADDING
#define CSS_PROP_PADDING(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, stylestructoffset_, animtype_) /* nothing */
#define DEFINED_CSS_PROP_PADDING
#endif
#ifndef CSS_PROP_BORDER
#define CSS_PROP_BORDER(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, stylestructoffset_, animtype_) /* nothing */
#define DEFINED_CSS_PROP_BORDER
#endif
#ifndef CSS_PROP_OUTLINE
#define CSS_PROP_OUTLINE(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, stylestructoffset_, animtype_) /* nothing */
#define DEFINED_CSS_PROP_OUTLINE
#endif
#ifndef CSS_PROP_XUL
#define CSS_PROP_XUL(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, stylestructoffset_, animtype_) /* nothing */
#define DEFINED_CSS_PROP_XUL
#endif
#ifndef CSS_PROP_COLUMN
#define CSS_PROP_COLUMN(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, stylestructoffset_, animtype_) /* nothing */
#define DEFINED_CSS_PROP_COLUMN
#endif
#ifndef CSS_PROP_SVG
#define CSS_PROP_SVG(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, stylestructoffset_, animtype_) /* nothing */
#define DEFINED_CSS_PROP_SVG
#endif
#ifndef CSS_PROP_SVGRESET
#define CSS_PROP_SVGRESET(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_, stylestructoffset_, animtype_) /* nothing */
#define DEFINED_CSS_PROP_SVGRESET
#endif

#ifndef CSS_PROP_BACKENDONLY
#define CSS_PROP_BACKENDONLY(name_, id_, method_, flags_, datastruct_, member_, parsevariant_, kwtable_) /* nothing */
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
// 'text-shadow' (see above) and 'box-shadow' (which is like the
// border properties).

// We include '-moz-background-inline-policy' (css3-background's
// 'background-break') in both as a background property, although this
// is somewhat questionable.

CSS_PROP_DISPLAY(
    -moz-appearance,
    appearance,
    MozAppearance,
    CSS_PROPERTY_PARSE_VALUE,
    Display,
    mAppearance,
    0,
    kAppearanceKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_SHORTHAND(
    -moz-outline-radius,
    _moz_outline_radius,
    MozOutlineRadius,
    CSS_PROPERTY_PARSE_FUNCTION)
CSS_PROP_OUTLINE(
    -moz-outline-radius-topleft,
    _moz_outline_radius_topLeft,
    MozOutlineRadiusTopleft,
    CSS_PROPERTY_PARSE_FUNCTION |
        CSS_PROPERTY_STORES_CALC,
    Margin,
    mOutlineRadius.mTopLeft,
    0,
    nsnull,
    offsetof(nsStyleOutline, mOutlineRadius),
    eStyleAnimType_Corner_TopLeft)
CSS_PROP_OUTLINE(
    -moz-outline-radius-topright,
    _moz_outline_radius_topRight,
    MozOutlineRadiusTopright,
    CSS_PROPERTY_PARSE_FUNCTION |
        CSS_PROPERTY_STORES_CALC,
    Margin,
    mOutlineRadius.mTopRight,
    0,
    nsnull,
    offsetof(nsStyleOutline, mOutlineRadius),
    eStyleAnimType_Corner_TopRight)
CSS_PROP_OUTLINE(
    -moz-outline-radius-bottomright,
    _moz_outline_radius_bottomRight,
    MozOutlineRadiusBottomright,
    CSS_PROPERTY_PARSE_FUNCTION |
        CSS_PROPERTY_STORES_CALC,
    Margin,
    mOutlineRadius.mBottomRight,
    0,
    nsnull,
    offsetof(nsStyleOutline, mOutlineRadius),
    eStyleAnimType_Corner_BottomRight)
CSS_PROP_OUTLINE(
    -moz-outline-radius-bottomleft,
    _moz_outline_radius_bottomLeft,
    MozOutlineRadiusBottomleft,
    CSS_PROPERTY_PARSE_FUNCTION |
        CSS_PROPERTY_STORES_CALC,
    Margin,
    mOutlineRadius.mBottomLeft,
    0,
    nsnull,
    offsetof(nsStyleOutline, mOutlineRadius),
    eStyleAnimType_Corner_BottomLeft)
CSS_PROP_TEXT(
    -moz-tab-size,
    _moz_tab_size,
    MozTabSize,
    CSS_PROPERTY_PARSE_VALUE,
    Text,
    mTabSize,
    0,
    nsnull,
    offsetof(nsStyleText, mTabSize),
    eStyleAnimType_None)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_FONT(
    -x-system-font,
    _x_system_font,
    MozSystemFont,
    CSS_PROPERTY_PARSE_INACCESSIBLE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE,
    Font,
    mSystemFont,
    0,
    kFontKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
#endif
CSS_PROP_BACKENDONLY(
    azimuth,
    azimuth,
    Azimuth,
    CSS_PROPERTY_PARSE_VALUE |
        CSS_PROPERTY_VALUE_PARSER_FUNCTION,
    Aural,
    mAzimuth,
    0,
    kAzimuthKTable)
CSS_PROP_SHORTHAND(
    background,
    background,
    Background,
    CSS_PROPERTY_PARSE_FUNCTION)
CSS_PROP_BACKGROUND(
    background-attachment,
    background_attachment,
    BackgroundAttachment,
    CSS_PROPERTY_PARSE_FUNCTION |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE |
        CSS_PROPERTY_VALUE_LIST_USES_COMMAS,
    Color,
    mBackAttachment,
    0,
    kBackgroundAttachmentKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_BACKGROUND(
    background-clip,
    background_clip,
    BackgroundClip,
    CSS_PROPERTY_PARSE_FUNCTION |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE |
        CSS_PROPERTY_VALUE_LIST_USES_COMMAS,
    Color,
    mBackClip,
    0,
    kBackgroundOriginKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_BACKGROUND(
    background-color,
    background_color,
    BackgroundColor,
    CSS_PROPERTY_PARSE_VALUE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE |
        CSS_PROPERTY_IGNORED_WHEN_COLORS_DISABLED,
    Color,
    mBackColor,
    0,
    nsnull,
    offsetof(nsStyleBackground, mBackgroundColor),
    eStyleAnimType_Color)
CSS_PROP_BACKGROUND(
    background-image,
    background_image,
    BackgroundImage,
    CSS_PROPERTY_PARSE_FUNCTION |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE |
        CSS_PROPERTY_VALUE_LIST_USES_COMMAS |
        CSS_PROPERTY_IGNORED_WHEN_COLORS_DISABLED |
        CSS_PROPERTY_START_IMAGE_LOADS,
    Color,
    mBackImage,
    0,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_BACKGROUND(
    -moz-background-inline-policy,
    _moz_background_inline_policy,
    MozBackgroundInlinePolicy,
    CSS_PROPERTY_PARSE_VALUE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE,
    Color,
    mBackInlinePolicy,
    0,
    kBackgroundInlinePolicyKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_BACKGROUND(
    background-origin,
    background_origin,
    BackgroundOrigin,
    CSS_PROPERTY_PARSE_FUNCTION |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE |
        CSS_PROPERTY_VALUE_LIST_USES_COMMAS,
    Color,
    mBackOrigin,
    0,
    kBackgroundOriginKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_BACKGROUND(
    background-position,
    background_position,
    BackgroundPosition,
    CSS_PROPERTY_PARSE_FUNCTION |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE |
        CSS_PROPERTY_VALUE_LIST_USES_COMMAS |
        CSS_PROPERTY_STORES_CALC,
    Color,
    mBackPosition,
    0,
    kBackgroundPositionKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_Custom)
CSS_PROP_BACKGROUND(
    background-repeat,
    background_repeat,
    BackgroundRepeat,
    CSS_PROPERTY_PARSE_FUNCTION |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE |
        CSS_PROPERTY_VALUE_LIST_USES_COMMAS,
    Color,
    mBackRepeat,
    0,
    kBackgroundRepeatKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_BACKGROUND(
    background-size,
    background_size,
    BackgroundSize,
    CSS_PROPERTY_PARSE_FUNCTION |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE |
        CSS_PROPERTY_VALUE_LIST_USES_COMMAS |
        CSS_PROPERTY_STORES_CALC,
    Color,
    mBackSize,
    0,
    kBackgroundSizeKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_Custom)
CSS_PROP_DISPLAY(
    -moz-binding,
    binding,
    MozBinding,
    CSS_PROPERTY_PARSE_VALUE,
    Display,
    mBinding,
    0,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None) // XXX bug 3935
CSS_PROP_SHORTHAND(
    border,
    border,
    Border,
    CSS_PROPERTY_PARSE_FUNCTION)
CSS_PROP_SHORTHAND(
    border-bottom,
    border_bottom,
    BorderBottom,
    CSS_PROPERTY_PARSE_FUNCTION)
CSS_PROP_BORDER(
    border-bottom-color,
    border_bottom_color,
    BorderBottomColor,
    CSS_PROPERTY_PARSE_VALUE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_IGNORED_WHEN_COLORS_DISABLED,
    Margin,
    mBorderColor.mBottom,
    0,
    kBorderColorKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_Custom)
CSS_PROP_BORDER(
    -moz-border-bottom-colors,
    border_bottom_colors,
    MozBorderBottomColors,
    CSS_PROPERTY_PARSE_FUNCTION |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_IGNORED_WHEN_COLORS_DISABLED,
    Margin,
    mBorderColors.mBottom,
    0,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_BORDER(
    border-bottom-style,
    border_bottom_style,
    BorderBottomStyle,
    CSS_PROPERTY_PARSE_VALUE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER,
    Margin,
    mBorderStyle.mBottom,
    0,
    kBorderStyleKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)  // on/off will need reflow
CSS_PROP_BORDER(
    border-bottom-width,
    border_bottom_width,
    BorderBottomWidth,
    CSS_PROPERTY_PARSE_VALUE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER,
    Margin,
    mBorderWidth.mBottom,
    0,
    kBorderWidthKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_Custom)
CSS_PROP_TABLEBORDER(
    border-collapse,
    border_collapse,
    BorderCollapse,
    CSS_PROPERTY_PARSE_VALUE,
    Table,
    mBorderCollapse,
    0,
    kBorderCollapseKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_SHORTHAND(
    border-color,
    border_color,
    BorderColor,
    CSS_PROPERTY_PARSE_FUNCTION)
CSS_PROP_SHORTHAND(
    -moz-border-end,
    border_end,
    MozBorderEnd,
    CSS_PROPERTY_PARSE_FUNCTION)
CSS_PROP_SHORTHAND(
    -moz-border-end-color,
    border_end_color,
    MozBorderEndColor,
    CSS_PROPERTY_PARSE_FUNCTION)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_BORDER(
    border-end-color-value,
    border_end_color_value,
    BorderEndColorValue,
    CSS_PROPERTY_PARSE_INACCESSIBLE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER,
    Margin,
    mBorderEndColor,
    0,
    kBorderColorKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
#endif
CSS_PROP_SHORTHAND(
    -moz-border-end-style,
    border_end_style,
    MozBorderEndStyle,
    CSS_PROPERTY_PARSE_FUNCTION)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_BORDER(
    border-end-style-value,
    border_end_style_value,
    BorderEndStyleValue,
    CSS_PROPERTY_PARSE_INACCESSIBLE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER,
    Margin,
    mBorderEndStyle,
    0,
    kBorderStyleKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
#endif
CSS_PROP_SHORTHAND(
    -moz-border-end-width,
    border_end_width,
    MozBorderEndWidth,
    CSS_PROPERTY_PARSE_FUNCTION)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_BORDER(
    border-end-width-value,
    border_end_width_value,
    BorderEndWidthValue,
    CSS_PROPERTY_PARSE_INACCESSIBLE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER,
    Margin,
    mBorderEndWidth,
    0,
    kBorderWidthKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
#endif
CSS_PROP_BORDER(
    -moz-border-image,
    border_image,
    MozBorderImage,
    CSS_PROPERTY_PARSE_FUNCTION |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_START_IMAGE_LOADS |
        CSS_PROPERTY_IMAGE_IS_IN_ARRAY_0,
    Margin,
    mBorderImage,
    0,
    kBorderImageKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_SHORTHAND(
    border-left,
    border_left,
    BorderLeft,
    CSS_PROPERTY_PARSE_FUNCTION)
CSS_PROP_SHORTHAND(
    border-left-color,
    border_left_color,
    BorderLeftColor,
    CSS_PROPERTY_PARSE_FUNCTION)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_BORDER(
    border-left-color-value,
    border_left_color_value,
    BorderLeftColorValue,
    CSS_PROPERTY_PARSE_INACCESSIBLE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_IGNORED_WHEN_COLORS_DISABLED |
        CSS_PROPERTY_REPORT_OTHER_NAME,
    Margin,
    mBorderColor.mLeft,
    0,
    kBorderColorKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_Custom)
CSS_PROP_BORDER(
    border-left-color-ltr-source,
    border_left_color_ltr_source,
    BorderLeftColorLTRSource,
    CSS_PROPERTY_PARSE_INACCESSIBLE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_DIRECTIONAL_SOURCE |
        CSS_PROPERTY_IGNORED_WHEN_COLORS_DISABLED,
    Margin,
    mBorderLeftColorLTRSource,
    0,
    kBoxPropSourceKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_BORDER(
    border-left-color-rtl-source,
    border_left_color_rtl_source,
    BorderLeftColorRTLSource,
    CSS_PROPERTY_PARSE_INACCESSIBLE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_DIRECTIONAL_SOURCE |
        CSS_PROPERTY_IGNORED_WHEN_COLORS_DISABLED,
    Margin,
    mBorderLeftColorRTLSource,
    0,
    kBoxPropSourceKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
#endif
CSS_PROP_BORDER(
    -moz-border-left-colors,
    border_left_colors,
    MozBorderLeftColors,
    CSS_PROPERTY_PARSE_FUNCTION |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_IGNORED_WHEN_COLORS_DISABLED,
    Margin,
    mBorderColors.mLeft,
    0,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_SHORTHAND(
    border-left-style,
    border_left_style,
    BorderLeftStyle,
    CSS_PROPERTY_PARSE_FUNCTION) // on/off will need reflow
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_BORDER(
    border-left-style-value,
    border_left_style_value,
    BorderLeftStyleValue,
    CSS_PROPERTY_PARSE_INACCESSIBLE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_REPORT_OTHER_NAME,
    Margin,
    mBorderStyle.mLeft,
    0,
    kBorderStyleKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_BORDER(
    border-left-style-ltr-source,
    border_left_style_ltr_source,
    BorderLeftStyleLTRSource,
    CSS_PROPERTY_PARSE_INACCESSIBLE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_DIRECTIONAL_SOURCE,
    Margin,
    mBorderLeftStyleLTRSource,
    0,
    kBoxPropSourceKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_BORDER(
    border-left-style-rtl-source,
    border_left_style_rtl_source,
    BorderLeftStyleRTLSource,
    CSS_PROPERTY_PARSE_INACCESSIBLE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_DIRECTIONAL_SOURCE,
    Margin,
    mBorderLeftStyleRTLSource,
    0,
    kBoxPropSourceKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
#endif
CSS_PROP_SHORTHAND(
    border-left-width,
    border_left_width,
    BorderLeftWidth,
    CSS_PROPERTY_PARSE_FUNCTION)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_BORDER(
    border-left-width-value,
    border_left_width_value,
    BorderLeftWidthValue,
    CSS_PROPERTY_PARSE_INACCESSIBLE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_REPORT_OTHER_NAME,
    Margin,
    mBorderWidth.mLeft,
    0,
    kBorderWidthKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_Custom)
CSS_PROP_BORDER(
    border-left-width-ltr-source,
    border_left_width_ltr_source,
    BorderLeftWidthLTRSource,
    CSS_PROPERTY_PARSE_INACCESSIBLE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_DIRECTIONAL_SOURCE,
    Margin,
    mBorderLeftWidthLTRSource,
    0,
    kBoxPropSourceKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_BORDER(
    border-left-width-rtl-source,
    border_left_width_rtl_source,
    BorderLeftWidthRTLSource,
    CSS_PROPERTY_PARSE_INACCESSIBLE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_DIRECTIONAL_SOURCE,
    Margin,
    mBorderLeftWidthRTLSource,
    0,
    kBoxPropSourceKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
#endif
CSS_PROP_SHORTHAND(
    border-right,
    border_right,
    BorderRight,
    CSS_PROPERTY_PARSE_FUNCTION)
CSS_PROP_SHORTHAND(
    border-right-color,
    border_right_color,
    BorderRightColor,
    CSS_PROPERTY_PARSE_FUNCTION)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_BORDER(
    border-right-color-value,
    border_right_color_value,
    BorderRightColorValue,
    CSS_PROPERTY_PARSE_INACCESSIBLE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_IGNORED_WHEN_COLORS_DISABLED |
        CSS_PROPERTY_REPORT_OTHER_NAME,
    Margin,
    mBorderColor.mRight,
    0,
    kBorderColorKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_Custom)
CSS_PROP_BORDER(
    border-right-color-ltr-source,
    border_right_color_ltr_source,
    BorderRightColorLTRSource,
    CSS_PROPERTY_PARSE_INACCESSIBLE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_DIRECTIONAL_SOURCE |
        CSS_PROPERTY_IGNORED_WHEN_COLORS_DISABLED,
    Margin,
    mBorderRightColorLTRSource,
    0,
    kBoxPropSourceKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_BORDER(
    border-right-color-rtl-source,
    border_right_color_rtl_source,
    BorderRightColorRTLSource,
    CSS_PROPERTY_PARSE_INACCESSIBLE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_DIRECTIONAL_SOURCE |
        CSS_PROPERTY_IGNORED_WHEN_COLORS_DISABLED,
    Margin,
    mBorderRightColorRTLSource,
    0,
    kBoxPropSourceKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
#endif
CSS_PROP_BORDER(
    -moz-border-right-colors,
    border_right_colors,
    MozBorderRightColors,
    CSS_PROPERTY_PARSE_FUNCTION |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_IGNORED_WHEN_COLORS_DISABLED,
    Margin,
    mBorderColors.mRight,
    0,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_SHORTHAND(
    border-right-style,
    border_right_style,
    BorderRightStyle,
    CSS_PROPERTY_PARSE_FUNCTION) // on/off will need reflow
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_BORDER(
    border-right-style-value,
    border_right_style_value,
    BorderRightStyleValue,
    CSS_PROPERTY_PARSE_INACCESSIBLE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_REPORT_OTHER_NAME,
    Margin,
    mBorderStyle.mRight,
    0,
    kBorderStyleKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_BORDER(
    border-right-style-ltr-source,
    border_right_style_ltr_source,
    BorderRightStyleLTRSource,
    CSS_PROPERTY_PARSE_INACCESSIBLE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_DIRECTIONAL_SOURCE,
    Margin,
    mBorderRightStyleLTRSource,
    0,
    kBoxPropSourceKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_BORDER(
    border-right-style-rtl-source,
    border_right_style_rtl_source,
    BorderRightStyleRTLSource,
    CSS_PROPERTY_PARSE_INACCESSIBLE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_DIRECTIONAL_SOURCE,
    Margin,
    mBorderRightStyleRTLSource,
    0,
    kBoxPropSourceKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
#endif
CSS_PROP_SHORTHAND(
    border-right-width,
    border_right_width,
    BorderRightWidth,
    CSS_PROPERTY_PARSE_FUNCTION)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_BORDER(
    border-right-width-value,
    border_right_width_value,
    BorderRightWidthValue,
    CSS_PROPERTY_PARSE_INACCESSIBLE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_REPORT_OTHER_NAME,
    Margin,
    mBorderWidth.mRight,
    0,
    kBorderWidthKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_Custom)
CSS_PROP_BORDER(
    border-right-width-ltr-source,
    border_right_width_ltr_source,
    BorderRightWidthLTRSource,
    CSS_PROPERTY_PARSE_INACCESSIBLE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_DIRECTIONAL_SOURCE,
    Margin,
    mBorderRightWidthLTRSource,
    0,
    kBoxPropSourceKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_BORDER(
    border-right-width-rtl-source,
    border_right_width_rtl_source,
    BorderRightWidthRTLSource,
    CSS_PROPERTY_PARSE_INACCESSIBLE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_DIRECTIONAL_SOURCE,
    Margin,
    mBorderRightWidthRTLSource,
    0,
    kBoxPropSourceKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
#endif
CSS_PROP_TABLEBORDER(
    border-spacing,
    border_spacing,
    BorderSpacing,
    CSS_PROPERTY_PARSE_FUNCTION,
    Table,
    mBorderSpacing,
    0,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_Custom) // XXX bug 3935
CSS_PROP_SHORTHAND(
    -moz-border-start,
    border_start,
    MozBorderStart,
    CSS_PROPERTY_PARSE_FUNCTION)
CSS_PROP_SHORTHAND(
    -moz-border-start-color,
    border_start_color,
    MozBorderStartColor,
    CSS_PROPERTY_PARSE_FUNCTION)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_BORDER(
    border-start-color-value,
    border_start_color_value,
    BorderStartColorValue,
    CSS_PROPERTY_PARSE_INACCESSIBLE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER,
    Margin,
    mBorderStartColor,
    0,
    kBorderColorKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
#endif
CSS_PROP_SHORTHAND(
    -moz-border-start-style,
    border_start_style,
    MozBorderStartStyle,
    CSS_PROPERTY_PARSE_FUNCTION)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_BORDER(
    border-start-style-value,
    border_start_style_value,
    BorderStartStyleValue,
    CSS_PROPERTY_PARSE_INACCESSIBLE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER,
    Margin,
    mBorderStartStyle,
    0,
    kBorderStyleKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
#endif
CSS_PROP_SHORTHAND(
    -moz-border-start-width,
    border_start_width,
    MozBorderStartWidth,
    CSS_PROPERTY_PARSE_FUNCTION)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_BORDER(
    border-start-width-value,
    border_start_width_value,
    BorderStartWidthValue,
    CSS_PROPERTY_PARSE_INACCESSIBLE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER,
    Margin,
    mBorderStartWidth,
    0,
    kBorderWidthKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
#endif
CSS_PROP_SHORTHAND(
    border-style,
    border_style,
    BorderStyle,
    CSS_PROPERTY_PARSE_FUNCTION)  // on/off will need reflow
CSS_PROP_SHORTHAND(
    border-top,
    border_top,
    BorderTop,
    CSS_PROPERTY_PARSE_FUNCTION)
CSS_PROP_BORDER(
    border-top-color,
    border_top_color,
    BorderTopColor,
    CSS_PROPERTY_PARSE_VALUE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_IGNORED_WHEN_COLORS_DISABLED,
    Margin,
    mBorderColor.mTop,
    0,
    kBorderColorKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_Custom)
CSS_PROP_BORDER(
    -moz-border-top-colors,
    border_top_colors,
    MozBorderTopColors,
    CSS_PROPERTY_PARSE_FUNCTION |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_IGNORED_WHEN_COLORS_DISABLED,
    Margin,
    mBorderColors.mTop,
    0,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_BORDER(
    border-top-style,
    border_top_style,
    BorderTopStyle,
    CSS_PROPERTY_PARSE_VALUE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER,
    Margin,
    mBorderStyle.mTop,
    0,
    kBorderStyleKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)  // on/off will need reflow
CSS_PROP_BORDER(
    border-top-width,
    border_top_width,
    BorderTopWidth,
    CSS_PROPERTY_PARSE_VALUE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER,
    Margin,
    mBorderWidth.mTop,
    0,
    kBorderWidthKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_Custom)
CSS_PROP_SHORTHAND(
    border-width,
    border_width,
    BorderWidth,
    CSS_PROPERTY_PARSE_FUNCTION)
CSS_PROP_SHORTHAND(
    border-radius,
    border_radius,
    BorderRadius,
    CSS_PROPERTY_PARSE_FUNCTION)
CSS_PROP_BORDER(
    border-top-left-radius,
    border_top_left_radius,
    BorderTopLeftRadius,
    CSS_PROPERTY_PARSE_FUNCTION |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_STORES_CALC,
    Margin,
    mBorderRadius.mTopLeft,
    0,
    nsnull,
    offsetof(nsStyleBorder, mBorderRadius),
    eStyleAnimType_Corner_TopLeft)
CSS_PROP_BORDER(
    border-top-right-radius,
    border_top_right_radius,
    BorderTopRightRadius,
    CSS_PROPERTY_PARSE_FUNCTION |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_STORES_CALC,
    Margin,
    mBorderRadius.mTopRight,
    0,
    nsnull,
    offsetof(nsStyleBorder, mBorderRadius),
    eStyleAnimType_Corner_TopRight)
CSS_PROP_BORDER(
    border-bottom-right-radius,
    border_bottom_right_radius,
    BorderBottomRightRadius,
    CSS_PROPERTY_PARSE_FUNCTION |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_STORES_CALC,
    Margin,
    mBorderRadius.mBottomRight,
    0,
    nsnull,
    offsetof(nsStyleBorder, mBorderRadius),
    eStyleAnimType_Corner_BottomRight)
CSS_PROP_BORDER(
    border-bottom-left-radius,
    border_bottom_left_radius,
    BorderBottomLeftRadius,
    CSS_PROPERTY_PARSE_FUNCTION |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_STORES_CALC,
    Margin,
    mBorderRadius.mBottomLeft,
    0,
    nsnull,
    offsetof(nsStyleBorder, mBorderRadius),
    eStyleAnimType_Corner_BottomLeft)
CSS_PROP_POSITION(
    bottom,
    bottom,
    Bottom,
    CSS_PROPERTY_PARSE_VALUE |
        CSS_PROPERTY_STORES_CALC,
    Position,
    mOffset.mBottom,
    0,
    nsnull,
    offsetof(nsStylePosition, mOffset),
    eStyleAnimType_Sides_Bottom)
CSS_PROP_BORDER(
    box-shadow,
    box_shadow,
    BoxShadow,
    CSS_PROPERTY_PARSE_FUNCTION |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_VALUE_LIST_USES_COMMAS |
        CSS_PROPERTY_IGNORED_WHEN_COLORS_DISABLED,
    Margin,
    mBoxShadow,
    0,
    kBoxShadowTypeKTable,
    offsetof(nsStyleBorder, mBoxShadow),
    eStyleAnimType_Shadow)
CSS_PROP_POSITION(
    -moz-box-sizing,
    box_sizing,
    MozBoxSizing,
    CSS_PROPERTY_PARSE_VALUE,
    Position,
    mBoxSizing,
    0,
    kBoxSizingKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None) // XXX bug 3935
CSS_PROP_TABLEBORDER(
    caption-side,
    caption_side,
    CaptionSide,
    CSS_PROPERTY_PARSE_VALUE,
    Table,
    mCaptionSide,
    0,
    kCaptionSideKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_DISPLAY(
    clear,
    clear,
    Clear,
    CSS_PROPERTY_PARSE_VALUE,
    Display,
    mClear,
    0,
    kClearKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_DISPLAY(
    clip,
    clip,
    Clip,
    CSS_PROPERTY_PARSE_FUNCTION,
    Display,
    mClip,
    0,
    nsnull,
    offsetof(nsStyleDisplay, mClip),
    eStyleAnimType_Custom)
CSS_PROP_COLOR(
    color,
    color,
    Color,
    CSS_PROPERTY_PARSE_VALUE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE |
        CSS_PROPERTY_IGNORED_WHEN_COLORS_DISABLED,
    Color,
    mColor,
    0,
    nsnull,
    offsetof(nsStyleColor, mColor),
    eStyleAnimType_Color)
CSS_PROP_COLUMN(
    -moz-column-count,
    _moz_column_count,
    MozColumnCount,
    CSS_PROPERTY_PARSE_VALUE,
    Column,
    mColumnCount,
    0,
    nsnull,
    offsetof(nsStyleColumn, mColumnCount),
    eStyleAnimType_Custom)
CSS_PROP_COLUMN(
    -moz-column-width,
    _moz_column_width,
    MozColumnWidth,
    CSS_PROPERTY_PARSE_VALUE,
    Column,
    mColumnWidth,
    0,
    nsnull,
    offsetof(nsStyleColumn, mColumnWidth),
    eStyleAnimType_Coord)
CSS_PROP_COLUMN(
    -moz-column-gap,
    _moz_column_gap,
    MozColumnGap,
    CSS_PROPERTY_PARSE_VALUE,
    Column,
    mColumnGap,
    0,
    nsnull,
    offsetof(nsStyleColumn, mColumnGap),
    eStyleAnimType_Coord)
CSS_PROP_SHORTHAND(
    -moz-column-rule,
    _moz_column_rule,
    MozColumnRule,
    CSS_PROPERTY_PARSE_FUNCTION)
CSS_PROP_COLUMN(
    -moz-column-rule-color,
    _moz_column_rule_color,
    MozColumnRuleColor,
    CSS_PROPERTY_PARSE_VALUE |
        CSS_PROPERTY_IGNORED_WHEN_COLORS_DISABLED,
    Column,
    mColumnRuleColor,
    0,
    kBorderColorKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_Custom)
CSS_PROP_COLUMN(
    -moz-column-rule-style,
    _moz_column_rule_style,
    MozColumnRuleStyle,
    CSS_PROPERTY_PARSE_VALUE,
    Column,
    mColumnRuleStyle,
    0,
    kBorderStyleKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_COLUMN(
    -moz-column-rule-width,
    _moz_column_rule_width,
    MozColumnRuleWidth,
    CSS_PROPERTY_PARSE_VALUE,
    Column,
    mColumnRuleWidth,
    0,
    kBorderWidthKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_Custom)
CSS_PROP_CONTENT(
    content,
    content,
    Content,
    CSS_PROPERTY_PARSE_FUNCTION |
        CSS_PROPERTY_START_IMAGE_LOADS,
    Content,
    mContent,
    0,
    kContentKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_CONTENT(
    counter-increment,
    counter_increment,
    CounterIncrement,
    CSS_PROPERTY_PARSE_FUNCTION,
    Content,
    mCounterIncrement,
    0,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None) // XXX bug 137285
CSS_PROP_CONTENT(
    counter-reset,
    counter_reset,
    CounterReset,
    CSS_PROPERTY_PARSE_FUNCTION,
    Content,
    mCounterReset,
    0,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None) // XXX bug 137285
CSS_PROP_SHORTHAND(
    cue,
    cue,
    Cue,
    CSS_PROPERTY_PARSE_FUNCTION)
CSS_PROP_BACKENDONLY(
    cue-after,
    cue_after,
    CueAfter,
    CSS_PROPERTY_PARSE_VALUE,
    Aural,
    mCueAfter,
    0,
    nsnull)
CSS_PROP_BACKENDONLY(
    cue-before,
    cue_before,
    CueBefore,
    CSS_PROPERTY_PARSE_VALUE,
    Aural,
    mCueBefore,
    0,
    nsnull)
CSS_PROP_USERINTERFACE(
    cursor,
    cursor,
    Cursor,
    CSS_PROPERTY_PARSE_FUNCTION |
        CSS_PROPERTY_VALUE_LIST_USES_COMMAS |
        CSS_PROPERTY_START_IMAGE_LOADS |
        CSS_PROPERTY_IMAGE_IS_IN_ARRAY_0,
    UserInterface,
    mCursor,
    0,
    kCursorKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_VISIBILITY(
    direction,
    direction,
    Direction,
    CSS_PROPERTY_PARSE_VALUE,
    Display,
    mDirection,
    0,
    kDirectionKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_DISPLAY(
    display,
    display,
    Display,
    CSS_PROPERTY_PARSE_VALUE,
    Display,
    mDisplay,
    0,
    kDisplayKTable,
    offsetof(nsStyleDisplay, mDisplay),
    eStyleAnimType_EnumU8)
CSS_PROP_BACKENDONLY(
    elevation,
    elevation,
    Elevation,
    CSS_PROPERTY_PARSE_VALUE,
    Aural,
    mElevation,
    0,
    kElevationKTable)
CSS_PROP_TABLEBORDER(
    empty-cells,
    empty_cells,
    EmptyCells,
    CSS_PROPERTY_PARSE_VALUE,
    Table,
    mEmptyCells,
    0,
    kEmptyCellsKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_DISPLAY(
    float,
    float,
    CssFloat,
    CSS_PROPERTY_PARSE_VALUE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER,
    Display,
    mFloat,
    0,
    kFloatKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_BORDER(
    -moz-float-edge,
    float_edge,
    MozFloatEdge,
    CSS_PROPERTY_PARSE_VALUE,
    Margin,
    mFloatEdge,
    0,
    kFloatEdgeKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None) // XXX bug 3935
CSS_PROP_SHORTHAND(
    font,
    font,
    Font,
    CSS_PROPERTY_PARSE_FUNCTION)
CSS_PROP_FONT(
    font-family,
    font_family,
    FontFamily,
    CSS_PROPERTY_PARSE_VALUE |
        CSS_PROPERTY_VALUE_PARSER_FUNCTION |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE,
    Font,
    mFamily,
    0,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_FONT(
    -moz-font-feature-settings,
    font_feature_settings,
    MozFontFeatureSettings,
    CSS_PROPERTY_PARSE_VALUE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE,
    Font,
    mFontFeatureSettings,
    0,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_FONT(
    -moz-font-language-override,
    font_language_override,
    MozFontLanguageOverride,
    CSS_PROPERTY_PARSE_VALUE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE,
    Font,
    mFontLanguageOverride,
    0,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_FONT(
    font-size,
    font_size,
    FontSize,
    CSS_PROPERTY_PARSE_VALUE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE,
    Font,
    mSize,
    0,
    kFontSizeKTable,
    // Note that mSize is the correct place for *reading* the computed value,
    // but setting it requires setting mFont.size as well.
    offsetof(nsStyleFont, mSize),
    eStyleAnimType_nscoord)
CSS_PROP_FONT(
    font-size-adjust,
    font_size_adjust,
    FontSizeAdjust,
    CSS_PROPERTY_PARSE_VALUE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE,
    Font,
    mSizeAdjust,
    0,
    nsnull,
    offsetof(nsStyleFont, mFont.sizeAdjust),
    eStyleAnimType_float)
CSS_PROP_FONT(
    font-stretch,
    font_stretch,
    FontStretch,
    CSS_PROPERTY_PARSE_VALUE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE,
    Font,
    mStretch,
    0,
    kFontStretchKTable,
    offsetof(nsStyleFont, mFont.stretch),
    eStyleAnimType_Custom)
CSS_PROP_FONT(
    font-style,
    font_style,
    FontStyle,
    CSS_PROPERTY_PARSE_VALUE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE,
    Font,
    mStyle,
    0,
    kFontStyleKTable,
    offsetof(nsStyleFont, mFont.style),
    eStyleAnimType_EnumU8)
CSS_PROP_FONT(
    font-variant,
    font_variant,
    FontVariant,
    CSS_PROPERTY_PARSE_VALUE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE,
    Font,
    mVariant,
    0,
    kFontVariantKTable,
    offsetof(nsStyleFont, mFont.variant),
    eStyleAnimType_EnumU8)
CSS_PROP_FONT(
    font-weight,
    font_weight,
    FontWeight,
    CSS_PROPERTY_PARSE_VALUE |
        CSS_PROPERTY_VALUE_PARSER_FUNCTION |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE,
    Font,
    mWeight,
    0,
    kFontWeightKTable,
    offsetof(nsStyleFont, mFont.weight),
    eStyleAnimType_Custom)
CSS_PROP_UIRESET(
    -moz-force-broken-image-icon,
    force_broken_image_icon,
    MozForceBrokenImageIcon,
    CSS_PROPERTY_PARSE_VALUE,
    UserInterface,
    mForceBrokenImageIcon,
    0,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None) // bug 58646
CSS_PROP_POSITION(
    height,
    height,
    Height,
    CSS_PROPERTY_PARSE_VALUE |
        CSS_PROPERTY_STORES_CALC,
    Position,
    mHeight,
    0,
    nsnull,
    offsetof(nsStylePosition, mHeight),
    eStyleAnimType_Coord)
CSS_PROP_LIST(
    -moz-image-region,
    image_region,
    MozImageRegion,
    CSS_PROPERTY_PARSE_FUNCTION,
    List,
    mImageRegion,
    0,
    nsnull,
    offsetof(nsStyleList, mImageRegion),
    eStyleAnimType_Custom)
CSS_PROP_UIRESET(
    ime-mode,
    ime_mode,
    ImeMode,
    CSS_PROPERTY_PARSE_VALUE,
    UserInterface,
    mIMEMode,
    0,
    kIMEModeKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_POSITION(
    left,
    left,
    Left,
    CSS_PROPERTY_PARSE_VALUE |
        CSS_PROPERTY_STORES_CALC,
    Position,
    mOffset.mLeft,
    0,
    nsnull,
    offsetof(nsStylePosition, mOffset),
    eStyleAnimType_Sides_Left)
CSS_PROP_TEXT(
    letter-spacing,
    letter_spacing,
    LetterSpacing,
    CSS_PROPERTY_PARSE_VALUE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE,
    Text,
    mLetterSpacing,
    0,
    nsnull,
    offsetof(nsStyleText, mLetterSpacing),
    eStyleAnimType_Coord)
CSS_PROP_TEXT(
    line-height,
    line_height,
    LineHeight,
    CSS_PROPERTY_PARSE_VALUE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE,
    Text,
    mLineHeight,
    0,
    kLineHeightKTable,
    offsetof(nsStyleText, mLineHeight),
    eStyleAnimType_Coord)
CSS_PROP_SHORTHAND(
    list-style,
    list_style,
    ListStyle,
    CSS_PROPERTY_PARSE_FUNCTION)
CSS_PROP_LIST(
    list-style-image,
    list_style_image,
    ListStyleImage,
    CSS_PROPERTY_PARSE_VALUE |
        CSS_PROPERTY_START_IMAGE_LOADS,
    List,
    mImage,
    0,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_LIST(
    list-style-position,
    list_style_position,
    ListStylePosition,
    CSS_PROPERTY_PARSE_VALUE,
    List,
    mPosition,
    0,
    kListStylePositionKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_LIST(
    list-style-type,
    list_style_type,
    ListStyleType,
    CSS_PROPERTY_PARSE_VALUE,
    List,
    mType,
    0,
    kListStyleKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_SHORTHAND(
    margin,
    margin,
    Margin,
    CSS_PROPERTY_PARSE_FUNCTION)
CSS_PROP_MARGIN(
    margin-bottom,
    margin_bottom,
    MarginBottom,
    CSS_PROPERTY_PARSE_VALUE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_STORES_CALC,
    Margin,
    mMargin.mBottom,
    0,
    nsnull,
    offsetof(nsStyleMargin, mMargin),
    eStyleAnimType_Sides_Bottom)
CSS_PROP_SHORTHAND(
    -moz-margin-end,
    margin_end,
    MozMarginEnd,
    CSS_PROPERTY_PARSE_FUNCTION)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_MARGIN(
    margin-end-value,
    margin_end_value,
    MarginEndValue,
    CSS_PROPERTY_PARSE_INACCESSIBLE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_STORES_CALC,
    Margin,
    mMarginEnd,
    0,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
#endif
CSS_PROP_SHORTHAND(
    margin-left,
    margin_left,
    MarginLeft,
    CSS_PROPERTY_PARSE_FUNCTION)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_MARGIN(
    margin-left-value,
    margin_left_value,
    MarginLeftValue,
    CSS_PROPERTY_PARSE_INACCESSIBLE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_REPORT_OTHER_NAME |
        CSS_PROPERTY_STORES_CALC,
    Margin,
    mMargin.mLeft,
    0,
    nsnull,
    offsetof(nsStyleMargin, mMargin),
    eStyleAnimType_Sides_Left)
CSS_PROP_MARGIN(
    margin-left-ltr-source,
    margin_left_ltr_source,
    MarginLeftLTRSource,
    CSS_PROPERTY_PARSE_INACCESSIBLE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_DIRECTIONAL_SOURCE,
    Margin,
    mMarginLeftLTRSource,
    0,
    kBoxPropSourceKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_MARGIN(
    margin-left-rtl-source,
    margin_left_rtl_source,
    MarginLeftRTLSource,
    CSS_PROPERTY_PARSE_INACCESSIBLE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_DIRECTIONAL_SOURCE,
    Margin,
    mMarginLeftRTLSource,
    0,
    kBoxPropSourceKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
#endif
CSS_PROP_SHORTHAND(
    margin-right,
    margin_right,
    MarginRight,
    CSS_PROPERTY_PARSE_FUNCTION)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_MARGIN(
    margin-right-value,
    margin_right_value,
    MarginRightValue,
    CSS_PROPERTY_PARSE_INACCESSIBLE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_REPORT_OTHER_NAME |
        CSS_PROPERTY_STORES_CALC,
    Margin,
    mMargin.mRight,
    0,
    nsnull,
    offsetof(nsStyleMargin, mMargin),
    eStyleAnimType_Sides_Right)
CSS_PROP_MARGIN(
    margin-right-ltr-source,
    margin_right_ltr_source,
    MarginRightLTRSource,
    CSS_PROPERTY_PARSE_INACCESSIBLE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_DIRECTIONAL_SOURCE,
    Margin,
    mMarginRightLTRSource,
    0,
    kBoxPropSourceKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_MARGIN(
    margin-right-rtl-source,
    margin_right_rtl_source,
    MarginRightRTLSource,
    CSS_PROPERTY_PARSE_INACCESSIBLE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_DIRECTIONAL_SOURCE,
    Margin,
    mMarginRightRTLSource,
    0,
    kBoxPropSourceKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
#endif
CSS_PROP_SHORTHAND(
    -moz-margin-start,
    margin_start,
    MozMarginStart,
    CSS_PROPERTY_PARSE_FUNCTION)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_MARGIN(
    margin-start-value,
    margin_start_value,
    MarginStartValue,
    CSS_PROPERTY_PARSE_INACCESSIBLE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_STORES_CALC,
    Margin,
    mMarginStart,
    0,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
#endif
CSS_PROP_MARGIN(
    margin-top,
    margin_top,
    MarginTop,
    CSS_PROPERTY_PARSE_VALUE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_STORES_CALC,
    Margin,
    mMargin.mTop,
    0,
    nsnull,
    offsetof(nsStyleMargin, mMargin),
    eStyleAnimType_Sides_Top)
CSS_PROP_CONTENT(
    marker-offset,
    marker_offset,
    MarkerOffset,
    CSS_PROPERTY_PARSE_VALUE,
    Content,
    mMarkerOffset,
    0,
    nsnull,
    offsetof(nsStyleContent, mMarkerOffset),
    eStyleAnimType_Coord)
CSS_PROP_BACKENDONLY(
    marks,
    marks,
    Marks,
    CSS_PROPERTY_PARSE_VALUE |
        CSS_PROPERTY_VALUE_PARSER_FUNCTION,
    Page,
    mMarks,
    0,
    kPageMarksKTable)
CSS_PROP_POSITION(
    max-height,
    max_height,
    MaxHeight,
    CSS_PROPERTY_PARSE_VALUE |
        CSS_PROPERTY_STORES_CALC,
    Position,
    mMaxHeight,
    0,
    nsnull,
    offsetof(nsStylePosition, mMaxHeight),
    eStyleAnimType_Coord)
CSS_PROP_POSITION(
    max-width,
    max_width,
    MaxWidth,
    CSS_PROPERTY_PARSE_VALUE |
        CSS_PROPERTY_STORES_CALC,
    Position,
    mMaxWidth,
    0,
    kWidthKTable,
    offsetof(nsStylePosition, mMaxWidth),
    eStyleAnimType_Coord)
CSS_PROP_POSITION(
    min-height,
    min_height,
    MinHeight,
    CSS_PROPERTY_PARSE_VALUE |
        CSS_PROPERTY_STORES_CALC,
    Position,
    mMinHeight,
    0,
    nsnull,
    offsetof(nsStylePosition, mMinHeight),
    eStyleAnimType_Coord)
CSS_PROP_POSITION(
    min-width,
    min_width,
    MinWidth,
    CSS_PROPERTY_PARSE_VALUE |
        CSS_PROPERTY_STORES_CALC,
    Position,
    mMinWidth,
    0,
    kWidthKTable,
    offsetof(nsStylePosition, mMinWidth),
    eStyleAnimType_Coord)
CSS_PROP_DISPLAY(
    opacity,
    opacity,
    Opacity,
    CSS_PROPERTY_PARSE_VALUE,
    Display,
    mOpacity,
    0,
    nsnull,
    offsetof(nsStyleDisplay, mOpacity),
    eStyleAnimType_float) // XXX bug 3935
CSS_PROP_BACKENDONLY(
    orphans,
    orphans,
    Orphans,
    CSS_PROPERTY_PARSE_VALUE,
    Breaks,
    mOrphans,
    0,
    nsnull)
CSS_PROP_SHORTHAND(
    outline,
    outline,
    Outline,
    CSS_PROPERTY_PARSE_FUNCTION)
CSS_PROP_OUTLINE(
    outline-color,
    outline_color,
    OutlineColor,
    CSS_PROPERTY_PARSE_VALUE |
        CSS_PROPERTY_IGNORED_WHEN_COLORS_DISABLED,
    Margin,
    mOutlineColor,
    0,
    kOutlineColorKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_Custom)
CSS_PROP_OUTLINE(
    outline-style,
    outline_style,
    OutlineStyle,
    CSS_PROPERTY_PARSE_VALUE,
    Margin,
    mOutlineStyle,
    0,
    kOutlineStyleKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_OUTLINE(
    outline-width,
    outline_width,
    OutlineWidth,
    CSS_PROPERTY_PARSE_VALUE,
    Margin,
    mOutlineWidth,
    0,
    kBorderWidthKTable,
    offsetof(nsStyleOutline, mOutlineWidth),
    eStyleAnimType_Coord)
CSS_PROP_OUTLINE(
    outline-offset,
    outline_offset,
    OutlineOffset,
    CSS_PROPERTY_PARSE_VALUE,
    Margin,
    mOutlineOffset,
    0,
    nsnull,
    offsetof(nsStyleOutline, mOutlineOffset),
    eStyleAnimType_nscoord)
CSS_PROP_SHORTHAND(
    overflow,
    overflow,
    Overflow,
    CSS_PROPERTY_PARSE_FUNCTION)
CSS_PROP_DISPLAY(
    overflow-x,
    overflow_x,
    OverflowX,
    CSS_PROPERTY_PARSE_VALUE,
    Display,
    mOverflowX,
    0,
    kOverflowSubKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_DISPLAY(
    overflow-y,
    overflow_y,
    OverflowY,
    CSS_PROPERTY_PARSE_VALUE,
    Display,
    mOverflowY,
    0,
    kOverflowSubKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_SHORTHAND(
    padding,
    padding,
    Padding,
    CSS_PROPERTY_PARSE_FUNCTION)
CSS_PROP_PADDING(
    padding-bottom,
    padding_bottom,
    PaddingBottom,
    CSS_PROPERTY_PARSE_VALUE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_STORES_CALC,
    Margin,
    mPadding.mBottom,
    0,
    nsnull,
    offsetof(nsStylePadding, mPadding),
    eStyleAnimType_Sides_Bottom)
CSS_PROP_SHORTHAND(
    -moz-padding-end,
    padding_end,
    MozPaddingEnd,
    CSS_PROPERTY_PARSE_FUNCTION)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_PADDING(
    padding-end-value,
    padding_end_value,
    PaddingEndValue,
    CSS_PROPERTY_PARSE_INACCESSIBLE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_STORES_CALC,
    Margin,
    mPaddingEnd,
    0,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
#endif
CSS_PROP_SHORTHAND(
    padding-left,
    padding_left,
    PaddingLeft,
    CSS_PROPERTY_PARSE_FUNCTION)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_PADDING(
    padding-left-value,
    padding_left_value,
    PaddingLeftValue,
    CSS_PROPERTY_PARSE_INACCESSIBLE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_REPORT_OTHER_NAME |
        CSS_PROPERTY_STORES_CALC,
    Margin,
    mPadding.mLeft,
    0,
    nsnull,
    offsetof(nsStylePadding, mPadding),
    eStyleAnimType_Sides_Left)
CSS_PROP_PADDING(
    padding-left-ltr-source,
    padding_left_ltr_source,
    PaddingLeftLTRSource,
    CSS_PROPERTY_PARSE_INACCESSIBLE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_DIRECTIONAL_SOURCE,
    Margin,
    mPaddingLeftLTRSource,
    0,
    kBoxPropSourceKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_PADDING(
    padding-left-rtl-source,
    padding_left_rtl_source,
    PaddingLeftRTLSource,
    CSS_PROPERTY_PARSE_INACCESSIBLE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_DIRECTIONAL_SOURCE,
    Margin,
    mPaddingLeftRTLSource,
    0,
    kBoxPropSourceKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
#endif
CSS_PROP_SHORTHAND(
    padding-right,
    padding_right,
    PaddingRight,
    CSS_PROPERTY_PARSE_FUNCTION)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_PADDING(
    padding-right-value,
    padding_right_value,
    PaddingRightValue,
    CSS_PROPERTY_PARSE_INACCESSIBLE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_REPORT_OTHER_NAME |
        CSS_PROPERTY_STORES_CALC,
    Margin,
    mPadding.mRight,
    0,
    nsnull,
    offsetof(nsStylePadding, mPadding),
    eStyleAnimType_Sides_Right)
CSS_PROP_PADDING(
    padding-right-ltr-source,
    padding_right_ltr_source,
    PaddingRightLTRSource,
    CSS_PROPERTY_PARSE_INACCESSIBLE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_DIRECTIONAL_SOURCE,
    Margin,
    mPaddingRightLTRSource,
    0,
    kBoxPropSourceKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_PADDING(
    padding-right-rtl-source,
    padding_right_rtl_source,
    PaddingRightRTLSource,
    CSS_PROPERTY_PARSE_INACCESSIBLE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_DIRECTIONAL_SOURCE,
    Margin,
    mPaddingRightRTLSource,
    0,
    kBoxPropSourceKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
#endif
CSS_PROP_SHORTHAND(
    -moz-padding-start,
    padding_start,
    MozPaddingStart,
    CSS_PROPERTY_PARSE_FUNCTION)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_PADDING(
    padding-start-value,
    padding_start_value,
    PaddingStartValue,
    CSS_PROPERTY_PARSE_INACCESSIBLE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_STORES_CALC,
    Margin,
    mPaddingStart,
    0,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
#endif
CSS_PROP_PADDING(
    padding-top,
    padding_top,
    PaddingTop,
    CSS_PROPERTY_PARSE_VALUE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER |
        CSS_PROPERTY_STORES_CALC,
    Margin,
    mPadding.mTop,
    0,
    nsnull,
    offsetof(nsStylePadding, mPadding),
    eStyleAnimType_Sides_Top)
CSS_PROP_BACKENDONLY(
    page,
    page,
    Page,
    CSS_PROPERTY_PARSE_VALUE,
    Breaks,
    mPage,
    0,
    nsnull)
CSS_PROP_DISPLAY(
    page-break-after,
    page_break_after,
    PageBreakAfter,
    CSS_PROPERTY_PARSE_VALUE,
    Display,
    mBreakAfter,
    0,
    kPageBreakKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None) // temp fix for bug 24000
CSS_PROP_DISPLAY(
    page-break-before,
    page_break_before,
    PageBreakBefore,
    CSS_PROPERTY_PARSE_VALUE,
    Display,
    mBreakBefore,
    0,
    kPageBreakKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None) // temp fix for bug 24000
CSS_PROP_BACKENDONLY(
    page-break-inside,
    page_break_inside,
    PageBreakInside,
    CSS_PROPERTY_PARSE_VALUE,
    Breaks,
    mPageBreakInside,
    0,
    kPageBreakInsideKTable)
CSS_PROP_SHORTHAND(
    pause,
    pause,
    Pause,
    CSS_PROPERTY_PARSE_FUNCTION)
CSS_PROP_BACKENDONLY(
    pause-after,
    pause_after,
    PauseAfter,
    CSS_PROPERTY_PARSE_VALUE,
    Aural,
    mPauseAfter,
    0,
    nsnull)
CSS_PROP_BACKENDONLY(
    pause-before,
    pause_before,
    PauseBefore,
    CSS_PROPERTY_PARSE_VALUE,
    Aural,
    mPauseBefore,
    0,
    nsnull)
CSS_PROP_BACKENDONLY(
    pitch,
    pitch,
    Pitch,
    CSS_PROPERTY_PARSE_VALUE,
    Aural,
    mPitch,
    0,
    kPitchKTable)
CSS_PROP_BACKENDONLY(
    pitch-range,
    pitch_range,
    PitchRange,
    CSS_PROPERTY_PARSE_VALUE,
    Aural,
    mPitchRange,
    0,
    nsnull)
CSS_PROP_VISIBILITY(
    pointer-events,
    pointer_events,
    PointerEvents,
    CSS_PROPERTY_PARSE_VALUE,
    Display,
    mPointerEvents,
    0,
    kPointerEventsKTable,
    offsetof(nsStyleVisibility, mPointerEvents),
    eStyleAnimType_EnumU8)
CSS_PROP_DISPLAY(
    position,
    position,
    Position,
    CSS_PROPERTY_PARSE_VALUE,
    Display,
    mPosition,
    0,
    kPositionKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_QUOTES(
    quotes,
    quotes,
    Quotes,
    CSS_PROPERTY_PARSE_FUNCTION,
    Content,
    mQuotes,
    0,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_DISPLAY(
    resize,
    resize,
    Resize,
    CSS_PROPERTY_PARSE_VALUE,
    Display,
    mResize,
    0,
    kResizeKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_BACKENDONLY(
    richness,
    richness,
    Richness,
    CSS_PROPERTY_PARSE_VALUE,
    Aural,
    mRichness,
    0,
    nsnull)
CSS_PROP_POSITION(
    right,
    right,
    Right,
    CSS_PROPERTY_PARSE_VALUE |
        CSS_PROPERTY_STORES_CALC,
    Position,
    mOffset.mRight,
    0,
    nsnull,
    offsetof(nsStylePosition, mOffset),
    eStyleAnimType_Sides_Right)
CSS_PROP_BACKENDONLY(
    size,
    size,
    Size,
    CSS_PROPERTY_PARSE_FUNCTION,
    Page,
    mSize,
    0,
    kPageSizeKTable)
CSS_PROP_BACKENDONLY(
    speak,
    speak,
    Speak,
    CSS_PROPERTY_PARSE_VALUE,
    Aural,
    mSpeak,
    0,
    kSpeakKTable)
CSS_PROP_BACKENDONLY(
    speak-header,
    speak_header,
    SpeakHeader,
    CSS_PROPERTY_PARSE_VALUE,
    Aural,
    mSpeakHeader,
    0,
    kSpeakHeaderKTable)
CSS_PROP_BACKENDONLY(
    speak-numeral,
    speak_numeral,
    SpeakNumeral,
    CSS_PROPERTY_PARSE_VALUE,
    Aural,
    mSpeakNumeral,
    0,
    kSpeakNumeralKTable)
CSS_PROP_BACKENDONLY(
    speak-punctuation,
    speak_punctuation,
    SpeakPunctuation,
    CSS_PROPERTY_PARSE_VALUE,
    Aural,
    mSpeakPunctuation,
    0,
    kSpeakPunctuationKTable)
CSS_PROP_BACKENDONLY(
    speech-rate,
    speech_rate,
    SpeechRate,
    CSS_PROPERTY_PARSE_VALUE,
    Aural,
    mSpeechRate,
    0,
    kSpeechRateKTable)
CSS_PROP_BACKENDONLY(
    stress,
    stress,
    Stress,
    CSS_PROPERTY_PARSE_VALUE,
    Aural,
    mStress,
    0,
    nsnull)
CSS_PROP_TABLE(
    table-layout,
    table_layout,
    TableLayout,
    CSS_PROPERTY_PARSE_VALUE,
    Table,
    mLayout,
    0,
    kTableLayoutKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_TEXT(
    text-align,
    text_align,
    TextAlign,
    CSS_PROPERTY_PARSE_VALUE,
    Text,
    mTextAlign,
    0,
    kTextAlignKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_TEXTRESET(
    text-decoration,
    text_decoration,
    TextDecoration,
    CSS_PROPERTY_PARSE_VALUE |
        CSS_PROPERTY_VALUE_PARSER_FUNCTION |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE,
    Text,
    mDecoration,
    0,
    kTextDecorationKTable,
    offsetof(nsStyleTextReset, mTextDecoration),
    eStyleAnimType_EnumU8)
CSS_PROP_TEXT(
    text-indent,
    text_indent,
    TextIndent,
    CSS_PROPERTY_PARSE_VALUE |
        CSS_PROPERTY_STORES_CALC,
    Text,
    mTextIndent,
    0,
    nsnull,
    offsetof(nsStyleText, mTextIndent),
    eStyleAnimType_Coord)
CSS_PROP_TEXT(
    text-shadow,
    text_shadow,
    TextShadow,
    CSS_PROPERTY_PARSE_FUNCTION |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE |
        CSS_PROPERTY_VALUE_LIST_USES_COMMAS |
        CSS_PROPERTY_IGNORED_WHEN_COLORS_DISABLED,
    Text,
    mTextShadow,
    0,
    nsnull,
    offsetof(nsStyleText, mTextShadow),
    eStyleAnimType_Shadow)
CSS_PROP_TEXT(
    text-transform,
    text_transform,
    TextTransform,
    CSS_PROPERTY_PARSE_VALUE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE,
    Text,
    mTextTransform,
    0,
    kTextTransformKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_DISPLAY(
    -moz-transform,
    _moz_transform,
    MozTransform,
    CSS_PROPERTY_PARSE_FUNCTION,
    Display,
    mTransform,
    0,
    kDisplayKTable,
    offsetof(nsStyleDisplay, mSpecifiedTransform),
    eStyleAnimType_Custom)
CSS_PROP_DISPLAY(
    -moz-transform-origin,
    _moz_transform_origin,
    MozTransformOrigin,
    CSS_PROPERTY_PARSE_FUNCTION |
        CSS_PROPERTY_STORES_CALC,
    Display,
    mTransformOrigin,
    0,
    kBackgroundPositionKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_Custom)
CSS_PROP_POSITION(
    top,
    top,
    Top,
    CSS_PROPERTY_PARSE_VALUE |
        CSS_PROPERTY_STORES_CALC,
    Position,
    mOffset.mTop,
    0,
    nsnull,
    offsetof(nsStylePosition, mOffset),
    eStyleAnimType_Sides_Top)
CSS_PROP_SHORTHAND(
    -moz-transition,
    transition,
    MozTransition,
    CSS_PROPERTY_PARSE_FUNCTION)
CSS_PROP_DISPLAY(
    -moz-transition-delay,
    transition_delay,
    MozTransitionDelay,
    CSS_PROPERTY_PARSE_FUNCTION |
        CSS_PROPERTY_VALUE_LIST_USES_COMMAS,
    Display,
    mTransitionDelay,
    0,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_DISPLAY(
    -moz-transition-duration,
    transition_duration,
    MozTransitionDuration,
    CSS_PROPERTY_PARSE_FUNCTION |
        CSS_PROPERTY_VALUE_LIST_USES_COMMAS,
    Display,
    mTransitionDuration,
    0,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_DISPLAY(
    -moz-transition-property,
    transition_property,
    MozTransitionProperty,
    CSS_PROPERTY_PARSE_FUNCTION |
        CSS_PROPERTY_VALUE_LIST_USES_COMMAS,
    Display,
    mTransitionProperty,
    0,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_DISPLAY(
    -moz-transition-timing-function,
    transition_timing_function,
    MozTransitionTimingFunction,
    CSS_PROPERTY_PARSE_FUNCTION |
        CSS_PROPERTY_VALUE_LIST_USES_COMMAS,
    Display,
    mTransitionTimingFunction,
    0,
    kTransitionTimingFunctionKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_TEXTRESET(
    unicode-bidi,
    unicode_bidi,
    UnicodeBidi,
    CSS_PROPERTY_PARSE_VALUE,
    Text,
    mUnicodeBidi,
    0,
    kUnicodeBidiKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_USERINTERFACE(
    -moz-user-focus,
    user_focus,
    MozUserFocus,
    CSS_PROPERTY_PARSE_VALUE,
    UserInterface,
    mUserFocus,
    0,
    kUserFocusKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None) // XXX bug 3935
CSS_PROP_USERINTERFACE(
    -moz-user-input,
    user_input,
    MozUserInput,
    CSS_PROPERTY_PARSE_VALUE,
    UserInterface,
    mUserInput,
    0,
    kUserInputKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None) // XXX ??? // XXX bug 3935
CSS_PROP_USERINTERFACE(
    -moz-user-modify,
    user_modify,
    MozUserModify,
    CSS_PROPERTY_PARSE_VALUE,
    UserInterface,
    mUserModify,
    0,
    kUserModifyKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None) // XXX bug 3935
CSS_PROP_UIRESET(
    -moz-user-select,
    user_select,
    MozUserSelect,
    CSS_PROPERTY_PARSE_VALUE,
    UserInterface,
    mUserSelect,
    0,
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
    CSS_PROPERTY_PARSE_VALUE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE |
        CSS_PROPERTY_STORES_CALC,
    Text,
    mVerticalAlign,
    0,
    kVerticalAlignKTable,
    offsetof(nsStyleTextReset, mVerticalAlign),
    eStyleAnimType_Coord)
CSS_PROP_VISIBILITY(
    visibility,
    visibility,
    Visibility,
    CSS_PROPERTY_PARSE_VALUE,
    Display,
    mVisibility,
    0,
    kVisibilityKTable,
    offsetof(nsStyleVisibility, mVisible),
    eStyleAnimType_EnumU8)  // reflow for collapse
CSS_PROP_BACKENDONLY(
    voice-family,
    voice_family,
    VoiceFamily,
    CSS_PROPERTY_PARSE_VALUE |
        CSS_PROPERTY_VALUE_PARSER_FUNCTION,
    Aural,
    mVoiceFamily,
    0,
    nsnull)
CSS_PROP_BACKENDONLY(
    volume,
    volume,
    Volume,
    CSS_PROPERTY_PARSE_VALUE,
    Aural,
    mVolume,
    0,
    kVolumeKTable)
CSS_PROP_TEXT(
    white-space,
    white_space,
    WhiteSpace,
    CSS_PROPERTY_PARSE_VALUE,
    Text,
    mWhiteSpace,
    0,
    kWhitespaceKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_BACKENDONLY(
    widows,
    widows,
    Widows,
    CSS_PROPERTY_PARSE_VALUE,
    Breaks,
    mWidows,
    0,
    nsnull)
CSS_PROP_POSITION(
    width,
    width,
    Width,
    CSS_PROPERTY_PARSE_VALUE |
        CSS_PROPERTY_STORES_CALC,
    Position,
    mWidth,
    0,
    kWidthKTable,
    offsetof(nsStylePosition, mWidth),
    eStyleAnimType_Coord)
CSS_PROP_UIRESET(
    -moz-window-shadow,
    _moz_window_shadow,
    MozWindowShadow,
    CSS_PROPERTY_PARSE_VALUE,
    UserInterface,
    mWindowShadow,
    0,
    kWindowShadowKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_TEXT(
    word-spacing,
    word_spacing,
    WordSpacing,
    CSS_PROPERTY_PARSE_VALUE |
        CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE,
    Text,
    mWordSpacing,
    0,
    nsnull,
    offsetof(nsStyleText, mWordSpacing),
    eStyleAnimType_nscoord)
CSS_PROP_TEXT(
    word-wrap,
    word_wrap,
    WordWrap,
    CSS_PROPERTY_PARSE_VALUE,
    Text,
    mWordWrap,
    0,
    kWordwrapKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_POSITION(
    z-index,
    z_index,
    ZIndex,
    CSS_PROPERTY_PARSE_VALUE,
    Position,
    mZIndex,
    0,
    nsnull,
    offsetof(nsStylePosition, mZIndex),
    eStyleAnimType_Coord)
CSS_PROP_XUL(
    -moz-box-align,
    box_align,
    MozBoxAlign,
    CSS_PROPERTY_PARSE_VALUE,
    XUL,
    mBoxAlign,
    0,
    kBoxAlignKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None) // XXX bug 3935
CSS_PROP_XUL(
    -moz-box-direction,
    box_direction,
    MozBoxDirection,
    CSS_PROPERTY_PARSE_VALUE,
    XUL,
    mBoxDirection,
    0,
    kBoxDirectionKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None) // XXX bug 3935
CSS_PROP_XUL(
    -moz-box-flex,
    box_flex,
    MozBoxFlex,
    CSS_PROPERTY_PARSE_VALUE,
    XUL,
    mBoxFlex,
    0,
    nsnull,
    offsetof(nsStyleXUL, mBoxFlex),
    eStyleAnimType_float) // XXX bug 3935
CSS_PROP_XUL(
    -moz-box-orient,
    box_orient,
    MozBoxOrient,
    CSS_PROPERTY_PARSE_VALUE,
    XUL,
    mBoxOrient,
    0,
    kBoxOrientKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None) // XXX bug 3935
CSS_PROP_XUL(
    -moz-box-pack,
    box_pack,
    MozBoxPack,
    CSS_PROPERTY_PARSE_VALUE,
    XUL,
    mBoxPack,
    0,
    kBoxPackKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None) // XXX bug 3935
CSS_PROP_XUL(
    -moz-box-ordinal-group,
    box_ordinal_group,
    MozBoxOrdinalGroup,
    CSS_PROPERTY_PARSE_VALUE,
    XUL,
    mBoxOrdinal,
    0,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_XUL(
    -moz-stack-sizing,
    stack_sizing,
    MozStackSizing,
    CSS_PROPERTY_PARSE_VALUE,
    XUL,
    mStackSizing,
    0,
    kStackSizingKTable,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)

#ifdef MOZ_MATHML
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_FONT(
    -moz-script-level,
    script_level,
    ScriptLevel,
    CSS_PROPERTY_PARSE_VALUE,
    Font,
    mScriptLevel,
    0,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_FONT(
    -moz-script-size-multiplier,
    script_size_multiplier,
    ScriptSizeMultiplier,
    CSS_PROPERTY_PARSE_VALUE,
    Font,
    mScriptSizeMultiplier,
    0,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_FONT(
    -moz-script-min-size,
    script_min_size,
    ScriptMinSize,
    CSS_PROPERTY_PARSE_VALUE,
    Font,
    mScriptMinSize,
    0,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
#endif
#endif

CSS_PROP_SVGRESET(
    clip-path,
    clip_path,
    ClipPath,
    CSS_PROPERTY_PARSE_VALUE,
    SVG,
    mClipPath,
    0,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_SVG(
    clip-rule,
    clip_rule,
    ClipRule,
    CSS_PROPERTY_PARSE_VALUE,
    SVG,
    mClipRule,
    0,
    kFillRuleKTable,
    offsetof(nsStyleSVG, mClipRule),
    eStyleAnimType_EnumU8)
CSS_PROP_SVG(
    color-interpolation,
    color_interpolation,
    ColorInterpolation,
    CSS_PROPERTY_PARSE_VALUE,
    SVG,
    mColorInterpolation,
    0,
    kColorInterpolationKTable,
    offsetof(nsStyleSVG, mColorInterpolation),
    eStyleAnimType_EnumU8)
CSS_PROP_SVG(
    color-interpolation-filters,
    color_interpolation_filters,
    ColorInterpolationFilters,
    CSS_PROPERTY_PARSE_VALUE,
    SVG,
    mColorInterpolationFilters,
    0,
    kColorInterpolationKTable,
    offsetof(nsStyleSVG, mColorInterpolationFilters),
    eStyleAnimType_EnumU8)
CSS_PROP_SVGRESET(
    dominant-baseline,
    dominant_baseline,
    DominantBaseline,
    CSS_PROPERTY_PARSE_VALUE,
    SVG,
    mDominantBaseline,
    0,
    kDominantBaselineKTable,
    offsetof(nsStyleSVGReset, mDominantBaseline),
    eStyleAnimType_EnumU8)
CSS_PROP_SVG(
    fill,
    fill,
    Fill,
    CSS_PROPERTY_PARSE_FUNCTION,
    SVG,
    mFill,
    0,
    nsnull,
    offsetof(nsStyleSVG, mFill),
    eStyleAnimType_PaintServer)
CSS_PROP_SVG(
    fill-opacity,
    fill_opacity,
    FillOpacity,
    CSS_PROPERTY_PARSE_VALUE,
    SVG,
    mFillOpacity,
    0,
    nsnull,
    offsetof(nsStyleSVG, mFillOpacity),
    eStyleAnimType_float)
CSS_PROP_SVG(
    fill-rule,
    fill_rule,
    FillRule,
    CSS_PROPERTY_PARSE_VALUE,
    SVG,
    mFillRule,
    0,
    kFillRuleKTable,
    offsetof(nsStyleSVG, mFillRule),
    eStyleAnimType_EnumU8)
CSS_PROP_SVGRESET(
    filter,
    filter,
    Filter,
    CSS_PROPERTY_PARSE_VALUE,
    SVG,
    mFilter,
    0,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_SVGRESET(
    flood-color,
    flood_color,
    FloodColor,
    CSS_PROPERTY_PARSE_VALUE,
    SVG,
    mFloodColor,
    0,
    nsnull,
    offsetof(nsStyleSVGReset, mFloodColor),
    eStyleAnimType_Color)
CSS_PROP_SVGRESET(
    flood-opacity,
    flood_opacity,
    FloodOpacity,
    CSS_PROPERTY_PARSE_VALUE,
    SVG,
    mFloodOpacity,
    0,
    nsnull,
    offsetof(nsStyleSVGReset, mFloodOpacity),
    eStyleAnimType_float)
CSS_PROP_SVG(
    image-rendering,
    image_rendering,
    ImageRendering,
    CSS_PROPERTY_PARSE_VALUE,
    SVG,
    mImageRendering,
    0,
    kImageRenderingKTable,
    offsetof(nsStyleSVG, mImageRendering),
    eStyleAnimType_EnumU8)
CSS_PROP_SVGRESET(
    lighting-color,
    lighting_color,
    LightingColor,
    CSS_PROPERTY_PARSE_VALUE,
    SVG,
    mLightingColor,
    0,
    nsnull,
    offsetof(nsStyleSVGReset, mLightingColor),
    eStyleAnimType_Color)
CSS_PROP_SHORTHAND(
    marker,
    marker,
    Marker,
    CSS_PROPERTY_PARSE_FUNCTION)
CSS_PROP_SVG(
    marker-end,
    marker_end,
    MarkerEnd,
    CSS_PROPERTY_PARSE_VALUE,
    SVG,
    mMarkerEnd,
    0,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_SVG(
    marker-mid,
    marker_mid,
    MarkerMid,
    CSS_PROPERTY_PARSE_VALUE,
    SVG,
    mMarkerMid,
    0,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_SVG(
    marker-start,
    marker_start,
    MarkerStart,
    CSS_PROPERTY_PARSE_VALUE,
    SVG,
    mMarkerStart,
    0,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_SVGRESET(
    mask,
    mask,
    Mask,
    CSS_PROPERTY_PARSE_VALUE,
    SVG,
    mMask,
    0,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_SVG(
    shape-rendering,
    shape_rendering,
    ShapeRendering,
    CSS_PROPERTY_PARSE_VALUE,
    SVG,
    mShapeRendering,
    0,
    kShapeRenderingKTable,
    offsetof(nsStyleSVG, mShapeRendering),
    eStyleAnimType_EnumU8)
CSS_PROP_SVGRESET(
    stop-color,
    stop_color,
    StopColor,
    CSS_PROPERTY_PARSE_VALUE,
    SVG,
    mStopColor,
    0,
    nsnull,
    offsetof(nsStyleSVGReset, mStopColor),
    eStyleAnimType_Color)
CSS_PROP_SVGRESET(
    stop-opacity,
    stop_opacity,
    StopOpacity,
    CSS_PROPERTY_PARSE_VALUE,
    SVG,
    mStopOpacity,
    0,
    nsnull,
    offsetof(nsStyleSVGReset, mStopOpacity),
    eStyleAnimType_float)
CSS_PROP_SVG(
    stroke,
    stroke,
    Stroke,
    CSS_PROPERTY_PARSE_FUNCTION,
    SVG,
    mStroke,
    0,
    nsnull,
    offsetof(nsStyleSVG, mStroke),
    eStyleAnimType_PaintServer)
CSS_PROP_SVG(
    stroke-dasharray,
    stroke_dasharray,
    StrokeDasharray,
    CSS_PROPERTY_PARSE_FUNCTION |
        CSS_PROPERTY_VALUE_LIST_USES_COMMAS,
    SVG,
    mStrokeDasharray,
    0,
    nsnull,
    CSS_PROP_NO_OFFSET, /* property stored in 2 separate members */
    eStyleAnimType_Custom)
CSS_PROP_SVG(
    stroke-dashoffset,
    stroke_dashoffset,
    StrokeDashoffset,
    CSS_PROPERTY_PARSE_VALUE,
    SVG,
    mStrokeDashoffset,
    0,
    nsnull,
    offsetof(nsStyleSVG, mStrokeDashoffset),
    eStyleAnimType_Coord)
CSS_PROP_SVG(
    stroke-linecap,
    stroke_linecap,
    StrokeLinecap,
    CSS_PROPERTY_PARSE_VALUE,
    SVG,
    mStrokeLinecap,
    0,
    kStrokeLinecapKTable,
    offsetof(nsStyleSVG, mStrokeLinecap),
    eStyleAnimType_EnumU8)
CSS_PROP_SVG(
    stroke-linejoin,
    stroke_linejoin,
    StrokeLinejoin,
    CSS_PROPERTY_PARSE_VALUE,
    SVG,
    mStrokeLinejoin,
    0,
    kStrokeLinejoinKTable,
    offsetof(nsStyleSVG, mStrokeLinejoin),
    eStyleAnimType_EnumU8)
CSS_PROP_SVG(
    stroke-miterlimit,
    stroke_miterlimit,
    StrokeMiterlimit,
    CSS_PROPERTY_PARSE_VALUE,
    SVG,
    mStrokeMiterlimit,
    0,
    nsnull,
    offsetof(nsStyleSVG, mStrokeMiterlimit),
    eStyleAnimType_float)
CSS_PROP_SVG(
    stroke-opacity,
    stroke_opacity,
    StrokeOpacity,
    CSS_PROPERTY_PARSE_VALUE,
    SVG,
    mStrokeOpacity,
    0,
    nsnull,
    offsetof(nsStyleSVG, mStrokeOpacity),
    eStyleAnimType_float)
CSS_PROP_SVG(
    stroke-width,
    stroke_width,
    StrokeWidth,
    CSS_PROPERTY_PARSE_VALUE,
    SVG,
    mStrokeWidth,
    0,
    nsnull,
    offsetof(nsStyleSVG, mStrokeWidth),
    eStyleAnimType_Coord)
CSS_PROP_SVG(
    text-anchor,
    text_anchor,
    TextAnchor,
    CSS_PROPERTY_PARSE_VALUE,
    SVG,
    mTextAnchor,
    0,
    kTextAnchorKTable,
    offsetof(nsStyleSVG, mTextAnchor),
    eStyleAnimType_EnumU8)
CSS_PROP_SVG(
    text-rendering,
    text_rendering,
    TextRendering,
    CSS_PROPERTY_PARSE_VALUE,
    SVG,
    mTextRendering,
    0,
    kTextRenderingKTable,
    offsetof(nsStyleSVG, mTextRendering),
    eStyleAnimType_EnumU8)

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
    Lang,
    CSS_PROPERTY_PARSE_VALUE,
    Display,
    mLang,
    0,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_TABLE(
    X,
    X,
    Cols,
    CSS_PROPERTY_PARSE_VALUE,
    Table,
    mCols,
    0,
    nsnull,
    CSS_PROP_NO_OFFSET,
    eStyleAnimType_None)
CSS_PROP_TABLE(
    X,
    X,
    Span,
    CSS_PROPERTY_PARSE_VALUE,
    Table,
    mSpan,
    0,
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
