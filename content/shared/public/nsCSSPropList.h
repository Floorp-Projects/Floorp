/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/******

  This file contains the list of all parsed CSS properties.  It is
  designed to be used as inline input through the magic of C
  preprocessing.  All entries must be enclosed in the appropriate
  CSS_PROP_* macro which will have cruel and unusual things done to it.
  It is recommended (but not strictly necessary) to keep all entries in
  alphabetical order.

  The arguments to CSS_PROP_* are:

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

  4. 'hint' says what needs to be recomputed when the property changes.

  5. 'datastruct' says which nsRuleData* struct this property goes in.

  6. 'member' gives the name of the member variable in the nsRuleData
  struct.

  7. 'type' gives the |nsCSSType| of the data in the nsRuleData struct
  and in the nsCSSDeclaration backend.

  8. 'iscoord' says whether the property is a coordinate property for
  which we use an explicit inherit value in the *style structs* (since
  inheritance requires knowledge of layout).

  Which CSS_PROP_* macro a property is in depends on which nsStyle* its
  computed value lives in (unless it is a shorthand, in which case it
  gets CSS_PROP_SHORTHAND).

 ******/


/*************************************************************************/


// XXX Should we really be using CSS_PROP_SHORTHAND for 'border-spacing',
// 'background-position', and 'size'?


// All includers must explicitly define CSS_PROP_NOTIMPLEMENTED if they
// want this.  (Only the DOM cares.)
#ifndef CSS_PROP_NOTIMPLEMENTED
#define CSS_PROP_NOTIMPLEMENTED(name_, id_, method_, hint_) /* nothing */
#define DEFINED_CSS_PROP_NOTIMPLEMENTED
#endif

// All includers must explicitly define CSS_PROP_SHORTHAND if they
// want it.
#ifndef CSS_PROP_SHORTHAND
#define CSS_PROP_SHORTHAND(name_, id_, method_, hint_) /* nothing */
#define DEFINED_CSS_PROP_SHORTHAND
#endif


// Callers may define CSS_PROP_LIST_EXCLUDE_INTERNAL if they want to
// exclude internal properties that are not represented in the DOM (only
// the DOM style code defines this).

// A caller who wants all the properties can define the |CSS_PROP|
// macro.
#ifdef CSS_PROP

#define USED_CSS_PROP
#define CSS_PROP_FONT(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_) CSS_PROP(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_)
#define CSS_PROP_COLOR(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_) CSS_PROP(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_)
#define CSS_PROP_BACKGROUND(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_) CSS_PROP(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_)
#define CSS_PROP_LIST(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_) CSS_PROP(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_)
#define CSS_PROP_POSITION(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_) CSS_PROP(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_)
#define CSS_PROP_TEXT(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_) CSS_PROP(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_)
#define CSS_PROP_TEXTRESET(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_) CSS_PROP(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_)
#define CSS_PROP_DISPLAY(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_) CSS_PROP(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_)
#define CSS_PROP_VISIBILITY(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_) CSS_PROP(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_)
#define CSS_PROP_CONTENT(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_) CSS_PROP(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_)
#define CSS_PROP_QUOTES(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_) CSS_PROP(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_)
#define CSS_PROP_USERINTERFACE(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_) CSS_PROP(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_)
#define CSS_PROP_UIRESET(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_) CSS_PROP(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_)
#define CSS_PROP_TABLE(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_) CSS_PROP(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_)
#define CSS_PROP_TABLEBORDER(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_) CSS_PROP(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_)
#define CSS_PROP_MARGIN(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_) CSS_PROP(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_)
#define CSS_PROP_PADDING(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_) CSS_PROP(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_)
#define CSS_PROP_BORDER(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_) CSS_PROP(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_)
#define CSS_PROP_OUTLINE(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_) CSS_PROP(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_)
#define CSS_PROP_XUL(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_) CSS_PROP(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_)
#ifdef MOZ_SVG
#define CSS_PROP_SVG(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_) CSS_PROP(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_)
#endif

// For properties that are stored in the CSS backend but are not
// computed.  An includer may define this in addition to CSS_PROP, but
// otherwise we treat it as the same.
#ifndef CSS_PROP_BACKENDONLY
#define CSS_PROP_BACKENDONLY(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_) CSS_PROP(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_)
#define DEFINED_CSS_PROP_BACKENDONLY
#endif

#else /* !defined(CSS_PROP) */

// An includer who does not define CSS_PROP can define any or all of the
// per-struct macros that are equivalent to it, and the rest will be
// ignored.

#ifndef CSS_PROP_FONT
#define CSS_PROP_FONT(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_) /* nothing */
#define DEFINED_CSS_PROP_FONT
#endif
#ifndef CSS_PROP_COLOR
#define CSS_PROP_COLOR(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_) /* nothing */
#define DEFINED_CSS_PROP_COLOR
#endif
#ifndef CSS_PROP_BACKGROUND
#define CSS_PROP_BACKGROUND(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_) /* nothing */
#define DEFINED_CSS_PROP_BACKGROUND
#endif
#ifndef CSS_PROP_LIST
#define CSS_PROP_LIST(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_) /* nothing */
#define DEFINED_CSS_PROP_LIST
#endif
#ifndef CSS_PROP_POSITION
#define CSS_PROP_POSITION(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_) /* nothing */
#define DEFINED_CSS_PROP_POSITION
#endif
#ifndef CSS_PROP_TEXT
#define CSS_PROP_TEXT(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_) /* nothing */
#define DEFINED_CSS_PROP_TEXT
#endif
#ifndef CSS_PROP_TEXTRESET
#define CSS_PROP_TEXTRESET(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_) /* nothing */
#define DEFINED_CSS_PROP_TEXTRESET
#endif
#ifndef CSS_PROP_DISPLAY
#define CSS_PROP_DISPLAY(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_) /* nothing */
#define DEFINED_CSS_PROP_DISPLAY
#endif
#ifndef CSS_PROP_VISIBILITY
#define CSS_PROP_VISIBILITY(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_) /* nothing */
#define DEFINED_CSS_PROP_VISIBILITY
#endif
#ifndef CSS_PROP_CONTENT
#define CSS_PROP_CONTENT(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_) /* nothing */
#define DEFINED_CSS_PROP_CONTENT
#endif
#ifndef CSS_PROP_QUOTES
#define CSS_PROP_QUOTES(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_) /* nothing */
#define DEFINED_CSS_PROP_QUOTES
#endif
#ifndef CSS_PROP_USERINTERFACE
#define CSS_PROP_USERINTERFACE(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_) /* nothing */
#define DEFINED_CSS_PROP_USERINTERFACE
#endif
#ifndef CSS_PROP_UIRESET
#define CSS_PROP_UIRESET(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_) /* nothing */
#define DEFINED_CSS_PROP_UIRESET
#endif
#ifndef CSS_PROP_TABLE
#define CSS_PROP_TABLE(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_) /* nothing */
#define DEFINED_CSS_PROP_TABLE
#endif
#ifndef CSS_PROP_TABLEBORDER
#define CSS_PROP_TABLEBORDER(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_) /* nothing */
#define DEFINED_CSS_PROP_TABLEBORDER
#endif
#ifndef CSS_PROP_MARGIN
#define CSS_PROP_MARGIN(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_) /* nothing */
#define DEFINED_CSS_PROP_MARGIN
#endif
#ifndef CSS_PROP_PADDING
#define CSS_PROP_PADDING(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_) /* nothing */
#define DEFINED_CSS_PROP_PADDING
#endif
#ifndef CSS_PROP_BORDER
#define CSS_PROP_BORDER(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_) /* nothing */
#define DEFINED_CSS_PROP_BORDER
#endif
#ifndef CSS_PROP_OUTLINE
#define CSS_PROP_OUTLINE(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_) /* nothing */
#define DEFINED_CSS_PROP_OUTLINE
#endif
#ifndef CSS_PROP_XUL
#define CSS_PROP_XUL(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_) /* nothing */
#define DEFINED_CSS_PROP_XUL
#endif
#ifdef MOZ_SVG
#ifndef CSS_PROP_SVG
#define CSS_PROP_SVG(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_) /* nothing */
#define DEFINED_CSS_PROP_SVG
#endif
#endif /* defined(MOZ_SVG) */

#ifndef CSS_PROP_BACKENDONLY
#define CSS_PROP_BACKENDONLY(name_, id_, method_, hint_, datastruct_, member_, type_, iscoord_) /* nothing */
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

CSS_PROP_DISPLAY(-moz-appearance, appearance, MozAppearance, NS_STYLE_HINT_REFLOW, Display, mAppearance, eCSSType_Value, PR_FALSE)
CSS_PROP_SHORTHAND(-moz-border-radius, _moz_border_radius, MozBorderRadius, NS_STYLE_HINT_VISUAL)
CSS_PROP_BORDER(-moz-border-radius-topleft, _moz_border_radius_topLeft, MozBorderRadiusTopleft, NS_STYLE_HINT_VISUAL, Margin, mBorderRadius.mTop, eCSSType_Value, PR_TRUE)
CSS_PROP_BORDER(-moz-border-radius-topright, _moz_border_radius_topRight, MozBorderRadiusTopright, NS_STYLE_HINT_VISUAL, Margin, mBorderRadius.mRight, eCSSType_Value, PR_TRUE)
CSS_PROP_BORDER(-moz-border-radius-bottomleft, _moz_border_radius_bottomLeft, MozBorderRadiusBottomleft, NS_STYLE_HINT_VISUAL, Margin, mBorderRadius.mLeft, eCSSType_Value, PR_TRUE)
CSS_PROP_BORDER(-moz-border-radius-bottomright, _moz_border_radius_bottomRight, MozBorderRadiusBottomright, NS_STYLE_HINT_VISUAL, Margin, mBorderRadius.mBottom, eCSSType_Value, PR_TRUE)
CSS_PROP_SHORTHAND(-moz-outline-radius, _moz_outline_radius, MozOutlineRadius, NS_STYLE_HINT_VISUAL)
CSS_PROP_OUTLINE(-moz-outline-radius-topleft, _moz_outline_radius_topLeft, MozOutlineRadiusTopleft, NS_STYLE_HINT_VISUAL, Margin, mOutlineRadius.mTop, eCSSType_Value, PR_TRUE)
CSS_PROP_OUTLINE(-moz-outline-radius-topright, _moz_outline_radius_topRight, MozOutlineRadiusTopright, NS_STYLE_HINT_VISUAL, Margin, mOutlineRadius.mRight, eCSSType_Value, PR_TRUE)
CSS_PROP_OUTLINE(-moz-outline-radius-bottomleft, _moz_outline_radius_bottomLeft, MozOutlineRadiusBottomleft, NS_STYLE_HINT_VISUAL, Margin, mOutlineRadius.mLeft, eCSSType_Value, PR_TRUE)
CSS_PROP_OUTLINE(-moz-outline-radius-bottomright, _moz_outline_radius_bottomRight, MozOutlineRadiusBottomright, NS_STYLE_HINT_VISUAL, Margin, mOutlineRadius.mBottom, eCSSType_Value, PR_TRUE)
CSS_PROP_BACKENDONLY(azimuth, azimuth, Azimuth, NS_STYLE_HINT_AURAL, Aural, mAzimuth, eCSSType_Value, PR_FALSE)
CSS_PROP_SHORTHAND(background, background, Background, NS_STYLE_HINT_VISUAL)
CSS_PROP_BACKGROUND(background-attachment, background_attachment, BackgroundAttachment, NS_STYLE_HINT_FRAMECHANGE, Color, mBackAttachment, eCSSType_Value, PR_FALSE)
CSS_PROP_BACKGROUND(-moz-background-clip, _moz_background_clip, MozBackgroundClip, NS_STYLE_HINT_VISUAL, Color, mBackClip, eCSSType_Value, PR_FALSE)
CSS_PROP_BACKGROUND(background-color, background_color, BackgroundColor, NS_STYLE_HINT_VISUAL, Color, mBackColor, eCSSType_Value, PR_FALSE)
CSS_PROP_BACKGROUND(background-image, background_image, BackgroundImage, NS_STYLE_HINT_VISUAL, Color, mBackImage, eCSSType_Value, PR_FALSE)
CSS_PROP_BACKGROUND(-moz-background-inline-policy, _moz_background_inline_policy, MozBackgroundInlinePolicy, NS_STYLE_HINT_VISUAL, Color, mBackInlinePolicy, eCSSType_Value, PR_FALSE)
CSS_PROP_BACKGROUND(-moz-background-origin, _moz_background_origin, MozBackgroundOrigin, NS_STYLE_HINT_VISUAL, Color, mBackOrigin, eCSSType_Value, PR_FALSE)
CSS_PROP_SHORTHAND(background-position, background_position, BackgroundPosition, NS_STYLE_HINT_VISUAL)
CSS_PROP_BACKGROUND(background-repeat, background_repeat, BackgroundRepeat, NS_STYLE_HINT_VISUAL, Color, mBackRepeat, eCSSType_Value, PR_FALSE)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_BACKGROUND(-x-background-x-position, background_x_position, BackgroundXPosition, NS_STYLE_HINT_VISUAL, Color, mBackPositionX, eCSSType_Value, PR_FALSE) // XXX bug 3935
CSS_PROP_BACKGROUND(-x-background-y-position, background_y_position, BackgroundYPosition, NS_STYLE_HINT_VISUAL, Color, mBackPositionY, eCSSType_Value, PR_FALSE) // XXX bug 3935
#endif /* !defined (CSS_PROP_LIST_EXCLUDE_INTERNAL) */
CSS_PROP_DISPLAY(-moz-binding, binding, MozBinding, NS_STYLE_HINT_FRAMECHANGE, Display, mBinding, eCSSType_Value, PR_FALSE) // XXX bug 3935
CSS_PROP_SHORTHAND(border, border, Border, NS_STYLE_HINT_REFLOW)
CSS_PROP_SHORTHAND(border-bottom, border_bottom, BorderBottom, NS_STYLE_HINT_REFLOW)
CSS_PROP_BORDER(border-bottom-color, border_bottom_color, BorderBottomColor, NS_STYLE_HINT_VISUAL, Margin, mBorderColor.mBottom, eCSSType_Value, PR_FALSE)
CSS_PROP_BORDER(-moz-border-bottom-colors, border_bottom_colors, MozBorderBottomColors, NS_STYLE_HINT_VISUAL, Margin, mBorderColors.mBottom, eCSSType_ValueList, PR_FALSE)
CSS_PROP_BORDER(border-bottom-style, border_bottom_style, BorderBottomStyle, NS_STYLE_HINT_REFLOW, Margin, mBorderStyle.mBottom, eCSSType_Value, PR_FALSE)  // on/off will need reflow
CSS_PROP_BORDER(border-bottom-width, border_bottom_width, BorderBottomWidth, NS_STYLE_HINT_REFLOW, Margin, mBorderWidth.mBottom, eCSSType_Value, PR_FALSE)
CSS_PROP_TABLEBORDER(border-collapse, border_collapse, BorderCollapse, NS_STYLE_HINT_FRAMECHANGE, Table, mBorderCollapse, eCSSType_Value, PR_FALSE)
CSS_PROP_SHORTHAND(border-color, border_color, BorderColor, NS_STYLE_HINT_VISUAL)
CSS_PROP_SHORTHAND(border-left, border_left, BorderLeft, NS_STYLE_HINT_REFLOW)
CSS_PROP_BORDER(border-left-color, border_left_color, BorderLeftColor, NS_STYLE_HINT_VISUAL, Margin, mBorderColor.mLeft, eCSSType_Value, PR_FALSE)
CSS_PROP_BORDER(-moz-border-left-colors, border_left_colors, MozBorderLeftColors, NS_STYLE_HINT_VISUAL, Margin, mBorderColors.mLeft, eCSSType_ValueList, PR_FALSE)
CSS_PROP_BORDER(border-left-style, border_left_style, BorderLeftStyle, NS_STYLE_HINT_REFLOW, Margin, mBorderStyle.mLeft, eCSSType_Value, PR_FALSE)  // on/off will need reflow
CSS_PROP_BORDER(border-left-width, border_left_width, BorderLeftWidth, NS_STYLE_HINT_REFLOW, Margin, mBorderWidth.mLeft, eCSSType_Value, PR_FALSE)
CSS_PROP_SHORTHAND(border-right, border_right, BorderRight, NS_STYLE_HINT_REFLOW)
CSS_PROP_BORDER(border-right-color, border_right_color, BorderRightColor, NS_STYLE_HINT_VISUAL, Margin, mBorderColor.mRight, eCSSType_Value, PR_FALSE)
CSS_PROP_BORDER(-moz-border-right-colors, border_right_colors, MozBorderRightColors, NS_STYLE_HINT_VISUAL, Margin, mBorderColors.mRight, eCSSType_ValueList, PR_FALSE)
CSS_PROP_BORDER(border-right-style, border_right_style, BorderRightStyle, NS_STYLE_HINT_REFLOW, Margin, mBorderStyle.mRight, eCSSType_Value, PR_FALSE)  // on/off will need reflow
CSS_PROP_BORDER(border-right-width, border_right_width, BorderRightWidth, NS_STYLE_HINT_REFLOW, Margin, mBorderWidth.mRight, eCSSType_Value, PR_FALSE)
CSS_PROP_SHORTHAND(border-spacing, border_spacing, BorderSpacing, NS_STYLE_HINT_REFLOW)
CSS_PROP_SHORTHAND(border-style, border_style, BorderStyle, NS_STYLE_HINT_REFLOW)  // on/off will need reflow
CSS_PROP_SHORTHAND(border-top, border_top, BorderTop, NS_STYLE_HINT_REFLOW)
CSS_PROP_BORDER(border-top-color, border_top_color, BorderTopColor, NS_STYLE_HINT_VISUAL, Margin, mBorderColor.mTop, eCSSType_Value, PR_FALSE)
CSS_PROP_BORDER(-moz-border-top-colors, border_top_colors, MozBorderTopColors, NS_STYLE_HINT_VISUAL, Margin, mBorderColors.mTop, eCSSType_ValueList, PR_FALSE)
CSS_PROP_BORDER(border-top-style, border_top_style, BorderTopStyle, NS_STYLE_HINT_REFLOW, Margin, mBorderStyle.mTop, eCSSType_Value, PR_FALSE)  // on/off will need reflow
CSS_PROP_BORDER(border-top-width, border_top_width, BorderTopWidth, NS_STYLE_HINT_REFLOW, Margin, mBorderWidth.mTop, eCSSType_Value, PR_FALSE)
CSS_PROP_SHORTHAND(border-width, border_width, BorderWidth, NS_STYLE_HINT_REFLOW)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_TABLEBORDER(-x-border-x-spacing, border_x_spacing, BorderXSpacing, NS_STYLE_HINT_REFLOW, Table, mBorderSpacingX, eCSSType_Value, PR_FALSE) // XXX bug 3935
CSS_PROP_TABLEBORDER(-x-border-y-spacing, border_y_spacing, BorderYSpacing, NS_STYLE_HINT_REFLOW, Table, mBorderSpacingY, eCSSType_Value, PR_FALSE) // XXX bug 3935
#endif /* !defined (CSS_PROP_LIST_EXCLUDE_INTERNAL) */
CSS_PROP_POSITION(bottom, bottom, Bottom, NS_STYLE_HINT_REFLOW, Position, mOffset.mBottom, eCSSType_Value, PR_TRUE)
CSS_PROP_POSITION(-moz-box-sizing, box_sizing, MozBoxSizing, NS_STYLE_HINT_REFLOW, Position, mBoxSizing, eCSSType_Value, PR_FALSE) // XXX bug 3935
CSS_PROP_TABLEBORDER(caption-side, caption_side, CaptionSide, NS_STYLE_HINT_REFLOW, Table, mCaptionSide, eCSSType_Value, PR_FALSE)
CSS_PROP_DISPLAY(clear, clear, Clear, NS_STYLE_HINT_REFLOW, Display, mClear, eCSSType_Value, PR_FALSE)
CSS_PROP_DISPLAY(clip, clip, Clip, nsChangeHint_SyncFrameView, Display, mClip, eCSSType_Rect, PR_FALSE)
CSS_PROP_COLOR(color, color, Color, NS_STYLE_HINT_VISUAL, Color, mColor, eCSSType_Value, PR_FALSE)
CSS_PROP_CONTENT(content, content, Content, NS_STYLE_HINT_FRAMECHANGE, Content, mContent, eCSSType_ValueList, PR_FALSE)
CSS_PROP_NOTIMPLEMENTED(counter-increment, counter_increment, CounterIncrement, NS_STYLE_HINT_REFLOW)
CSS_PROP_NOTIMPLEMENTED(counter-reset, counter_reset, CounterReset, NS_STYLE_HINT_REFLOW)
CSS_PROP_CONTENT(-moz-counter-increment, _moz_counter_increment, MozCounterIncrement, NS_STYLE_HINT_REFLOW, Content, mCounterIncrement, eCSSType_CounterData, PR_FALSE) // XXX bug 137285
CSS_PROP_CONTENT(-moz-counter-reset, _moz_counter_reset, MozCounterReset, NS_STYLE_HINT_REFLOW, Content, mCounterReset, eCSSType_CounterData, PR_FALSE) // XXX bug 137285
CSS_PROP_SHORTHAND(cue, cue, Cue, NS_STYLE_HINT_AURAL)
CSS_PROP_BACKENDONLY(cue-after, cue_after, CueAfter, NS_STYLE_HINT_AURAL, Aural, mCueAfter, eCSSType_Value, PR_FALSE)
CSS_PROP_BACKENDONLY(cue-before, cue_before, CueBefore, NS_STYLE_HINT_AURAL, Aural, mCueBefore, eCSSType_Value, PR_FALSE)
CSS_PROP_USERINTERFACE(cursor, cursor, Cursor, NS_STYLE_HINT_VISUAL, UserInterface, mCursor, eCSSType_ValueList, PR_FALSE)
CSS_PROP_VISIBILITY(direction, direction, Direction, NS_STYLE_HINT_REFLOW, Display, mDirection, eCSSType_Value, PR_FALSE)
CSS_PROP_DISPLAY(display, display, Display, NS_STYLE_HINT_FRAMECHANGE, Display, mDisplay, eCSSType_Value, PR_FALSE)
CSS_PROP_BACKENDONLY(elevation, elevation, Elevation, NS_STYLE_HINT_AURAL, Aural, mElevation, eCSSType_Value, PR_FALSE)
CSS_PROP_TABLEBORDER(empty-cells, empty_cells, EmptyCells, NS_STYLE_HINT_VISUAL, Table, mEmptyCells, eCSSType_Value, PR_FALSE)
CSS_PROP_DISPLAY(float, float, CssFloat, NS_STYLE_HINT_FRAMECHANGE, Display, mFloat, eCSSType_Value, PR_FALSE)
CSS_PROP_BORDER(-moz-float-edge, float_edge, MozFloatEdge, NS_STYLE_HINT_REFLOW, Margin, mFloatEdge, eCSSType_Value, PR_FALSE) // XXX bug 3935
CSS_PROP_SHORTHAND(font, font, Font, NS_STYLE_HINT_REFLOW)
CSS_PROP_FONT(font-family, font_family, FontFamily, NS_STYLE_HINT_REFLOW, Font, mFamily, eCSSType_Value, PR_FALSE)
CSS_PROP_FONT(font-size, font_size, FontSize, NS_STYLE_HINT_REFLOW, Font, mSize, eCSSType_Value, PR_FALSE)
CSS_PROP_FONT(font-size-adjust, font_size_adjust, FontSizeAdjust, NS_STYLE_HINT_REFLOW, Font, mSizeAdjust, eCSSType_Value, PR_FALSE)
CSS_PROP_BACKENDONLY(font-stretch, font_stretch, FontStretch, NS_STYLE_HINT_REFLOW, Font, mStretch, eCSSType_Value, PR_FALSE)
CSS_PROP_FONT(font-style, font_style, FontStyle, NS_STYLE_HINT_REFLOW, Font, mStyle, eCSSType_Value, PR_FALSE)
CSS_PROP_FONT(font-variant, font_variant, FontVariant, NS_STYLE_HINT_REFLOW, Font, mVariant, eCSSType_Value, PR_FALSE)
CSS_PROP_FONT(font-weight, font_weight, FontWeight, NS_STYLE_HINT_REFLOW, Font, mWeight, eCSSType_Value, PR_FALSE)
CSS_PROP_UIRESET(-moz-force-broken-image-icon, force_broken_image_icon, MozForceBrokenImageIcon, NS_STYLE_HINT_FRAMECHANGE, UserInterface, mForceBrokenImageIcon, eCSSType_Value, PR_FALSE) // bug 58646
CSS_PROP_POSITION(height, height, Height, NS_STYLE_HINT_REFLOW, Position, mHeight, eCSSType_Value, PR_TRUE)
CSS_PROP_LIST(-moz-image-region, image_region, MozImageRegion, NS_STYLE_HINT_REFLOW, List, mImageRegion, eCSSType_Rect, PR_TRUE)
CSS_PROP_UIRESET(-moz-key-equivalent, key_equivalent, MozKeyEquivalent, NS_STYLE_HINT_CONTENT, UserInterface, mKeyEquivalent, eCSSType_ValueList, PR_FALSE) // This will need some other notification, but what? // XXX bug 3935
CSS_PROP_POSITION(left, left, Left, NS_STYLE_HINT_REFLOW, Position, mOffset.mLeft, eCSSType_Value, PR_TRUE)
CSS_PROP_TEXT(letter-spacing, letter_spacing, LetterSpacing, NS_STYLE_HINT_REFLOW, Text, mLetterSpacing, eCSSType_Value, PR_TRUE)
CSS_PROP_TEXT(line-height, line_height, LineHeight, NS_STYLE_HINT_REFLOW, Text, mLineHeight, eCSSType_Value, PR_TRUE)
CSS_PROP_SHORTHAND(list-style, list_style, ListStyle, NS_STYLE_HINT_REFLOW)
CSS_PROP_LIST(list-style-image, list_style_image, ListStyleImage, NS_STYLE_HINT_REFLOW, List, mImage, eCSSType_Value, PR_FALSE)
CSS_PROP_LIST(list-style-position, list_style_position, ListStylePosition, NS_STYLE_HINT_REFLOW, List, mPosition, eCSSType_Value, PR_FALSE)
CSS_PROP_LIST(list-style-type, list_style_type, ListStyleType, NS_STYLE_HINT_REFLOW, List, mType, eCSSType_Value, PR_FALSE)
CSS_PROP_SHORTHAND(margin, margin, Margin, NS_STYLE_HINT_REFLOW)
CSS_PROP_MARGIN(margin-bottom, margin_bottom, MarginBottom, NS_STYLE_HINT_REFLOW, Margin, mMargin.mBottom, eCSSType_Value, PR_TRUE)
CSS_PROP_MARGIN(margin-left, margin_left, MarginLeft, NS_STYLE_HINT_REFLOW, Margin, mMargin.mLeft, eCSSType_Value, PR_TRUE)
CSS_PROP_MARGIN(margin-right, margin_right, MarginRight, NS_STYLE_HINT_REFLOW, Margin, mMargin.mRight, eCSSType_Value, PR_TRUE)
CSS_PROP_MARGIN(margin-top, margin_top, MarginTop, NS_STYLE_HINT_REFLOW, Margin, mMargin.mTop, eCSSType_Value, PR_TRUE)
CSS_PROP_CONTENT(marker-offset, marker_offset, MarkerOffset, NS_STYLE_HINT_REFLOW, Content, mMarkerOffset, eCSSType_Value, PR_TRUE)
CSS_PROP_BACKENDONLY(marks, marks, Marks, NS_STYLE_HINT_VISUAL, Page, mMarks, eCSSType_Value, PR_FALSE)
CSS_PROP_POSITION(max-height, max_height, MaxHeight, NS_STYLE_HINT_REFLOW, Position, mMaxHeight, eCSSType_Value, PR_TRUE)
CSS_PROP_POSITION(max-width, max_width, MaxWidth, NS_STYLE_HINT_REFLOW, Position, mMaxWidth, eCSSType_Value, PR_TRUE)
CSS_PROP_POSITION(min-height, min_height, MinHeight, NS_STYLE_HINT_REFLOW, Position, mMinHeight, eCSSType_Value, PR_TRUE)
CSS_PROP_POSITION(min-width, min_width, MinWidth, NS_STYLE_HINT_REFLOW, Position, mMinWidth, eCSSType_Value, PR_TRUE)
CSS_PROP_VISIBILITY(-moz-opacity, opacity, MozOpacity, NS_STYLE_HINT_FRAMECHANGE, Display, mOpacity, eCSSType_Value, PR_FALSE) // XXX bug 3935
CSS_PROP_BACKENDONLY(orphans, orphans, Orphans, NS_STYLE_HINT_REFLOW, Breaks, mOrphans, eCSSType_Value, PR_FALSE)
CSS_PROP_NOTIMPLEMENTED(outline, outline, Outline, NS_STYLE_HINT_VISUAL)
CSS_PROP_NOTIMPLEMENTED(outline-color, outline_color, OutlineColor, NS_STYLE_HINT_VISUAL)
CSS_PROP_NOTIMPLEMENTED(outline-style, outline_style, OutlineStyle, NS_STYLE_HINT_VISUAL)
CSS_PROP_NOTIMPLEMENTED(outline-width, outline_width, OutlineWidth, NS_STYLE_HINT_VISUAL)
CSS_PROP_SHORTHAND(-moz-outline, _moz_outline, MozOutline, NS_STYLE_HINT_VISUAL)  // XXX This is temporary fix for nsbeta3+ Bug 48973, turning outline into -moz-outline  XXX bug 48973
CSS_PROP_OUTLINE(-moz-outline-color, _moz_outline_color, MozOutlineColor, NS_STYLE_HINT_VISUAL, Margin, mOutlineColor, eCSSType_Value, PR_FALSE) // XXX bug 48973
CSS_PROP_OUTLINE(-moz-outline-style, _moz_outline_style, MozOutlineStyle, NS_STYLE_HINT_VISUAL, Margin, mOutlineStyle, eCSSType_Value, PR_FALSE) // XXX bug 48973
CSS_PROP_OUTLINE(-moz-outline-width, _moz_outline_width, MozOutlineWidth, NS_STYLE_HINT_VISUAL, Margin, mOutlineWidth, eCSSType_Value, PR_TRUE) // XXX bug 48973
CSS_PROP_DISPLAY(overflow, overflow, Overflow, NS_STYLE_HINT_FRAMECHANGE, Display, mOverflow, eCSSType_Value, PR_FALSE)
CSS_PROP_SHORTHAND(padding, padding, Padding, NS_STYLE_HINT_REFLOW)
CSS_PROP_PADDING(padding-bottom, padding_bottom, PaddingBottom, NS_STYLE_HINT_REFLOW, Margin, mPadding.mBottom, eCSSType_Value, PR_TRUE)
CSS_PROP_PADDING(padding-left, padding_left, PaddingLeft, NS_STYLE_HINT_REFLOW, Margin, mPadding.mLeft, eCSSType_Value, PR_TRUE)
CSS_PROP_PADDING(padding-right, padding_right, PaddingRight, NS_STYLE_HINT_REFLOW, Margin, mPadding.mRight, eCSSType_Value, PR_TRUE)
CSS_PROP_PADDING(padding-top, padding_top, PaddingTop, NS_STYLE_HINT_REFLOW, Margin, mPadding.mTop, eCSSType_Value, PR_TRUE)
CSS_PROP_BACKENDONLY(page, page, Page, NS_STYLE_HINT_REFLOW, Breaks, mPage, eCSSType_Value, PR_FALSE)
CSS_PROP_DISPLAY(page-break-after, page_break_after, PageBreakAfter, NS_STYLE_HINT_REFLOW, Display, mBreakAfter, eCSSType_Value, PR_FALSE) // temp fix for bug 24000
CSS_PROP_DISPLAY(page-break-before, page_break_before, PageBreakBefore, NS_STYLE_HINT_REFLOW, Display, mBreakBefore, eCSSType_Value, PR_FALSE) // temp fix for bug 24000
CSS_PROP_BACKENDONLY(page-break-inside, page_break_inside, PageBreakInside, NS_STYLE_HINT_REFLOW, Breaks, mPageBreakInside, eCSSType_Value, PR_FALSE)
CSS_PROP_SHORTHAND(pause, pause, Pause, NS_STYLE_HINT_AURAL)
CSS_PROP_BACKENDONLY(pause-after, pause_after, PauseAfter, NS_STYLE_HINT_AURAL, Aural, mPauseAfter, eCSSType_Value, PR_FALSE)
CSS_PROP_BACKENDONLY(pause-before, pause_before, PauseBefore, NS_STYLE_HINT_AURAL, Aural, mPauseBefore, eCSSType_Value, PR_FALSE)
CSS_PROP_BACKENDONLY(pitch, pitch, Pitch, NS_STYLE_HINT_AURAL, Aural, mPitch, eCSSType_Value, PR_FALSE)
CSS_PROP_BACKENDONLY(pitch-range, pitch_range, PitchRange, NS_STYLE_HINT_AURAL, Aural, mPitchRange, eCSSType_Value, PR_FALSE)
CSS_PROP_BACKENDONLY(play-during, play_during, PlayDuring, NS_STYLE_HINT_AURAL, Aural, mPlayDuring, eCSSType_Value, PR_FALSE)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_BACKENDONLY(-x-play-during-flags, play_during_flags, PlayDuringFlags, NS_STYLE_HINT_AURAL, Aural, mPlayDuringFlags, eCSSType_Value, PR_FALSE) // XXX why is this here?
#endif /* !defined (CSS_PROP_LIST_EXCLUDE_INTERNAL) */
CSS_PROP_DISPLAY(position, position, Position, NS_STYLE_HINT_FRAMECHANGE, Display, mPosition, eCSSType_Value, PR_FALSE)
CSS_PROP_QUOTES(quotes, quotes, Quotes, NS_STYLE_HINT_REFLOW, Content, mQuotes, eCSSType_Quotes, PR_FALSE)
CSS_PROP_UIRESET(-moz-resizer, resizer, MozResizer, NS_STYLE_HINT_FRAMECHANGE, UserInterface, mResizer, eCSSType_Value, PR_FALSE) // XXX bug 3935
CSS_PROP_BACKENDONLY(richness, richness, Richness, NS_STYLE_HINT_AURAL, Aural, mRichness, eCSSType_Value, PR_FALSE)
CSS_PROP_POSITION(right, right, Right, NS_STYLE_HINT_REFLOW, Position, mOffset.mRight, eCSSType_Value, PR_TRUE)
CSS_PROP_SHORTHAND(size, size, Size, NS_STYLE_HINT_REFLOW)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_BACKENDONLY(-x-size-height, size_height, SizeHeight, NS_STYLE_HINT_REFLOW, Page, mSizeHeight, eCSSType_Value, PR_FALSE) // XXX bug 3935
CSS_PROP_BACKENDONLY(-x-size-width, size_width, SizeWidth, NS_STYLE_HINT_REFLOW, Page, mSizeWidth, eCSSType_Value, PR_FALSE) // XXX bug 3935
#endif /* !defined (CSS_PROP_LIST_EXCLUDE_INTERNAL) */
CSS_PROP_BACKENDONLY(speak, speak, Speak, NS_STYLE_HINT_AURAL, Aural, mSpeak, eCSSType_Value, PR_FALSE)
CSS_PROP_BACKENDONLY(speak-header, speak_header, SpeakHeader, NS_STYLE_HINT_AURAL, Aural, mSpeakHeader, eCSSType_Value, PR_FALSE)
CSS_PROP_BACKENDONLY(speak-numeral, speak_numeral, SpeakNumeral, NS_STYLE_HINT_AURAL, Aural, mSpeakNumeral, eCSSType_Value, PR_FALSE)
CSS_PROP_BACKENDONLY(speak-punctuation, speak_punctuation, SpeakPunctuation, NS_STYLE_HINT_AURAL, Aural, mSpeakPunctuation, eCSSType_Value, PR_FALSE)
CSS_PROP_BACKENDONLY(speech-rate, speech_rate, SpeechRate, NS_STYLE_HINT_AURAL, Aural, mSpeechRate, eCSSType_Value, PR_FALSE)
CSS_PROP_BACKENDONLY(stress, stress, Stress, NS_STYLE_HINT_AURAL, Aural, mStress, eCSSType_Value, PR_FALSE)
CSS_PROP_TABLE(table-layout, table_layout, TableLayout, NS_STYLE_HINT_REFLOW, Table, mLayout, eCSSType_Value, PR_FALSE)
CSS_PROP_TEXT(text-align, text_align, TextAlign, NS_STYLE_HINT_REFLOW, Text, mTextAlign, eCSSType_Value, PR_FALSE)
CSS_PROP_TEXTRESET(text-decoration, text_decoration, TextDecoration, NS_STYLE_HINT_VISUAL, Text, mDecoration, eCSSType_Value, PR_FALSE)
CSS_PROP_TEXT(text-indent, text_indent, TextIndent, NS_STYLE_HINT_REFLOW, Text, mTextIndent, eCSSType_Value, PR_TRUE)
CSS_PROP_BACKENDONLY(text-shadow, text_shadow, TextShadow, NS_STYLE_HINT_VISUAL, Text, mTextShadow, eCSSType_Shadow, PR_FALSE)
CSS_PROP_TEXT(text-transform, text_transform, TextTransform, NS_STYLE_HINT_REFLOW, Text, mTextTransform, eCSSType_Value, PR_FALSE)
CSS_PROP_POSITION(top, top, Top, NS_STYLE_HINT_REFLOW, Position, mOffset.mTop, eCSSType_Value, PR_TRUE)
CSS_PROP_TEXTRESET(unicode-bidi, unicode_bidi, UnicodeBidi, NS_STYLE_HINT_REFLOW, Text, mUnicodeBidi, eCSSType_Value, PR_FALSE)
CSS_PROP_USERINTERFACE(-moz-user-focus, user_focus, MozUserFocus, NS_STYLE_HINT_CONTENT, UserInterface, mUserFocus, eCSSType_Value, PR_FALSE) // XXX bug 3935
CSS_PROP_USERINTERFACE(-moz-user-input, user_input, MozUserInput, NS_STYLE_HINT_FRAMECHANGE, UserInterface, mUserInput, eCSSType_Value, PR_FALSE) // XXX ??? // XXX bug 3935
CSS_PROP_USERINTERFACE(-moz-user-modify, user_modify, MozUserModify, NS_STYLE_HINT_FRAMECHANGE, UserInterface, mUserModify, eCSSType_Value, PR_FALSE) // XXX bug 3935
CSS_PROP_UIRESET(-moz-user-select, user_select, MozUserSelect, NS_STYLE_HINT_CONTENT, UserInterface, mUserSelect, eCSSType_Value, PR_FALSE) // XXX bug 3935
CSS_PROP_TEXTRESET(vertical-align, vertical_align, VerticalAlign, NS_STYLE_HINT_REFLOW, Text, mVerticalAlign, eCSSType_Value, PR_TRUE)
CSS_PROP_VISIBILITY(visibility, visibility, Visibility, NS_STYLE_HINT_REFLOW, Display, mVisibility, eCSSType_Value, PR_FALSE)  // reflow for collapse
CSS_PROP_BACKENDONLY(voice-family, voice_family, VoiceFamily, NS_STYLE_HINT_AURAL, Aural, mVoiceFamily, eCSSType_Value, PR_FALSE)
CSS_PROP_BACKENDONLY(volume, volume, Volume, NS_STYLE_HINT_AURAL, Aural, mVolume, eCSSType_Value, PR_FALSE)
CSS_PROP_TEXT(white-space, white_space, WhiteSpace, NS_STYLE_HINT_REFLOW, Text, mWhiteSpace, eCSSType_Value, PR_FALSE)
CSS_PROP_BACKENDONLY(widows, widows, Widows, NS_STYLE_HINT_REFLOW, Breaks, mWidows, eCSSType_Value, PR_FALSE)
CSS_PROP_POSITION(width, width, Width, NS_STYLE_HINT_REFLOW, Position, mWidth, eCSSType_Value, PR_TRUE)
CSS_PROP_TEXT(word-spacing, word_spacing, WordSpacing, NS_STYLE_HINT_REFLOW, Text, mWordSpacing, eCSSType_Value, PR_TRUE)
CSS_PROP_POSITION(z-index, z_index, ZIndex, NS_STYLE_HINT_REFLOW, Position, mZIndex, eCSSType_Value, PR_FALSE)

CSS_PROP_XUL(-moz-box-align, box_align, MozBoxAlign, NS_STYLE_HINT_REFLOW, XUL, mBoxAlign, eCSSType_Value, PR_FALSE) // XXX bug 3935
CSS_PROP_XUL(-moz-box-direction, box_direction, MozBoxDirection, NS_STYLE_HINT_REFLOW, XUL, mBoxDirection, eCSSType_Value, PR_FALSE) // XXX bug 3935
CSS_PROP_XUL(-moz-box-flex, box_flex, MozBoxFlex, NS_STYLE_HINT_REFLOW, XUL, mBoxFlex, eCSSType_Value, PR_FALSE) // XXX bug 3935
CSS_PROP_XUL(-moz-box-orient, box_orient, MozBoxOrient, NS_STYLE_HINT_REFLOW, XUL, mBoxOrient, eCSSType_Value, PR_FALSE) // XXX bug 3935
CSS_PROP_XUL(-moz-box-pack, box_pack, MozBoxPack, NS_STYLE_HINT_REFLOW, XUL, mBoxPack, eCSSType_Value, PR_FALSE) // XXX bug 3935
CSS_PROP_XUL(-moz-box-ordinal-group, box_ordinal_group, MozBoxOrdinalGroup, NS_STYLE_HINT_FRAMECHANGE, XUL, mBoxOrdinal, eCSSType_Value, PR_FALSE)

#ifdef MOZ_SVG
// XXX treat SVG's CSS Properties as internal for now.
// Do we want to create an nsIDOMSVGCSS2Properties interface?
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_SVG(fill, fill, Fill, NS_STYLE_HINT_VISUAL, SVG, mFill, eCSSType_Value, PR_FALSE)
CSS_PROP_SVG(fill-opacity, fill_opacity, FillOpacity, NS_STYLE_HINT_VISUAL, SVG, mFillOpacity, eCSSType_Value, PR_FALSE)
CSS_PROP_SVG(fill-rule, fill_rule, FillRule, NS_STYLE_HINT_VISUAL, SVG, mFillRule, eCSSType_Value, PR_FALSE)
CSS_PROP_SVG(stroke, stroke, Stroke, NS_STYLE_HINT_VISUAL, SVG, mStroke, eCSSType_Value, PR_FALSE)
CSS_PROP_SVG(stroke-dasharray, stroke_dasharray, StrokeDasharray, NS_STYLE_HINT_VISUAL, SVG, mStrokeDasharray, eCSSType_Value, PR_FALSE)
CSS_PROP_SVG(stroke-dashoffset, stroke_dashoffset, StrokeDashoffset, NS_STYLE_HINT_VISUAL, SVG, mStrokeDashoffset, eCSSType_Value, PR_FALSE)
CSS_PROP_SVG(stroke-linecap, stroke_linecap, StrokeLinecap, NS_STYLE_HINT_VISUAL, SVG, mStrokeLinecap, eCSSType_Value, PR_FALSE)
CSS_PROP_SVG(stroke-linejoin, stroke_linejoin, StrokeLinejoin, NS_STYLE_HINT_VISUAL, SVG, mStrokeLinejoin, eCSSType_Value, PR_FALSE)
CSS_PROP_SVG(stroke-miterlimit, stroke_miterlimit, StrokeMiterlimit, NS_STYLE_HINT_VISUAL, SVG, mStrokeMiterlimit, eCSSType_Value, PR_FALSE)
CSS_PROP_SVG(stroke-opacity, stroke_opacity, StrokeOpacity, NS_STYLE_HINT_VISUAL, SVG, mStrokeOpacity, eCSSType_Value, PR_FALSE)
CSS_PROP_SVG(stroke-width, stroke_width, StrokeWidth, NS_STYLE_HINT_VISUAL, SVG, mStrokeWidth, eCSSType_Value, PR_FALSE)
#endif /* !defined (CSS_PROP_LIST_EXCLUDE_INTERNAL) */
#endif

// Callers that want information on the properties that are in
// the style structs but not in the nsCSS* structs should define
// |CSS_PROP_INCLUDE_NOT_CSS|.  (Some of these are also in nsRuleData*,
// and a distinction might be needed at some point.)
// The first 4 parameters don't matter, but some compilers don't like
// empty arguments to macros.
#ifdef CSS_PROP_INCLUDE_NOT_CSS
CSS_PROP_VISIBILITY(X, X, X, X, Display, mLang, eCSSType_Value, PR_FALSE)
CSS_PROP_TABLE(X, X, X, X, Table, mFrame, eCSSType_Value, PR_FALSE)
CSS_PROP_TABLE(X, X, X, X, Table, mRules, eCSSType_Value, PR_FALSE)
CSS_PROP_TABLE(X, X, X, X, Table, mCols, eCSSType_Value, PR_FALSE)
CSS_PROP_TABLE(X, X, X, X, Table, mSpan, eCSSType_Value, PR_FALSE)
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
#ifdef MOZ_SVG
#undef CSS_PROP_SVG
#endif
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
#ifdef MOZ_SVG
#ifdef DEFINED_CSS_PROP_SVG
#undef CSS_PROP_SVG
#undef DEFINED_CSS_PROP_SVG
#endif
#endif /* defined(MOZ_SVG) */
#ifdef DEFINED_CSS_PROP_BACKENDONLY
#undef CSS_PROP_BACKENDONLY
#undef DEFINED_CSS_PROP_BACKENDONLY
#endif

#endif /* !defined(USED_CSS_PROP) */

#ifdef DEFINED_CSS_PROP_NOTIMPLEMENTED
#undef CSS_PROP_NOTIMPLEMENTED
#undef DEFINED_CSS_PROP_NOTIMPLEMENTED
#endif

#ifdef DEFINED_CSS_PROP_SHORTHAND
#undef CSS_PROP_SHORTHAND
#undef DEFINED_CSS_PROP_SHORTHAND
#endif
