/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef LAYSTYLE_H
#define LAYSTYLE_H

#define ALLOW_NEG_MARGINS
/* #undef ALLOW_NEG_MARGINS */

/* prefix for the implicit id's needed for style attributes */
#define NSIMPLICITID "nsImplicitID"
#define NS_STYLE_NAME_ATTR "ns_ss_name"

#define STYLE_NEED_TO_POP_TABLE 		"POP_TABLE"
#define STYLE_NEED_TO_POP_LIST 			"POP_LIST"
#define STYLE_NEED_TO_POP_MARGINS		"POP_MARGINS"
#define STYLE_NEED_TO_POP_FONT 			"POP_FONT"
#define STYLE_NEED_TO_POP_PRE 			"POP_PRE"
#define STYLE_NEED_TO_POP_ALIGNMENT 	"POP_ALIGNMENT"
#define STYLE_NEED_TO_POP_LINE_HEIGHT 	"POP_LINE_HEIGHT"
#define STYLE_NEED_TO_POP_LAYER 	"POP_LAYER"
#define STYLE_NEED_TO_POP_CONTENT_HIDING 	"POP_HIDE_CONTENT"

#define STYLE_NEED_TO_RESET_PRE "RESET_PRE"

#define FONTSIZE_STYLE			"fontSize"
#define FONTFACE_STYLE			"fontFamily"
#define FONTWEIGHT_STYLE		"fontWeight"

#define FONTSTYLE_STYLE			"fontStyle"
#define NORMAL_STYLE			"normal"
#define ITALIC_STYLE			"italic"
#define OBLIQUE_STYLE			"oblique"

/* color and background properties */
#define COLOR_STYLE				"color"
#define BG_COLOR_STYLE 			"backgroundColor"
#define BG_IMAGE_STYLE 			"backgroundImage"
#define BG_REPEAT_STYLE			"backgroundRepeat"
#define BG_REPEAT_ALL_STYLE		"repeat"
#define BG_REPEAT_X_STYLE		"repeat-x"
#define BG_REPEAT_Y_STYLE		"repeat-y"
#define BG_REPEAT_NONE_STYLE		"no-repeat"

/* text properties */
#define TEXTDECORATION_STYLE	"textDecoration"
#define VERTICAL_ALIGN_STYLE	"verticalAlign"
#define TEXT_TRANSFORM_STYLE	"textTransform"
#define TEXT_ALIGN_STYLE		"textAlign"
#define TEXTINDENT_STYLE		"textIndent"
#define LINE_HEIGHT_STYLE		"lineHeight"

#define BLINK_STYLE 			"blink"
#define UNDERLINE_STYLE 		"underline"
#define OVERLINE_STYLE  		"overline"
#define STRIKEOUT_STYLE  		"line-through"


/* box properties */
#define BORDERWIDTH_STYLE		"borderWidth"
#define BORDER_STYLE_STYLE		"borderStyle"
#define BORDER_COLOR_STYLE		"borderColor"
#define BORDERRIGHTWIDTH_STYLE	"borderRightWidth"
#define BORDERLEFTWIDTH_STYLE	"borderLeftWidth"
#define BORDERTOPWIDTH_STYLE	"borderTopWidth"
#define BORDERBOTTOMWIDTH_STYLE	"borderBottomWidth"

#define LEFTMARGIN_STYLE		"marginLeft"
#define RIGHTMARGIN_STYLE		"marginRight"
#define TOPMARGIN_STYLE			"marginTop"
#define BOTTOMMARGIN_STYLE		"marginBottom"

#define PADDING_STYLE			"padding"
#define LEFTPADDING_STYLE		"paddingLeft"
#define RIGHTPADDING_STYLE		"paddingRight"
#define TOPPADDING_STYLE		"paddingTop"
#define BOTTOMPADDING_STYLE		"paddingBottom"

#define WIDTH_STYLE			"width"
#define LAYER_WIDTH_STYLE               "_layer_width" /* Bogus style used for layer widths */
#define HEIGHT_STYLE			"height"

#define HORIZONTAL_ALIGN_STYLE	"align"   /* css float property */
#define CLEAR_STYLE				"clear"

#define PAGE_BREAK_BEFORE_STYLE	"pageBreakBefore"
#define PAGE_BREAK_AFTER_STYLE	"pageBreakAfter"

/* classification properties */
#define DISPLAY_STYLE			"display"
#define BLOCK_STYLE			"block"
#define INLINE_STYLE			"inline"
#define NONE_STYLE			"none"
#define LIST_ITEM_STYLE			"list-item"

#define LIST_STYLE_TYPE_STYLE		"listStyleType"
#define WHITESPACE_STYLE		"whiteSpace"

/* layer styles */
#define POSITION_STYLE			"position"
#define ABSOLUTE_STYLE			"absolute"
#define RELATIVE_STYLE			"relative"
#define TOP_STYLE				"top"
#define LEFT_STYLE				"left"
#define CLIP_STYLE				"clip"
#define ZINDEX_STYLE			"zIndex"
#define VISIBILITY_STYLE		"visibility"
#define OVERFLOW_STYLE			"overflow"
#define LAYER_SRC_STYLE			"includeSource"
#define LAYER_BG_COLOR_STYLE    "layerBackgroundColor"
#define LAYER_BG_IMAGE_STYLE    "layerBackgroundImage"

/* link color styles */
#define LINK_COLOR		"linkColor"
#define VISITED_COLOR		"visitedColor"
#define ACTIVE_COLOR		"activeColor"
#define LINK_BORDER		"linkBorder"

XP_BEGIN_PROTOS

extern int32
LO_GetWidthFromStyleSheet(MWContext *context, lo_DocState *state);

extern int32
LO_GetHeightFromStyleSheet(MWContext *context, lo_DocState *state);

extern PushTagStatus
LO_PushTagOnStyleStack(MWContext *context, lo_DocState *state, PA_Tag *tag);

extern void
LO_PopStyleTag(MWContext *context, lo_DocState **state, PA_Tag *tag);

extern void
LO_PopStyleTagByIndex(MWContext *context, lo_DocState **state, TagType tag_type, int32 index);

extern XP_Bool
LO_PopAllTagsAbove(MWContext *context, 
				   lo_DocState **state, 
				   TagType tag_type, 
				   TagType not_below_this,
				   TagType or_this,
				   TagType or_this_either);

extern XP_Bool
LO_ImplicitPop(MWContext *context, lo_DocState **state, PA_Tag *tag);

extern XP_Bool
LO_CheckForContentHiding(lo_DocState *state);

extern void
LO_AdjustSSUnits(SS_Number *number, char *style_type, MWContext *context, lo_DocState *state);

extern PA_Tag *
LO_CreateStyleSheetDummyTag(PA_Tag *old_tag);

extern XP_Bool
LO_StyleSheetsEnabled(MWContext *context);

/* function body is in laytags.c */
extern int
lo_list_bullet_type(char *type_string, TagType tag_type);

XP_END_PROTOS

#endif /* LAYSTYLE_H */
