/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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


/******************
 * Structures used by the layout engine which will be passed
 * on to the front end in the front-end to layout interface
 * functions.
 *
 * All the major element structures have an entry
 * called FE_Data.  This is to be used by the front end to
 * store front-end specific data that the layout ill pass
 * back to it later.
 ******************/
#ifndef _LayoutElements_
#define _LayoutElements_


#include "xp_core.h"
#include "xp_mem.h"
#include "xp_rect.h"
#include "cl_types.h"
#include "ntypes.h"
#include "edttypes.h"
#include "il_types.h"

/*
 * Colors - some might say that some of this should be user-customizable.
 */

#define LO_DEFAULT_FG_RED         0
#define LO_DEFAULT_FG_GREEN       0
#define LO_DEFAULT_FG_BLUE        0

#define LO_DEFAULT_BG_RED         192
#define LO_DEFAULT_BG_GREEN       192
#define LO_DEFAULT_BG_BLUE        192

#define LO_UNVISITED_ANCHOR_RED   0
#define LO_UNVISITED_ANCHOR_GREEN 0
#define LO_UNVISITED_ANCHOR_BLUE  238

#define LO_VISITED_ANCHOR_RED     85
#define LO_VISITED_ANCHOR_GREEN   26
#define LO_VISITED_ANCHOR_BLUE    139

#define LO_SELECTED_ANCHOR_RED    255
#define LO_SELECTED_ANCHOR_GREEN  0
#define LO_SELECTED_ANCHOR_BLUE   0

/*
 * Color enumeration for getting/setting defaults
 */
#define LO_COLOR_BG	0
#define LO_COLOR_FG	1
#define LO_COLOR_LINK	2
#define LO_COLOR_VLINK	3
#define LO_COLOR_ALINK	4

#define LO_NCOLORS	5


/*
 * Values for scrolling grid cells to control scrollbars
 * AUTO means only use them if needed.
 */
#define LO_SCROLL_NO	0
#define LO_SCROLL_YES	1
#define LO_SCROLL_AUTO	2
#define LO_SCROLL_NEVER 3

/*
 * Relative font sizes that the user can tweak from the FE
 */
#define XP_SMALL  1
#define XP_MEDIUM 2
#define XP_LARGE  3

/* --------------------------------------------------------------------------
 * Layout bit flags for text and bullet types
 */
#define LO_UNKNOWN      -1
#define LO_NONE         0
#define LO_TEXT         1
#define LO_LINEFEED     2
#define LO_HRULE        3
#define LO_IMAGE        4
#define LO_BULLET       5
#define LO_FORM_ELE     6
#define LO_SUBDOC       7
#define LO_TABLE        8
#define LO_CELL         9
#define LO_EMBED        10
#define LO_EDGE         11
#define LO_JAVA         12
#define LO_SCRIPT       13
#define LO_OBJECT       14
#define LO_PARAGRAPH	15
#define LO_CENTER		16
#define LO_MULTICOLUMN	17
#define LO_FLOAT		18	
#define LO_TEXTBLOCK	19
#define LO_LIST			20
#define LO_DESCTITLE	21
#define LO_DESCTEXT		22
#define LO_BLOCKQUOTE	23
#define LO_LAYER		24
#define LO_HEADING		25
#define LO_SPAN			26
#define LO_DIV			27
#define LO_BUILTIN      28
#define LO_SPACER		29

#define LO_FONT_NORMAL      0x0000
#define LO_FONT_BOLD        0x0001
#define LO_FONT_ITALIC      0x0002
#define LO_FONT_FIXED       0x0004

#define LO_ATTR_ANCHOR      0x0008
#define LO_ATTR_UNDERLINE   0x0010
#define LO_ATTR_STRIKEOUT   0x0020
#define LO_ATTR_BLINK	    0x0040
#define LO_ATTR_ISMAP	    0x0080
#define LO_ATTR_ISFORM	    0x0100
#define LO_ATTR_DISPLAYED   0x0200
#define LO_ATTR_BACKDROP    0x0400
#define LO_ATTR_STICKY      0x0800
#define LO_ATTR_INTERNAL_IMAGE  0x1000
#define LO_ATTR_MOCHA_IMAGE 0x2000 /* Image loaded at behest of JavaScript */
#define LO_ATTR_ON_IMAGE_LIST   0x4000 /* Image on top_state->image_list */
#define LO_ATTR_CELL_BACKDROP   0x8000
#define LO_ATTR_SPELL       0x00010000
#define LO_ATTR_INLINEINPUT     0x00020000
#define LO_ATTR_INLINEINPUTTHICK     0x00040000
#define LO_ATTR_INLINEINPUTDOTTED    0x00080000
#define	LO_ATTR_LAYER_BACKDROP       0x00100000

/*
 * Different bullet types.   The front end should only ever see
 *  bullets of type BULLET_ROUND and BULLET_SQUARE
 */
#define BULLET_NONE             0
#define BULLET_BASIC            1
#define BULLET_NUM              2
#define BULLET_NUM_L_ROMAN      3
#define BULLET_NUM_S_ROMAN      4
#define BULLET_ALPHA_L          5
#define BULLET_ALPHA_S          6
#define BULLET_ROUND            7
#define BULLET_SQUARE           8
#define BULLET_MQUOTE           9
/* A bullet type of WORDBREAK in a LO_TEXT element tells the relayout code to
   skip that text element. */
#define WORDBREAK			   10

/*
 * Different border types.
 */
#define	BORDER_NONE				0
#define	BORDER_DOTTED			1
#define	BORDER_DASHED			2
#define	BORDER_SOLID			3
#define	BORDER_DOUBLE			4
#define	BORDER_GROOVE			5
#define	BORDER_RIDGE			6
#define	BORDER_INSET			7
#define	BORDER_OUTSET			8

/* Values for flag in LO_AnchorData */
#define ANCHOR_SUPPRESS_FEEDBACK        1

/*
 * Generic union of all the possible element structure types.
 * Defined at end of file.
 */
union LO_Element_struct;

/*
 * Anchors can be targeted to named windows now, so anchor data is
 * more than just a URL, it is a URL, and a possible target window
 * name.
 */
struct LO_AnchorData_struct {
    PA_Block anchor;        /* really a (char *) */
    PA_Block target;        /* really a (char *) */
    PA_Block alt;           /* really a (char *) */
#ifdef MOCHA
    LO_Element *element;	/* Needed for mocha reflection
                               of layout position. */
    CL_Layer *layer;	/* Layer containing anchor */
    struct JSObject *mocha_object;
    XP_Bool event_handler_present; /*Indicates whether there's a JS handler */
#endif
    uint8 flags;            /* Indicates whether to suppress visual feedback */
    double prevalue;	/* <A HREF="http://gagan/PrefetchingThoughts.html" PRE=0.5> */
};

/*
 * Basic RGB triplet
 */
struct LO_Color_struct {
    uint8 red, green, blue;
};

/* 
 * Contains initialization parameters for new layers. Exported here
 * so that mocha can set these on resize.
 */
/* Possible values for clip_policy */
#define LO_AUTO_EXPAND_NONE          0x00
#define LO_AUTO_EXPAND_CLIP_BOTTOM   0x01
#define LO_AUTO_EXPAND_CLIP_RIGHT    0x02
#define LO_AUTO_EXPAND_CLIP_TOP      0x04
#define LO_AUTO_EXPAND_CLIP_LEFT     0x08
#define LO_AUTO_EXPAND_CLIP (LO_AUTO_EXPAND_CLIP_LEFT |    \
    						 LO_AUTO_EXPAND_CLIP_RIGHT |   \
    						 LO_AUTO_EXPAND_CLIP_TOP  |    \
    						 LO_AUTO_EXPAND_CLIP_BOTTOM)

typedef struct LO_BlockInitializeStruct_struct {
    XP_Bool has_left;
    int32 left;
    XP_Bool has_top;
    int32 top;
    XP_Bool has_width;
    int32 width;
    XP_Bool has_height;
    int32 height;
    char *name;
    char *id;
    char *above;
    char *below;
    XP_Bool has_zindex;
    int32 zindex;
    char *visibility;
    char *bgcolor;
    XP_Bool is_style_bgcolor;
    char *bgimage;
    char *src;
    XP_Rect *clip;
    int clip_expansion_policy;
    char *overflow;
    PA_Tag *tag;
    PA_Tag *ss_tag;
} LO_BlockInitializeStruct;
 
/*
 * Attributes of the text we are currently drawing in.
 *
 * if font weight is specified then the "bold" fontmask attribute
 * should be ignored
 *
 * if font face is specified then the "fixed" fontmask attribute 
 * should be ignored
 */
struct LO_TextAttr_struct {
    int16 size;
    int32 fontmask;   /* bold, italic, fixed */									 
    LO_Color fg, bg;
    Bool no_background; /* unused, text on image? */
    int32 attrmask;   /* anchor, underline, strikeout, blink(gag) */
    char *font_face;
    double point_size;   /* 1.0pt through 72.0pt and above */
    uint16 font_weight;  /* 100, 200, ... 900 */
    void *FE_Data;     /* For the front end to store font IDs */
    struct LO_TextAttr_struct *next; /* to chain in hash table */
    int16 charset;
};
 
/*
 * Information about the current text segment as rendered
 * by the front end that the layout engine can use to place
 * the text segment.
 */
struct LO_TextInfo_struct {
    intn max_width;
    int16 ascent, descent;
    int16 lbearing, rbearing;
};
 

/*
 * Bit flags for the ele_attrmask.
 */
#define	LO_ELE_BREAKABLE	  0x0001
#define	LO_ELE_SECURE		  0x0002
#define	LO_ELE_SELECTED		  0x0004
#define	LO_ELE_BREAKING		  0x0008
#define	LO_ELE_FLOATING		  0x0010
#define	LO_ELE_SHADED		  0x0020
#define	LO_ELE_DELAY_CENTER	  0x0040
#define	LO_ELE_DELAY_RIGHT	  0x0080
#define	LO_ELE_DELAY_JUSTIFY  0x00c0
#define	LO_ELE_DELAY_ALIGNED  0x00c0
#define LO_ELE_INVISIBLE      0x0100 /* Invisible form, applet or embed; Not from tag attributes */
#define LO_ELE_HIDDEN         0x0200 /* Element occupies no screen real-estate */
#define LO_ELE_DRAWN          0x0400 /* Indicates that the element has been drawn at least once before */
#define LO_ELE_STREAM_STARTED 0x0800
/* Special selection signal for Composer
 * Used for visual feedback during dragNdrop of table cells
 * (used for cells that will replaced)
 * and also for the non-focus cells when > 1 cells are selected
 *   and the cell properties are being edited
*/
#define	LO_ELE_SELECTED_SPECIAL 0x1000
#ifdef DOM
#define	LO_ELE_IN_SPAN			0x2000
#endif /* DOM */

/*
 * This structure heads up a group of text elements. It contains the actual text buffer
 * and all the word breaking information.
 */
typedef struct LO_TextBlock_struct {
    /* the base layout struct */
    int16 type;
    int16 x_offset;
    int32 ele_id;
    int32 x, y;
    int32 y_offset;
    int32 width, height;
    int32 line_height;
    LO_Element *next;
    LO_Element *prev;
    ED_Element *edit_element;
    int32 edit_offset;

    /* textblock specific */
    LO_TextAttr *	text_attr;
    LO_AnchorData *	anchor_href;
    uint16			ele_attrmask; /* breakable, secure, selected, etc. */
    uint16			format_mode;
    	
    LO_TextStruct *	startTextElement;
    LO_TextStruct *	endTextElement;
    	
    uint8 *			text_buffer;
    uint32			buffer_length;
    uint32			buffer_write_index;
    uint32			last_buffer_write_index;
    uint32			buffer_read_index;
    uint32			last_line_break;
    	
    uint32 * 		break_table;
    uint32			break_length;
    uint32			break_write_index;
    uint32			break_read_index;
    uint32			last_break_offset;
    uint32			multibyte_index;
    uint32			multibyte_length;

    LO_TextStruct *	old_break;
    uint32			old_break_pos;
    uint32			old_break_width;
    	
    uint32			totalWidth;
    uint32			totalChars;
    uint8			break_pending;
    uint8			last_char_is_whitespace;

    int16			ascent;				/* font information for relayout */
    int16			descent;
} LO_TextBlock;


/*
 * This is the structure that the layout module will
 * pass to the front-end and ask the front end to render
 * this text segment in the x,y location specified.
 * The selected flag in combination with the text_attr
 * structure will tell the front-end how to render the text.
 */
struct LO_TextStruct_struct {
    int16 type;
    int16 x_offset;
    int32 ele_id;
    int32 x, y;
    int32 y_offset;
    int32 width, height;
    int32 line_height;
    LO_Element *next;
    LO_Element *prev;
    ED_Element *edit_element;
    int32 edit_offset;

    void *FE_Data;
#if defined(XP_WIN)
    union {
        PA_Block text;		/* lock to a (char *) */
        char *pText;
    };
#else
    PA_Block text;		/* lock to a (char *) */
#endif
    uint16 block_offset;	/* offset to this line in the parent text block */
    uint16 doc_width;		/* doc width that this line was layed out into */
    LO_AnchorData *anchor_href;
    LO_TextAttr *text_attr;
    uint16 ele_attrmask; /* breakable, secure, selected, etc. */
    int16 text_len;
    int16 sel_start;
    int16 sel_end;
    intn bullet_type;		/* set to MQUOTE in lo_make_quote_text, and other things in lo_PlaceBulletStr. */
};



/*****************
 * Similar to the structures above, these are the structures
 * used to manage image elements between the layout and the 
 * front-end.
 ****************/
 
/*
 * Current status of a displayed image.
 * ImageData (above) can be reused for a single
 * image displayed multiple times, but ImageAttr is
 * unique to a particular instance of a displayed image.
 */
struct LO_ImageAttr_struct {
    int32 attrmask;   /* anchor, delayed, ismap */
    intn alignment;
    int32 layer_id;
    intn form_id;
    char *usemap_name;	/* Use named client-side map if here */
    void *usemap_ptr;	/* private, used by layout only! */
    intn submit_x;		/* for form, last click coord */
    intn submit_y;		/* for form, last click coord */
};
 

/* Suppression of feedback such as the drawing of the delayed icon, temporary
   border and alt text when an image is coming in, and the drawing of keyboard
   navigation feedback. */
typedef enum {
    LO_SUPPRESS_UNDEFINED,
    LO_SUPPRESS,
    LO_DONT_SUPPRESS
} lo_SuppressMode;

/*
 * This is the structure that the layout module will
 * pass to the front-end and ask the front end to render
 * this image at the x,y location specified.
 * The selected flag in combination with the image_attr
 * structure will tell the front-end how to render the image.
 * alt is alternate text to display if the image is not to
 * be displayed.
 */
struct LO_ImageStruct_struct {
    int16 type;
    int16 x_offset;
    int32 ele_id;
    int32 x, y;
    int32 y_offset;
    int32 width, height;
    int32 line_height;
    LO_Element *next;
    LO_Element *prev;
    ED_Element *edit_element;
    int32 edit_offset;

    int32 layer_id;

    int32 percent_width;	/* For relayout */
    int32 percent_height;	/* For relayout */
    
#ifdef MOCHA
    struct JSObject *mocha_object;
#endif
    CL_Layer *layer;
    XP_Rect valid_rect;
    IL_ImageReq *image_req; /* Image library representation. */
    IL_ImageReq *lowres_image_req; /* Handle on lowres image while
                                      highres image is coming in. */
    uint8 is_icon;          /* Is this an image or an icon? */
    uint16 icon_number;     /* If this is an icon, the icon number. */
    PA_Block alt;
    LO_AnchorData *anchor_href;
    PA_Block image_url;
    PA_Block lowres_image_url;
    LO_ImageAttr *image_attr;
    LO_TextAttr *text_attr;
    int32 border_width;
    int32 border_vert_space;
    int32 border_horiz_space;
    uint16 ele_attrmask; /* floating, secure, selected, etc. */
    uint16 image_status;
    uint8 is_transparent;
    lo_SuppressMode suppress_mode; /* For temp icon, temp border, etc. */
    int16 alt_len;
    int16 sel_start;
    int16 sel_end;
    intn seq_num;
    PRPackedBool pending_mocha_event;
    LO_ImageStruct *next_image; /* next image in list for this document */
};

/* Tiling mode for implementing CSS's background-repeat property */
typedef enum {
    /* The code depends on specific values for this enum - don't change them. */
    LO_TILE_BOTH  = 0,          /* Tile in both X and Y direction */
    LO_TILE_HORIZ = 1,          /* Tile horizontally only */
    LO_TILE_VERT  = 2,          /* Tile vertically only */
    LO_NO_TILE    = 3           /* Don't tile */
} lo_TileMode;

/* An (optionally) tiled backdrop image, optionally on top of a solid color */
typedef struct {
    LO_Color *bg_color;         /* Solid color, or NULL, if transparent */
    char *url;                  /* String URL or NULL, if no backdrop image */
    lo_TileMode tile_mode;      /* CSS tiling mode for backdrop image */
} lo_Backdrop;

struct LO_SubDocStruct_struct {
    int16 type;
    int16 x_offset;
    int32 ele_id;
    int32 x, y;
    int32 y_offset;
    int32 width, height;
    int32 line_height;
    LO_Element *next;
    LO_Element *prev;
    ED_Element *edit_element;
    int32 edit_offset;

    void *FE_Data;
    void *state;	/* Hack-o-rama to opaque the lo_DocState type */
    lo_Backdrop backdrop; /* To carry over into CellStruct */
    LO_AnchorData *anchor_href;
    LO_TextAttr *text_attr;
    int32 alignment;
    int32 vert_alignment;
    int32 horiz_alignment;
    int32 border_width;
    int32 border_vert_space;
    int32 border_horiz_space;
    uint16 ele_attrmask; /* floating, secure, selected, etc. */
    int16 sel_start;
    int16 sel_end;
};
 
 
/*
 * This is the structure that the layout module will
 * pass to the front-end and ask the front end to render
 * this form element at the x,y location specified.
 * The widget_data field holds state data for this form
 * element, such as the current text in a text entry area.
 */
#define FORM_TYPE_NONE          0
#define FORM_TYPE_TEXT          1
#define FORM_TYPE_RADIO         2
#define FORM_TYPE_CHECKBOX      3
#define FORM_TYPE_HIDDEN        4
#define FORM_TYPE_SUBMIT        5
#define FORM_TYPE_RESET         6
#define FORM_TYPE_PASSWORD      7
#define FORM_TYPE_BUTTON        8
#define FORM_TYPE_JOT           9
#define FORM_TYPE_SELECT_ONE    10
#define FORM_TYPE_SELECT_MULT   11
#define FORM_TYPE_TEXTAREA      12
#define FORM_TYPE_ISINDEX       13
#define FORM_TYPE_IMAGE         14
#define FORM_TYPE_FILE          15
#define FORM_TYPE_KEYGEN        16
#define FORM_TYPE_READONLY      17
#define FORM_TYPE_OBJECT        18

#define FORM_METHOD_GET		0
#define FORM_METHOD_POST	1

#define TEXTAREA_WRAP_OFF	0
#define TEXTAREA_WRAP_SOFT	1
#define TEXTAREA_WRAP_HARD	2

#define INPUT_TYPE_ENCODING_NORMAL 0
#define INPUT_TYPE_ENCODING_MACBIN 1



/*
 * Union of all different form element data structures
 */
union LO_FormElementData_struct;

/*
 * This is the element_data structure for elements whose
 * element_type = FORM_TYPE_SELECT_ONE, FORM_TYPE_SELECT_MULT.
 */

/*
 * nesting deeper and deeper, harder and harder, go, go, oh, OH, OHHHHH!!
 * Sorry, got carried away there.
 */
struct lo_FormElementOptionData_struct {
    PA_Block text_value;
    PA_Block value;
    Bool def_selected;
    Bool selected;
};


struct lo_FormElementSelectData_struct {
    int32 type;
    void *FE_Data;
    PA_Block name;
    int32 size;
    Bool disabled;
    Bool multiple;
    Bool options_valid;
    int32 option_cnt;
    PA_Block options;	/* really a (lo_FormElementOptionData *) */
    PA_Tag *saved_tag;	/* mocha weazels need for threading */
};

/*
 * This is the element_data structure for elements whose
 * element_type = FORM_TYPE_TEXT, FORM_TYPE_PASSWORD, FORM_TYPE_FILE.
 */
struct lo_FormElementTextData_struct {
	int32 type;
	void *FE_Data;
	PA_Block name;
	PA_Block default_text;
	Bool disabled;
	Bool read_only;
	PA_Block current_text;
	int32 size;
	int32 max_size;
	int32 encoding;
};


/*
 * This is the element_data structure for elements whose
 * element_type = FORM_TYPE_TEXTAREA.
 */
struct lo_FormElementTextareaData_struct {
	int32 type;
	void *FE_Data;
	PA_Block name;
	PA_Block default_text;
	Bool disabled;
	Bool read_only;
	PA_Block current_text;
	int32 rows;
	int32 cols;
	uint8 auto_wrap;
	PA_Tag *saved_tag;	/* mocha weazels need for threading */
};


/*
 * This is the element_data structure for elements whose
 * element_type = FORM_TYPE_HIDDEN, FORM_TYPE_SUBMIT, FORM_TYPE_RESET,
 * 		  FORM_TYPE_BUTTON, FORM_TYPE_READONLY.
 */
struct lo_FormElementMinimalData_struct {
    int32 type;
    void *FE_Data;
    PA_Block name;
    PA_Block value;
    Bool disabled;
};


/*
 * This is the element_data structure for elements whose
 * element_type = FORM_TYPE_RADIO, FORM_TYPE_CHECKBOX.
 */
struct lo_FormElementToggleData_struct {
    int32 type;
    void *FE_Data;
    PA_Block name;
    PA_Block value;
    Bool disabled;
    Bool default_toggle;
    Bool toggled;
};


/*
 * This is the element_data structure for elements whose
 * element_type = FORM_TYPE_OBJECT.
 */
struct lo_FormElementObjectData_struct {
    int32 type;
    void *FE_Data;
    PA_Block name;
    LO_JavaAppStruct *object;
};


/*
 * This is the element_data structure for elements whose
 * element_type = FORM_TYPE_KEYGEN.
 */
struct lo_FormElementKeygenData_struct {
    int32 type;
    PA_Block name;
    PA_Block challenge;
    PA_Block key_type;
    PA_Block pqg; /* DSA only */
    char *value_str;
    PRBool dialog_done;
};


union LO_FormElementData_struct {
    int32 type;
    lo_FormElementTextData ele_text;
    lo_FormElementTextareaData ele_textarea;
    lo_FormElementMinimalData ele_minimal;
    lo_FormElementToggleData ele_toggle;
    lo_FormElementObjectData ele_object;
    lo_FormElementSelectData ele_select;
    lo_FormElementKeygenData ele_keygen;
};


struct LO_FormSubmitData_struct {
    PA_Block action;
    PA_Block encoding;
    PA_Block window_target;
    int32 method;
    int32 value_cnt;
    PA_Block name_array;
    PA_Block value_array;
    PA_Block type_array;
    PA_Block encoding_array;
};

 
struct LO_FormElementStruct_struct {
    int16 type;
    int16 x_offset;
    int32 ele_id;
    int32 x, y;
    int32 y_offset;
    int32 width, height;
    int32 line_height;
    LO_Element *next;
    LO_Element *prev;
    ED_Element *edit_element;
    int32 edit_offset;

    int32 layer_id;
#ifdef MOCHA
    struct JSObject *mocha_object;
    XP_Bool event_handler_present; /*Indicates whether there's a JS handler */
#endif

    int16 border_vert_space;
    int16 border_horiz_space;
    int32 element_index;
    LO_FormElementData *element_data;
    LO_TextAttr *text_attr;
    int32 baseline;
    uint16 ele_attrmask; /* secure, selected, etc. */
    int16 form_id;
    int16 sel_start;
    int16 sel_end;
    CL_Layer *layer;
};


/*
 * This is the structure that the layout module will
 * pass to the front-end and ask the front end to render
 * a linefeed at the x,y location specified.
 * Linefeeds are rendered as just a filled rectangle.
 * Height should be the height of the tallest element
 * on the line.
 */

#define LO_LINEFEED_BREAK_SOFT 0
#define LO_LINEFEED_BREAK_HARD 1
#define LO_LINEFEED_BREAK_PARAGRAPH 2

#define LO_CLEAR_NONE 0
#define LO_CLEAR_TO_LEFT 1
#define LO_CLEAR_TO_RIGHT 2
#define LO_CLEAR_TO_ALL 3
#define LO_CLEAR_TO_BOTH 4

struct LO_LinefeedStruct_struct {
    int16 type;
    int16 x_offset;
    int32 ele_id;
    int32 x, y;
    int32 y_offset;
    int32 width, height;
    int32 line_height;
    LO_Element *next;
    LO_Element *prev;
    ED_Element *edit_element;
    int32 edit_offset;

    void *FE_Data;
    LO_AnchorData *anchor_href;
    LO_TextAttr *text_attr;
    int32 baseline;
    uint16 ele_attrmask; /* breaking, secure, selected, etc. */
    int16 sel_start;
    int16 sel_end;
    uint8 break_type;		/* is this an end-of-paragraph or a break? */
    uint8 clear_type;		/* the clear="" attribute */
};
 
 
struct LO_HorizRuleStruct_struct {
    int16 type;
    int16 x_offset;
    int32 ele_id;
    int32 x, y;
    int32 y_offset;
    int32 width, height;
    int32 line_height;
    LO_Element *next;
    LO_Element *prev;
    ED_Element *edit_element;
    int32 edit_offset;

    int32 end_x, end_y;
    void *FE_Data;
    uint16 ele_attrmask; /* shaded, secure, selected, etc. */
    int16 alignment;
    int16 thickness; /* 1 - 100 */
    int16 sel_start;
    int16 sel_end;

    int32 percent_width; /* needed for relayout. */
    int32 width_in_initial_layout;  /* needed for relayout. */
};
 
struct LO_BulletStruct_struct {
    int16 type;
    int16 x_offset;
    int32 ele_id;
    int32 x, y;
    int32 y_offset;
    int32 width, height;
    int32 line_height;
    LO_Element *next;
    LO_Element *prev;
    ED_Element *edit_element;
    int32 edit_offset;

    void *FE_Data;
    int32 bullet_type;
    int32 bullet_size;
    LO_TextAttr *text_attr;
    uint16 ele_attrmask; /* secure, selected, etc. */
    int16 level;
    int16 sel_start;
    int16 sel_end;
};


struct LO_TableStruct_struct {
    int16 type;
    int16 x_offset;
    int32 ele_id;
    int32 x, y;
    int32 y_offset;
    int32 width, height;
    int32 line_height;
    LO_Element *next;
    LO_Element *prev;
    ED_Element *edit_element;
    int32 edit_offset;

    void *FE_Data;
    LO_AnchorData *anchor_href;
    int32 expected_y;
    int32 alignment;
    LO_Color border_color;
    int32 border_style;
    int32 border_width;
    int32 border_top_width;
    int32 border_right_width;
    int32 border_bottom_width;
    int32 border_left_width;
    int32 border_vert_space;
    int32 border_horiz_space;
    uint16 ele_attrmask; /* floating, secure, selected, etc. */
    int16 sel_start;
    int16 sel_end;
    int32 inter_cell_space;  /*cmanske: CELLSPACING value - used when drawing table selection feedback */
    void *table;		/* Actually a lo_TableRec *.  Added for relayout */
};

struct LO_CellStruct_struct {
    int16 type;
    int16 x_offset;
    int32 ele_id;
    int32 x, y;
    int32 y_offset;
    int32 width, height;
    int32 line_height;
    LO_Element *next;
    LO_Element *prev;
    ED_Element *edit_element;
    int32 edit_offset;

    void *FE_Data;
    LO_Element *cell_list;
    LO_Element *cell_list_end;
    LO_Element *cell_float_list;
    lo_Backdrop backdrop;
    int32 border_width;
    int32 border_vert_space;
    int32 border_horiz_space;
    uint16 ele_attrmask; /* secure, selected, etc. */
    int16 sel_start;
    int16 sel_end;
    int32 inter_cell_space;  /*cmanske: CELLSPACING value - used when drawing cell selection feedback */		
    CL_Layer *cell_inflow_layer; /* Non-NULL if this cell
                                    exists to encapsulate
                                    an in-flow layer (ILAYER) */
    CL_Layer *cell_bg_layer; /* BGCOLOR/BACKGROUND for cell */
    void *table_cell;	/* An lo_TableCell *.  For use during relayout without reload. */
    void *table_row;	/* An lo_TableRow *.  Parent row state struct */
    void *table;		/* An lo_TableRec *. Parent table state struct */
    Bool isCaption;		/* Needed for relayout without reload */
};

#ifdef OJI
struct lo_NVList {
    uint32 n; /* number of name/value pairs */
    char** names;
    char** values;
};

#define LO_NVList_Init( pList ) (pList)->n=0; (pList)->names=NULL; (pList)->values=NULL
#endif

struct LO_CommonPluginStruct_struct {
    int16 type;
    int16 x_offset;
    int32 ele_id;
    int32 x, y;
    int32 y_offset;
    int32 width, height;
    int32 line_height;
    LO_Element *next;
    LO_Element *prev;
    ED_Element *edit_element;
    int32 edit_offset;

    void * FE_Data;
    void *session_data;
    int32 alignment;
    int32 border_width;
    int32 border_vert_space;
    int32 border_horiz_space;
    uint16 ele_attrmask; /* floating, secure, selected, etc. */
    int32 embed_index;	/* Unique ID within this doc */
    PA_Tag *tag;
    CL_Layer *layer;

    int32 percent_width; /* needed for relayout. */
    int32 percent_height; /* needed for relayout. */
    PA_Block base_url;
#ifdef MOCHA
    struct JSObject *mocha_object;
#endif		
};

struct LO_BuiltinStruct_struct {
		int16 type;
		int16 x_offset;
		int32 ele_id;
		int32 x, y;
		int32 y_offset;
		int32 width, height;
		int32 line_height;
		LO_Element *next;
		LO_Element *prev;
		ED_Element *edit_element;
		int32 edit_offset;

		void * FE_Data;
		void *session_data;
		PA_Block builtin_src;
#ifndef OJI
		int32 attribute_cnt;
		char **attribute_list;
		char **value_list;
#else
                lo_NVList attributes;
#endif /* OJI */
		int32 alignment;
		int32 border_width;
		int32 border_vert_space;
		int32 border_horiz_space;
		uint16 ele_attrmask; /* floating, secure, selected, etc. */
		int32 builtin_index;	/* Unique ID within this doc */
		struct LO_BuiltinStruct_struct *nextBuiltin;
#ifdef MOCHA
		struct JSObject *mocha_object;
#endif
		PA_Tag *tag;
		CL_Layer *layer;

		int32 percent_width; /* needed for relayout. */
		int32 percent_height; /* needed for relayout. */
};

struct LO_EmbedStruct_struct {
    struct LO_CommonPluginStruct_struct objTag;     /* "superclass" */

    struct LO_EmbedStruct_struct *nextEmbed;
    PA_Block embed_src;
#ifdef OJI
    struct lo_NVList attributes;
    struct lo_NVList parameters;
#else
    int32 attribute_cnt;
    char **attribute_list;
    char **value_list;
#endif /* OJI */
};

#define LO_JAVA_SELECTOR_APPLET             0
#define LO_JAVA_SELECTOR_OBJECT_JAVA        1
#define LO_JAVA_SELECTOR_OBJECT_JAVAPROGRAM 2
#define LO_JAVA_SELECTOR_OBJECT_JAVABEAN    3

struct LO_JavaAppStruct_struct {
    struct LO_CommonPluginStruct_struct objTag;     /* "superclass" */

    /* linked list thread for applets in the current document.
     * should be "prev" since they're in reverse order but who's
     * counting? */
    struct LO_JavaAppStruct_struct *nextApplet;

    /* selector_type indicates whether the tag was an
     * APPLET tag or OBJECT tag, and if it was an OBJECT tag,
     * whether the protocol selector in CLASSID was "java:",
     * "javaprogram:", or "javabean:".
     */
    int32 selector_type;
    PA_Block attr_code;
    PA_Block attr_codebase;
    PA_Block attr_archive;
    PA_Block attr_name;
    Bool may_script;
#ifdef OJI
    struct lo_NVList attributes;
    struct lo_NVList parameters;
#else /* OJI */
    int32 param_cnt;
    char **param_names;
    char **param_values;
#endif /* OJI */
};


struct LO_EdgeStruct_struct {
    int16 type;
    int16 x_offset;
    int32 ele_id;
    int32 x, y;
    int32 y_offset;
    int32 width, height;
    int32 line_height;
    LO_Element *next;
    LO_Element *prev;

    void * FE_Data;
    void *lo_edge_info;
    LO_Color *bg_color;
    int16 size;
    Bool visible;
    Bool movable;
    Bool is_vertical;
    int32 left_top_bound;
    int32 right_bottom_bound;
};

struct LO_Any_struct {
    int16 type;
    int16 x_offset;
    int32 ele_id;
    int32 x, y;
    int32 y_offset;
    int32 width, height;
    int32 line_height;
    LO_Element *next;
    LO_Element *prev;
    ED_Element *edit_element;
    int32 edit_offset;
};

/* some DocState mutating elements.  */
typedef struct LO_CenterStruct_struct {
    LO_Any lo_any;

    /* nothing really to center elements */
    Bool is_end;
} LO_CenterStruct;

typedef struct LO_MulticolumnStruct_struct {
    LO_Any lo_any;

    Bool is_end;
    PA_Tag *tag;
    void *multicol;   /* Pointer to lo_MultiCol struct */
} LO_MulticolumnStruct;

typedef struct LO_SpacerStruct_struct {
    LO_Any lo_any;
    
    int32 size;
    int8 type;
    int32 alignment;
    Bool floating;
    Bool is_percent_width;
    int32 width;
    Bool is_percent_height;
    int32 height;
    PA_Tag *tag;
} LO_SpacerStruct;

typedef struct LO_NoBreakStruct_struct {
    LO_Any lo_any;

    /* nothing really to nobreak elements */
    Bool is_end;
} LO_NoBreakStruct;

typedef struct LO_ParagraphStruct_struct {
    LO_Any lo_any;

    Bool is_end;
    intn blank_lines;

    /* only used in the close element */
    Bool implicit_end;

    /* only used in the open element */
    Bool alignment_set;
    int32 alignment;
    Bool has_top_ss_margin;
    Bool has_bottom_ss_margin;
} LO_ParagraphStruct;

typedef struct LO_FloatStruct_struct {
    LO_Any lo_any;

    LO_Element *float_ele;
} LO_FloatStruct;

typedef struct LO_ListStruct_struct {
    LO_Any lo_any;

    Bool is_end;

    PA_Tag *tag;
    int32 bullet_type;
    int32 bullet_start;
    int32 quote_type;
    Bool compact;

} LO_ListStruct;

typedef struct LO_DescTitle_struct {
    LO_Any lo_any;
} LO_DescTitleStruct;

typedef struct LO_DescText_struct {
    LO_Any lo_any;
} LO_DescTextStruct;

typedef struct LO_BlockQuote_struct {
    LO_Any lo_any;

    Bool is_end;
    int8 quote_type;
    PA_Tag *tag;

} LO_BlockQuoteStruct;

typedef struct LO_Layer_struct {
    LO_Any lo_any;

    Bool is_end;
    LO_BlockInitializeStruct *initParams;
} LO_LayerStruct;

typedef struct LO_Heading_struct {
    LO_Any lo_any;
    
    Bool is_end;
    int32 alignment;
} LO_HeadingStruct;

typedef struct LO_Span_struct {
    LO_Any lo_any;

    Bool is_end;
#ifdef DOM
    /* should probably keep some stuff in here... perhaps
       the tag, so we can look up the inline style. */
    void *name_rec;  /* Points to lo_NameList * on the named span list */
#endif

} LO_SpanStruct;

typedef struct LO_Div_struct {
    LO_Any lo_any;

    Bool is_end;
    /* should probably keep some stuff in here... perhaps
       the tag, so we can look up the inline style. */
} LO_DivStruct;

union LO_Element_struct {
    int16 type;
    LO_Any lo_any;
    LO_TextStruct lo_text;
    LO_TextBlock lo_textBlock;
    LO_ImageStruct lo_image;
    LO_SubDocStruct lo_subdoc;
    LO_FormElementStruct lo_form;
    LO_LinefeedStruct lo_linefeed;
    LO_HorizRuleStruct lo_hrule;
    LO_BulletStruct lo_bullet;
    LO_TableStruct lo_table;
    LO_CellStruct lo_cell;
    LO_CommonPluginStruct lo_plugin;
    LO_EmbedStruct lo_embed;
    LO_EdgeStruct lo_edge;
    LO_JavaAppStruct lo_java;

    /* These next elements do nothing but mutate the doc state. */
    LO_CenterStruct lo_center;
    LO_FloatStruct lo_float;
    LO_ParagraphStruct lo_paragraph;
    LO_MulticolumnStruct lo_multicolumn;
    LO_NoBreakStruct lo_nobreak;
    LO_ListStruct lo_list;
    LO_DescTitleStruct lo_desctitle;
    LO_DescTextStruct lo_desctext;
	LO_BlockQuoteStruct lo_blockquote;
	LO_LayerStruct lo_layer;
	LO_HeadingStruct lo_heading;
	LO_SpanStruct lo_span;
	LO_DivStruct lo_div;
	LO_SpacerStruct lo_spacer;
	LO_BuiltinStruct lo_builtin;
};

struct LO_ObjectStruct_struct {
    LO_Element lo_element;
    void* stream_closure;
};

/* #ifndef NO_TAB_NAVIGATION */
#define tabFocusListMax		8
struct LO_tabFocus_struct {
    LO_Element		*pElement;
    int32			mapAreaIndex;		/* 0 means no focus, start with index 1. */
    LO_AnchorData	*pAnchor;
};
/* NO_TAB_NAVIGATION  */

struct LO_Position_struct {
    LO_Element* element;  /* The element */
    int32 position;    /* The position within the element. */
};

/* begin always <= end. Both begin and end are included in
 * the selected region. (i.e. it's closed. Half-open would
 * be better, since then we could also express insertion
 * points.)
 */

struct LO_Selection_struct {
    LO_Position begin;
    LO_Position end;
};

/* Hit test results */

#define LO_HIT_UNKNOWN  0
#define LO_HIT_LINE  1
#define LO_HIT_ELEMENT  2

#define LO_HIT_LINE_REGION_BEFORE  0
#define LO_HIT_LINE_REGION_AFTER  1
 
#define LO_HIT_ELEMENT_REGION_BEFORE  0
#define LO_HIT_ELEMENT_REGION_MIDDLE  1
#define LO_HIT_ELEMENT_REGION_AFTER  2

struct LO_HitLineResult_struct {
    int16 type;         /* LO_HIT_LINE */
    int16 region;     /* LO_HIT_LINE_POSITION_XXX */
    LO_Selection selection; /* The line */
};

struct LO_HitElementResult_struct {
    int16 type;         /* LO_HIT_ELEMENT */
    int16 region;     /* LO_HIT_ELEMENT_POSITION_XXX */
    LO_Position position;    /* The element that was hit. */

};

union LO_HitResult_struct {
    int16 type;
    LO_HitLineResult lo_hitLine;
    LO_HitElementResult lo_hitElement;
};

/* Logical navigation chunks */

#define LO_NA_CHARACTER 0
#define LO_NA_WORD 1
#define LO_NA_LINEEDGE 2
#define LO_NA_DOCUMENT 3

/* It's unclear if we shouldn't figure out how to move these
   declarations and defines into layout-private header files. */
typedef enum LO_LayerType_enum {
    LO_BLINK_LAYER,
    LO_IMAGE_LAYER,
    LO_HTML_BLOCK_LAYER,	/* Contents of <LAYER> tag, not including bg */
    LO_HTML_BODY_LAYER,		/* Contents of <BODY> tag, not including bg */
    LO_HTML_BACKGROUND_LAYER,	/* Background for <BODY> or <LAYER> */
    LO_GROUP_LAYER,		/* Parent with child background and content layers */
    LO_EMBEDDED_OBJECT_LAYER	/* Form, applet, plugin */
} LO_LayerType;

#define LO_DOCUMENT_LAYER_NAME "_DOCUMENT"
#define LO_BACKGROUND_LAYER_NAME "_BACKGROUND"
#define LO_BODY_LAYER_NAME "_BODY"
#define LO_BLINK_GROUP_NAME "_BLINKGROUP"
#define LO_CONTENT_LAYER_NAME "_CONTENT"
#define LO_EMBED_LAYER_NAME "_EMBED"
#define LO_BUILTIN_LAYER_NAME "_BUILTIN"

#define LO_DOCUMENT_LAYER_ID 0

#endif /* _LayoutElements_ */
