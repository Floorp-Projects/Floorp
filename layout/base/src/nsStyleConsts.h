/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef nsStyleConsts_h___
#define nsStyleConsts_h___

#include "nsFont.h"

// XXX fold this into nsIStyleContext and group by nsStyleXXX struct

// Defines for various style related constants

// See nsStyleColor
#define NS_STYLE_BG_ATTACHMENT_FIXED            0x01
#define NS_STYLE_BG_ATTACHMENT_SCROLL           0x02

// See nsStyleColor
#define NS_STYLE_BG_COLOR_TRANSPARENT           0x01
#define NS_STYLE_BG_IMAGE_NONE                  0x02
#define NS_STYLE_BG_X_POSITION_PERCENT          0x04
#define NS_STYLE_BG_X_POSITION_LENGTH           0x08
#define NS_STYLE_BG_Y_POSITION_PERCENT          0x10
#define NS_STYLE_BG_Y_POSITION_LENGTH           0x20

// See nsStyleColor
#define NS_STYLE_BG_REPEAT_OFF                  0x00
#define NS_STYLE_BG_REPEAT_X                    0x01
#define NS_STYLE_BG_REPEAT_Y                    0x02
#define NS_STYLE_BG_REPEAT_XY                   0x03

// See nsStyleSpacing mBorder enum values
#define NS_STYLE_BORDER_WIDTH_THIN              0
#define NS_STYLE_BORDER_WIDTH_MEDIUM            1
#define NS_STYLE_BORDER_WIDTH_THICK             2
// XXX chopping block #define NS_STYLE_BORDER_WIDTH_LENGTH_VALUE      3

// See nsStyleSpacing mBorderStyle
#define NS_STYLE_BORDER_STYLE_NONE              0
#define NS_STYLE_BORDER_STYLE_GROOVE            1
#define NS_STYLE_BORDER_STYLE_RIDGE             2
#define NS_STYLE_BORDER_STYLE_DOTTED            3
#define NS_STYLE_BORDER_STYLE_DASHED            4
#define NS_STYLE_BORDER_STYLE_SOLID             5
#define NS_STYLE_BORDER_STYLE_DOUBLE            6
#define NS_STYLE_BORDER_STYLE_BLANK             7
#define NS_STYLE_BORDER_STYLE_INSET             8
#define NS_STYLE_BORDER_STYLE_OUTSET            9

// See nsStyleDisplay
#define NS_STYLE_CLEAR_NONE                     0
#define NS_STYLE_CLEAR_LEFT                     1
#define NS_STYLE_CLEAR_RIGHT                    2
#define NS_STYLE_CLEAR_LEFT_AND_RIGHT           3
#define NS_STYLE_CLEAR_LINE                     4
#define NS_STYLE_CLEAR_BLOCK                    5
#define NS_STYLE_CLEAR_COLUMN                   6
#define NS_STYLE_CLEAR_PAGE                     7

// See nsStyleColor
#define NS_STYLE_CURSOR_INHERIT                 0
#define NS_STYLE_CURSOR_AUTO                    1
#define NS_STYLE_CURSOR_DEFAULT                 2
#define NS_STYLE_CURSOR_HAND                    3
#define NS_STYLE_CURSOR_IBEAM                   4

// See nsStyleDisplay
#define NS_STYLE_DIRECTION_LTR                  0
#define NS_STYLE_DIRECTION_RTL                  1
#define NS_STYLE_DIRECTION_INHERIT              2

// See nsStyleDisplay
#define NS_STYLE_DISPLAY_NONE                   0
#define NS_STYLE_DISPLAY_BLOCK                  1
#define NS_STYLE_DISPLAY_INLINE                 2
#define NS_STYLE_DISPLAY_LIST_ITEM              3

// See nsStyleDisplay
#define NS_STYLE_FLOAT_NONE                     0
#define NS_STYLE_FLOAT_LEFT                     1
#define NS_STYLE_FLOAT_RIGHT                    2

// See nsStyleFont
#define NS_STYLE_FONT_STYLE_NORMAL              0
#define NS_STYLE_FONT_STYLE_ITALIC              1
#define NS_STYLE_FONT_STYLE_OBLIQUE             2

// See nsStyleFont
#define NS_STYLE_FONT_VARIANT_NORMAL            0
#define NS_STYLE_FONT_VARIANT_SMALL_CAPS        1

// See nsStyleFont
#define NS_STYLE_FONT_WEIGHT_NORMAL             400
#define NS_STYLE_FONT_WEIGHT_BOLD               700
#define NS_STYLE_FONT_WEIGHT_BOLDER             100
#define NS_STYLE_FONT_WEIGHT_LIGHTER            -100

// See nsStyleFont
#define NS_STYLE_FONT_SIZE_XXSMALL              0
#define NS_STYLE_FONT_SIZE_XSMALL               1
#define NS_STYLE_FONT_SIZE_SMALL                2
#define NS_STYLE_FONT_SIZE_MEDIUM               3
#define NS_STYLE_FONT_SIZE_LARGE                4
#define NS_STYLE_FONT_SIZE_XLARGE               5
#define NS_STYLE_FONT_SIZE_XXLARGE              6
#define NS_STYLE_FONT_SIZE_LARGER               7
#define NS_STYLE_FONT_SIZE_SMALLER              8

// See nsStylePosition.mPosition
#define NS_STYLE_POSITION_NORMAL                0
#define NS_STYLE_POSITION_RELATIVE              1
#define NS_STYLE_POSITION_ABSOLUTE              2
#define NS_STYLE_POSITION_FIXED                 3

// See nsStylePosition.mClip
#define NS_STYLE_CLIP_AUTO                      0x00
#define NS_STYLE_CLIP_INHERIT                   0x01
#define NS_STYLE_CLIP_RECT                      0x02
#define NS_STYLE_CLIP_TYPE_MASK                 0x0F
#define NS_STYLE_CLIP_LEFT_AUTO                 0x10
#define NS_STYLE_CLIP_TOP_AUTO                  0x20
#define NS_STYLE_CLIP_RIGHT_AUTO                0x40
#define NS_STYLE_CLIP_BOTTOM_AUTO               0x80

// See nsStylePosition.mOverflow
#define NS_STYLE_OVERFLOW_VISIBLE               0
#define NS_STYLE_OVERFLOW_HIDDEN                1
#define NS_STYLE_OVERFLOW_SCROLL                2
#define NS_STYLE_OVERFLOW_AUTO                  3

// See nsStyleDisplay
#define NS_STYLE_LINE_HEIGHT_NORMAL             0

// See nsStyleList
#define NS_STYLE_LIST_STYLE_IMAGE_NONE          0

// See nsStyleList
#define NS_STYLE_LIST_STYLE_NONE                0
#define NS_STYLE_LIST_STYLE_DISC                1
#define NS_STYLE_LIST_STYLE_CIRCLE              2
#define NS_STYLE_LIST_STYLE_SQUARE              3
#define NS_STYLE_LIST_STYLE_DECIMAL             4
#define NS_STYLE_LIST_STYLE_LOWER_ROMAN         5
#define NS_STYLE_LIST_STYLE_UPPER_ROMAN         6
#define NS_STYLE_LIST_STYLE_LOWER_ALPHA         7
#define NS_STYLE_LIST_STYLE_UPPER_ALPHA         8
#define NS_STYLE_LIST_STYLE_BASIC               9       // not in css

// See nsStyleList
#define NS_STYLE_LIST_STYLE_POSITION_INSIDE     0
#define NS_STYLE_LIST_STYLE_POSITION_OUTSIDE    1

// See nsStyleSpacing
#define NS_STYLE_MARGIN_SIZE_AUTO               0

// See nsStyleText (word/letter)
#define NS_STYLE_SPACING_NORMAL                 0

// See nsStyleText
#define NS_STYLE_TEXT_ALIGN_LEFT                0
#define NS_STYLE_TEXT_ALIGN_RIGHT               1
#define NS_STYLE_TEXT_ALIGN_CENTER              2
#define NS_STYLE_TEXT_ALIGN_JUSTIFY             3

// See nsStyleText, nsStyleFont
#define NS_STYLE_TEXT_DECORATION_NONE           0
#define NS_STYLE_TEXT_DECORATION_UNDERLINE      NS_FONT_DECORATION_UNDERLINE
#define NS_STYLE_TEXT_DECORATION_OVERLINE       NS_FONT_DECORATION_OVERLINE
#define NS_STYLE_TEXT_DECORATION_LINE_THROUGH   NS_FONT_DECORATION_LINE_THROUGH
#define NS_STYLE_TEXT_DECORATION_BLINK          0x8

// See nsStyleText
#define NS_STYLE_TEXT_TRANSFORM_NONE            0
#define NS_STYLE_TEXT_TRANSFORM_CAPITALIZE      1
#define NS_STYLE_TEXT_TRANSFORM_LOWERCASE       2
#define NS_STYLE_TEXT_TRANSFORM_UPPERCASE       3

// See nsStyleText
// Note: these values pickup after the text-align values because there
// are a few html cases where an object can have both types of
// alignment applied with a single attribute
#define NS_STYLE_VERTICAL_ALIGN_BASELINE        10
#define NS_STYLE_VERTICAL_ALIGN_SUB             11
#define NS_STYLE_VERTICAL_ALIGN_SUPER           12
#define NS_STYLE_VERTICAL_ALIGN_TOP             13
#define NS_STYLE_VERTICAL_ALIGN_TEXT_TOP        14
#define NS_STYLE_VERTICAL_ALIGN_MIDDLE          15
#define NS_STYLE_VERTICAL_ALIGN_TEXT_BOTTOM     16
#define NS_STYLE_VERTICAL_ALIGN_BOTTOM          17
//#define NS_STYLE_VERTICAL_ALIGN_LENGTH          18
//#define NS_STYLE_VERTICAL_ALIGN_PERCENT         19

// See nsStyleDisplay
#define NS_STYLE_VISIBILITY_HIDDEN              0
#define NS_STYLE_VISIBILITY_VISIBLE             1

// See nsStyleText
#define NS_STYLE_WHITESPACE_NORMAL              0
#define NS_STYLE_WHITESPACE_PRE                 1
#define NS_STYLE_WHITESPACE_NOWRAP              2

// See nsStyleTable (here for HTML 4.0 for now, should probably change to side flags)
#define NS_STYLE_TABLE_FRAME_NONE               0
#define NS_STYLE_TABLE_FRAME_ABOVE              1
#define NS_STYLE_TABLE_FRAME_BELOW              2
#define NS_STYLE_TABLE_FRAME_HSIDES             3
#define NS_STYLE_TABLE_FRAME_VSIDES             4
#define NS_STYLE_TABLE_FRAME_LEFT               5
#define NS_STYLE_TABLE_FRAME_RIGHT              6
#define NS_STYLE_TABLE_FRAME_BOX                7
#define NS_STYLE_TABLE_FRAME_BORDER             8

// See nsStyleTable
#define NS_STYLE_TABLE_RULES_NONE               0
#define NS_STYLE_TABLE_RULES_GROUPS             1
#define NS_STYLE_TABLE_RULES_ROWS               2
#define NS_STYLE_TABLE_RULES_COLS               3
#define NS_STYLE_TABLE_RULES_ALL                4


#endif /* nsStyleConsts_h___ */
