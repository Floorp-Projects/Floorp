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
 *  layers.h - Compositing and layer related definitions and prototypes
 */


#ifndef _LAYERS_H_
#define _LAYERS_H_

#include "prtypes.h"
#include "fe_rgn.h"
#include "xp_rect.h"
#include "cl_types.h"


/******************Definitions and Types************/


/* Maximum and minimum permitted values for coordinates for layer 
   positions and bbox's.  
   Note: We don't use the extrema values of 32-bit integers.
   Instead, we deliberately leave headroom to avoid overflow of the
   coordinates as a result of translating the rect.  */
#define CL_MAX_COORD    ( 0x3fffffffL)
#define CL_MIN_COORD    (-0x40000000L)
#define CL_MAX_RECT {CL_MIN_COORD, CL_MIN_COORD, CL_MAX_COORD, CL_MAX_COORD}


typedef struct CL_Color 
{
    uint8 red, green, blue;
} CL_Color;
    
typedef enum CL_LayerPosition { 
    CL_ABOVE,
    CL_BELOW
} CL_LayerPosition;


/* A function that repaints a given region in a specified drawable.   */
/* The region returned defines the area actually drawn. If this       */
/* area is a complex region (it cannot be defined by the FE_Region    */
/* primitive) and allow_transparency is FALSE (we are in the front-   */
/* to-back phase of the compositor loop), this should return NULL.    */
/* In this case, the painter function will be called again during     */
/* the back-to-front phase.                                           */
/* The update_region is specified in window coordinates. The layer    */
/* is responsible for translating this into its own coordinate system */
/* The region passed back must also be in the window coordinate sys.  */
typedef void (*CL_PainterFunc)(CL_Drawable *drawable, 
                               CL_Layer *layer, 
                               FE_Region update_region);

/* 
 * The painter_func returns a region that represents the area drawn.
 * This function is called after the compositor uses the region.
 */
typedef void (*CL_RegionCleanupFunc)(CL_Layer *layer,
                                     FE_Region drawn_region);

/* A function that handles events that relate directly to the layer */
/* A return value of TRUE indicates that the layer has accepted and */
/* dealt with the event. FALSE indicates that the event should be   */
/* passed down to the next layer.                                   */
typedef PRBool (*CL_EventHandlerFunc)(CL_Layer *layer,
                                      CL_Event *event);

/* Function that's called when the layer is destroyed */
typedef void (*CL_DestroyFunc)(CL_Layer *layer);

/* Function that's called to retrieve user-defined data from layer */
typedef void *(*CL_GetClientDataFunc)(CL_Layer *layer);

/* Function that's called to set user-defined data for a layer */
typedef void (*CL_SetClientDataFunc)(CL_Layer *layer, void *client_data);

/* Function that's called when a layer is moved */
typedef void (*CL_PositionChangedFunc)(CL_Layer *layer, 
                                       int32 x_offset, int32 y_offset);

/* Function that's called when a layer's hidden state changes */
typedef void (*CL_VisibilityChangedFunc)(CL_Layer *layer, PRBool visible);

/* Function that's called when a layer's bounding box changes */
typedef void (*CL_BboxChangedFunc)(CL_Layer *layer, XP_Rect *new_bbox);

/* 
 * A C++-like vtable. These are virtual methods that CL_Layer
 * sub-classes should override. I never thought I'd say this,
 * but...I'd rather be using C++.
 */
typedef struct CL_LayerVTable 
{
    CL_PainterFunc painter_func; 
    CL_RegionCleanupFunc region_cleanup_func; 
    CL_EventHandlerFunc event_handler_func; 
    CL_DestroyFunc destroy_func;
    CL_GetClientDataFunc get_client_data_func;
    CL_SetClientDataFunc set_client_data_func;
    CL_PositionChangedFunc position_changed_func;
    CL_VisibilityChangedFunc visibility_changed_func;
    CL_BboxChangedFunc bbox_changed_func;
} CL_LayerVTable;

/* Function called by CL_ForEachLayerInGroup() */
typedef PRBool (*CL_LayerInGroupFunc)(CL_Layer *layer, void *closure);

/* Function called by CL_ForEachChildOfLayer() */
typedef PRBool (*CL_ChildOfLayerFunc)(CL_Layer *layer, void *closure);

typedef enum 
{
    CL_EVENT_MOUSE_BUTTON_DOWN,
    CL_EVENT_MOUSE_BUTTON_UP,
    CL_EVENT_MOUSE_BUTTON_MULTI_CLICK,
    CL_EVENT_MOUSE_MOVE,
    CL_EVENT_KEY_DOWN,
    CL_EVENT_KEY_UP,
    CL_EVENT_MOUSE_ENTER,
    CL_EVENT_MOUSE_LEAVE,
    CL_EVENT_KEY_FOCUS_GAINED,
    CL_EVENT_KEY_FOCUS_LOST
} CL_EventType;
   
typedef enum
{
    CL_FOCUS_POLICY_CLICK, /* Layer gets keyboard focus when you click in it */
    CL_FOCUS_POLICY_MOUSE_ENTER /* ...when you the mouse enters its bounds */
} CL_KeyboardFocusPolicy;

/* 
 * A generic event struct. I took the Mac toolbox model of having
 * a single event structure encode all types of events. If we had
 * more events (and this could happen in the near future), I'd go
 * with the X model of having event-specific structures and a 
 * union of them that's passed around.
 */
struct CL_Event {
    CL_EventType type;   /* What type of event is this */
    void *fe_event;      /* FE-specific event information */
    int32 fe_event_size; /* Size of FE-specific event info */
    int32 x, y;          /* The current position of the pointer */
    uint32 which;        /* For mouse events: which button is down
                            0 = none, 1 onwards starts from the left.
                            For key events: code for the key pressed */
    uint32 modifiers;    /* Modifiers currently in place e.g. shift key */
    uint32 data;         /* Other event specific information.
                            For multi-click events: the number of clicks */
};

#define CL_IS_MOUSE_EVENT(evnt)                                \
       (((evnt)->type == CL_EVENT_MOUSE_BUTTON_DOWN) ||        \
        ((evnt)->type == CL_EVENT_MOUSE_BUTTON_MULTI_CLICK) || \
        ((evnt)->type == CL_EVENT_MOUSE_BUTTON_UP) ||          \
        ((evnt)->type == CL_EVENT_MOUSE_MOVE))

#define CL_IS_KEY_EVENT(evnt) \
       (((evnt)->type == CL_EVENT_KEY_DOWN) ||  \
        ((evnt)->type == CL_EVENT_KEY_UP))

#define CL_IS_FOCUS_EVENT(evnt) \
       (((evnt)->type == CL_EVENT_MOUSE_ENTER) ||  \
        ((evnt)->type == CL_EVENT_MOUSE_LEAVE) ||  \
        ((evnt)->type == CL_EVENT_KEY_FOCUS_GAINED) || \
        ((evnt)->type == CL_EVENT_KEY_FOCUS_LOST))

typedef enum CL_DrawableState {
    CL_UNLOCK_DRAWABLE                   = 0x00,
    CL_LOCK_DRAWABLE_FOR_READ            = 0x01,
    CL_LOCK_DRAWABLE_FOR_WRITE           = 0x02,
    CL_LOCK_DRAWABLE_FOR_READ_WRITE      = 0x03
} CL_DrawableState;

/* Function that's called to insure that a drawable used as a backing
   store can still be accessed and check that it has not been altered
   since the last time the drawable was unlocked.  This functions
   permits a compositor client to purge its backing store when unlocked. */
typedef PRBool (*CL_LockDrawableFunc)(CL_Drawable *drawable,
                                      CL_DrawableState state);
/* Function that's called when the drawable is destroyed */
typedef void (*CL_DestroyDrawableFunc)(CL_Drawable *drawable);

/* Function that's called to indicate that the drawable will be used.
 * No other drawable calls will be made until we InitDrawable. */
typedef void (*CL_InitDrawableFunc)(CL_Drawable *drawable);

/* Function that's called to indicate that we're temporarily done with
 * the drawable. The drawable won't be used until we call InitDrawable
 * again. */
typedef void (*CL_RelinquishDrawableFunc)(CL_Drawable *drawable);

/* Function that's called to set coordinate origin of a drawable */
typedef void (*CL_SetDrawableOriginFunc)(CL_Drawable *drawable,
                                         int32 x_offset, int32 y_offset);

typedef void (*CL_GetDrawableOriginFunc)(CL_Drawable *drawable,
                                         int32 *x_offset, int32 *y_offset);

/* Function that's called to set drawing clip region for a drawable */
typedef void (*CL_SetDrawableClipFunc)(CL_Drawable *drawable,
                                       FE_Region clip_region);

/* Function that's called to to restore drawing clip region to prior value */
typedef void (*CL_RestoreDrawableClipFunc)(CL_Drawable *drawable);

/* BLIT from one drawable to another */
typedef void (*CL_CopyPixelsFunc)(CL_Drawable *drawable_src, 
                                  CL_Drawable *drawable_dest,
                                  FE_Region region);

/* Function that's called to set drawing clip region for a drawable */
typedef void (*CL_SetDrawableDimensionsFunc)(CL_Drawable *drawable,
                                             uint32 width, uint32 height);

/* 
 * A C++-like vtable. These are virtual methods that CL_Layer
 * sub-classes should override.
 */
typedef struct CL_DrawableVTable
{
    CL_LockDrawableFunc           lock_func;
    CL_InitDrawableFunc           init_func;
    CL_RelinquishDrawableFunc     relinquish_func;
    CL_DestroyDrawableFunc        destroy_func; 
    CL_SetDrawableOriginFunc      set_origin_func;
    CL_GetDrawableOriginFunc      get_origin_func;
    CL_SetDrawableClipFunc        set_clip_func;
    CL_RestoreDrawableClipFunc    restore_clip_func;
    CL_CopyPixelsFunc             copy_pixels_func;
    CL_SetDrawableDimensionsFunc  set_dimensions_func;
} CL_DrawableVTable;

/* Bitmask for CL_CompositorTarget flags */
#define CL_WINDOW           (0x00)
#define CL_OFFSCREEN        (0x01)
#define CL_BACKING_STORE    (0x02)
#define CL_PRINTER          (0x03)    

typedef enum
{
    CL_DRAWING_METHOD_HYBRID,  /* Combination of f-t-b and b-t-f */
    CL_DRAWING_METHOD_BACK_TO_FRONT_ONLY /* Back-to-front drawing only */
} CL_DrawingMethod;

/*******************Prototypes**********************/

XP_BEGIN_PROTOS

/***************** LAYER METHODS *****************/

/* Values for flags argument to CL_NewLayer(), CL_SetFlags(), etc. */
#define CL_NO_FLAGS                    0x0000
#define CL_HIDDEN                      0x0001 /* Don't draw layer & children */
#define CL_PREFER_DRAW_ONSCREEN        0x0002 /* Prefer not to use backing store */
#define CL_PREFER_DRAW_OFFSCREEN       0x0004 /* Draw directly to pixmap ? */
#define CL_DONT_SCROLL_WITH_DOCUMENT   0x0008 /* Should this layer not scroll ? */
#define CL_CLIP_CHILD_LAYERS           0x0010 /* Clip drawing of descendents */
#define CL_DONT_CLIP_SELF              0x0020 /* Clip drawing of this layer ? */
#define CL_DONT_ENUMERATE              0x0040 /* Don't reflect into JavaScript*/
#define CL_OPAQUE                      0x0080 /* Layer has no transparency */
#define CL_CUTOUT                      0x0100 /* A layer drawn by somebody else */
#define CL_OVERRIDE_INHERIT_VISIBILITY 0x0200 /* Child visibility is independent
                                                 of parent's */

/* Allocate and free a layer */
extern CL_Layer *CL_NewLayer(char *name, int32 x_offset, int32 y_offset,
                             XP_Rect *bbox, 
                             CL_LayerVTable *vtable,
                             uint32 flags, void *client_data);

extern uint32 CL_GetLayerFlags(CL_Layer *layer);
extern PRBool CL_ChangeLayerFlag(CL_Layer *layer, uint32 flag, PRBool value);

extern void CL_DestroyLayer(CL_Layer *layer);

/* Frees an entire sub-tree, rooted at the specified node */
extern void CL_DestroyLayerTree(CL_Layer *root);

/* Insert a layer into the layer tree as a child of the parent layer.
 * If sibling is NULL, the layer is added as the topmost (in z-order) child 
 * if where==CL_ABOVE or the bottommost child if where==CL_BELOW.
 * If sibling is non-NULL, the layer is added above or below (in z-order)
 * sibling based on the value of where.
 */ 
extern void CL_InsertChild(CL_Layer *parent, CL_Layer *child, 
                           CL_Layer *sibling, CL_LayerPosition where);

/*
 * Alternate function for inserting a layer based on its relative z-order.
 */
extern void CL_InsertChildByZ(CL_Layer *parent, CL_Layer *child, int32 z_order);

/* Removes a layer from a parent's sub-tree */
extern void CL_RemoveChild(CL_Layer *parent, CL_Layer *child);

/* Call the function for each child layer of the layer */
extern PRBool CL_ForEachChildOfLayer(CL_Layer *parent, 
                                     CL_ChildOfLayerFunc func,
                                     void *closure);

extern int32 CL_GetLayerZIndex(CL_Layer *layer);

/* Change the physical position or dimensions of a layer */
/* May trigger asynchronous drawing to update screen     */
extern void CL_OffsetLayer(CL_Layer *layer, int32 x_offset, int32 y_offset);
extern void CL_MoveLayer(CL_Layer *layer, int32 x, int32 y);
extern void CL_ResizeLayer(CL_Layer *layer, int32 width, int32 height);

/* Setters and getters */
extern void CL_GetLayerBbox(CL_Layer *layer, XP_Rect *bbox);
extern void CL_SetLayerBbox(CL_Layer *layer, XP_Rect *bbox);
extern void CL_GetLayerBboxAbsolute(CL_Layer *layer, XP_Rect *bbox);

extern char *CL_GetLayerName(CL_Layer *layer);
extern void CL_SetLayerName(CL_Layer *layer, char *name);

/* Hide or show a layer. This may result in a redraw */
extern void CL_SetLayerHidden(CL_Layer *layer, PRBool hidden);
extern PRBool CL_GetLayerHidden(CL_Layer *layer);

extern void *CL_GetLayerClientData(CL_Layer *layer);
extern void CL_SetLayerClientData(CL_Layer *layer, void *client_data);

extern CL_Compositor *CL_GetLayerCompositor(CL_Layer *layer);
extern CL_Layer *CL_GetLayerAbove(CL_Layer *layer);
extern CL_Layer *CL_GetLayerBelow(CL_Layer *layer);
extern CL_Layer *CL_GetLayerSiblingAbove(CL_Layer *layer);
extern CL_Layer *CL_GetLayerSiblingBelow(CL_Layer *layer);
extern CL_Layer *CL_GetLayerChildByName(CL_Layer *layer, char *name);
extern CL_Layer *CL_GetLayerChildByIndex(CL_Layer *layer, uint index);
extern int CL_GetLayerChildCount(CL_Layer *layer);

extern int32 CL_GetLayerXOffset(CL_Layer *layer);
extern int32 CL_GetLayerYOffset(CL_Layer *layer);
extern int32 CL_GetLayerXOrigin(CL_Layer *layer);
extern int32 CL_GetLayerYOrigin(CL_Layer *layer);

extern void CL_GetLayerVTable(CL_Layer *layer, CL_LayerVTable *vtable);
extern void CL_SetLayerVTable(CL_Layer *layer, CL_LayerVTable *vtable);

extern CL_Layer *CL_GetLayerParent(CL_Layer *layer);

extern void CL_SetLayerUniformColor(CL_Layer *layer, CL_Color *color);

/***************** COMPOSITOR METHODS *****************/

/* Allocate and free a compositor */
extern CL_Compositor *CL_NewCompositor(CL_Drawable *primary,
                                       CL_Drawable *backingstore,
                                       int32 x_offset, int32 y_offset,
                                       int32 width, int32 height,
                                       uint32 frame_rate);
extern void CL_DestroyCompositor(CL_Compositor *compositor);

/* 
 * Called to indicate that the compositor's contents have changed. This 
 * is useful when we call into client code and need to confirm, on return,
 * that the world hasn't changed around us.
 */
extern void CL_IncrementCompositorGeneration(CL_Compositor *compositor);

/* Find layer by name. This carries out a breadth-first search of the layer tree */
extern CL_Layer *CL_FindLayer(CL_Compositor *compositor, char *name);

/*
 * Inform the compositor that some part of the window needs to be 
 * refreshed. The region to be refreshed is expressed in window
 * coordinates.
 */
extern void CL_RefreshWindowRegion(CL_Compositor *compositor,
                                   FE_Region refresh_region);
extern void CL_RefreshWindowRect(CL_Compositor *compositor,
                                 XP_Rect *refresh_rect);

/*
 * Inform the compositor that some part of a layer has changed and
 * needs to be redrawn. The region to be updated is expressed in
 * the layer's coordinate system. If update_now is TRUE, the 
 * composite is done synchronously. Otherwise, it is done at the
 * next timer callback.
 */
void 
CL_UpdateLayerRegion(CL_Compositor *compositor, CL_Layer *layer,
                     FE_Region update_region, PRBool update_now);

/*
 * Version of CL_UpdateLayerRegion where the update region is
 * expressed as a rectangle in the layer's coordinate system.
 * Note that the rectangle can have 32-bit coordinates, whereas
 * the corresponding region cannot.
 */
void
CL_UpdateLayerRect(CL_Compositor *compositor, CL_Layer *layer,
                   XP_Rect *update_rect, PRBool update_now);

/* Redraws all regions changed since last call to CL_CompositeNow().
 * This is called by the timer callback. It can be called otherwise
 * to force a synchronous composite. The cutoutp parameter determines
 * whether to subtract out cutout layers while drawing.
 */
extern void CL_CompositeNow(CL_Compositor *compositor);

/* Indicates that the window the compositor is drawing to has been resized */
extern void CL_ResizeCompositorWindow(CL_Compositor *compositor, 
                                      int32 width, int32 height);


/* 
 * Indicates that the window the compositor is drawing to has been scrolled.
 * x_origin and y_origin are the new offsets of the top-left of the window
 * relative to the top-left of the document.
*/
extern void CL_ScrollCompositorWindow(CL_Compositor *compositor, 
                                      int32 x_origin, int32 y_origin);

/* 
 * Dispatch an event to the correct layer. The return value
 * indicates whether the event was actually handled.
 */
extern PRBool CL_DispatchEvent(CL_Compositor *compositor, CL_Event *event);

/* 
 * All events go to the corresponding layer. 
 * A layer value of NULL releases the capture.
 */
extern PRBool CL_GrabMouseEvents(CL_Compositor *compositor, CL_Layer *layer);
extern PRBool CL_GrabKeyEvents(CL_Compositor *compositor, CL_Layer *layer);

/* Functions to determine whether a layer has grabbed mouse or key events. */
extern PRBool CL_IsMouseEventGrabber(CL_Compositor *compositor,
                                     CL_Layer *layer);
extern PRBool CL_IsKeyEventGrabber(CL_Compositor *compositor, CL_Layer *layer);

/* Functions to determine which layer has grabbed  key events. */
extern CL_Layer *CL_GetKeyEventGrabber(CL_Compositor *compositor);

/* Setters and getters */

/* Sets whether a compositor offscreen-drawing should occur or not
   If enabled is PR_TRUE, the compositor will use offscreen 
   drawing if applicable.                                          */
extern void CL_SetCompositorOffscreenDrawing(CL_Compositor *compositor, 
                                             CL_OffscreenMode mode);

extern CL_OffscreenMode CL_GetCompositorOffscreenDrawing(CL_Compositor *compositor);

/* Sets a compositor's enabled state. Disabling a compositor will  */
/* stop timed composites (but will not prevent explicit composites */
/* from happening). Enabling a compositor restarts timed draws.    */
extern PRBool CL_GetCompositorEnabled(CL_Compositor *compositor);
extern void CL_SetCompositorEnabled(CL_Compositor *compositor, 
                                    PRBool enabled);

extern CL_Drawable *CL_GetCompositorDrawable(CL_Compositor *compositor);
extern void CL_SetCompositorDrawable(CL_Compositor *compositor, 
                                     CL_Drawable *drawable);

extern CL_Layer *CL_GetCompositorRoot(CL_Compositor *compositor);
extern void CL_SetCompositorRoot(CL_Compositor *compositor, CL_Layer *root);

extern uint32 CL_GetCompositorFrameRate(CL_Compositor *compositor);
extern void CL_SetCompositorFrameRate(CL_Compositor *compositor, 
                                      uint32 frame_rate);

extern int32 CL_GetCompositorXOffset(CL_Compositor *compositor);
extern int32 CL_GetCompositorYOffset(CL_Compositor *compositor);
extern void CL_GetCompositorWindowSize(CL_Compositor *compositor,
                                       XP_Rect *window_size);

/* Sets the default keyboard focus change policy */
extern void CL_SetKeyboardFocusPolicy(CL_Compositor *compositor,
                                      CL_KeyboardFocusPolicy policy);

extern void CL_SetCompositorDrawingMethod(CL_Compositor *compositor,
                                          CL_DrawingMethod method);

/***************** Group-related Compositor methods *****************/

/* Adds a layer to a group. Returns TRUE if a new group had to be created */
extern PRBool CL_AddLayerToGroup(CL_Compositor *compositor,
                                  CL_Layer *layer,
                                  char *name);

/* Remove a layer from a group */
extern void CL_RemoveLayerFromGroup(CL_Compositor *compositor,
                                    CL_Layer *layer,
                                    char *name);

/* Get rid of the layer group */
extern void CL_DestroyLayerGroup(CL_Compositor *compositor,
                                 char *name);

/* Hide or unhide all layers in the group */
extern void CL_HideLayerGroup(CL_Compositor *compositor, char *name);
extern void CL_UnhideLayerGroup(CL_Compositor *compositor, char *name);

/* Move all the layers in a group */
extern void CL_OffsetLayerGroup(CL_Compositor *compositor, char *name,
                                int32 x_offset, int32 y_offset);

/* Execute function for each layer in the group */
extern PRBool CL_ForEachLayerInGroup(CL_Compositor *compositor,
                                     char *name,
                                     CL_LayerInGroupFunc func,
                                     void *closure);

/***************** Utility Compositor methods  *****************/

extern void CL_CreateDefaultLayers(CL_Compositor *compositor);

/* Convert rectangle from window to document coordinate system */
extern void CL_WindowToDocumentRect(CL_Compositor *compositor, 
                                    XP_Rect *rect);

/* Convert rectangle from document to window coordinate system */
extern void CL_DocumentToWindowRect(CL_Compositor *compositor, 
                                    XP_Rect *rect);
/* 
 * This is here temporarily. This is similar to CL_UpdateLayerRect,
 * except the rectangle is specified in document coordinates. For
 * now, it's a convenience function. In the future, the callers of
 * this function should be able to provide a layer.
 */
extern void CL_UpdateDocumentRect(CL_Compositor *compositor,
                                  XP_Rect *update_rect, PRBool update_now);

/* Convert a rect from window to layer coordinate system */
extern void CL_WindowToLayerRect(CL_Compositor *compositor, CL_Layer *layer,
                                 XP_Rect *rect);

/* Convert a rect from layer to window coordinate system */
extern void CL_LayerToWindowRect(CL_Compositor *compositor, CL_Layer *layer,
                                 XP_Rect *rect);

/* Convert a region from layer to window coordinate system */
extern void CL_LayerToWindowRegion(CL_Compositor *compositor, CL_Layer *layer,
                                   FE_Region region);

/* Convert a region from window to layer coordinate system */
extern void CL_WindowToLayerRegion(CL_Compositor *compositor, CL_Layer *layer,
                                   FE_Region region);

/***************** Drawable methods *****************/

extern CL_Drawable *CL_NewDrawable(uint width, uint height,
                                   uint32 flags,
                                   CL_DrawableVTable *vtable,
                                   void *client_data);

extern void CL_DestroyDrawable(CL_Drawable *drawable);

extern void *CL_GetDrawableClientData(CL_Drawable *drawable);

extern CL_Color *CL_GetDrawableBgColor(CL_Drawable *drawable, XP_Rect *win_rect);


/************* Other utility functions **************/
extern float CL_RegionEntropy(FE_Region region);
extern void CL_ForEachRectCoveringRegion(FE_Region region, FE_RectInRegionFunc func,
					 void *closure);

XP_END_PROTOS

#endif /* _LAYERS_H_ */

