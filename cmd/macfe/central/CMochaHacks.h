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
 * macmocha.h
 * MacFE mocha hacks
 *
 */

#include "structs.h" // mjc
#include "libevent.h"

/*
 * static class that encapsulates most of the Macha hacks
 */
class CMochaHacks
{
private:
	static LO_Element*		sMouseOverElement;		// layout element the cursor is over
	static MWContext*		sMouseOverElementContext;	// context associated with sMouseOverElement
	static LO_AnchorData*	sMouseOverMapArea;		// AREA tag the cursor is over

public:
	static void ClearSelectionForContext( MWContext* context )
	{
		if (context == sMouseOverElementContext)
		{
			sMouseOverElement = NULL;
			sMouseOverElementContext = NULL;
			sMouseOverMapArea = NULL;
		}
	}
	static void SendOutOfElementEvent(MWContext * winContext, CL_Layer* layer, SPoint32 where); // add layer param 1997-03-02 mjc
	static void SendOutOfMapAreaEvent(MWContext * winContext, CL_Layer* layer, SPoint32 where); // add layer param 1997-03-02 mjc
	static void	ResetMochaMouse();

	static Boolean	IsMouseOverElement(LO_Element* inElement)
					{	return inElement == sMouseOverElement;	}
	static Boolean	IsMouseOverMapArea(LO_AnchorData* inAnchorData)
					{	return inAnchorData == sMouseOverMapArea;	}

	static void	SetMouseOverElement(LO_Element* inElement, MWContext* inElementContext = NULL)
				{
					sMouseOverElement = inElement;
					sMouseOverElementContext = inElementContext;
				}
	static void RemoveReferenceToMouseOverElementContext(MWContext *context)
	{
		if (sMouseOverElementContext == context)
			sMouseOverElementContext = NULL;
	}
	static void	SetMouseOverMapArea(LO_AnchorData* inAnchorData)
				{	sMouseOverMapArea = inAnchorData;	}

	static LO_Element*	GetMouseOverElement()
						{	return sMouseOverElement;	}
	static LO_AnchorData*	GetMouseOverMapArea()
							{	return sMouseOverMapArea;	}

	static uint32 MochaModifiers(const UInt16 inModifiers);
	static uint32 MochaModifiersFromKeyboard(void);
					
	// manage windows declared as dependent in javascript
	static Boolean 	IsDependent(MWContext* inContext);
	static void 	AddDependent(MWContext* inParent, MWContext* inChild);
	static void 	RemoveDependents(MWContext* inContext);
	
	// Whenever a window or frame moves or resizes send an event to javascript
	static void		SendMoveEvent(MWContext* inContext, int32 inX, int32 inY);
	
	static void		SendEvent(MWContext* inContext, int32 inType, LO_Element* inElement = nil);
	
	// Send navigation events - currently not cancellable
	static void		SendBackEvent(MWContext* inContext)
					{ SendEvent(inContext, EVENT_BACK); }
					
	static void		SendForwardEvent(MWContext* inContext)
					{ SendEvent(inContext, EVENT_FORWARD); }
};

/*
 * CMochaEventCallback
 * class that encapsulates sending of mocha events
 * Subclasses should override EventComplete
 */

class CMochaEventCallback	{
public:

// Constructors
					CMochaEventCallback();
	virtual 		~CMochaEventCallback();

// Mocha interface
	void 			SendEvent(MWContext * context, LO_Element * element, int32 type, CL_Layer* layer, SPoint32 where);
	void 			SendEvent(MWContext * context, LO_AnchorData * data, int32 type, CL_Layer* layer, SPoint32 where);

	// MochaCallback calls EventComplete. You'll be deleted after this call
	virtual void	Complete(MWContext * context, LO_Element * element, 
                 int32 type, ETEventStatus status);

	static void MochaCallback(MWContext * context, LO_Element * element, 
                 int32 type, void * inCallback, ETEventStatus status);

private:
// Old Mocha calls used to accept either LO_Element, or LO_AnchorData
// New ones only accept LO_Element, so sometimes we need to create/dispose
// dummy layout elements. This is encapsulated in this class
	LO_Element		* fDummyElement;
};