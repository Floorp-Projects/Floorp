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

// CHTMLClickRecord.h

#pragma once

#include "lo_ele.h"
#include "cstring.h"
#include "Netscape_Constants.h"

class CNSContext;
class CHTMLView;
struct SMouseDownEvent;

class CHTMLClickRecord
{
	friend class CHTMLView;
	friend class CBrowserView;
	friend class CEditView;
	
	public:
	
		enum EClickState {
			eUndefined					=	0,
			eMouseHystersis				=	6,
			eMouseDragging				=	1,
			eMouseTimeout,
			eMouseUpEarly,
			eHandledByAttachment
		};
		
					CHTMLClickRecord(
								Point 				inLocalWhere,
								const SPoint32&		inImageWhere,
								CNSContext* 		inContext,
								LO_Element* 		inElementelement,
								CL_Layer* 			inLayer = NULL);

		void				Recalc(void);


		static Boolean 		PixelReallyInElement(
								const SPoint32& 	inImagePoint,
								LO_Element* 		inElement);

		static EClickState	WaitForMouseAction(
								const Point&	 	inInitialPoint,
								Int32				inWhen,
								Int32				inDelay);
		static EClickState	WaitForMouseAction(
								const SMouseDownEvent& inMouseDown,
								LAttachable*		inAttachable,
								Int32				inDelay,
								Boolean				inExecuteContextMenuAttachment = true);
								// This version allows a CContextMenuAttachment to work.

		Boolean				IsClickOnAnchor(void) const;
		Boolean				IsClickOnEdge(void) const;
		
		const char*			GetClickURL() const { return (const char*)mClickURL; }
		
	protected:
		
		EClickKind			CalcImageClick(void);
		EClickKind			CalcTextClick(void);
		EClickKind			CalcEdgeClick(void);
		
		Point				mLocalWhere;
		SPoint32			mImageWhere;
		
		LO_Element* 		mElement;
		LO_AnchorData*		mAnchorData;	
		CNSContext*			mContext;
		CL_Layer*			mLayer;
		EClickKind			mClickKind;
		


		cstring				mClickURL;
		cstring				mImageURL;
		cstring				mWindowTarget;

			// Click in Edge members
		Boolean				mIsVerticalEdge;	// If we are dragging verticaly
		Int32				mEdgeUpperBound;	// top and bottom (if vertical)
		Int32				mEdgeLowerBound;	// or left and right (if horizontal)


// ее access
	Boolean	IsAnchor();		// The key routine, initializes all the fields bellow
	Boolean IsEdge();
	void	ResetNamedWindow();	// No named windows here
// ее variables
	void		CalculatePosition();	// Initializes all the variables

	public:
		Boolean             IsVerticalEdge(void) { return mIsVerticalEdge; };
		LO_Element*         GetLayoutElement(void) { return mElement; };
		SPoint32			GetImageWhere(void) { return mImageWhere; };
		CL_Layer* 			GetLayer(void) { return mLayer; };
		
};


