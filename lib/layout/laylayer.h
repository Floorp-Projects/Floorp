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


/*
 * Layer-specific data structures and declarations for HTML layout library.
 */
#ifndef _LAYLAYER_H
#define _LAYLAYER_H

#define LO_IGNORE_TAG_MARKER 0xFFFF

typedef struct lo_Block lo_Block; /* Forward declaration */

/*
 * This struct represents the layout side of a layer created with
 * a <(I)LAYER> tag. It holds onto the contents of the layer (in a cell)
 * along with lists of elements that are part of the layer's
 * "document".
 */
struct lo_LayerDocState 
{
    int32 id;
    LO_CellStruct *cell;        /* Contents of layer */
    int32 height;               /* Value of layer's HEIGHT attribute, or zero */

    PRPackedBool is_constructed_layer; /* Created with JS new operator ? */
    PRPackedBool overrideScrollWidth; /* Only used for _DOCUMENT layer */
    PRPackedBool overrideScrollHeight;
    int32 contentWidth;         /* Dimensions of content, not including
                                   child layers. */
    int32 contentHeight;
    
    XP_Rect viewRect;           /* Non-scrolling clip rectangle */
    
    CL_Layer *layer;
    void *mocha_object;
    lo_DocLists *doc_lists;      /* Layout element lists, e.g. embeds, forms */
    lo_Block *temp_block;       /* Data used only during HTML layer layout */
};

struct lo_LayerStack {
    lo_LayerDocState *layer_state;
    lo_LayerStack *next;
};

/* A layer closure is the piece of layout-specific data that gets
   attached as client-data to a CL_Layer.  They come in all different
   flavors, depending on the purpose of the layer. */

/* Cheap inheritance: Struct members common to all types of layer closures */
#define LO_LAYER_CLOSURE_STRUCT_MEMBERS                                       \
    LO_LayerType type;                                                        \
    MWContext *context

typedef struct lo_BlinkLayerClosure {
    LO_LAYER_CLOSURE_STRUCT_MEMBERS;
    LO_TextStruct *text;   /* The text element that's blinking */
} lo_BlinkLayerClosure;

typedef struct lo_ImageLayerClosure {
    LO_LAYER_CLOSURE_STRUCT_MEMBERS;
    LO_ImageStruct *image;   /* The layout image element */
} lo_ImageLayerClosure;

typedef struct lo_CellBGLayerClosure {
    LO_LAYER_CLOSURE_STRUCT_MEMBERS;
    LO_CellStruct *cell;    /* The cell that we're the background for */
} lo_CellBGLayerClosure;

typedef struct lo_HTMLBlockClosure {
    LO_LAYER_CLOSURE_STRUCT_MEMBERS;
    int32 x_offset;         /* Layout imposed offset for in-flow layers */
    int32 y_offset;
    lo_DocState *state;     /* The layout document state */
    lo_LayerDocState *layer_state; /* The layer doc state */
    PRBool is_inflow;
} lo_HTMLBlockClosure;

typedef struct lo_HTMLBodyClosure {
    LO_LAYER_CLOSURE_STRUCT_MEMBERS;
    CL_Layer *layer;
} lo_HTMLBodyClosure;

typedef struct lo_EmbeddedObjectClosure {
    LO_LAYER_CLOSURE_STRUCT_MEMBERS;
    LO_Element *element;	/* Plugin, Java applet, or form element */
    PRPackedBool is_windowed;
} lo_EmbeddedObjectClosure;

/* This struct belongs to the main container layer for a group of
   related layers, i.e. the layer that is reflected by the <LAYER> or
   <ILAYER> tags.  It has child layers that represent the layer's
   background, HTML contents, table cell backdrop layers, etc. */
typedef struct lo_GroupLayerClosure {
    LO_LAYER_CLOSURE_STRUCT_MEMBERS;
    int32 x_offset;              /* Layout imposed offset for in-flow layers */
    int32 y_offset;
    CL_Layer *content_layer;     /* The child content layer    */
    CL_Layer *background_layer;  /* The child background layer */
	int32 wrap_width;            /* Pixel position at which text wraps   */
    int clip_expansion_policy;
    PRBool is_inflow;
    lo_LayerDocState *layer_state; /* The layer doc state */
} lo_GroupLayerClosure;

/* A background layer encompasses either the entire canvas (document),
   a single layer, or a single background cell. */
typedef enum {
    BG_DOCUMENT,
    BG_LAYER,
    BG_CELL
} lo_BackgroundType;

typedef struct lo_BackgroundLayerClosure {
    LO_LAYER_CLOSURE_STRUCT_MEMBERS;
    LO_Color *bg_color;
    LO_ImageStruct *backdrop_image;
    lo_TileMode tile_mode;
    lo_BackgroundType bg_type;
	LO_CellStruct *cell;
} lo_BackgroundLayerClosure;

typedef struct lo_AnyLayerClosure {
    LO_LAYER_CLOSURE_STRUCT_MEMBERS;
} lo_AnyLayerClosure;

typedef union lo_LayerClosure {
    lo_AnyLayerClosure any_closure;
    lo_BlinkLayerClosure blink_closure;
    lo_ImageLayerClosure image_closure;
    lo_CellBGLayerClosure cellbg_closure;
    lo_HTMLBlockClosure block_closure;
    lo_HTMLBodyClosure body_closure;
    lo_GroupLayerClosure group_closure;
    lo_BackgroundLayerClosure background_closure;
    lo_EmbeddedObjectClosure object_closure; /* plugin, java, form */
} lo_LayerClosure;


extern void
lo_CreateBlinkLayer (MWContext *context, LO_TextStruct *text, 
                     CL_Layer *parent);
extern void
lo_DestroyBlinkers(MWContext *context);
extern void
lo_UpdateBlinkLayers(MWContext *context);

extern CL_Layer *
lo_CreateBlockLayer(MWContext *context, 
                    char *name,
                    PRBool is_inflow,
                    int32 x_offset, int32 y_offset,
                    int32 wrap_width,
                    lo_LayerDocState *layer_state, 
                    lo_DocState *state);
extern void
lo_CreateDefaultLayers(MWContext *context,
                       CL_Layer **doc_layer, CL_Layer **body_layer);

extern void
lo_AttachHTMLLayer(MWContext *context, CL_Layer *layer, CL_Layer *parent,
                   char *above, char *below, int32 z_order);

extern PRBool lo_InsideLayer(lo_DocState *state);
extern PRBool lo_InsideInflowLayer(lo_DocState *state);

extern lo_LayerDocState * lo_NewLayerState(MWContext *context);
extern void lo_DeleteLayerState(MWContext *context, lo_DocState *state,
                                lo_LayerDocState *layer_state);
extern void lo_AddLineListToLayer(MWContext *, lo_DocState *, LO_Element *);

extern CL_Layer *
lo_CreateCellBackgroundLayer(MWContext *context, LO_CellStruct *cell,
                             CL_Layer *parent_layer, int16 table_nesting_level);
extern CL_Layer *
lo_CreateEmbedLayer(MWContext *context, CL_Layer *parent,
                    int32 x, int32 y, int32 width, int32 height,
                    LO_EmbedStruct *embed, lo_DocState *state);
extern CL_Layer *
lo_CreateEmbeddedObjectLayer(MWContext *context, lo_DocState *state,
			     LO_Element *tptr);
extern CL_Layer *
lo_CreateImageLayer (MWContext *context, LO_ImageStruct *text, 
                     CL_Layer *parent);
extern void lo_ActivateImageLayer(MWContext *context, LO_ImageStruct *image);

extern void
lo_PushLayerState(lo_TopState *top_state, lo_LayerDocState *layer_state);
extern lo_LayerDocState *
lo_PopLayerState(lo_DocState *state);
extern lo_LayerDocState *
lo_CurrentLayerState(lo_DocState *state);
extern void
lo_DeleteLayerStack(lo_DocState *state);

extern int32
lo_CurrentLayerId(lo_DocState *state);
extern CL_Layer *
lo_CurrentLayer(lo_DocState *state);
extern PRBool
lo_IsCurrentLayerDynamic(lo_DocState *state);
extern PRBool
lo_IsTagInSourcedLayer(lo_DocState *state, PA_Tag *tag);
extern PRBool
lo_IsAnyCurrentAncestorSourced(lo_DocState *state);
extern PRBool
lo_IsAnyCurrentAncestorDynamic(lo_DocState *state);

extern lo_LayerDocState *
lo_GetLayerState(CL_Layer *layer);

extern int32
lo_GetEnclosingLayerHeight(lo_DocState *state);

extern int32
lo_GetEnclosingLayerWidth(lo_DocState *state);

LO_CellStruct *
lo_GetCellFromLayer(MWContext *context, CL_Layer *layer);

extern void
lo_OffsetInflowLayer(CL_Layer *layer, int32 dx, int32 dy);

extern void
lo_BlockLayerTag(MWContext *context, lo_DocState *state, PA_Tag *tag);
extern void
lo_UnblockLayerTag(lo_DocState *state);
extern void
lo_FinishLayerLayout(MWContext *context, lo_DocState *state, int mocha_event);

extern void
lo_GetLayerXYShift(CL_Layer *layer, int32 *xp, int32 *yp);

extern void
lo_DestroyLayers(MWContext *context);

extern lo_LayerDocState *
lo_append_to_layer_array(MWContext *context, lo_TopState *top_state,
                         lo_DocState *state, 
                         lo_LayerDocState *layer_state);

extern void
lo_SetLayerBackdropTileMode(CL_Layer *layer, lo_TileMode tile_mode);
extern void lo_EndLayerTag(MWContext *, lo_DocState *, PA_Tag *tag);

#endif /* _LAYLAYER_H */
