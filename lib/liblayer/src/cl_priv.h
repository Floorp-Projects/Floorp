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
 *  cl_priv.h Private prototypes, definitions, etc.
 */


#ifndef _CL_PRIV_H_
#define _CL_PRIV_H_

/***************** Configuration *******************/
#define CL_GROUP_HASH_TABLE_SIZE 10

#define CL_THREAD_SAFE 1

/* Adjust frame-rate to CPU capacity */
#define CL_ADAPT_FRAME_RATE

/* Auto-adaptive frame-rate parameters, in frames per second */
#define CL_FRAME_RATE_MAX       60
#define CL_FRAME_RATE_MIN        8
#define CL_FRAME_RATE_INITIAL   15

#define CL_CPU_LOAD           0.90
#define CL_MIN_TIMEOUT           1 /* No matter what, leave at least this many
                                      milliseconds between the end of
                                      one compositor run and the start
                                      of the next */
#define LOG2_DELAY_LINE_LENGTH   5 /* 2^5 == 32, Adaptation filter maximum length */
#define DELAY_LINE_LENGTH       (1 << LOG2_DELAY_LINE_LENGTH)
#define SLOW_FILTER_DURATION   300 /* Time, in milliseconds, over which filter runs */

#include "fe_rgn.h"
#include "xp_rect.h"
#include "plhash.h"
#include "layers.h"
#ifdef CL_THREAD_SAFE
#    include "prmon.h"
#endif


/******************Definitions and Types************/

#ifdef CL_THREAD_SAFE
#    define LOCK_COMPOSITOR(compositor) PR_EnterMonitor(compositor->monitor)
#else
#    define LOCK_COMPOSITOR(compositor)
#endif

#ifdef CL_THREAD_SAFE
#    define UNLOCK_COMPOSITOR(compositor) PR_ExitMonitor(compositor->monitor)
#else
#    define UNLOCK_COMPOSITOR(compositor)
#endif

/* XXX - bit of a race condition ? */
#define LOCK_LAYERS(layer)                                                     \
   do {if (layer->compositor) LOCK_COMPOSITOR(layer->compositor);} while(0)

#define UNLOCK_LAYERS(layer)                                                   \
   do {if (layer->compositor) UNLOCK_COMPOSITOR(layer->compositor);} while (0)


#define CL_CONTAINMENT_LIST_SIZE 20
#define CL_CONTAINMENT_LIST_INCREMENT 5
#define CL_NEXT_CONTAINMENT_LIST(i) (((i) + 1) % 2)

extern int cl_trace_level;

#ifdef DEBUG
#    define CL_TRACE(l,t) {if(cl_trace_level > l) {XP_TRACE(t);}}
#else
#    define CL_TRACE(l,t) {}
#endif

typedef enum 
{
    CL_REACHED_NOTHING,
    CL_REACHED_BOTTOM_UNDRAWN,
    CL_REACHED_TOP_UNDRAWN
} CL_BackToFrontStatus;


/* 
 * The CL_Layer "class". It's opaque as far as everyone else
 * is concerned.
 */
struct CL_Layer {
    char *name;               /* For reference via HTML/JavaScript */
    XP_Rect bbox;             /* Bounds of clipping rectangle,
				 in layer's coordinates */
    XP_Rect clipped_bbox;     /* Position and dimensions of visible
                                 portion of layer, in document coordinates */
    int32 x_origin;           /* Origin of layer, in doc coordinates */
    int32 y_origin;
    int32 x_offset;           /* Origin of layer, in parent layer coordinates */
    int32 y_offset;
    int32 z_index;	      /* Relative z-ordering among layers */
    CL_LayerVTable vtable;    /* Layer-specific virtual "methods" */
    PRPackedBool visible;     /* Is layer considered visible for compositing ?*/
    PRPackedBool descendant_visible;
    PRPackedBool hidden;      /* Turn off drawing for this layer & children */
    PRPackedBool clip_children; /* Clip drawing of descendent layers */
    PRPackedBool clip_self;   /* Clip drawing of this layer ? */
    PRPackedBool prefer_draw_offscreen; /* Prefer draw directly to a pixmap ? */
    PRPackedBool prefer_draw_onscreen; /* Prefer not to use backing store */
    PRPackedBool scrolling;   /* Should this layer be scrolled with the page */
    PRPackedBool opaque;      /* Does this layer have no transparent areas ? */
    PRPackedBool enumerated;
    PRPackedBool cutout;
    PRPackedBool override_inherit_visibility; /* Child visibility is independent 
                                                 of parent's */
    CL_Color *uniform_color;  /* If non-null, layer is solid, uniform color */
    void *client_data;        /* Custom data field */
    CL_Compositor *compositor;/* Back-pointer from a layer to its compositor */
    CL_Layer *parent;         /* Parent of this layer */
    CL_Layer *sib_above;      /* Sibling layer above this one */
    CL_Layer *sib_below;      /* Sibling layer below this one */
    CL_Layer *top_child;      /* Uppermost child layer */
    CL_Layer *bottom_child;   /* Lowermost child layer  */
    void *mocha_object;       /* FIXME - temporary */

    /* Temporary flags used during drawing. Only valid if layer is visible. */
    PRPackedBool offscreen_layer_drawn_above;
    PRPackedBool draw_needed; 
    PRPackedBool descendant_draw_needed;

    /* The following variables represent the state of this layer during
       the previous composite cycle. */
    PRPackedBool prev_visible;
    XP_Rect prev_clipped_bbox;
    int32 prev_x_origin;
    int32 prev_y_origin;

    XP_Rect win_clipped_bbox; /* Temporary rectangle used during drawing,
                                 contains clipped_bbox in window coordinates,
                                 clipped by window rectangle. */
    FE_Region draw_region;  /* Temporary region used during drawing.
                               Holds the region of the layer to be
                               drawn as calculated during the front-to-back
                               pass. */
    CL_Layer *uniformly_colored_layer_below; /* Only set for opaque,
                                                solid-colored layers */
};

/*
 * These lists are used for mouse enter/leave event handling. Each list
 * holds a path starting from a layer up to the root of the layer tree.
 * This list is held for the last mouse event grabber. If the mouse moves
 * and there's a new mouse event grabber, we create such a list for the
 * new grabber. The determination of which layers get mouse enter and leave
 * events is done by tracing down the two lists from the root down. A
 * better explanation of this can be found in the source.
 */
typedef struct CL_EventContainmentList {
    CL_Layer **layer_list;
    int32 list_size;
    int32 list_head;
} CL_EventContainmentList;

/* 
 * The CL_Compositor "class". It's opaque as far as everyone else
 * is concerned.
 */
struct CL_Compositor {
    uint32 gen_id;                   /* Generation id of the compositor */
    CL_Drawable *primary_drawable;   /* Destination window/offscreen-pixmap */
    CL_Drawable *backing_store;      /* Offscreen pixmap */
    void *composite_timeout;         /* Timer used to schedule compositor  */	
    int32 x_offset, y_offset;        /* Window origin in document coordinates */
    XP_Rect window_size;             /* The rectangle we're compositing  */
    FE_Region window_region;         /* The corresponding region         */
    FE_Region update_region;         /* Region to be drawn at next composite */
    FE_Region backing_store_region;  /* Valid area in backing store */
    FE_Region cutout_region;	     /* Temporary region used during
					drawing.  Holds the region of
					the window which is considered
					to be out of bounds for the
					compositor, e.g.  a native
					window object, such as an
					applet */
    CL_Layer *root;                  /* The root of the layer tree */
    PRPackedBool recompute_update_region; /* Has any descendant layer
                                             had its visibility, bbox,
                                             or position modified ? */
    PRPackedBool enabled;            /* Should the compositor draw */
    PRPackedBool back_to_front_only; /* No front-to-back compositing */
    PRPackedBool offscreen_enabled;  /* If PR_FALSE, no offscreen compositing */
    PRPackedBool offscreen_inhibited;/* If PR_TRUE, no offscreen compositing */
    PRPackedBool offscreen_initialized;/* PR_TRUE, if cl_InitDrawable() called */
    
    /* Adaptive frame-rate state variables */
    float frame_period;              /* in milliseconds */
    int32 delay_line[DELAY_LINE_LENGTH]; /* Samples of lateness at intervals */
    unsigned int delay_line_index;   /* Index into delay_line */
    int64 nominal_deadline64;        /* Next time compositor should draw */

    PRHashTable *group_table;        /* Hash table of groups */
    CL_Layer *mouse_event_grabber;   /* This layer gets all mouse events */
    CL_Layer *key_event_grabber;     /* This layer gets all key events */
    int32 last_mouse_x;              /* The last position of the mouse */
    int32 last_mouse_y;
    int32 last_mouse_button;         /* The last mouse button pressed */
    CL_Layer *last_mouse_event_grabber; /* The last layer to grab a mouse event */
    CL_EventContainmentList event_containment_lists[2];  
                                    /* Lists used to trace event containment */
    int32 last_containment_list;    /* Which list was used last */

    CL_KeyboardFocusPolicy focus_policy; /* Policy for default keyboard focus */
    PRBool composite_in_progress;
    CL_Layer *uniformly_colored_layer_stack; /* Topmost uniformly-colored layer */
#ifdef CL_THREAD_SAFE
    PRMonitor *monitor;              /* Ensures thread-safety */
#endif
	uint32 nothing_to_do_count;      /* how many times we had nothing to do */
};

/* This represents the target of compositor drawing (either a window
   or an off-screen pixmap).  If the drawable is a window, it may have
   a backing store. */
struct CL_Drawable
{
    CL_Compositor *compositor;
    CL_DrawableVTable vtable;   /* Drawable-specific virtual "methods" */
    void *client_data;          /* Handle to platform-private data */
    FE_Region clip_region;      /* All drawing is clipped by this region,
                                   specified in window coordinates */
    uint32 flags;               /* CL_OFFSCREEN, CL_BACKING_STORE, etc. */
    int32 x_offset;             /* Coordinate system origin in document coord */
    int32 y_offset;
    uint16 width;               /* Width and height, in pixels */
    uint16 height;
};

extern void cl_LayerAdded(CL_Compositor *compositor, CL_Layer *layer);
extern void cl_LayerRemoved(CL_Compositor *compositor, CL_Layer *layer);
extern void cl_LayerMoved(CL_Compositor *compositor, CL_Layer *layer);
extern void cl_LayerDestroyed(CL_Compositor *compositor, CL_Layer *layer);
extern void cl_ParentChanged(CL_Layer *layer);
extern void cl_UpdateLayer(CL_Layer *layer, PRBool composite_now);
extern void cl_SetCompositorRecursive(CL_Layer *layer, 
                                      CL_Compositor *compositor);

extern void cl_InitDrawable(CL_Drawable *drawable);
extern void cl_RelinquishDrawable(CL_Drawable *drawable);
extern void cl_SetDrawableOrigin(CL_Drawable *drawable,
                                 int32 x_offset, int32 y_offset);
extern void cl_GetDrawableOrigin(CL_Drawable *drawable,
                                 int32* x_offset, int32* y_offset);
extern void cl_SetDrawableClip(CL_Drawable *drawable, FE_Region clip_region);
extern void cl_RestoreDrawableClip(CL_Drawable *drawable);
extern CL_Drawable *cl_LockDrawableForReadWrite(CL_Drawable *drawable);
extern CL_Drawable *cl_LockDrawableForWrite(CL_Drawable *drawable);
extern CL_Drawable *cl_LockDrawableForRead(CL_Drawable *drawable);
extern void cl_UnlockDrawable(CL_Drawable *drawable);
extern void cl_CopyPixels(CL_Drawable *src, CL_Drawable *dest, FE_Region region);
extern void cl_SetDrawableDimensions(CL_Drawable *drawable,
                                     uint32 width, uint32 height);

extern void cl_XorRegion(FE_Region src1, FE_Region src2, FE_Region dst);
extern void cl_sanitize_bbox(XP_Rect *bbox);
extern void cl_start_compositor_timeouts(CL_Compositor *compositor);


#endif /* _CL_PRIV_H_ */

