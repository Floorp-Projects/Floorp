/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * macmocha.cp
 * MacFE mocha hacks
 *
 */

#include "CMochaHacks.h"
#include "lo_ele.h"
#include "fe_proto.h" // for FE_DestroyWindow
#include "proto.h" // 1997-03-02 mjc
#include "layers.h"
#include "macutil.h"

LO_Element* CMochaHacks::sMouseOverElement = NULL;	// layout element the cursor is over
MWContext* CMochaHacks::sMouseOverElementContext = NULL;	// context associated with sMouseOverElement
LO_AnchorData* CMochaHacks::sMouseOverMapArea = NULL;	// AREA tag the cursor is over

// <where> is document-relative
void 
CMochaHacks::SendOutOfElementEvent(MWContext * winContext, CL_Layer* layer, SPoint32 where) // add layer param 1997-03-02 mjc
{
	Assert_(winContext);
	
	try
	{
		if ( sMouseOverElement )
		{
			// ET_SendEvent now takes a JSEvent struct instead of an int type
			JSEvent* event = XP_NEW_ZAP(JSEvent);
			if (event)
			{
				// 97-06-21 pkc -- If we have an sMouseOverElementContext then use it
				// instead of winContext
				MWContext* theContext = sMouseOverElementContext ? sMouseOverElementContext : winContext;
				event->type = EVENT_MOUSEOUT;
				event->x = where.h;
				event->y = where.v;
				event->docx = event->x + CL_GetLayerXOrigin(layer);
				event->docy = event->y + CL_GetLayerYOrigin(layer);
				
				int32 x_offset, y_offset;
				FE_GetWindowOffset(theContext, &x_offset, &y_offset);
				event->screenx = event->docx + x_offset;
				event->screeny = event->docy + y_offset;
				event->layer_id = LO_GetIdFromLayer(theContext, layer);
				ET_SendEvent( theContext, sMouseOverElement, event, NULL, NULL );
				sMouseOverElement = NULL;
				sMouseOverElementContext = NULL;
			}
		}
			
	}
	catch(...)
	{
	}
}

void 
CMochaHacks::SendOutOfMapAreaEvent(MWContext * winContext, CL_Layer* layer, SPoint32 where) // add layer param 1997-03-02 mjc
{
	Assert_(winContext);
	
	try
	{
		if ( sMouseOverMapArea )
		{
			CMochaEventCallback * cb = new CMochaEventCallback;	// Need it because of LO_AnchorData
			cb->SendEvent( winContext, sMouseOverMapArea, EVENT_MOUSEOUT, layer, where );
			sMouseOverMapArea = NULL;
		}
	}
	catch(...)
	{
	}
}

//
// CMochaEventCallback
//
void
CMochaHacks::ResetMochaMouse()
{
	sMouseOverElement = NULL;
	sMouseOverMapArea = NULL;
}

// Returns mocha modifier bitset given mac modifiers.
uint32 
CMochaHacks::MochaModifiers(const UInt16 inModifiers)
{
	return ((inModifiers & shiftKey) ? EVENT_SHIFT_MASK : 0) |
			((inModifiers & controlKey) ? EVENT_CONTROL_MASK : 0) |
			((inModifiers & optionKey) ? EVENT_ALT_MASK : 0) |
			((inModifiers & cmdKey) ? EVENT_META_MASK : 0);
}

// Returns mocha modifiers by reading the keyboard.
uint32 
CMochaHacks::MochaModifiersFromKeyboard(void)
{
	union
	{
		KeyMap asMap;
		Byte asBytes[16];
	};

	::GetKeys(asMap);
	return ((asBytes[kShiftKey >> 3] & (1 << (kShiftKey & 0x07))) ? EVENT_SHIFT_MASK : 0) |
			((asBytes[kCtlKey >> 3] & (1 << (kCtlKey & 0x07))) ? EVENT_CONTROL_MASK : 0) |
			((asBytes[kOptionKey >> 3] & (1 << (kOptionKey & 0x07))) ? EVENT_ALT_MASK : 0) |
			((asBytes[kCommandKey >> 3] & (1 << (kCommandKey & 0x07))) ? EVENT_META_MASK : 0);
}
	

// Returns true if the window is a dependent of another.
// Parameters:
// 	inContext: the context for this window.
Boolean 	
CMochaHacks::IsDependent(MWContext* inContext)
{
	return (inContext->js_parent != nil);
}

// Add a window as a dependent of another.
// Called in FE_MakeNewWindow.
// Parameters:
//		inParent: the parent context.
//		inChild: the context for this window which will be made a dependent of the parent.
void 
CMochaHacks::AddDependent(MWContext* inParent, MWContext* inChild)
{
	// inParent could be a grid context, but dependencies are between windows so find root context.
	MWContext* theParentRoot = XP_GetNonGridContext(inParent);
	if (theParentRoot != nil && inChild != nil)
	{
		if (theParentRoot->js_dependent_list == NULL)
			theParentRoot->js_dependent_list = XP_ListNew();
		if (theParentRoot->js_dependent_list != NULL)
		{
			XP_ListAddObject(theParentRoot->js_dependent_list, inChild);
			inChild->js_parent = theParentRoot;
		}
	}

}

// Remove dependents of the window.
// Called in destructor for window.
// Parameters:
// 	inContext: the context for this window.
void 
CMochaHacks::RemoveDependents(MWContext* inContext)
{
	// FE_DestroyWindow makes this recursive; keep track of how many levels deep we are.
	static int recursionLevel = 0;
	++recursionLevel;
	if (inContext->js_dependent_list)
	{
		MWContext *depContext;
		// destroy windows which are dependent on this window
		for (int i = 1; i <= XP_ListCount(inContext->js_dependent_list); i++)
		{
			depContext = (MWContext *)XP_ListGetObjectNum(inContext->js_dependent_list, i);
			FE_DestroyWindow(depContext);
		}
		XP_ListDestroy(inContext->js_dependent_list);
		inContext->js_dependent_list = NULL;
	}
	--recursionLevel;
	// remove self from parent's dependent list but only if we're the window
	// at the top of the chain (don't alter lists we're iterating over).
	if (recursionLevel == 0 && inContext->js_parent != nil)
	{
		if (XP_ListCount(inContext->js_parent->js_dependent_list) == 1)
		{
			// if the last element in the list, destroy the list.
			XP_ListDestroy(inContext->js_parent->js_dependent_list);
			inContext->js_parent->js_dependent_list = NULL;
		}
		else XP_ListRemoveObject(inContext->js_parent->js_dependent_list, inContext);
	}
}

// Send move event to mocha every time a window or pane is moved.
void 
CMochaHacks::SendMoveEvent(MWContext* inContext, int32 inX, int32 inY)
{
	JSEvent *event;
	
	event = XP_NEW_ZAP(JSEvent);
	if (event)
	{
		event->type = EVENT_MOVE;
		event->x = inX;
		event->y = inY;
	}
	ET_SendEvent(inContext, 0, event, 0, 0);
}

// Send the event specified, with no callback.
void
CMochaHacks::SendEvent(MWContext* inContext, int32 inType, LO_Element* inElement)
{
	JSEvent *event;
	event = XP_NEW_ZAP(JSEvent);
	if (event)
	{
		event->type = inType;
		ET_SendEvent(inContext, inElement, event, 0, 0);
	}
}

//
// CMochaEventCallback
//

#ifdef DEBUG
static int sCallbackCount = 0;
#endif

CMochaEventCallback::CMochaEventCallback()
{
#ifdef DEBUG
	sCallbackCount++;
#endif
	fDummyElement = NULL;
}

CMochaEventCallback::~CMochaEventCallback()
{
#ifdef DEBUG
	sCallbackCount--;
#endif
	if (fDummyElement != NULL)
		XP_FREE( fDummyElement );
}

//
// Plain SendEvent
//
void
CMochaEventCallback::SendEvent(MWContext * context, LO_Element * element, int32 type, CL_Layer* layer, SPoint32 where)
{
	// ET_SendEvent now takes a JSEvent struct instead of an int type
	JSEvent* event = XP_NEW_ZAP(JSEvent);
	if (event)
	{
		event->type = type;
		event->x = where.h;
		event->y = where.v;
		event->docx = event->x + CL_GetLayerXOrigin(layer);
		event->docy = event->y + CL_GetLayerYOrigin(layer);
		
		int32 x_offset, y_offset;
		FE_GetWindowOffset(context, &x_offset, &y_offset);
		event->screenx = event->docx + x_offset;
		event->screeny = event->docy + y_offset;
		event->layer_id = LO_GetIdFromLayer(context, layer);
		ET_SendEvent( context, element, event, MochaCallback, this);
		// PR_Yield();	To speed up processing?
	}
}

// 
// LO_AnchorData SendEvent
//
void
CMochaEventCallback::SendEvent(MWContext * context, LO_AnchorData * data, int32 type, CL_Layer* layer, SPoint32 where)
{
	// Create fake layout element
	fDummyElement = XP_NEW_ZAP(LO_Element);
	if (fDummyElement)
	{
		fDummyElement->type = LO_TEXT;
		fDummyElement->lo_text.anchor_href = data;
		fDummyElement->lo_text.text = data->anchor;
		
		// ET_SendEvent now takes a JSEvent struct instead of an int type
		JSEvent* event = XP_NEW_ZAP(JSEvent);
		if (event)
		{
			event->type = type;
			event->x = where.h;
			event->y = where.v;
			event->docx = event->x + CL_GetLayerXOrigin(layer);
			event->docy = event->y + CL_GetLayerYOrigin(layer);
			
			int32 x_offset, y_offset;
			FE_GetWindowOffset(context, &x_offset, &y_offset);
			event->screenx = event->docx + x_offset;
			event->screeny = event->docy + y_offset;
			event->layer_id = LO_GetIdFromLayer(context, layer);
			ET_SendEvent( context, fDummyElement, event, MochaCallback, this);
		}
	}
}

// 
// EventComplete, does nothing
//
void
CMochaEventCallback::Complete(MWContext * /*context*/, LO_Element * /*element*/, 
										int32 /*type*/, ETEventStatus /*status*/)
{
// EVENT_OK means we should handle the event/
// EVENT_CANCEL, EVENT_PANIC, means mocha has cancelled the click
}

//
// MochaCallback, called by mocha after event is processed
//
void CMochaEventCallback::MochaCallback(MWContext * context, LO_Element * element, 
                 int32 type, void * inCallback, ETEventStatus status)
{
	CMochaEventCallback * callback = (CMochaEventCallback *) inCallback;
	callback->Complete( context, element, type, status );
	delete callback;
}