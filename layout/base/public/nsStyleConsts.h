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
#ifndef nsStyleConsts_h___
#define nsStyleConsts_h___

#include "nsFont.h"

// XXX fold this into nsStyleContext and group by nsStyleXXX struct

// Indices into border/padding/margin arrays
#define NS_SIDE_TOP     0
#define NS_SIDE_RIGHT   1
#define NS_SIDE_BOTTOM  2
#define NS_SIDE_LEFT    3

#define NS_FOR_CSS_SIDES(var_) for (PRInt32 var_ = 0; var_ < 4; ++var_)

// {margin,padding}-{left,right}-{ltr,rtl}-source
#define NS_BOXPROP_SOURCE_PHYSICAL 0
#define NS_BOXPROP_SOURCE_LOGICAL  1

// box-sizing
#define NS_STYLE_BOX_SIZING_CONTENT       0
#define NS_STYLE_BOX_SIZING_PADDING       1
#define NS_STYLE_BOX_SIZING_BORDER        2

// float-edge
#define NS_STYLE_FLOAT_EDGE_CONTENT       0
#define NS_STYLE_FLOAT_EDGE_PADDING       1
#define NS_STYLE_FLOAT_EDGE_BORDER        2
#define NS_STYLE_FLOAT_EDGE_MARGIN        3

// key-equivalent
#define NS_STYLE_KEY_EQUIVALENT_NONE      0

// user-focus
#define NS_STYLE_USER_FOCUS_NONE            0
#define NS_STYLE_USER_FOCUS_IGNORE          1
#define NS_STYLE_USER_FOCUS_NORMAL          2
#define NS_STYLE_USER_FOCUS_SELECT_ALL      3
#define NS_STYLE_USER_FOCUS_SELECT_BEFORE   4
#define NS_STYLE_USER_FOCUS_SELECT_AFTER    5
#define NS_STYLE_USER_FOCUS_SELECT_SAME     6
#define NS_STYLE_USER_FOCUS_SELECT_MENU     7

// user-select
#define NS_STYLE_USER_SELECT_NONE       0
#define NS_STYLE_USER_SELECT_TEXT       1
#define NS_STYLE_USER_SELECT_ELEMENT    2
#define NS_STYLE_USER_SELECT_ELEMENTS   3
#define NS_STYLE_USER_SELECT_ALL        4
#define NS_STYLE_USER_SELECT_TOGGLE     5
#define NS_STYLE_USER_SELECT_TRI_STATE  6
#define NS_STYLE_USER_SELECT_AUTO       7 // internal value - please use nsFrame::IsSelectable()
#define NS_STYLE_USER_SELECT_MOZ_ALL    8 // force selection of all children - bug 48096

// user-input
#define NS_STYLE_USER_INPUT_NONE      0
#define NS_STYLE_USER_INPUT_ENABLED   1
#define NS_STYLE_USER_INPUT_DISABLED  2
#define NS_STYLE_USER_INPUT_AUTO      3

// user-modify
#define NS_STYLE_USER_MODIFY_READ_ONLY   0
#define NS_STYLE_USER_MODIFY_READ_WRITE  1
#define NS_STYLE_USER_MODIFY_WRITE_ONLY  2

// box-align
#define NS_STYLE_BOX_ALIGN_STRETCH     0
#define NS_STYLE_BOX_ALIGN_START       1
#define NS_STYLE_BOX_ALIGN_CENTER      2
#define NS_STYLE_BOX_ALIGN_BASELINE    3
#define NS_STYLE_BOX_ALIGN_END         4

// box-pack
#define NS_STYLE_BOX_PACK_START        0
#define NS_STYLE_BOX_PACK_CENTER       1
#define NS_STYLE_BOX_PACK_END          2
#define NS_STYLE_BOX_PACK_JUSTIFY      3

// box-direction
#define NS_STYLE_BOX_DIRECTION_NORMAL    0
#define NS_STYLE_BOX_DIRECTION_REVERSE   1

// box-orient
#define NS_STYLE_BOX_ORIENT_HORIZONTAL 0
#define NS_STYLE_BOX_ORIENT_VERTICAL   1

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
#define NS_STYLE_COLOR_TRANSPARENT        0
#define NS_STYLE_COLOR_INVERT             1
#define NS_STYLE_COLOR_MOZ_USE_TEXT_COLOR      2

// See nsStyleColor
#define NS_COLOR_MOZ_HYPERLINKTEXT              -1
#define NS_COLOR_MOZ_VISITEDHYPERLINKTEXT       -2
#define NS_COLOR_MOZ_ACTIVEHYPERLINKTEXT        -3

// See nsStyleBackground
#define NS_STYLE_BG_COLOR_TRANSPARENT           0x01
#define NS_STYLE_BG_IMAGE_NONE                  0x02
#define NS_STYLE_BG_X_POSITION_PERCENT          0x04
#define NS_STYLE_BG_X_POSITION_LENGTH           0x08
#define NS_STYLE_BG_Y_POSITION_PERCENT          0x10
#define NS_STYLE_BG_Y_POSITION_LENGTH           0x20

// See nsStyleBackground
#define NS_STYLE_BG_ATTACHMENT_SCROLL     0
#define NS_STYLE_BG_ATTACHMENT_FIXED      1

// See nsStyleBackground
#define NS_STYLE_BG_CLIP_BORDER           0
#define NS_STYLE_BG_CLIP_PADDING          1

// See nsStyleBackground
#define NS_STYLE_BG_INLINE_POLICY_EACH_BOX      0
#define NS_STYLE_BG_INLINE_POLICY_CONTINUOUS    1
#define NS_STYLE_BG_INLINE_POLICY_BOUNDING_BOX  2

// See nsStyleBackground
#define NS_STYLE_BG_ORIGIN_BORDER         0
#define NS_STYLE_BG_ORIGIN_PADDING        1
#define NS_STYLE_BG_ORIGIN_CONTENT        2

// See nsStyleBackground
#define NS_STYLE_BG_REPEAT_OFF                  0x00
#define NS_STYLE_BG_REPEAT_X                    0x01
#define NS_STYLE_BG_REPEAT_Y                    0x02
#define NS_STYLE_BG_REPEAT_XY                   0x03

// See nsStyleTable
#define NS_STYLE_BORDER_COLLAPSE                0
#define NS_STYLE_BORDER_SEPARATE                1

// See nsStyleBorder mBorder enum values
#define NS_STYLE_BORDER_WIDTH_THIN              0
#define NS_STYLE_BORDER_WIDTH_MEDIUM            1
#define NS_STYLE_BORDER_WIDTH_THICK             2
// XXX chopping block #define NS_STYLE_BORDER_WIDTH_LENGTH_VALUE      3

// See nsStyleBorder mBorderStyle
#define NS_STYLE_BORDER_STYLE_NONE              0
#define NS_STYLE_BORDER_STYLE_GROOVE            1
#define NS_STYLE_BORDER_STYLE_RIDGE             2
#define NS_STYLE_BORDER_STYLE_DOTTED            3
#define NS_STYLE_BORDER_STYLE_DASHED            4
#define NS_STYLE_BORDER_STYLE_SOLID             5
#define NS_STYLE_BORDER_STYLE_DOUBLE            6
#define NS_STYLE_BORDER_STYLE_INSET             7
#define NS_STYLE_BORDER_STYLE_OUTSET            8
#define NS_STYLE_BORDER_STYLE_HIDDEN            9
#define NS_STYLE_BORDER_STYLE_BG_INSET          10
#define NS_STYLE_BORDER_STYLE_BG_OUTSET         11
#define NS_STYLE_BORDER_STYLE_BG_SOLID          12
// a bit ORed onto the style for table border collapsing indicating that the style was 
// derived from a table with its rules attribute set
#define NS_STYLE_BORDER_STYLE_RULES_MASK      0x10  

// See nsStyleDisplay
#define NS_STYLE_CLEAR_NONE                     0
#define NS_STYLE_CLEAR_LEFT                     1
#define NS_STYLE_CLEAR_RIGHT                    2
#define NS_STYLE_CLEAR_LEFT_AND_RIGHT           3
#define NS_STYLE_CLEAR_LINE                     4
#define NS_STYLE_CLEAR_BLOCK                    5
#define NS_STYLE_CLEAR_COLUMN                   6
#define NS_STYLE_CLEAR_PAGE                     7
#define NS_STYLE_CLEAR_LAST_VALUE NS_STYLE_CLEAR_PAGE

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
#define NS_STYLE_CURSOR_COPY                    17   // CSS3
#define NS_STYLE_CURSOR_ALIAS                   18
#define NS_STYLE_CURSOR_CONTEXT_MENU            19
#define NS_STYLE_CURSOR_CELL                    20
#define NS_STYLE_CURSOR_GRAB                    21
#define NS_STYLE_CURSOR_GRABBING                22
#define NS_STYLE_CURSOR_SPINNING                23
#define NS_STYLE_CURSOR_COUNT_UP                24
#define NS_STYLE_CURSOR_COUNT_DOWN              25
#define NS_STYLE_CURSOR_COUNT_UP_DOWN           26
#define NS_STYLE_CURSOR_MOZ_ZOOM_IN             27
#define NS_STYLE_CURSOR_MOZ_ZOOM_OUT            28


// See nsStyleDisplay
#define NS_STYLE_DIRECTION_LTR                  0
#define NS_STYLE_DIRECTION_RTL                  1
#define NS_STYLE_DIRECTION_INHERIT              2

// See nsStyleDisplay
#define NS_STYLE_DISPLAY_NONE                   0
#define NS_STYLE_DISPLAY_BLOCK                  1
#define NS_STYLE_DISPLAY_INLINE                 2
#define NS_STYLE_DISPLAY_INLINE_BLOCK           3
#define NS_STYLE_DISPLAY_LIST_ITEM              4
#define NS_STYLE_DISPLAY_MARKER                 5
#define NS_STYLE_DISPLAY_RUN_IN                 6
#define NS_STYLE_DISPLAY_COMPACT                7
#define NS_STYLE_DISPLAY_TABLE                  8
#define NS_STYLE_DISPLAY_INLINE_TABLE           9
#define NS_STYLE_DISPLAY_TABLE_ROW_GROUP        10
#define NS_STYLE_DISPLAY_TABLE_COLUMN           11
#define NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP     12
#define NS_STYLE_DISPLAY_TABLE_HEADER_GROUP     13
#define NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP     14
#define NS_STYLE_DISPLAY_TABLE_ROW              15
#define NS_STYLE_DISPLAY_TABLE_CELL             16
#define NS_STYLE_DISPLAY_TABLE_CAPTION          17
#define NS_STYLE_DISPLAY_BOX                    18
#define NS_STYLE_DISPLAY_INLINE_BOX             19
#define NS_STYLE_DISPLAY_GRID                   20
#define NS_STYLE_DISPLAY_INLINE_GRID            21
#define NS_STYLE_DISPLAY_GRID_GROUP             22
#define NS_STYLE_DISPLAY_GRID_LINE              23
#define NS_STYLE_DISPLAY_STACK                  24
#define NS_STYLE_DISPLAY_INLINE_STACK           25
#define NS_STYLE_DISPLAY_DECK                   26
#define NS_STYLE_DISPLAY_BULLETINBOARD          27
#define NS_STYLE_DISPLAY_POPUP                  28
#define NS_STYLE_DISPLAY_GROUPBOX               29
#define NS_STYLE_DISPLAY_PAGE_BREAK             30

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
#define NS_STYLE_FONT_WEIGHT_BOLDER             1
#define NS_STYLE_FONT_WEIGHT_LIGHTER            -1

// See nsStyleFont
#define NS_STYLE_FONT_SIZE_XXSMALL              0
#define NS_STYLE_FONT_SIZE_XSMALL               1
#define NS_STYLE_FONT_SIZE_SMALL                2
#define NS_STYLE_FONT_SIZE_MEDIUM               3
#define NS_STYLE_FONT_SIZE_LARGE                4
#define NS_STYLE_FONT_SIZE_XLARGE               5
#define NS_STYLE_FONT_SIZE_XXLARGE              6
#define NS_STYLE_FONT_SIZE_XXXLARGE             7  // Only used by <font size="7">. Not specifiable in CSS.
#define NS_STYLE_FONT_SIZE_LARGER               8
#define NS_STYLE_FONT_SIZE_SMALLER              9

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
#define NS_STYLE_FONT_FACE_MASK                 0xFF // used to flag generic fonts

// See nsStyleFont - system fonts
#define NS_STYLE_FONT_CAPTION                   1		// css2
#define NS_STYLE_FONT_ICON                      2
#define NS_STYLE_FONT_MENU                      3
#define NS_STYLE_FONT_MESSAGE_BOX               4
#define NS_STYLE_FONT_SMALL_CAPTION             5
#define NS_STYLE_FONT_STATUS_BAR                6
#define NS_STYLE_FONT_WINDOW										7		// css3
#define NS_STYLE_FONT_DOCUMENT									8
#define NS_STYLE_FONT_WORKSPACE									9
#define NS_STYLE_FONT_DESKTOP										10
#define NS_STYLE_FONT_INFO											11
#define NS_STYLE_FONT_DIALOG										12
#define NS_STYLE_FONT_BUTTON										13
#define NS_STYLE_FONT_PULL_DOWN_MENU						14
#define NS_STYLE_FONT_LIST											15
#define NS_STYLE_FONT_FIELD											16

// See nsStylePosition.mPosition
#define NS_STYLE_POSITION_STATIC                0
#define NS_STYLE_POSITION_RELATIVE              1
#define NS_STYLE_POSITION_ABSOLUTE              2
#define NS_STYLE_POSITION_FIXED                 3

// See nsStylePosition.mClip
#define NS_STYLE_CLIP_AUTO                      0x00
#define NS_STYLE_CLIP_RECT                      0x01
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
#define NS_STYLE_OVERFLOW_SCROLLBARS_NONE				4
#define NS_STYLE_OVERFLOW_SCROLLBARS_HORIZONTAL	5
#define NS_STYLE_OVERFLOW_SCROLLBARS_VERTICAL		6

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
#define NS_STYLE_LIST_STYLE_OLD_LOWER_ROMAN       19
#define NS_STYLE_LIST_STYLE_OLD_UPPER_ROMAN       20
#define NS_STYLE_LIST_STYLE_OLD_LOWER_ALPHA       21
#define NS_STYLE_LIST_STYLE_OLD_UPPER_ALPHA       22
#define NS_STYLE_LIST_STYLE_OLD_DECIMAL           23
#define NS_STYLE_LIST_STYLE_MOZ_CJK_HEAVENLY_STEM     24
#define NS_STYLE_LIST_STYLE_MOZ_CJK_EARTHLY_BRANCH    25
#define NS_STYLE_LIST_STYLE_MOZ_TRAD_CHINESE_INFORMAL 26
#define NS_STYLE_LIST_STYLE_MOZ_TRAD_CHINESE_FORMAL   27
#define NS_STYLE_LIST_STYLE_MOZ_SIMP_CHINESE_INFORMAL 28
#define NS_STYLE_LIST_STYLE_MOZ_SIMP_CHINESE_FORMAL   29
#define NS_STYLE_LIST_STYLE_MOZ_JAPANESE_INFORMAL     30
#define NS_STYLE_LIST_STYLE_MOZ_JAPANESE_FORMAL       31
#define NS_STYLE_LIST_STYLE_MOZ_ARABIC_INDIC          32
#define NS_STYLE_LIST_STYLE_MOZ_PERSIAN               33
#define NS_STYLE_LIST_STYLE_MOZ_URDU                  34 
#define NS_STYLE_LIST_STYLE_MOZ_DEVANAGARI            35
#define NS_STYLE_LIST_STYLE_MOZ_GURMUKHI              36
#define NS_STYLE_LIST_STYLE_MOZ_GUJARATI              37
#define NS_STYLE_LIST_STYLE_MOZ_ORIYA                 38
#define NS_STYLE_LIST_STYLE_MOZ_KANNADA               39
#define NS_STYLE_LIST_STYLE_MOZ_MALAYALAM             40
#define NS_STYLE_LIST_STYLE_MOZ_BENGALI               41
#define NS_STYLE_LIST_STYLE_MOZ_TAMIL                 42
#define NS_STYLE_LIST_STYLE_MOZ_TELUGU                43
#define NS_STYLE_LIST_STYLE_MOZ_THAI                  44
#define NS_STYLE_LIST_STYLE_MOZ_LAO                   45
#define NS_STYLE_LIST_STYLE_MOZ_MYANMAR               46
#define NS_STYLE_LIST_STYLE_MOZ_KHMER                 47
#define NS_STYLE_LIST_STYLE_MOZ_HANGUL                48
#define NS_STYLE_LIST_STYLE_MOZ_HANGUL_CONSONANT      49
#define NS_STYLE_LIST_STYLE_MOZ_ETHIOPIC_HALEHAME     50
#define NS_STYLE_LIST_STYLE_MOZ_ETHIOPIC_NUMERIC      51
#define NS_STYLE_LIST_STYLE_MOZ_ETHIOPIC_HALEHAME_AM  52
#define NS_STYLE_LIST_STYLE_MOZ_ETHIOPIC_HALEHAME_TI_ER  53
#define NS_STYLE_LIST_STYLE_MOZ_ETHIOPIC_HALEHAME_TI_ET  54

// See nsStyleList
#define NS_STYLE_LIST_STYLE_POSITION_INSIDE     0
#define NS_STYLE_LIST_STYLE_POSITION_OUTSIDE    1

// See nsStyleMargin
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
#define NS_STYLE_TEXT_DECORATION_BLINK          0x08
#define NS_STYLE_TEXT_DECORATION_OVERRIDE_ALL   0x10
#define NS_STYLE_TEXT_DECORATION_PREF_ANCHORS   0x20
#define NS_STYLE_TEXT_DECORATION_LINES_MASK     (NS_STYLE_TEXT_DECORATION_UNDERLINE | NS_STYLE_TEXT_DECORATION_OVERLINE | NS_STYLE_TEXT_DECORATION_LINE_THROUGH)

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

#define NS_STYLE_TABLE_EMPTY_CELLS_HIDE            0
#define NS_STYLE_TABLE_EMPTY_CELLS_SHOW            1
#define NS_STYLE_TABLE_EMPTY_CELLS_SHOW_BACKGROUND 2

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

// See nsStyleColumn
#define NS_STYLE_COLUMN_COUNT_AUTO              0
#define NS_STYLE_COLUMN_COUNT_UNLIMITED         (-1)

#ifdef MOZ_SVG
// Some of our constants must map to the same values as those defined in
// nsISVG{,Path,Glyph}GeometrySource.idl/
// I don't want to add a dependency on the SVG module
// everywhere by #include'ing nsISVG{,Path,Glyph}GeometrySource.h, so these consts
// have to be kept in sync manually.

// dominant-baseline
#define NS_STYLE_DOMINANT_BASELINE_AUTO              0
#define NS_STYLE_DOMINANT_BASELINE_USE_SCRIPT        1
#define NS_STYLE_DOMINANT_BASELINE_NO_CHANGE         2
#define NS_STYLE_DOMINANT_BASELINE_RESET_SIZE        3
#define NS_STYLE_DOMINANT_BASELINE_ALPHABETIC        4
#define NS_STYLE_DOMINANT_BASELINE_HANGING           5
#define NS_STYLE_DOMINANT_BASELINE_IDEOGRAPHIC       6
#define NS_STYLE_DOMINANT_BASELINE_MATHEMATICAL      7
#define NS_STYLE_DOMINANT_BASELINE_CENTRAL           8
#define NS_STYLE_DOMINANT_BASELINE_MIDDLE            9
#define NS_STYLE_DOMINANT_BASELINE_TEXT_AFTER_EDGE  10
#define NS_STYLE_DOMINANT_BASELINE_TEXT_BEFORE_EDGE 11
#define NS_STYLE_DOMINANT_BASELINE_TEXT_TOP         12
#define NS_STYLE_DOMINANT_BASELINE_TEXT_BOTTOM      13

// fill-rule
#define NS_STYLE_FILL_RULE_NONZERO              0 /* == nsISVGGeometrySource::FILL_RULE_NONZERO */
#define NS_STYLE_FILL_RULE_EVENODD              1 /* == nsISVGGeometrySource::FILL_RULE_EVENODD */

// pointer-events
#define NS_STYLE_POINTER_EVENTS_NONE            0
#define NS_STYLE_POINTER_EVENTS_VISIBLEPAINTED  1
#define NS_STYLE_POINTER_EVENTS_VISIBLEFILL     2
#define NS_STYLE_POINTER_EVENTS_VISIBLESTROKE   3
#define NS_STYLE_POINTER_EVENTS_VISIBLE         4
#define NS_STYLE_POINTER_EVENTS_PAINTED         5
#define NS_STYLE_POINTER_EVENTS_FILL            6
#define NS_STYLE_POINTER_EVENTS_STROKE          7
#define NS_STYLE_POINTER_EVENTS_ALL             8

// shape-rendering
#define NS_STYLE_SHAPE_RENDERING_AUTO               0 /* == nsISVGPathGeometrySource::SHAPE_RENDERING_AUTO */
#define NS_STYLE_SHAPE_RENDERING_OPTIMIZESPEED      1 /* == nsISVGPathGeometrySource::SHAPE_RENDERING_OPTIMIZESPEED */
#define NS_STYLE_SHAPE_RENDERING_CRISPEDGES         2 /* == nsISVGPathGeometrySource::SHAPE_RENDERING_CRISPEDGES */
#define NS_STYLE_SHAPE_RENDERING_GEOMETRICPRECISION 3 /* == nsISVGPathGeometrySource::SHAPE_RENDERING_GEOMETRICPRECISION */


// stroke-linecap
#define NS_STYLE_STROKE_LINECAP_BUTT            0 /* == nsISVGGeometrySource::STROKE_LINECAP_BUTT */
#define NS_STYLE_STROKE_LINECAP_ROUND           1 /* == nsISVGGeometrySource::STROKE_LINECAP_ROUND */
#define NS_STYLE_STROKE_LINECAP_SQUARE          2 /* == nsISVGGeometrySource::STROKE_LINECAP_SQUARE */

// stroke-linejoin
#define NS_STYLE_STROKE_LINEJOIN_MITER          0 /* == nsISVGGeometrySource::STROKE_LINEJOIN_MITER */
#define NS_STYLE_STROKE_LINEJOIN_ROUND          1 /* == nsISVGGeometrySource::STROKE_LINEJOIN_ROUND */
#define NS_STYLE_STROKE_LINEJOIN_BEVEL          2 /* == nsISVGGeometrySource::STROKE_LINEJOIN_BEVEL */

// text-anchor
#define NS_STYLE_TEXT_ANCHOR_START              0 
#define NS_STYLE_TEXT_ANCHOR_MIDDLE             1 
#define NS_STYLE_TEXT_ANCHOR_END                2 

// text-rendering
#define NS_STYLE_TEXT_RENDERING_AUTO               0 /* == nsISVGGlyphGeometrySource::TEXT_RENDERING_AUTO */
#define NS_STYLE_TEXT_RENDERING_OPTIMIZESPEED      1 /* == nsISVGG.G.S.::TEXT_RENDERING_OPTIMIZESPEED */
#define NS_STYLE_TEXT_RENDERING_OPTIMIZELEGIBILITY 2 /* == nsISVGG.G.S.::TEXT_RENDERING_OPTIMIZELEGIBILITY */
#define NS_STYLE_TEXT_RENDERING_GEOMETRICPRECISION 3 /* == nsISVGG.G.S.::TEXT_RENDERING_GEOMETRICPRECISION */

#endif // MOZ_SVG

#endif /* nsStyleConsts_h___ */
