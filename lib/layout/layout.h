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

#ifndef _Layout_h_
#define _Layout_h_

#define M12N                    /* XXXM12N include m12n changes, exclude old
                                   stuff. */

#include "xp_core.h"
#include "net.h"
#include "cl_types.h"

#include "xp_obs.h"             /* Observer/observables. */
#include "il_types.h"

/* stylesheets stuff */
#include "stystruc.h"
#include "stystack.h"

#include "libmocha.h"

#define MEMORY_ARENAS 1

#define NEXT_ELEMENT	state->top_state->element_id++

#define MAX_INPUT_WRITE_LEVEL   10
#define LINE_BUF_INC		200
#define FONT_HASH_SIZE		127
#define DEFAULT_BASE_FONT_SIZE	3

#define PRE_TEXT_NO		0
#define PRE_TEXT_YES		1
#define PRE_TEXT_WRAP		2
#define PRE_TEXT_COLS		3

#define QUOTE_NONE		0
#define QUOTE_MQUOTE		1
#define QUOTE_JWZ		2
#define QUOTE_CITE		3

#define SUBDOC_NOT		0
#define SUBDOC_IS		1
#define SUBDOC_CELL		2
#define SUBDOC_CAPTION		3

#define LO_ALIGN_DEFAULT	-1
#define LO_ALIGN_CENTER		0
#define LO_ALIGN_LEFT		1
#define LO_ALIGN_RIGHT		2
#define LO_ALIGN_TOP		3
#define LO_ALIGN_BOTTOM		4
#define LO_ALIGN_BASELINE	5
#define LO_ALIGN_NCSA_CENTER	6
#define LO_ALIGN_NCSA_BOTTOM	7
#define LO_ALIGN_NCSA_TOP	8
#define LO_ALIGN_JUSTIFY	9

#define ICON_X_OFFSET 4
#define ICON_Y_OFFSET 4

#ifdef EDITOR
extern char* lo_alignStrings[];
#endif

#define	S_FORM_TYPE_TEXT	"text"
#define	S_FORM_TYPE_RADIO	"radio"
#define	S_FORM_TYPE_CHECKBOX	"checkbox"
#define	S_FORM_TYPE_HIDDEN	"hidden"
#define	S_FORM_TYPE_SUBMIT	"submit"
#define	S_FORM_TYPE_RESET	"reset"
#define	S_FORM_TYPE_PASSWORD	"password"
#define	S_FORM_TYPE_BUTTON	"button"
#define	S_FORM_TYPE_IMAGE	"image"
#define	S_FORM_TYPE_FILE	"file"
#define	S_FORM_TYPE_JOT		"jot"
#define	S_FORM_TYPE_READONLY	"readonly"
#define	S_FORM_TYPE_OBJECT	"object"

#define	S_AREA_SHAPE_DEFAULT	"default"
#define	S_AREA_SHAPE_RECT	"rect"
#define	S_AREA_SHAPE_CIRCLE	"circle"
#define	S_AREA_SHAPE_POLY	"poly"
#define	S_AREA_SHAPE_POLYGON	"polygon"	/* maps to AREA_SHAPE_POLY */

#define	AREA_SHAPE_UNKNOWN	0
#define	AREA_SHAPE_DEFAULT	1
#define	AREA_SHAPE_RECT		2
#define	AREA_SHAPE_CIRCLE	3
#define	AREA_SHAPE_POLY		4

#define	BODY_ATTR_BACKGROUND	0x01
#define	BODY_ATTR_BGCOLOR	0x02
#define	BODY_ATTR_TEXT		0x04
#define	BODY_ATTR_LINK		0x08
#define	BODY_ATTR_VLINK		0x10
#define	BODY_ATTR_ALINK		0x20
#define	BODY_ATTR_MARGINS	0x40
#define	BODY_ATTR_JAVA		0x80

#define DEF_TAB_WIDTH		8

extern LO_Color lo_master_colors[];

typedef struct lo_NameList_struct {
	int32 x, y;
	LO_Element *element;
	PA_Block name; 	/* really a (char *) */
	struct lo_NameList_struct *next;
#ifdef MOCHA
        uint32 index;           /* Needed because with nested tables, the list
                                   is not guaranteed to be ordered. */
	struct JSObject *mocha_object;
#endif
} lo_NameList;

typedef struct lo_SavedFormListData_struct {
	XP_Bool valid;
	int32 data_index;
	int32 data_count;
	PA_Block data_list; 	/* really a (LO_FormElementData **) */
	struct lo_SavedFormListData_struct *next;
} lo_SavedFormListData;

typedef void (*lo_FreeProc)(MWContext*, void*);

typedef struct lo_EmbedDataElement {
	void *data;
	lo_FreeProc freeProc;
} lo_EmbedDataElement;

typedef struct lo_SavedEmbedListData_struct {
	int32 embed_count;
	PA_Block embed_data_list; 	/* really a (lo_EmbedDataElement **) */
} lo_SavedEmbedListData;

typedef struct lo_SavedGridData_struct {
	int32 main_width;
	int32 main_height;
	struct lo_GridRec_struct *the_grid;
} lo_SavedGridData;

typedef struct lo_MarginStack_struct {
	int32 margin;
	int32 y_min, y_max;
	struct lo_MarginStack_struct *next;
} lo_MarginStack;


typedef struct lo_FontStack_struct {
	int32 tag_type;
	LO_TextAttr *text_attr;
	struct lo_FontStack_struct *next;
} lo_FontStack;


typedef struct lo_AlignStack_struct {
	intn type;
	int32 alignment;
	struct lo_AlignStack_struct *next;
} lo_AlignStack;


typedef struct lo_ListStack_struct {
	intn level;
	int type;
	int32 value;
	Bool compact;
	int bullet_type;
	int8 quote_type;
    int32 mquote_line_num; /* First line to put the mquote bullets on. */
	int32 mquote_x; /* How far to indent the mquote. */
	int32 old_left_margin;
	int32 old_right_margin;
	struct lo_ListStack_struct *next;
} lo_ListStack;

typedef struct lo_LayerDocState lo_LayerDocState;
typedef struct lo_LayerStack lo_LayerStack;

typedef struct lo_ObjectStack_struct {
	LO_ObjectStruct *object;
	struct lo_ObjectStack_struct *next;
	uint32 param_count;
	char** param_names;
	char** param_values;
	char* data_url;
	MWContext* context;
	struct lo_DocState_struct* state;
	PA_Tag* real_tag;
	PA_Tag* clone_tag;
	Bool read_params;
	Bool formatted_object;
} lo_ObjectStack;


typedef struct lo_LineHeightStack_struct {
	int32  height;
	struct lo_LineHeightStack_struct *next;
} lo_LineHeightStack;

typedef struct lo_FormData_struct {
	intn id;
	PA_Block action;
	PA_Block encoding;
	PA_Block window_target;
	int32 method;
	int32 form_ele_cnt;
	int32 form_ele_size;
	PA_Block form_elements;		/* really a (LO_Element **) */
	struct lo_FormData_struct *next;
#ifdef MOCHA
	PA_Block name;
	struct JSObject *mocha_object;
#endif
} lo_FormData;


typedef struct lo_TableCell_struct {
	Bool start_in_form;
	intn form_id;
	int32 form_ele_cnt;
	int32 form_data_index;
	int32 embed_count_base;
	int32 url_count_base;
        int32 image_list_count_base;
        int32 applet_list_count_base;
        int32 embed_list_count_base;
        int32 current_layer_num_base;
	Bool must_relayout;
	intn is_a_subdoc;
	PA_Tag *subdoc_tags;
	PA_Tag *subdoc_tags_end;
	int32 max_y;
	int32 horiz_alignment;
	int32 vert_alignment;
	int32 cell_base_x;
	int32 cell_base_y;

	Bool cell_done;
	Bool nowrap;
	Bool is_a_header;
	int32 specified_width, specified_height;
	int32 percent_width, percent_height;
	int32 min_width, max_width;
	int32 height, baseline;
	int32 rowspan, colspan;
	LO_CellStruct *cell;
	struct lo_TableCell_struct *next;
	int32 beginning_tag_count;
	Bool in_nested_table;	/* Can be removed once we remove subdoc tags.  							 
							 * Used to figure out whether we shoul free subdoc tags or not 
							 */
} lo_TableCell;


typedef struct lo_TableRow_struct {
	Bool row_done;
	Bool has_percent_width_cells;
    lo_Backdrop backdrop;       /* default for cells to inherit */
	int32 cells;			/* is a counter that gets updated as a new cell is seen for a row */
	int32 cells_in_row;		/* is the exact number of LO_CELLS in the line list for each row.  Calculated in lo_fill_cell_array */
	int32 vert_alignment;
	int32 horiz_alignment;
	lo_TableCell *cell_list;
	lo_TableCell *cell_ptr;
	struct lo_TableRow_struct *next;
} lo_TableRow;


typedef struct lo_TableCaption_struct {
	int32 vert_alignment;
	int32 horiz_alignment;
	int32 min_width, max_width;
	int32 height;
	int32 rowspan, colspan;
	LO_SubDocStruct *subdoc;
	LO_CellStruct *cell_ele;	/* Kept around for relayout without reload */
} lo_TableCaption;


typedef struct lo_table_span_struct {
	int32 dim, min_dim;
	int32 span;
	struct lo_table_span_struct *next;
} lo_table_span;

typedef struct lo_TableRec_struct {
	intn draw_borders;
	Bool has_percent_width_cells;
	Bool table_width_fixed;
	Bool has_percent_height_cells;
	int32 percent_width;
	int32 percent_height;
    lo_Backdrop backdrop;       /* default for cells to inherit */
	int32 rows, cols;
	int32 width, height;
	int32 inner_top_pad;
	int32 inner_bottom_pad;
	int32 inner_left_pad;
	int32 inner_right_pad;
	int32 inter_cell_pad;
	int32 default_cell_width;
	int32 fixed_cols;
	int32 fixed_width_remaining;
	lo_table_span *width_spans;
	lo_table_span *width_span_ptr;
	lo_table_span *height_spans;
	lo_table_span *height_span_ptr;
	lo_TableCaption *caption;
	LO_TableStruct *table_ele;
	LO_SubDocStruct *current_subdoc;
	lo_TableRow *row_list;
	lo_TableRow *row_ptr;
	int32 *fixed_col_widths;	
} lo_TableRec;


/*
 * This structure contains all the element lists associated with a
 * document that are reflected into mocha. There's one per top-level
 * document (in the top_state), and one per layer.
 * XXX BUGBUG The things that are missing from this list include:
 *  - The savedData stuff
 *  - A document's map_list
 * Why? I don't feel comfortable moving these guys around at this point.
 */
typedef struct lo_DocLists_struct {
    LO_EmbedStruct *embed_list;     /* embeds linked in reverse order */
    int32 embed_list_count;         /* The number of embeds in the list */
    
    LO_JavaAppStruct *applet_list;  /* applets linked in reverse order */
    int32 applet_list_count;        /* The number of applets in the list */

    LO_ImageStruct *image_list;	    /* list of images in doc */
    LO_ImageStruct *last_image;	    /* last image in image_list */
    uint32 image_list_count;        /* Number of images in the list */

    lo_NameList *name_list;	/* list of positions of named anchors */
    uint32 anchor_count;        /* Count of named anchors in this document */

    lo_FormData *form_list;	/* list of forms in doc */
    intn current_form_num;      /* The id of the next form to lay out */

#ifdef XP_WIN16
    intn ulist_array_size;
    XP_Block ulist_array;	/* really a (XP_Block *) */
#endif /* XP_WIN16 */
    XP_Block url_list;		/* really a (LO_AnchorData **) PA_Block */
    intn url_list_len;          /* Number of URLs in the list */
    intn url_list_size;         /* Size of the list buffer */

} lo_DocLists;

typedef struct lo_MultiCol_struct {
	LO_Element *end_last_line;
	int32 start_ele;
	int32 start_line, end_line;
	int32 start_y, end_y;
	int32 start_x;
	int32 cols;
	int32 col_width;
	int32 orig_margin;
	int32 orig_min_width;
	int32 orig_display_blocking_element_y;
	int32 gutter;
	Bool orig_display_blocked;
	Bool close_table;
	int32 width;			/* For relayout without reload */
	Bool isPercentWidth;	/* For relayout without reload */
	struct lo_MultiCol_struct *next;
} lo_MultiCol;

#define	LO_PERCENT_NORMAL	0
#define	LO_PERCENT_FREE		1
#define	LO_PERCENT_FIXED	2

typedef struct lo_GridPercent_struct {
	uint8 type;
	int32 value;
	int32 original_value;
} lo_GridPercent;

typedef struct lo_GridHistory_struct {
	void *hist;		/* really a SHIST history pointer */
	int32 position;
} lo_GridHistory;

typedef struct lo_GridCellRec_struct {
	int32 x, y;
	int32 width, height;
	int32 margin_width, margin_height;
	intn width_percent;
	intn height_percent;
	char *url;
	char *name;
	MWContext *context;
	void *hist_list;        /* really a (XP_List *) */
	lo_GridHistory *hist_array;
	int32 hist_indx;
	int8 scrolling;
	Bool resizable;
	Bool no_edges;
	int16 edge_size;
	int8 color_priority;
	LO_Color *border_color;
	struct lo_GridEdge_struct *side_edges[4]; /* top, bottom, left, right */
	struct lo_GridCellRec_struct *next;
	struct lo_GridRec_struct *subgrid;

	Bool needs_restructuring;
} lo_GridCellRec;

typedef struct lo_GridEdge_struct {
	int32 x, y;
	int32 width, height;
	Bool is_vertical;
	int32 left_top_bound;
	int32 right_bottom_bound;
	int32 cell_cnt;
	int32 cell_max;
	lo_GridCellRec **cell_array;
	LO_EdgeStruct *fe_edge;
	struct lo_GridEdge_struct *next;

	Bool dealt_with_in_relayout;
} lo_GridEdge;

typedef struct lo_GridRec_struct {
	int32 main_width, main_height;
	int32 current_hist, max_hist, hist_size;
	int32 grid_cell_border;
	int32 grid_cell_min_dim;
	int32 rows, cols;
	int32 current_cell_margin_width;
	int32 current_cell_margin_height;
	lo_GridPercent *row_percents;
	lo_GridPercent *col_percents;
	lo_GridCellRec *cell_list;
	lo_GridCellRec *cell_list_last;
	int32 cell_count;
	Bool no_edges;
	int16 edge_size;
	int8 color_priority;
	LO_Color *border_color;
	lo_GridEdge *edge_list;
	struct lo_GridRec_struct *subgrid;
} lo_GridRec;

extern lo_GridCellRec *
lo_ContextToCell(MWContext *context, Bool reconnect, lo_GridRec **grid_ptr);


typedef struct lo_MapRec_struct {
	char *name;
	struct lo_MapAreaRec_struct *areas;
	struct lo_MapAreaRec_struct *areas_last;
	struct lo_MapRec_struct *next;
} lo_MapRec;

typedef struct lo_MapAreaRec_struct {
	int16 type;
	int16 alt_len;
	PA_Block alt;
	int32 *coords;
	int32 coord_cnt;
	LO_AnchorData *anchor;
	struct lo_MapAreaRec_struct *next;
} lo_MapAreaRec;

#ifdef HTML_CERTIFICATE_SUPPORT
typedef struct lo_Certificate_struct {
	PA_Block name;		/* really a (char *) */
	void *cert;		/* really a (char *) */
	struct lo_Certificate_struct *next;
} lo_Certificate;
#endif /* HTML_CERTIFICATE_SUPPORT */

typedef struct lo_TopState_struct 	lo_TopState;


#ifdef TEST_16BIT
#define XP_WIN16
#endif /* TEST_16BIT */
typedef struct lo_DocState_struct {
    lo_TopState *top_state;
    PA_Tag *subdoc_tags;	/* tags that created this subdoc */
    PA_Tag *subdoc_tags_end;	/* End of subdoc tags list */

    int32 base_x, base_y;	/* upper left of document */
    int32 x, y;			/* upper left of element in progress */
    int32 width;		/* width of element in progress */
    int32 win_width;            /* Document or subdoc dimensions */
    int32 win_height;
    int32 max_height;		/* farthest down item so far. */
    int32 max_width;		/* width of widest line so far. */
    int32 min_width;		/* minimum possible line width */
    int32 break_holder;		/* holder for last break position width */
    int32 line_num;		/* line number of the next line to be placed */
    int32 preformat_cols;	/* Column to break special PRE_TEXT_COLS at */
    intn linefeed_state;
    int8 preformatted;		/* processing preformatted (maybe wrap) text */
    Bool allow_amp_escapes;	/* Let & entites be expanded */
    Bool breakable;		/* Working on a breakable element */
    Bool delay_align;		/* in subdoc, delay aligning lines */
    Bool display_blocked;	/* do not display during layout */
    Bool in_paragraph;		/* inside an HTML 3.0 paragraph container */
    int32 display_blocking_element_id;
    int32 display_blocking_element_y;

    int32 win_top;
    int32 win_bottom;
    int32 win_left;
    int32 win_right;
    int32 left_margin;
    int32 right_margin;
    lo_MarginStack *left_margin_stack;
    lo_MarginStack *right_margin_stack;

    int32 text_divert;	/* the tag type of a text diverter (like P_TITLE) */

#ifdef XP_WIN16
    intn larray_array_size;
    XP_Block larray_array;	/* really a (XP_Block *) */
#endif /* XP_WIN16 */
    intn line_array_size;
    XP_Block line_array;	/* really a (LO_Element **) */
    LO_Element *line_list;	/* Element list for the current line so far */
    LO_Element *end_last_line;	/* Element at end of last line */

    LO_Element *float_list;	/* Floating element list for the current doc */

    intn base_font_size;	/* size of font at bottom of font stack */
    lo_FontStack *font_stack;

    lo_AlignStack *align_stack;

    lo_ListStack *list_stack;

    lo_LineHeightStack *line_height_stack;

    LO_TextInfo text_info;	/* for text of last text element created */
    /* this is a buffer for a single (currently processed) line of text */
    PA_Block line_buf;		/* lock to a (char *) */
    int32 line_buf_len;
    int32 line_buf_size;
    int32 baseline;
    int32 line_height;
    int32 default_line_height;
    int32 break_pos;		/* position in middle of current element */
    int32 break_width;
    Bool last_char_CR;
    Bool trailing_space;
    Bool at_begin_line;
    Bool hide_content;  /* set to not display text */

    LO_TextStruct *old_break;
    LO_TextBlock *old_break_block;
    int32 old_break_pos;	/* in middle of older element on this line */
    int32 old_break_width;

    PA_Block current_named_anchor;	/* lock to a (char *) */
    LO_AnchorData *current_anchor;

    intn cur_ele_type;
    LO_Element *current_ele;	/* element in process, no end tag yet */

    LO_JavaAppStruct *current_java; /* java applet in process, no end tag yet */
	
    lo_TableRec *current_table;
    lo_GridRec *current_grid;
    lo_MultiCol *current_multicol;
    intn layer_nest_level;	/* Layer nesting level for this (sub)doc */

    Bool start_in_form;		/* Was in a form at start of (sub)doc */
    intn form_id;		/* first form entering this (sub)doc */
    int32 form_ele_cnt;		/* first form element entering this (sub)doc */
    int32 form_data_index;	/* first form data entering this (sub)doc */
    int32 embed_count_base;	/* number of embedded elements (applets and embeds) before beginning this (sub)doc */
    /* 
     * Just to clarify: the embed_count is the total of all applets and 
     * embeds in the document. This is used for indexing into the saved
     * data structure. The applet_list_count and embed_list_count are
     * applets and embeds in the current container. This is used to index
     * into the applet and embed lists of the container.
     */
    int32 url_count_base;	/* number of anchors before beginning this (sub)doc */
    int32 image_list_count_base;  /* number of images before this (sub)doc */
    int32 applet_list_count_base; /* number of applets before this (sub)doc */
    int32 embed_list_count_base;  /* number of embeds before this (sub)doc */
    int32 current_layer_num_base; /* id of last layer before this (sub)doc */
    int32 current_layer_num_max;  /* id of the last layer in this (sub)doc */
    Bool must_relayout_subdoc;
    Bool allow_percent_width;
    Bool allow_percent_height;
    intn is_a_subdoc;
    int32 current_subdoc;
    XP_Block sub_documents;	/* really a lo_DocState ** array */
    struct lo_DocState_struct *sub_state;

    LO_Element *current_cell;

    Bool extending_start;
    LO_Element *selection_start;
    int32 selection_start_pos;
    LO_Element *selection_end;
    int32 selection_end_pos;
    LO_Element *selection_new;
    int32 selection_new_pos;
    CL_Layer *selection_layer;

    LO_Color text_fg;
    LO_Color text_bg;
    LO_Color anchor_color;
    LO_Color visited_anchor_color;
    LO_Color active_anchor_color;
    ED_Element *edit_current_element;
    int edit_current_offset;
    Bool edit_force_offset;
    Bool edit_relayout_display_blocked;
#ifdef MOCHA
    Bool in_relayout;	/* true if we're in lo_RelayoutSubdoc */
#endif

    int32 tab_stop;
    int32 beginning_tag_count;  /* the tag count at the beginning of this state */
    
    LO_TextBlock * cur_text_block;
    Bool need_min_width;
    uint32 src_text_offset;
} lo_DocState;

#ifdef TEST_16BIT
#undef XP_WIN16
#endif /* TEST_16BIT */

#define STATE_DEFAULT_FG_RED(state)		((state)->text_fg.red)
#define STATE_DEFAULT_FG_GREEN(state)		((state)->text_fg.green)
#define STATE_DEFAULT_FG_BLUE(state)		((state)->text_fg.blue)

#define STATE_DEFAULT_BG_RED(state)		((state)->text_bg.red)
#define STATE_DEFAULT_BG_GREEN(state)		((state)->text_bg.green)
#define STATE_DEFAULT_BG_BLUE(state)		((state)->text_bg.blue)

#define STATE_UNVISITED_ANCHOR_RED(state)	((state)->anchor_color.red)
#define STATE_UNVISITED_ANCHOR_GREEN(state)	((state)->anchor_color.green)
#define STATE_UNVISITED_ANCHOR_BLUE(state)	((state)->anchor_color.blue)

#define STATE_VISITED_ANCHOR_RED(state)		((state)->visited_anchor_color.red)
#define STATE_VISITED_ANCHOR_GREEN(state)	((state)->visited_anchor_color.green)
#define STATE_VISITED_ANCHOR_BLUE(state)	((state)->visited_anchor_color.blue)

#define STATE_SELECTED_ANCHOR_RED(state)	((state)->active_anchor_color.red)
#define STATE_SELECTED_ANCHOR_GREEN(state)	((state)->active_anchor_color.green)
#define STATE_SELECTED_ANCHOR_BLUE(state)	((state)->active_anchor_color.blue)


#ifdef MEMORY_ARENAS
typedef struct lo_arena_struct {
        struct lo_arena_struct *next;
        char *limit;
        char *avail;
} lo_arena;
#endif /* MEMORY_ARENAS */

/*
** This is just like SHIST_SavedData except the fields are more strictly
** typed.
*/
typedef struct lo_SavedData {
    lo_SavedFormListData*	FormList;	/* Data for all form elements in doc */
    lo_SavedEmbedListData*	EmbedList;	/* session data for all embeds and applets in doc */
    lo_SavedGridData*		Grid;		/* saved for grid history. */
#ifdef MOCHA
    PA_Block			OnLoad;		/* JavaScript onload event handler source */
    PA_Block			OnUnload;	/* JavaScript onunload event handler source */
    PA_Block			OnFocus;	/* JavaScript onfocus event handler source */
    PA_Block			OnBlur;		/* JavaScript onblur event handler source */
    PA_Block			OnHelp;		/* JavaScript onhelp event handler source */
    PA_Block			OnMouseOver;	/* JavaScript onmouseover event handler source */
    PA_Block			OnMouseOut;	/* JavaScript onmouseout event handler source */
    PA_Block			OnDragDrop;	/* JavaScript ondragdrop event handler source */
    PA_Block			OnMove;		/* JavaScript onmove event handler source */
    PA_Block			OnResize;	/* JavaScript onresize event handler source */
#endif
} lo_SavedData;

/* 
 * This is used when we have an input stream and we need to defer
 * the destruction of the main parser stream till we're done 
 * processing all inline streams.
 */
typedef void lo_FreeStreamFunc(MWContext *, NET_StreamClass *);

/*
 * Top level state
 */
struct lo_TopState_struct {
#ifdef MEMORY_ARENAS
    lo_arena *first_arena;
    lo_arena *current_arena;
#endif /* MEMORY_ARENAS */
    PA_Tag *tags;		/* Held tags while layout is blocked */
    PA_Tag **tags_end;		/* End of held tags list */
    PRPackedBool doc_specified_bg;
    PRPackedBool nothing_displayed;
    PRPackedBool in_head;	/* Still in the HEAD of the HTML doc. */
    PRPackedBool in_body;	/* In the BODY of the HTML doc. */
    PRPackedBool scrolling_doc;	/* Is this a special scrolling doc (hack) */
    PRPackedBool have_title;	/* set by first <TITLE> */
    PRPackedBool in_form;	/* true if in <FORM>...</FORM> */
    uint8 body_attr;		/* What attributes were set by BODY */
    char *unknown_head_tag;	/* ignore content in this case if non-NULL */
    char *base_target;		/* Base target of urls in this document */
    char *base_url;		/* Base url of this document */
    char *inline_stream_blocked_base_url; /* Base url for prefetched images */
    char *main_stream_blocked_base_url; /* Base url for prefetched images */
    char *url;			/* Real url of this document */
    char *name_target;		/* The #name part of the url */
    int32 element_id;
    LO_Element *layout_blocking_element;
    LO_Element *current_script;
    intn layout_status;

    lo_DocLists doc_lists;      /* Lists of reflected elements */

    lo_LayerDocState **layers;  /* Array of ptrs to all layers in current doc */
    int32 num_layers_allocated; /* Length of layers array */
    int32 current_layer_num;    /* Id of last layer to be laid out */
    int32 max_layer_num;        /* Largest id of any layer yet laid out */
    
    lo_LayerStack *layer_stack;

    lo_MapRec *map_list;	/* list of maps in doc */
    lo_MapRec *current_map;	/* Current open MAP tag */

    int32 embed_count;	        /* master count of embeds in this document */
    lo_SavedData savedData;	/* layout data */

    int32 total_bytes;
    int32 current_bytes;
    int32 layout_bytes;
    int32 script_bytes;
    intn layout_percent;

    lo_GridRec *the_grid;
    lo_GridRec *old_grid;

    PRPackedBool is_grid;	/* once a grid is started, ignore other tags */
    int ignore_tag_nest_level;   /* For NOSCRIPT, NOEMBED, NOLAYER */
    int ignore_layer_nest_level; /* For <LAYER SUPPRESS> */
    
    PRPackedBool in_nogrids;	/* special way to ignore tags in <noframes> */
    PRPackedBool in_applet;	/* special way to ignore tags in java applet */
    PRPackedBool is_binary;	/* hack for images instead of HTML doc */
    PRPackedBool insecure_images; /* secure doc with insecure images */
    PRPackedBool out_of_memory;	/* ran out of memory mid-layout */
    NET_ReloadMethod
         force_reload;		/* URL_Struct reload flag */
    intn auto_scroll;		/* Chat auto-scrolling layout, # of lines */
    intn security_level;	/* non-zero for secure docs. */
    URL_Struct *nurl;		/* the netlib url struct */

    PA_Block font_face_array;	/* really a (char **) */
    intn font_face_array_len;
    intn font_face_array_size;

    XP_Block text_attr_hash;	/* really a (LO_TextAttr **) */

    LO_Element *recycle_list;	/* List of Elements to be reused */
    LO_Element *trash;		/* List of Elements to discard later */

    lo_DocState *doc_state;

    ED_Buffer *edit_buffer;
#ifdef MOCHA
    uint32 script_tag_count;	/* number of script tags processed so far */
    uint script_lineno;	/* parser newline count at last <SCRIPT> tag */
    int8 in_script;		/* script type if in script, see below */
    PRPackedBool in_blocked_script; /* inside blocked <SCRIPT> tag container */
    int8 default_style_script_type;    /* the default script type or the last 
					* type of script encountered 
				 	*/
    PRPackedBool resize_reload;	/* is this a resize-induced reload? */
    PRPackedBool mocha_has_onload;
    PRPackedBool mocha_has_onunload;
    JSVersion version;
    uint32 mocha_loading_applets_count;
    uint32 mocha_loading_embeds_count;
    NET_StreamClass *mocha_write_stream;
    void *scriptData;
#endif

    struct pa_DocData_struct *doc_data;
    PA_Tag **input_write_point[MAX_INPUT_WRITE_LEVEL];
    uint32 input_write_level;
    PRPackedBool tag_from_inline_stream;

#ifdef HTML_CERTIFICATE_SUPPORT
    lo_Certificate *cert_list;	/* list of certificates in doc */
#endif
    StyleAndTagStack * style_stack;
    char *small_bm_icon;
    char *large_bm_icon;

    Bool diff_state;		/* set if our state record is different but at the same level */
    int32 state_pushes;		/* how many new states have we added */
    int32 state_pops;		/* how many states have we removed */
    CL_Layer *doc_layer;        /* Root document layer */
    CL_Layer *body_layer;       /* Layer enclosing contents of BODY tag */
    lo_ObjectStack *object_stack;	/* See layobj.c */
    lo_ObjectStack *object_cache;
    int32 tag_count;            /* sequential tag numbering */
    PRPackedBool flushing_blockage;
    PRPackedBool wedged_on_mocha;
	Bool in_cell_relayout;
	int16 table_nesting_level;	/* Counter to keep track of depth of nesting within tables */
#ifdef DEBUG_ScriptPlugin
	char * mimetype;
#endif 
};

/* Script type codes, stored in top_state->in_script. */
#define SCRIPT_TYPE_NOT		0
#define SCRIPT_TYPE_MOCHA	1
#define SCRIPT_TYPE_CSS		2
#define SCRIPT_TYPE_JSSS	3
#define SCRIPT_TYPE_UNKNOWN	4
#ifdef DEBUG_ScriptPlugin
#define SCRIPT_TYPE_PLUGIN  5
#endif
#ifdef MOCHA
extern void 
lo_DestroyScriptData(void *data); 
#endif

/* Define to get the reload flag of the current document */
#define FORCE_RELOAD_FLAG(top_state) \
	((top_state)->resize_reload ? NET_RESIZE_RELOAD : (top_state)->force_reload)


/*
	Internal Prototypes
*/

extern lo_FontStack *lo_DefaultFont(lo_DocState *, MWContext *);
extern Bool lo_FilterTag(MWContext *, lo_DocState *, PA_Tag *);
extern void lo_LayoutTag(MWContext *, lo_DocState *, PA_Tag *);
extern void lo_PreLayoutTag(MWContext *, lo_DocState *, PA_Tag *);
extern void lo_PostLayoutTag(MWContext *, lo_DocState *, PA_Tag *, XP_Bool);
extern void lo_SaveSubdocTags(MWContext *, lo_DocState *, PA_Tag *);
extern void lo_FreshText(lo_DocState *);
extern void lo_InsertLineBreak(MWContext *, lo_DocState *, uint32, uint32, Bool);
extern void lo_HardLineBreak(MWContext *, lo_DocState *, Bool);
extern void lo_HardLineBreakWithClearType(MWContext *, lo_DocState *, uint32, Bool);
extern void lo_SoftLineBreak(MWContext *, lo_DocState *, Bool);
extern void lo_SetSoftLineBreakState(MWContext *, lo_DocState *, Bool, intn);
extern void lo_SetLineBreakState(MWContext *context, lo_DocState *state, Bool breaking,
	uint32 break_type, intn linefeed_state, Bool relayout);
extern void lo_InsertWordBreak(MWContext *, lo_DocState *);
extern void lo_FormatText(MWContext *, lo_DocState *, char *);
extern void lo_PreformatedText(MWContext *, lo_DocState *, char *);
extern LO_Element * lo_RelayoutTextBlock ( MWContext * context, lo_DocState * state, LO_TextBlock * block, LO_TextStruct * fromElement );
extern Bool lo_ChangeText ( LO_TextBlock * block, char * text );
extern void lo_FlushLineBuffer(MWContext *, lo_DocState *);
extern void lo_FlushTextBlock ( MWContext *context, lo_DocState *state );
extern void lo_ResetFontStack(MWContext *, lo_DocState *);
extern void lo_ChangeBodyTextFGColor(MWContext *context, lo_DocState *state, LO_Color *color);
extern void lo_PushFont(lo_DocState *, intn, LO_TextAttr *);
extern LO_TextAttr *lo_PopFont(lo_DocState *, intn);
extern void lo_PopAllAnchors(lo_DocState *);
extern void lo_FlushLineList(MWContext *, lo_DocState *, uint32, uint32, Bool);
extern LO_LinefeedStruct *lo_NewLinefeed(lo_DocState *, MWContext *, uint32, uint32);
extern void lo_HorizontalRule(MWContext *, lo_DocState *, PA_Tag *);
extern void lo_FormatImage(MWContext *, lo_DocState *, PA_Tag *);
extern void lo_FormatEmbed(MWContext *, lo_DocState *, PA_Tag *);
extern void lo_RelayoutEmbed ( MWContext *context, lo_DocState *state,
	LO_EmbedStruct * embed, PA_Tag * tag );
extern void lo_FormatJavaApp(MWContext *, lo_DocState *, PA_Tag *);
extern void lo_JavaAppParam(MWContext *, lo_DocState *, PA_Tag *, LO_JavaAppStruct *);
extern void lo_CloseJavaApp(MWContext *, lo_DocState *, LO_JavaAppStruct *);
void lo_RelayoutJavaApp(MWContext *, lo_DocState *, PA_Tag *, LO_JavaAppStruct * );
extern void lo_ProcessSpacerTag(MWContext *, lo_DocState *, PA_Tag *);
extern void lo_ProcessBodyTag(MWContext *, lo_DocState *, PA_Tag *);
extern char *lo_ParseBackgroundAttribute(MWContext *, 
                                         lo_DocState *, PA_Tag *, 
                                         Bool);
extern void lo_BodyBackground(MWContext *, lo_DocState *, PA_Tag *, Bool);
extern void lo_BodyForeground(MWContext *, lo_DocState *, PA_Tag *);
extern int32 lo_ValueOrPercent(char *, Bool *);
extern void lo_BreakOldElement(MWContext *, lo_DocState *);
extern void lo_BreakOldTextBlockElement(MWContext *context, lo_DocState *state);
extern int32 lo_baseline_adjust(MWContext *context, lo_DocState *state, LO_Element *ele_list,
	int32 old_baseline, int32 old_line_height);
extern void lo_UpdateElementPosition ( lo_DocState * state, LO_Element * element );
extern void lo_CopyTextAttr(LO_TextAttr *, LO_TextAttr *);
extern LO_TextAttr *lo_FetchTextAttr(lo_DocState *, LO_TextAttr *);
extern void lo_FindLineMargins(MWContext *, lo_DocState *, Bool updateFE);
extern void lo_AddMarginStack(lo_DocState *, int32, int32, int32, int32,
				int32, int32, int32, intn);
extern LO_AnchorData *lo_NewAnchor(lo_DocState *, PA_Block, PA_Block);
extern void lo_DestroyAnchor(LO_AnchorData *anchor_data);
extern void lo_AddToUrlList(MWContext *, lo_DocState *, LO_AnchorData *);
extern void lo_AddEmbedData(MWContext *, void *, lo_FreeProc, int32);
extern lo_ListStack *lo_DefaultList(lo_DocState *);
extern void lo_PushList(lo_DocState *, PA_Tag *, int8);
extern lo_ListStack *lo_PopList(lo_DocState *, PA_Tag *);

extern void lo_PushAlignment(lo_DocState *, intn, int32);
extern lo_AlignStack *lo_PopAlignment(lo_DocState *);

extern void lo_PushLineHeight(lo_DocState *, int32);
extern lo_LineHeightStack *lo_PopLineHeight(lo_DocState *);

extern Bool lo_InitDocLists(MWContext *context, lo_DocLists *doc_lists);
extern void lo_DeleteDocLists(MWContext *context, lo_DocLists *doc_lists);

extern lo_TopState *lo_NewTopState(MWContext *, char *);
extern void lo_PushStateLevel(MWContext *context);
extern void lo_PopStateLevel(MWContext *context);
extern lo_DocState *lo_NewLayout(MWContext *, int32, int32, int32, int32,
				lo_DocState*);
extern lo_DocState *
lo_InitDocState(lo_DocState *state, MWContext *context,
                int32 width, int32 height,
                int32 margin_width, int32 margin_height,
                lo_DocState* clone_state,
                lo_DocLists *doc_lists,
                PRBool is_for_new_layer);

extern lo_SavedFormListData *lo_NewDocumentFormListData(void);
extern Bool lo_StoreTopState(int32, lo_TopState *);
extern lo_TopState *lo_FetchTopState(int32);
extern lo_DocState *lo_TopSubState(lo_TopState *);
extern lo_DocState *lo_CurrentSubState(lo_DocState *);

extern void lo_InheritParentState(MWContext *, lo_DocState *, lo_DocState *);
extern void lo_InheritParentColors(MWContext *context, lo_DocState *child_state, lo_DocState *parent_state);
extern void lo_GetImage(MWContext *context, IL_GroupContext *img_cx,
                        LO_ImageStruct *lo_image, XP_ObserverList obs_list,
                        NET_ReloadMethod requested_reload_method);
extern void lo_NoMoreImages(MWContext *);
extern void lo_FinishImage(MWContext *, lo_DocState *, LO_ImageStruct *);
extern void lo_FinishEmbed(MWContext *, lo_DocState *, LO_EmbedStruct *);
extern void lo_BlockedImageLayout(MWContext *, lo_DocState *, PA_Tag *, char *base_url);
extern void lo_PartialFormatImage(MWContext *, lo_DocState *, PA_Tag *);
extern void lo_FlushBlockage(MWContext *, lo_DocState *, lo_DocState *);
extern void lo_FinishLayout(MWContext *, lo_DocState *, int32);
extern void lo_FreeLayoutData(MWContext *, lo_DocState *);
extern void lo_CloseMochaWriteStream(lo_TopState *top_state, int mocha_event);
struct pa_DocData_struct;
extern LO_Element *lo_InternalDiscardDocument(MWContext *, lo_DocState *, struct pa_DocData_struct *, Bool bWholeDoc);
extern void lo_FreePartialTable(MWContext *, lo_DocState *, lo_TableRec *);
extern void lo_FreePartialSubDoc(MWContext *, lo_DocState *, LO_SubDocStruct *);
extern void lo_FreePartialCell(MWContext *context, lo_DocState *state,
				LO_CellStruct *cell);
extern void lo_FreeElement(MWContext *context, LO_Element *, Bool);
extern void lo_FreeElementList(MWContext *, LO_Element *);
extern void lo_FreeImageAttributes(LO_ImageAttr *image_attr);
extern void lo_ScrapeElement(MWContext *context, LO_Element *);
extern int32 lo_FreeRecycleList(MWContext *context, LO_Element *);
extern void lo_FreeGridRec(lo_GridRec *);
extern void lo_FreeGridCellRec(MWContext *, lo_GridRec *, lo_GridCellRec *);
extern void lo_FreeGridEdge(lo_GridEdge *);
extern void lo_FreeFormElementData(LO_FormElementData *element_data);
extern void lo_free_table_record(MWContext *context, lo_DocState *state,
			lo_TableRec *table, Bool partial);

#ifdef MEMORY_ARENAS
extern void lo_InitializeMemoryArena(lo_TopState *);
extern int32 lo_FreeMemoryArena(lo_arena *);
extern char *lo_MemoryArenaAllocate(lo_TopState *, int32);
#endif /* MEMORY_ARENAS */

extern void lo_fillin_text_info(MWContext *context, lo_DocState *state);
extern char *lo_FetchFontFace(MWContext *, lo_DocState *, char *);
extern PA_Block lo_FetchParamValue(MWContext *, PA_Tag *, char *);
extern PA_Block lo_ValueToAlpha(int32, Bool, intn *);
extern void lo_PlaceQuoteMarker(MWContext *, lo_DocState *, lo_ListStack *);
extern void lo_PlaceBulletStr(MWContext *, lo_DocState *);
extern void lo_PlaceBullet (MWContext *, lo_DocState *);
extern PA_Block lo_ValueToRoman(int32, Bool, intn *);
extern void lo_ConvertAllValues(MWContext *, char **, int32, uint);
extern void lo_CloseOutLayout(MWContext *, lo_DocState *);
extern void lo_CloseOutTable(MWContext *, lo_DocState *);
extern void lo_CloseParagraph(MWContext *context, lo_DocState **state, PA_Tag *tag, intn blank_lines);
extern void lo_CloseTable(MWContext *, lo_DocState *); /* from laytags.c */
extern void lo_ShiftMarginsUp(MWContext *, lo_DocState *, int32);
extern void lo_ClearToLeftMargin(MWContext *, lo_DocState *);
extern void lo_ClearToRightMargin(MWContext *, lo_DocState *);
extern void lo_ClearToBothMargins(MWContext *, lo_DocState *);
extern void lo_BeginForm(MWContext *, lo_DocState *, PA_Tag *);
extern void lo_ProcessInputTag(MWContext *, lo_DocState *, PA_Tag *);
extern void lo_FreeDocumentGridData(MWContext *, lo_SavedGridData *);
extern void lo_FreeDocumentEmbedListData(MWContext *, lo_SavedEmbedListData *);
extern void lo_FreeDocumentFormListData(MWContext *, lo_SavedFormListData *);
extern void lo_SaveFormElementStateInFormList(MWContext *context, lo_FormData *form_list, Bool discard_elements);
extern void lo_SaveFormElementState(MWContext *, lo_DocState *, Bool);
extern void lo_BeginTextareaTag(MWContext *, lo_DocState *, PA_Tag *);
extern void lo_EndTextareaTag(MWContext *, lo_DocState *, PA_Block );
extern void lo_BeginOptionTag(MWContext *, lo_DocState *, PA_Tag *);
extern void lo_EndOptionTag(MWContext *, lo_DocState *, PA_Block );
extern void lo_BeginSelectTag(MWContext *, lo_DocState *, PA_Tag *);
extern void lo_EndSelectTag(MWContext *, lo_DocState *);
extern void lo_ProcessKeygenTag(MWContext *, lo_DocState *, PA_Tag *);
extern void lo_BeginSubDoc(MWContext *, lo_DocState *, PA_Tag *);
extern void lo_EndSubDoc(MWContext *, lo_DocState *,lo_DocState *,LO_Element *);
extern void lo_BeginCell(MWContext *, lo_DocState *, PA_Tag *);
extern void lo_EndCell(MWContext *, lo_DocState *, LO_Element *);
extern void lo_RecreateGrid(MWContext *, lo_DocState *, lo_GridRec *);
extern void lo_BeginGrid(MWContext *, lo_DocState *, PA_Tag *);
extern void lo_EndGrid(MWContext *, lo_DocState *, lo_GridRec *);
extern void lo_BeginGridCell(MWContext *, lo_DocState *, PA_Tag *);
extern void lo_BeginSubgrid(MWContext *, lo_DocState *, PA_Tag *);
extern void lo_EndSubgrid(MWContext *, lo_DocState *, lo_GridRec *);
extern void lo_BeginMap(MWContext *, lo_DocState *, PA_Tag *);
extern void lo_EndMap(MWContext *, lo_DocState *, lo_MapRec *);
extern lo_MapRec *lo_FreeMap(lo_MapRec *);
extern void lo_BeginMapArea(MWContext *, lo_DocState *, PA_Tag *);
extern lo_MapRec *lo_FindNamedMap(MWContext *, lo_DocState *, char *);
extern LO_AnchorData *lo_MapXYToAnchorData(MWContext *, lo_DocState *,
				lo_MapRec *, int32, int32);
extern void lo_BeginLayerTag(MWContext *, lo_DocState *, PA_Tag *);
extern void lo_EndLayer(MWContext *, lo_DocState *, PRBool);
extern void lo_BeginMulticolumn(MWContext *context, lo_DocState *state, PA_Tag *tag, 
								LO_MulticolumnStruct *multicolEle);
extern void lo_EndMulticolumn(MWContext *context, lo_DocState *state, PA_Tag *tag,
			lo_MultiCol *multicol, Bool relayout);
extern void lo_AppendMultiColToLineList (MWContext *context, lo_DocState *state, 
										 LO_MulticolumnStruct *multicol);
extern void lo_AppendZeroWidthAndHeightLF(MWContext *context, lo_DocState *state);

extern void lo_BeginTable(MWContext *, lo_DocState *, PA_Tag *);
extern void lo_EndTable(MWContext *context, lo_DocState *state, lo_TableRec *table, Bool relayout);

extern void lo_BeginTableAttributes(MWContext *context,
			    lo_DocState *state,
			    char *align_attr,
			    char *border_attr,
				char *border_top_attr,
				char *border_bottom_attr,
				char *border_left_attr,
				char *border_right_attr,
				char *border_color_attr,
				char *border_style_attr,
			    char *vspace_attr,
			    char *hspace_attr,
			    char *bgcolor_attr,
			    char *background_attr,
			    char *width_attr,
			    char *height_attr,
			    char *cellpad_attr,
			    char *toppad_attr,
			    char *bottompad_attr,
			    char *leftpad_attr,
			    char *rightpad_attr,
			    char *cellspace_attr,
			    char *cols_attr);

extern void lo_BeginTableRow(MWContext *, lo_DocState *,lo_TableRec *,PA_Tag *);

void lo_BeginTableRowAttributes(MWContext *context,
                            lo_DocState *state,
                            lo_TableRec *table,
                            char *bgcolor_attr,
                            char *background_attr,     
                            char *valign_attr,
                            char *halign_attr);

extern void lo_EndTableRow(MWContext *, lo_DocState *, lo_TableRec *);
extern void lo_BeginTableCell(MWContext *, lo_DocState *, lo_TableRec *,
				PA_Tag *, Bool);
extern void lo_BeginTableCellAttributes(MWContext *context,
                            lo_DocState *state,
                            lo_TableRec *table,
                            char * colspan_attr,
                            char * rowspan_attr,
                            char * nowrap_attr,
                            char * bgcolor_attr,
                            char * background_attr,
                            lo_TileMode tile_mode,
                            char * valign_attr,
                            char * halign_attr,
                            char * width_attr,
                            char * height_attr,
                            Bool is_a_header,
			    Bool normal_borders);

extern void lo_EndTableCell(MWContext *, lo_DocState *, Bool relayout);
extern void lo_BeginTableCaption(MWContext *, lo_DocState *, lo_TableRec *,
				PA_Tag *);
extern void lo_EndTableCaption(MWContext *, lo_DocState *);
extern int32 lo_GetSubDocBaseline(LO_SubDocStruct *);
extern int32 lo_GetCellBaseline(LO_CellStruct *);
extern LO_CellStruct *lo_CaptureLines(MWContext *, lo_DocState *,
				int32, int32, int32, int32, int32);
extern LO_CellStruct *lo_SmallSquishSubDocToCell(MWContext *, lo_DocState *,
			LO_SubDocStruct *, int32 *ptr_dx, int32 *ptr_dy);
extern LO_CellStruct *lo_SquishSubDocToCell(MWContext *, lo_DocState *,
				LO_SubDocStruct *, Bool);
extern LO_Element *lo_JumpCellWall(MWContext *, lo_DocState *, LO_Element *);

extern LO_TextAttr *lo_GetElementTextAttr(LO_Element *element);
extern void lo_SetElementTextAttr(LO_Element *element, LO_TextAttr *attr);
extern void lo_GetElementBbox(LO_Element *element, XP_Rect *rect);
extern void lo_RefreshElement(LO_Element *element, CL_Layer *layer, 
                              Bool update_now);
extern LO_AnchorData *lo_GetElementAnchor(LO_Element *element);
extern void lo_GetElementFGColor(LO_Element *element, LO_Color *color);
extern void lo_SetElementFGColor(LO_Element *element, LO_Color *color);

extern void lo_ShiftElementList(LO_Element *, int32, int32);
extern void lo_RenumberCell(lo_DocState *, LO_CellStruct *);
extern void lo_ShiftCell(LO_CellStruct *, int32, int32);
extern int32 lo_CleanTextWhitespace(char *, int32);
extern void lo_ProcessIsIndexTag(MWContext *, lo_DocState *, PA_Tag *);
extern void lo_EndForm(MWContext *, lo_DocState *);
extern void lo_RecycleElements(MWContext *, lo_DocState *, LO_Element *);
extern void lo_relayout_recycle(MWContext *context, lo_DocState *state,
                LO_Element *elist);
extern LO_Element *lo_NewElement(MWContext *, lo_DocState *, intn,
				ED_Element *, intn edit_offset);
extern void lo_AppendToLineList(MWContext *, lo_DocState *, LO_Element *,int32);
extern void lo_SetLineArrayEntry(lo_DocState *, LO_Element *, int32);
extern LO_Element *lo_FirstElementOfLine(MWContext *, lo_DocState *, int32);
extern void lo_ClipLines(MWContext *, lo_DocState *, int32);
extern void lo_SetDefaultFontAttr(lo_DocState *, LO_TextAttr *, MWContext *);
extern Bool lo_EvalTrueOrFalse(char *str, Bool default_val);
extern intn lo_EvalAlignParam(char *str, Bool *floating);
extern intn lo_EvalVAlignParam(char *str);
extern void lo_EvalStyleSheetAlignment(StyleStruct *, intn *, Bool *floating);
void lo_SetStyleSheetLayerProperties(MWContext *context,
		                             lo_DocState *state,
			                         StyleStruct *style_struct,
				                     PA_Tag *tag);
char * lo_ParseStyleSheetURL(char *url_string);
extern intn lo_EvalCellAlignParam(char *str);
extern intn lo_EvalDivisionAlignParam(char *str);
extern PA_Block lo_ConvertToFELinebreaks(char *, int32, int32 *);
extern PA_Block lo_FEToNetLinebreaks(PA_Block);
extern void lo_CleanFormElementData(LO_FormElementData *);
extern Bool lo_SetNamedAnchor(lo_DocState *, PA_Block);
extern void lo_AddNameList(lo_DocState *, lo_DocState *);
extern void lo_CheckNameList(MWContext *, lo_DocState *, int32);
extern int32 lo_StripTextWhitespace(char *, int32);
extern int32 lo_StripTextNewlines(char *, int32);
extern int32 lo_ResolveInputType(char *);
extern Bool lo_IsValidTarget(char *);
extern Bool lo_IsAnchorInDoc(lo_DocState *, char *);

extern void lo_DisplayElement(MWContext *context, LO_Element *tptr,
			      int32 base_x, int32 base_y,
			      int32 x, int32 y,
			      uint32 width, uint32 height);
extern void lo_DisplaySubtext(MWContext *context, LO_TextStruct *text,
			      int32 start_pos, int32 end_pos, Bool need_bg,
			      CL_Layer *sel_layer);
extern void lo_DisplayText(MWContext *, LO_TextStruct *, Bool);
extern void lo_DisplayEmbed(MWContext *, LO_EmbedStruct *);
extern void lo_DisplayJavaApp(MWContext *, LO_JavaAppStruct *);
extern void lo_DisplayImageWithoutCompositor(MWContext *, LO_ImageStruct *);
extern void lo_DisplaySubImageWithoutCompositor(MWContext *, LO_ImageStruct *,
	int32, int32, uint32, uint32);
extern void lo_ClipImage(MWContext *, LO_ImageStruct *,
	int32, int32, uint32, uint32);
extern void lo_DisplaySubDoc(MWContext *, LO_SubDocStruct *);
extern void lo_DisplayCell(MWContext *, LO_CellStruct *);
extern void lo_DisplayCellContents(MWContext *context, LO_CellStruct *cell,
					   int32 base_x, int32 base_y,
					   int32 x, int32 y,
					   uint32 width, uint32 height);
extern void lo_DisplayEdge(MWContext *, LO_EdgeStruct *);
extern void lo_DisplayTable(MWContext *, LO_TableStruct *);
extern void lo_DisplayLineFeed(MWContext *,  LO_LinefeedStruct *, Bool);
extern void lo_DisplayHR(MWContext *, LO_HorizRuleStruct *);
extern void lo_DisplayBullet(MWContext *, LO_BullettStruct *);
extern void lo_DisplayFormElement(MWContext *, LO_FormElementStruct *);
extern void LO_HighlightSelection(MWContext *, Bool);

extern void lo_RefreshDocumentArea(MWContext *, lo_DocState *,
	int32, int32, uint32, uint32);

extern intn lo_GetCurrentGridCellStatus(lo_GridCellRec *);
extern char *lo_GetCurrentGridCellUrl(lo_GridCellRec *);
extern void lo_GetGridCellMargins(MWContext *, int32 *, int32 *);
extern LO_Element *lo_XYToGridEdge(MWContext *, lo_DocState *, int32, int32);
extern LO_Element *lo_XYToDocumentElement(MWContext *, lo_DocState *,
	int32, int32, Bool, Bool, Bool, int32 *, int32 *);
extern LO_Element *lo_XYToDocumentElement2(MWContext *, lo_DocState *,
	int32, int32, Bool, Bool, Bool, Bool, int32 *, int32 *);
extern LO_Element *lo_XYToCellElement(MWContext *, lo_DocState *,
	LO_CellStruct *, int32, int32, Bool, Bool, Bool);
extern LO_Element *lo_XYToNearestCellElement(MWContext *, lo_DocState *,
	LO_CellStruct *, int32, int32);
extern void lo_RegionToLines (MWContext *, lo_DocState *, int32 x, int32 y,
	uint32 width, uint32 height, Bool dontCrop,
	int32* topLine, int32* bottomLine);
extern int32 lo_PointToLine (MWContext *, lo_DocState *, int32 x, int32 y);
extern void lo_GetLineEnds(MWContext *, lo_DocState *,
	int32 line, LO_Element** retBegin, LO_Element** retEnd);
extern void lo_CalcAlignOffsets(lo_DocState *, LO_TextInfo *, intn,
	int32, int32, int16 *, int32 *, int32 *, int32 *);
extern intn LO_CalcPrintArea(MWContext *, int32 x, int32 y,
	uint32 width, uint32 height, int32* top, int32* bottom);
extern Bool lo_FindWord(MWContext *, lo_DocState *,
	LO_Position *where, LO_Selection *word);

/* Object */
extern void lo_FormatObject(MWContext*, lo_DocState*, PA_Tag*);
extern void lo_ObjectParam(MWContext*, lo_DocState*, PA_Tag*, uint32*, char***, char***);
extern void lo_ProcessObjectTag(MWContext*, lo_DocState*, PA_Tag*, Bool);
extern void lo_ProcessParamTag(MWContext*, lo_DocState*, PA_Tag*, Bool);
extern void lo_FormatEmbedObject(MWContext*, lo_DocState*, PA_Tag*, LO_EmbedStruct*, Bool,
								 uint32 count, char** names, char** values);
extern void lo_FormatJavaObject(MWContext *context, lo_DocState *state,
								PA_Tag *tag, LO_JavaAppStruct *object);
extern void lo_AppendParamList(uint32* count1, char*** names1, char*** values1,
				   			   uint32 count2, char** names2, char** values2);
extern void lo_DeleteObjectStack(lo_ObjectStack*);

extern void lo_BlockTag(MWContext *context, lo_DocState *state, 
	PA_Tag *tag);
extern void lo_ReflectFormElement(MWContext *context, 
	lo_DocState *doc_state, PA_Tag *tag, 
	LO_FormElementStruct *form_element);


extern void lo_ProcessParagraphElement(MWContext *context, lo_DocState **state,
				       LO_ParagraphStruct *paragraph,
				       XP_Bool in_relayout);

extern void lo_ProcessCenterElement(MWContext *context, lo_DocState **state,
				    LO_CenterStruct *center,
				    XP_Bool in_relayout);

extern void lo_ProcessMulticolumnElement(MWContext *context,
										 lo_DocState **state,
										 LO_MulticolumnStruct *multicolumn);

extern void lo_ProcessDescTitleElement(MWContext *context,
				       lo_DocState *state,
				       LO_DescTitleStruct *title,
				       XP_Bool in_relayout);
extern void lo_ProcessDescTextElement(MWContext *context,
				      lo_DocState *state,
				      LO_DescTextStruct *text,
				      XP_Bool in_relayout);
extern void lo_ProcessBlockQuoteElement(MWContext *context,
					lo_DocState *state,
					LO_BlockQuoteStruct *quote,
					XP_Bool in_relayout);

/* 
 * Functions that get called from layout and non-destructive relayout 
 *
 */
extern void lo_FillInImageGeometry( lo_DocState *state, LO_ImageStruct *image );
extern void lo_LayoutFloatImage( MWContext *context, lo_DocState *state, LO_ImageStruct *image, Bool updateFE );
extern void lo_LayoutInflowImage(MWContext *context, lo_DocState *state, LO_ImageStruct *image,
								Bool inRelayout, int32 *line_inc, int32 *baseline_inc);
extern void lo_UpdateStateAfterImageLayout( lo_DocState *state, LO_ImageStruct *image, int32 line_inc, int32 baseline_inc );

extern void lo_FillInJavaAppGeometry(lo_DocState *state, LO_JavaAppStruct *java_app, Bool relayout);
extern void lo_LayoutFloatJavaApp( MWContext *context, lo_DocState *state, LO_JavaAppStruct *java_app, Bool updateFE );
extern void lo_LayoutInflowJavaApp(MWContext *context, lo_DocState *state, LO_JavaAppStruct *java_app,
								   Bool inRelayout, int32 *line_inc, int32 *baseline_inc);
extern void lo_UpdateStateAfterJavaAppLayout( lo_DocState *state, LO_JavaAppStruct *java_app,
											  int32 line_inc, int32 baseline_inc );

extern void lo_FillInEmbedGeometry(lo_DocState *state, LO_EmbedStruct *embed, Bool relayout);
extern void lo_LayoutInflowEmbed(MWContext *context, lo_DocState *state, LO_EmbedStruct *embed,
								   Bool inRelayout, int32 *line_inc, int32 *baseline_inc);
extern void lo_LayoutFloatEmbed(MWContext *context, lo_DocState *state, LO_EmbedStruct *embed, Bool updateFE);
extern void lo_UpdateStateAfterEmbedLayout( lo_DocState *state, LO_EmbedStruct *embed,
											  int32 line_inc, int32 baseline_inc );

extern void lo_LayoutInflowFormElement(MWContext *context, lo_DocState *state, LO_FormElementStruct *form_element,
						   int32 *baseline_inc, Bool inRelayout);
extern void lo_UpdateStateAfterFormElement(MWContext *context, lo_DocState *state, LO_FormElementStruct *form_element,
										   int32 baseline_inc);

extern void lo_StartHorizontalRuleLayout(MWContext *, lo_DocState *, LO_HorizRuleStruct*);
extern void lo_FillInHorizontalRuleGeometry(MWContext *context, lo_DocState *state, LO_HorizRuleStruct *hrule);
extern void lo_FinishHorizontalRuleLayout(MWContext *, lo_DocState *, LO_HorizRuleStruct*);
extern void lo_UpdateStateAfterHorizontalRule(lo_DocState *state, LO_HorizRuleStruct *hrule);

extern void lo_FormatBullet(MWContext *context, lo_DocState *state,
			    LO_BulletStruct *bullet,
			    int32 *line_height,
			    int32 *baseline);
extern void lo_UpdateStateAfterBullet(MWContext *context, lo_DocState *state,
				      LO_BulletStruct *bullet,
				      int32 line_height,
				      int32 baseline);

extern void lo_FormatBulletStr(MWContext *context, lo_DocState *state,
			       LO_TextStruct *bullet_text,
			       int32 *line_height,
			       int32 *baseline);
extern void lo_UpdateStateAfterBulletStr(MWContext *context, lo_DocState *state,
					 LO_TextStruct *bullet_text,
					 int32 line_height,
					 int32 baseline);

extern void lo_SetupStateForList(MWContext *context,
				 lo_DocState *state,
				 LO_ListStruct *list,
				 XP_Bool in_resize_reflow);
extern void lo_UpdateStateAfterList(MWContext *context,
				    lo_DocState *state,
				    LO_ListStruct *list,
				    XP_Bool in_resize_reflow);

extern void lo_UpdateStateAfterLineBreak( MWContext *context, lo_DocState *state, Bool updateFE);
extern void lo_UpdateFEProgressBar( MWContext *context, lo_DocState *state );
extern void lo_UpdateFEDocSize( MWContext *context, lo_DocState *state );
extern void lo_FillInLineFeed( MWContext *context, lo_DocState *state, int32 break_type, uint32 clear_type, LO_LinefeedStruct *linefeed );

extern void lo_UpdateStateWhileFlushingLine( MWContext *context, lo_DocState *state );
extern void lo_AppendLineFeed( MWContext *context, lo_DocState *state, LO_LinefeedStruct *linefeed, int32 breaking, Bool updateFE );
extern void lo_UpdateStateAfterFlushingLine( MWContext *context, lo_DocState *state, LO_LinefeedStruct *linefeed, Bool inRelayout );
extern void lo_AppendFloatInLineList( lo_DocState *state, LO_Element *ele, LO_Element *restOfLine);

/*
 * Common code for layout and relayout of tables
 */
extern void lo_PositionTableElement(lo_DocState *state, LO_TableStruct *table_ele);
extern void lo_SetTableDimensions( lo_DocState *state, lo_TableRec *table, int32 allow_percent_width, int32 allow_percent_height);
extern void lo_CalcFixedColWidths( MWContext *context, lo_DocState *state, lo_TableRec *table);
extern void lo_UpdateStateAfterBeginTable( lo_DocState *state, lo_TableRec *table);
extern void lo_UpdateTableStateForBeginRow(lo_TableRec *table, lo_TableRow *table_row);
extern void lo_InitForBeginCell(lo_TableRow *table_row, lo_TableCell *table_cell);
extern void lo_InitSubDocForBeginCell( MWContext *context, lo_DocState *state, lo_TableRec *table);
extern void lo_InitTableRecord( lo_TableRec *table );
extern void lo_CreateCellFromSubDoc( MWContext *context, lo_DocState *state, LO_SubDocStruct *subdoc, LO_CellStruct *cell, int32 *ptr_dx, int32 *ptr_dy );
extern void lo_EndLayoutDuringReflow( MWContext *context, lo_DocState *state );
extern void lo_AppendLineListToLineArray( MWContext *context, lo_DocState *state, LO_Element *lastElementOnLineList);

/* Common code for layout and relayout of multicols */
extern void lo_StartMultiColInit( lo_DocState *state, lo_MultiCol *multicol );
extern void lo_SetupStateForBeginMulticol( lo_DocState *state, lo_MultiCol *multicol, int32 doc_width );

/* Common code for layout and relayout of spacers */
extern void lo_LayoutSpacerElement(MWContext *context, lo_DocState *state, LO_SpacerStruct *spacer, Bool relayout);

/* 
 * Adds a soft line break to the end of the line list and adds the line list to the line array.  
 * Resets line list to NULL.  Should be used during relayout only.
 */

extern void lo_rl_AddSoftBreakAndFlushLine( MWContext *context, lo_DocState *state );
extern void lo_rl_AddBreakAndFlushLine( MWContext *context, lo_DocState *state, int32 break_type, uint32 clear_type, Bool breaking);
extern void lo_rl_AppendLinefeedAndFlushLine( MWContext *context, lo_DocState *state, LO_LinefeedStruct *linefeed, int32 break_type, uint32 clear_type,
									  Bool breaking, Bool createNewLF);
extern void lo_FillInLineFeed( MWContext *context, lo_DocState *state, int32 break_type, uint32 clear_type, LO_LinefeedStruct *linefeed );

extern void lo_rl_ReflowCell( MWContext *context, lo_DocState *state, LO_CellStruct *cell);
extern void lo_rl_ReflowDocState( MWContext *context, lo_DocState *state );
extern void lo_PrepareElementForReuse( MWContext *context, lo_DocState *state, LO_Element * eptr,
		ED_Element *edit_element, intn edit_offset);
extern void LO_Reflow(MWContext *context, lo_DocState * state, LO_Element *startElement,
	LO_Element *endElement);

extern Bool lo_IsDummyLayoutElement(LO_Element *ele);

/* Useful Macros */

#define	COPY_COLOR(from_color, to_color)	\
	to_color.red = from_color.red;		\
	to_color.green = from_color.green;	\
	to_color.blue = from_color.blue


extern Bool lo_FindBestPositionOnLine( MWContext *pContext,
            lo_DocState* state, int32 iLine,
            int32 iDesiredX, int32 iDesiredY,
			Bool bForward, int32* pRetX, int32 *pRetY );

extern LO_Element * lo_GetLastElementInList( LO_Element *eleList );

#ifdef EDITOR

#ifdef DEBUG
/* lo_VerifyLayout does a consistency check on the layout structure.
 * returns TRUE if the layout structure is consistent. */
extern Bool lo_VerifyLayout(MWContext *pContext);

/* PrintLayout prints the layout structure. */
extern void lo_PrintLayout(MWContext *pContext);
extern Bool
lo_VerifyList( MWContext *pContext, lo_TopState* top_state,
              lo_DocState* state,
              LO_Element* start,
              LO_Element* end,
              LO_Element* floating,
              Bool print);

#endif /* DEBUG */

#endif /* EDITOR */

/*
 * Returns length in selectable parts of this element. 1 for images, length of text for text, etc.
 */
int32
lo_GetElementLength(LO_Element* eptr);

extern Bool lo_EditableElement(int iType);

/* Moves by one editable position forward or backward. Returns FALSE if it couldn't.
 * position can be denormalized on entry and may be denormalized on exit.
 */

Bool
lo_BumpEditablePosition(MWContext *context, lo_DocState *state,
    LO_Element **pEptr, int32 *pPosition, Bool direction);

#ifdef MOCHA

extern lo_TopState *
lo_GetMochaTopState(MWContext *context);

extern void
lo_BeginReflectForm(MWContext *context, lo_DocState *state, PA_Tag *tag,
                    lo_FormData *form);

extern void
lo_EndReflectForm(MWContext *context, lo_FormData *form);

extern void
lo_ReflectImage(MWContext *context, lo_DocState *doc_state, PA_Tag *tag,
                LO_ImageStruct *image_data, Bool blocked, int32 layer_id);

extern void
lo_ReflectNamedAnchor(MWContext *context, lo_DocState *state, PA_Tag *tag,
                      lo_NameList *name_rec, int32 layer_id);

extern void
lo_ReflectLink(MWContext *context, lo_DocState *state, PA_Tag *tag,
               LO_AnchorData *anchor_data, int32 layer_id, uint index);

extern XP_Bool
lo_ProcessContextEventHandlers(MWContext *context, lo_DocState *state,
                               PA_Tag *tag);

extern void
lo_RestoreContextEventHandlers(MWContext *context, lo_DocState *state,
                               PA_Tag *tag, SHIST_SavedData *saved_data);

extern void
lo_ProcessScriptTag(MWContext *context, lo_DocState *state, PA_Tag *tag,
					struct JSObject * /* scope to use. can be NULL */);


extern XP_Bool
lo_ProcessStyleAttribute(MWContext *context, lo_DocState *state, PA_Tag *tag, char *script_buff);

extern void
lo_ProcessStyleTag(MWContext *context, lo_DocState *state, PA_Tag *tag);

extern void
lo_BlockScriptTag(MWContext *context, lo_DocState *state, PA_Tag *tag);
#endif /* MOCHA */

extern lo_DocLists *
lo_GetCurrentDocLists(lo_DocState *state);

extern lo_DocLists *
lo_GetDocListsById(lo_DocState *state, int32 id);

Bool
lo_CloneTagAndBlockLayout(MWContext *context, lo_DocState *state, PA_Tag *tag);

void
lo_UnblockLayout(MWContext *context, lo_TopState *top_state);

void 
lo_UnblockLayoutAndFreeExtraElement(MWContext *context);

extern void
lo_SetBaseUrl(lo_TopState *top_state, char *url, Bool is_blocked);

extern void
lo_FlushedBlockedTags(lo_TopState *top_state);

extern int32 *
lo_parse_coord_list(char *str, int32 *value_cnt, Bool must_be_odd);

extern void
lo_add_leading_bullets(MWContext *context, lo_DocState *state,
                       int32 start,int32 end,int32 mquote_x);

extern void
lo_TeardownList(MWContext *context, lo_DocState *state, PA_Tag *tag);


extern void 
lo_SetDocumentDimension( MWContext* context, lo_DocState *state );

extern void
lo_ScriptEvalExitFn(void * data, char * str, size_t len, char * wysiwyg_url,
		    char * base_href, Bool valid);

extern Bool
lo_ConvertMochaEntities(MWContext * context, lo_DocState *state, PA_Tag * tag);

extern void
LO_EditorReflow(MWContext *context, ED_TagCursor *pCursor, 
			int32 iStartLine, int iStartEditOffset, XP_Bool bDisplayTables);

/********************** Image observers and observer lists. ******************/
/* The layout observer for an image request. */
extern void
lo_ImageObserver(XP_Observable observable, XP_ObservableMsg message,
                 void *message_data, void *closure);

/* Create a new observer list which will be passed into to IL_GetImage (via
   FE_GetImageInfo) in order to permit an Image Library request to be
   monitored. The layout image observer is added to this new observer list
   before the function returns. */
extern XP_ObserverList
lo_NewImageObserverList(MWContext *context, LO_ImageStruct *lo_image);


/* #ifndef NO_TAB_NAVIGATION */
extern lo_MapAreaRec *LO_getTabableMapArea(MWContext *context, LO_ImageStruct *image, int32 wantedIndex );
extern Bool LO_findApointInArea( lo_MapAreaRec *pArea, int32 *xx, int32 *yy );
/* NO_TAB_NAVIGATION  */

/****************************************************************************/
/* Converts ("snaps") input X, Y (doc coordinates) to X, Y needed for drop caret 
 *  and calls appropriate front-end FE_Display<Text|Generic|Image>Caret to use show where
 *  a drop would occur. It does NOT change current selection or internal caret position
 *  Returns the element that we will drop at and position 
 *   within this element for text data
*/
LO_Element * lo_PositionDropCaret(MWContext *pContext, int32 x, int32 y, int32 * pPosition);

void
lo_free_layout_state_data(MWContext *context, lo_DocState *state);

extern void
lo_use_default_doc_background(MWContext *context, lo_DocState *state);

Bool
lo_BindNamedAnchorToElement(lo_DocState *state, PA_Block name,
                           LO_Element *element);

/* Return the table element containing the given element (usually a LO_CELL type) */
LO_TableStruct *lo_GetParentTable(MWContext *pContext, LO_Element *pElement);

/* Return the cell element containing the given element */
LO_CellStruct *lo_GetParentCell(MWContext *pContext, LO_Element *pElement);

/* Find the first cell with with closest left border x-value less than the given x
 * value or, if bGetColumn=FALSE, find the cell with closest top border y-value less
 *   than the given Y value. Also returns the pointer to the last LO element in row/column so
 *   we can search for other cells until last is reached.
 * pElement is any element in the table
 * This is most efficient if pElement is LO_TABLE containg cursor,
 *   very efficient if pElement is LO_CELL,
 *   and least if any other type (must search through all nested tables to find enclosing cell)
*/
LO_Element* lo_GetFirstCellInColumnOrRow(MWContext *pContext, LO_Element *pElement, int32 x, int32 y, XP_Bool bGetColumn, LO_Element **ppLastCellInTable);

/* Returns first cell in table, and last cell if pLastCell != NULL 
 * pElement can be any LO_Element in the table (quickest search if it is a LO_TABLE or LO_CELL)
*/
LO_Element *lo_GetFirstAndLastCellsInTable(MWContext *pContext, LO_Element *pElement, LO_Element **ppLastCell);

/* Find first and last cell from any cell in the table 
 * pElement->type MUST be LO_CELL
*/
LO_Element *lo_GetFirstAndLastCellsInColumnOrRow(LO_Element *pElement, LO_Element **ppLastCell, XP_Bool bInColumn);

/* Check if all cells containing given cell are selected */
XP_Bool lo_AllCellsSelectedInColumnOrRow(LO_CellStruct *pCell, XP_Bool bInColumn );

int32 lo_GetNumberOfCellsInTable(LO_TableStruct *pTable );

/* Subtract borders and inter-cell spacing to get the available table width
 *   to use when cell width in HTML is "percent of table"
 * Element MUST be of type LO_CELL (LO_CellStruct not used to avoid unecessary casting)
*/
int32 lo_CalcTableWidthForPercentMode(LO_Element *pCellElement);

#endif /* _Layout_h_ */
