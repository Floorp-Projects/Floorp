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

// Style change hints
#define NS_STYLE_HINT_UNKNOWN    -1
#define NS_STYLE_HINT_NONE        0   // change has no impact
#define NS_STYLE_HINT_AURAL       1   // change was aural
#define NS_STYLE_HINT_CONTENT     2   // change was contentual (ie: SRC=)
#define NS_STYLE_HINT_VISUAL      3   // change was visual only (ie: COLOR=)
#define NS_STYLE_HINT_REFLOW      4   // change requires reflow (ie: WIDTH=)
#define NS_STYLE_HINT_FRAMECHANGE 5   // change requires frame change (ie: display:)
#define NS_STYLE_HINT_RECONSTRUCT_ALL 6 // change requires reconstruction of entire document (ie: style sheet change)
#define NS_STYLE_HINT_MAX         NS_STYLE_HINT_RECONSTRUCT_ALL

// Indicies into border/padding/margin arrays
#define NS_SIDE_TOP     0
#define NS_SIDE_RIGHT   1
#define NS_SIDE_BOTTOM  2
#define NS_SIDE_LEFT    3


// Azimuth - See nsStyleAural
#define NS_STYLE_AZIMUTH_LEFT_SIDE        0x00
#define NS_STYLE_AZIMUTH_FAR_LEFT         0x01
#define NS_STYLE_AZIMUTH_LEFT             0x02
#define NS_STYLE_AZIMUTH_CENTER_LEFT      0x03
#define NS_STYLE_AZIMUTH_CENTER           0x04
#define NS_STYLE_AZIMUTH_CENTER_RIGHT     0x05
#define NS_STYLE_AZIMUTH_RIGHT            0x06
#define NS_STYLE_AZIMUTH_FAR_RIGHT        0x07
#define NS_STYLE_AZIMUTH_RIGHT_SIDE       0x08
#define NS_STYLE_AZIMUTH_BEHIND           0x80  // bits
#define NS_STYLE_AZIMUTH_LEFTWARDS        0x10  // bits
#define NS_STYLE_AZIMUTH_RIGHTWARDS       0x20  // bits

// See nsStyleAural
#define NS_STYLE_ELEVATION_BELOW          1
#define NS_STYLE_ELEVATION_LEVEL          2
#define NS_STYLE_ELEVATION_ABOVE          3
#define NS_STYLE_ELEVATION_HIGHER         4
#define NS_STYLE_ELEVATION_LOWER          5

// See nsStyleAural
#define NS_STYLE_PITCH_X_LOW              1
#define NS_STYLE_PITCH_LOW                2
#define NS_STYLE_PITCH_MEDIUM             3
#define NS_STYLE_PITCH_HIGH               4
#define NS_STYLE_PITCH_X_HIGH             5

// See nsStyleAural
#define NS_STYLE_PLAY_DURING_MIX          0x01  // bit field
#define NS_STYLE_PLAY_DURING_REPEAT       0x02

// See nsStyleAural
#define NS_STYLE_SPEAK_NONE               0
#define NS_STYLE_SPEAK_NORMAL             1
#define NS_STYLE_SPEAK_SPELL_OUT          2

// See nsStyleAural 
#define NS_STYLE_SPEAK_HEADER_ONCE        0
#define NS_STYLE_SPEAK_HEADER_ALWAYS      1

// See nsStyleAural 
#define NS_STYLE_SPEAK_NUMERAL_DIGITS     0
#define NS_STYLE_SPEAK_NUMERAL_CONTINUOUS 1

// See nsStyleAural 
#define NS_STYLE_SPEAK_PUNCTUATION_NONE   0
#define NS_STYLE_SPEAK_PUNCTUATION_CODE   1

// See nsStyleAural 
#define NS_STYLE_SPEECH_RATE_X_SLOW       0
#define NS_STYLE_SPEECH_RATE_SLOW         1
#define NS_STYLE_SPEECH_RATE_MEDIUM       2
#define NS_STYLE_SPEECH_RATE_FAST         3
#define NS_STYLE_SPEECH_RATE_X_FAST       4
#define NS_STYLE_SPEECH_RATE_FASTER       10
#define NS_STYLE_SPEECH_RATE_SLOWER       11

// See nsStyleAural 
#define NS_STYLE_VOLUME_SILENT            0
#define NS_STYLE_VOLUME_X_SOFT            1
#define NS_STYLE_VOLUME_SOFT              2
#define NS_STYLE_VOLUME_MEDIUM            3
#define NS_STYLE_VOLUME_LOUD              4
#define NS_STYLE_VOLUME_X_LOUD            5

// See nsStyleColor
#define NS_STYLE_BG_ATTACHMENT_FIXED            0x01
#define NS_STYLE_BG_ATTACHMENT_SCROLL           0x02

// See nsStyleColor
#define NS_STYLE_COLOR_TRANSPARENT              0
#define NS_STYLE_COLOR_INVERT                   1

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

// See nsStyleTable
#define NS_STYLE_BORDER_COLLAPSE                0
#define NS_STYLE_BORDER_SEPARATE                1

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
#define NS_STYLE_BORDER_STYLE_HIDDEN            10
#define NS_STYLE_BORDER_STYLE_BG_INSET          11
#define NS_STYLE_BORDER_STYLE_BG_OUTSET         12

// See nsStyleDisplay
#define NS_STYLE_CLEAR_NONE                     0
#define NS_STYLE_CLEAR_LEFT                     1
#define NS_STYLE_CLEAR_RIGHT                    2
#define NS_STYLE_CLEAR_LEFT_AND_RIGHT           3
#define NS_STYLE_CLEAR_LINE                     4
#define NS_STYLE_CLEAR_BLOCK                    5
#define NS_STYLE_CLEAR_COLUMN                   6
#define NS_STYLE_CLEAR_PAGE                     7

// See
#define NS_STYLE_CONTENT_OPEN_QUOTE             0
#define NS_STYLE_CONTENT_CLOSE_QUOTE            1
#define NS_STYLE_CONTENT_NO_OPEN_QUOTE          2
#define NS_STYLE_CONTENT_NO_CLOSE_QUOTE         3

// See nsStyleColor
#define NS_STYLE_CURSOR_AUTO                    1
#define NS_STYLE_CURSOR_CROSSHAIR               2
#define NS_STYLE_CURSOR_DEFAULT                 3    // ie: an arrow
#define NS_STYLE_CURSOR_POINTER                 4    // for links
#define NS_STYLE_CURSOR_MOVE                    5 
#define NS_STYLE_CURSOR_E_RESIZE                6     
#define NS_STYLE_CURSOR_NE_RESIZE               7      
#define NS_STYLE_CURSOR_NW_RESIZE               8      
#define NS_STYLE_CURSOR_N_RESIZE                9     
#define NS_STYLE_CURSOR_SE_RESIZE               10      
#define NS_STYLE_CURSOR_SW_RESIZE               11     
#define NS_STYLE_CURSOR_S_RESIZE                12    
#define NS_STYLE_CURSOR_W_RESIZE                13    
#define NS_STYLE_CURSOR_TEXT                    14   // ie: i-beam
#define NS_STYLE_CURSOR_WAIT                    15
#define NS_STYLE_CURSOR_HELP                    16

// See nsStyleDisplay
#define NS_STYLE_DIRECTION_LTR                  0
#define NS_STYLE_DIRECTION_RTL                  1
#define NS_STYLE_DIRECTION_INHERIT              2

// See nsStyleDisplay
#define NS_STYLE_DISPLAY_NONE                   0
#define NS_STYLE_DISPLAY_BLOCK                  1
#define NS_STYLE_DISPLAY_INLINE                 2
#define NS_STYLE_DISPLAY_LIST_ITEM              3
#define NS_STYLE_DISPLAY_MARKER                 4
#define NS_STYLE_DISPLAY_RUN_IN                 5
#define NS_STYLE_DISPLAY_COMPACT                6
#define NS_STYLE_DISPLAY_TABLE                  7
#define NS_STYLE_DISPLAY_INLINE_TABLE           8
#define NS_STYLE_DISPLAY_TABLE_ROW_GROUP        9
#define NS_STYLE_DISPLAY_TABLE_COLUMN           10
#define NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP     11
#define NS_STYLE_DISPLAY_TABLE_HEADER_GROUP     12
#define NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP     13
#define NS_STYLE_DISPLAY_TABLE_ROW              14
#define NS_STYLE_DISPLAY_TABLE_CELL             15
#define NS_STYLE_DISPLAY_TABLE_CAPTION          16

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
#define NS_STYLE_FONT_WEIGHT_BOLDER             101
#define NS_STYLE_FONT_WEIGHT_LIGHTER            -101

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

// See nsStyleFont
#define NS_STYLE_FONT_STRETCH_ULTRA_CONDENSED   -4
#define NS_STYLE_FONT_STRETCH_EXTRA_CONDENSED   -3
#define NS_STYLE_FONT_STRETCH_CONDENSED         -2
#define NS_STYLE_FONT_STRETCH_SEMI_CONDENSED    -1
#define NS_STYLE_FONT_STRETCH_NORMAL            0
#define NS_STYLE_FONT_STRETCH_SEMI_EXPANDED     1
#define NS_STYLE_FONT_STRETCH_EXPANDED          2
#define NS_STYLE_FONT_STRETCH_EXTRA_EXPANDED    3
#define NS_STYLE_FONT_STRETCH_ULTRA_EXPANDED    4
#define NS_STYLE_FONT_STRETCH_WIDER             10
#define NS_STYLE_FONT_STRETCH_NARROWER          -10

// See nsStyleFont mFlags
#define NS_STYLE_FONT_DEFAULT                   0x00
#define NS_STYLE_FONT_SIZE_EXPLICIT             0x01
#define NS_STYLE_FONT_FACE_EXPLICIT             0x02

// See nsStyleFont - system fonts
#define NS_STYLE_FONT_CAPTION                   1
#define NS_STYLE_FONT_ICON                      2
#define NS_STYLE_FONT_MENU                      3
#define NS_STYLE_FONT_MESSAGE_BOX               4
#define NS_STYLE_FONT_SMALL_CAPTION             5
#define NS_STYLE_FONT_STATUS_BAR                6

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

// FRAME/FRAMESET/IFRAME specific values including backward compatibility. Boolean values with
// the same meaning (e.g. 1 & yes) may need to be distinguished for correct mode processing 
#define NS_STYLE_FRAME_YES                      0
#define NS_STYLE_FRAME_NO                       1
#define NS_STYLE_FRAME_0                        2
#define NS_STYLE_FRAME_1                        3
#define NS_STYLE_FRAME_ON                       4
#define NS_STYLE_FRAME_OFF                      5
#define NS_STYLE_FRAME_AUTO                     6
#define NS_STYLE_FRAME_SCROLL                   7
#define NS_STYLE_FRAME_NOSCROLL                 8

// See nsStylePosition.mOverflow
#define NS_STYLE_OVERFLOW_VISIBLE               0
#define NS_STYLE_OVERFLOW_HIDDEN                1
#define NS_STYLE_OVERFLOW_SCROLL                2
#define NS_STYLE_OVERFLOW_AUTO                  3

// See nsStyleList
#define NS_STYLE_LIST_STYLE_NONE                  0
#define NS_STYLE_LIST_STYLE_DISC                  1
#define NS_STYLE_LIST_STYLE_CIRCLE                2
#define NS_STYLE_LIST_STYLE_SQUARE                3
#define NS_STYLE_LIST_STYLE_DECIMAL               4
#define NS_STYLE_LIST_STYLE_DECIMAL_LEADING_ZERO  5
#define NS_STYLE_LIST_STYLE_LOWER_ROMAN           6
#define NS_STYLE_LIST_STYLE_UPPER_ROMAN           7
#define NS_STYLE_LIST_STYLE_LOWER_GREEK           8
#define NS_STYLE_LIST_STYLE_LOWER_ALPHA           9
#define NS_STYLE_LIST_STYLE_LOWER_LATIN           9   // == ALPHA
#define NS_STYLE_LIST_STYLE_UPPER_ALPHA           10
#define NS_STYLE_LIST_STYLE_UPPER_LATIN           10  // == ALPHA
#define NS_STYLE_LIST_STYLE_HEBREW                11
#define NS_STYLE_LIST_STYLE_ARMENIAN              12
#define NS_STYLE_LIST_STYLE_GEORGIAN              13
#define NS_STYLE_LIST_STYLE_CJK_IDEOGRAPHIC       14
#define NS_STYLE_LIST_STYLE_HIRAGANA              15
#define NS_STYLE_LIST_STYLE_KATAKANA              16
#define NS_STYLE_LIST_STYLE_HIRAGANA_IROHA        17
#define NS_STYLE_LIST_STYLE_KATAKANA_IROHA        18
#define NS_STYLE_LIST_STYLE_BASIC                 100  // not in css

// See nsStyleList
#define NS_STYLE_LIST_STYLE_POSITION_INSIDE     0
#define NS_STYLE_LIST_STYLE_POSITION_OUTSIDE    1

// See nsStyleSpacing
#define NS_STYLE_MARGIN_SIZE_AUTO               0

// See nsStyleText
// 
// Note: make sure the numbers are less than the numbers that start
// the vertical_align values below!
#define NS_STYLE_TEXT_ALIGN_DEFAULT             0
#define NS_STYLE_TEXT_ALIGN_LEFT                1
#define NS_STYLE_TEXT_ALIGN_RIGHT               2
#define NS_STYLE_TEXT_ALIGN_CENTER              3
#define NS_STYLE_TEXT_ALIGN_JUSTIFY             4
#define NS_STYLE_TEXT_ALIGN_CHAR                5   //align based on a certain character, for table cell
#define NS_STYLE_TEXT_ALIGN_MOZ_CENTER          6
#define NS_STYLE_TEXT_ALIGN_MOZ_RIGHT           7

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

// See nsStyleDisplay
#define NS_STYLE_VISIBILITY_HIDDEN              0
#define NS_STYLE_VISIBILITY_VISIBLE             1
#define NS_STYLE_VISIBILITY_COLLAPSE            2

// See nsStyleText
#define NS_STYLE_WHITESPACE_NORMAL              0
#define NS_STYLE_WHITESPACE_PRE                 1
#define NS_STYLE_WHITESPACE_NOWRAP              2
#define NS_STYLE_WHITESPACE_MOZ_PRE_WRAP        3

// See nsStyleText
#define NS_STYLE_UNICODE_BIDI_NORMAL            0
#define NS_STYLE_UNICODE_BIDI_EMBED             1
#define NS_STYLE_UNICODE_BIDI_OVERRIDE          2

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

#define NS_STYLE_TABLE_COLS_NONE                (-1)
#define NS_STYLE_TABLE_COLS_ALL                 PRInt32(1 << 30)

#define NS_STYLE_TABLE_LAYOUT_AUTO              0
#define NS_STYLE_TABLE_LAYOUT_FIXED             1

#define NS_STYLE_TABLE_EMPTY_CELLS_HIDE         0
#define NS_STYLE_TABLE_EMPTY_CELLS_SHOW         1

// CAPTION_SIDE uses NS_SIDE_*

// constants for cell "scope" attribute
#define NS_STYLE_CELL_SCOPE_ROW                 0
#define NS_STYLE_CELL_SCOPE_COL                 1
#define NS_STYLE_CELL_SCOPE_ROWGROUP            2
#define NS_STYLE_CELL_SCOPE_COLGROUP            3

// See nsStylePage
#define NS_STYLE_PAGE_MARKS_NONE                0x00
#define NS_STYLE_PAGE_MARKS_CROP                0x01
#define NS_STYLE_PAGE_MARKS_REGISTER            0x02

// See nsStylePage
#define NS_STYLE_PAGE_SIZE_AUTO                 0
#define NS_STYLE_PAGE_SIZE_PORTRAIT             1
#define NS_STYLE_PAGE_SIZE_LANDSCAPE            2

// See nsStyleBreaks
#define NS_STYLE_PAGE_BREAK_AUTO                0
#define NS_STYLE_PAGE_BREAK_ALWAYS              1
#define NS_STYLE_PAGE_BREAK_AVOID               2
#define NS_STYLE_PAGE_BREAK_LEFT                3
#define NS_STYLE_PAGE_BREAK_RIGHT               4

#endif /* nsStyleConsts_h___ */
