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

#include "mprint.h"

#include "CHTMLView.h"
#include "CBrowserContext.h"
#include "CHyperScroller.h"
#include "UGraphicGizmos.h"

#include "LListener.h"
#include "LPrintout.h"
#include "PascalString.h"
#include "LCaption.h"

#include "MoreMixedMode.h"


#define NONE_FORM				1
#define PAGE_NUMBER_FORM		2
#define DATE_FORM				3
#define LOCATION_FORM			4
#define TITLE_FORM				5

#include "macutil.h"
#include "earlmgr.h"
#include "resgui.h"
#include "macgui.h" // for UGraphics
#include "uerrmgr.h"
#include "shist.h"
#include "libi18n.h"
#include "xlate.h"
#include "np.h"
#include <LArray.h>
#include "mimages.h"
#include "mplugin.h"

//	***** BEGIN HACK ***** (Bug #83149)
#if defined (JAVA)
#include "MJava.h"
#endif
//	***** END HACK *****

#pragma mark --- CPrintHTMLView ---

//======================================
class CPrintHTMLView : public CHTMLView
//======================================
{
	private:
		typedef CHTMLView Inherited;
	public:
	
		enum { class_ID = 'PtHt' };
		enum printMode {
			epmBlockDisplay,
			epmLayout,
			epmDisplay
		};
								CPrintHTMLView(LStream* inStream);
		virtual					~CPrintHTMLView();
				void 			CopyCharacteristicsFrom(const CHTMLView* inHTMLView);

 		virtual void			CountPanels(Uint32 &outHorizPanels, Uint32 &outVertPanels);
		virtual Boolean			ScrollToPanel (const PanelSpec &inPanel);

		void					SetPrintMode (printMode inMode)
									{ mPrintMode = inMode; }
		void					InitializeCapture (void);
		void					CalculatePageBreaks (void);

		CL_Compositor*			GetCompositor (void)
									{ return mCompositor->mCompositor; }

	// overrides

		virtual void			AdaptToSuperFrameSize(
									Int32 inSurrWidthDelta,
									Int32 inSurrHeightDelta,
									Boolean inRefresh);

	protected:
		virtual void			FinishCreateSelf(void);
		virtual void			InstallBackgroundColor();
									// Sets mBackgroundColor. Called from ClearBackground().
									// The base class implementation uses the text background
									// preference, but derived classes can override this.
									// Here we do nothing, because the background color is set
									// in CopyCharacteristicsFrom().
		virtual	void			GetFullGridSize(
									Int32&					outWidth,
									Int32&					outHeight);
		void					CapturePosition (LO_Element *inElement);
		virtual void			ResetBackgroundColor() const;
									// Calls RGBBackColor(mBackgroundColor).  Printview overrides.
		virtual	void			DrawFrameFocus();
		virtual	void 			DrawBackground(
									const Rect& 			inArea,
									LO_ImageStruct*			inBackdrop = nil);								
		virtual void 			EraseBackground(
									int						inLocation,
									Int32					inX,
									Int32					inY,
									Uint32					inWidth,
									Uint32					inHeight,
									LO_Color*				inColor);

		Boolean					BelongsOnPage (LO_Element *inElement);
		inline Boolean			BelongsOnPage (LO_TextStruct *inElement)
									{ return BelongsOnPage ((LO_Element *) inElement); }
		inline Boolean			BelongsOnPage (LO_LinefeedStruct *inElement)
									{ return BelongsOnPage ((LO_Element *) inElement); }
		inline Boolean			BelongsOnPage (LO_HorizRuleStruct *inElement)
									{ return BelongsOnPage ((LO_Element *) inElement); }
		inline Boolean			BelongsOnPage (LO_BullettStruct *inElement)
									{ return BelongsOnPage ((LO_Element *) inElement); }
		inline Boolean			BelongsOnPage (LO_EmbedStruct *inElement)
									{ return BelongsOnPage ((LO_Element *) inElement); }
		inline Boolean			BelongsOnPage (LO_TableStruct *inElement)
									{ return BelongsOnPage ((LO_Element *) inElement); }
		inline Boolean			BelongsOnPage (LO_CellStruct *inElement)
									{ return BelongsOnPage ((LO_Element *) inElement); }
		Boolean					BelongsOnPage (LO_FormElementStruct *inElement);

		virtual void 			DisplaySubtext(
									int 					inLocation,
									LO_TextStruct*			inText,
									Int32 					inStartPos,
									Int32					inEndPos,
									XP_Bool 				inNeedBG);

		virtual void 			DisplayText(
									int 					inLocation,
									LO_TextStruct*			inText,
									XP_Bool 				inNeedBG);

		virtual void 			DisplayLineFeed(
									int 					inLocation,
									LO_LinefeedStruct*		inLinefeedStruct,
									XP_Bool 				inNeedBG);

		virtual void 			DisplayHR(
									int 					inLocation,
									LO_HorizRuleStruct*		inRuleStruct);

		virtual void 			DisplayBullet(
									int 					inLocation,
									LO_BullettStruct*		inBulletStruct);

		virtual void 			DisplayEmbed(
									int 					inLocation,
									LO_EmbedStruct*			inEmbedStruct);

		virtual void 			DisplayFormElement(
									int 					inLocation,
									LO_FormElementStruct* 	inFormElement);

		virtual void 			DisplayEdge(
									int 					inLocation,
									LO_EdgeStruct*			inEdgeStruct);

		virtual void 			DisplayTable(
									int 					inLocation,
									LO_TableStruct*			inTableStruct);

		virtual void 			DisplayCell(
									int 					inLocation,
									LO_CellStruct*			inCellStruct);

		virtual	void 			CreateGridView(
									CBrowserContext*		inGridContext,
									Int32					inX,
									Int32 					inY,
									Int32					inWidth,
									Int32					inHeight,
									Int8					inScrollMode,
									Bool 					inNoEdge);

		printMode				mPrintMode;
		LArray					mElementRects;
		LArray					mVPanelRects;
}; // class CPrintHTMLView

//-----------------------------------
CPrintHTMLView::CPrintHTMLView(LStream* inStream)
	:	mElementRects(sizeof(XP_Rect)),
		mVPanelRects(sizeof(XP_Rect)),

		Inherited(inStream)
//-----------------------------------
{
	SetPrintMode (epmDisplay);
	SetScrollMode(LO_SCROLL_NO, false);
}

//-----------------------------------
CPrintHTMLView::~CPrintHTMLView()
//-----------------------------------
{
}

// Override method to skip code in CHTMLView so that we don't call Repaginate() which
// will destroy CPrintHTMLView early in printing process.
//-----------------------------------
void CPrintHTMLView::AdaptToSuperFrameSize(
//-----------------------------------
	Int32					inSurrWidthDelta,
	Int32					inSurrHeightDelta,
	Boolean					inRefresh)
{
	LView::AdaptToSuperFrameSize(inSurrWidthDelta, inSurrHeightDelta, inRefresh);
}

//-----------------------------------
void CPrintHTMLView::CopyCharacteristicsFrom(const CHTMLView* inHTMLView)
//-----------------------------------
{
	if (CPrefs::GetBoolean(CPrefs::PrintBackground))
	{
		// Transfer the back color from the view we're printing to the print view.
		// This is necessary because, e.g., message views and browser views have
		// different default background colors.
		LO_Color bc = UGraphics::MakeLOColor(inHTMLView->GetBackgroundColor());
		SetBackgroundColor(bc.red, bc.green, bc.blue);
	}
}

//-----------------------------------
// our version works much like the inherited version (LView's), except we take into
// account an HTML page's variable height.
void CPrintHTMLView::CountPanels(
//-----------------------------------
	Uint32	&outHorizPanels,
	Uint32	&outVertPanels)
{
	SDimension32	imageSize;
	GetImageSize(imageSize);
	
	SDimension16	frameSize;
	GetFrameSize(frameSize);
	
	outHorizPanels = 1;
	if (frameSize.width > 0 && imageSize.width > 0)
		outHorizPanels = ((imageSize.width - 1) / frameSize.width) + 1;
	
	outVertPanels = mVPanelRects.GetCount();
}

//-----------------------------------
Boolean CPrintHTMLView::ScrollToPanel (const PanelSpec &inPanel) {
//-----------------------------------

	Boolean			panelInImage;
	SDimension16	frameSize;
	GetFrameSize(frameSize);

	Uint32	horizPanelCount;
	Uint32	vertPanelCount;
	CountPanels(horizPanelCount, vertPanelCount);

	// rely on horizPanelCount, but use mVPanelRects for vertical displacement
	// the simple horizontal calculation is sufficient, since we don't mess with
	// horizontal displacement.  But since we munge page heights, that's more complicated.

	panelInImage = false;
	if (inPanel.horizIndex <= horizPanelCount && inPanel.vertIndex <= vertPanelCount) {
		Int32	horizPos = frameSize.width * (inPanel.horizIndex - 1);
		Int32	vertPos;
		int32	pageHeight;
		XP_Rect	panelVRect;

		panelInImage = mVPanelRects.FetchItemAt (inPanel.vertIndex, &panelVRect);
		vertPos = panelVRect.top;
		pageHeight = panelVRect.bottom - panelVRect.top;
		if (panelInImage) {
			ScrollImageTo (horizPos, vertPos, false);

			// now make XP_CheckElementSpan work for this page
			PrintInfo	*pageInfo = ((MWContext *) *mContext)->prInfo;
			pageInfo->page_topy = vertPos;
			pageInfo->page_break = vertPos + pageHeight;
	  		pageInfo->page_height = pageHeight;
	  		pageInfo->page_width = frameSize.width;

			if (mCompositor)
				CL_ResizeCompositorWindow (*mCompositor, frameSize.width, pageHeight);
	  	}
	}
	
	return panelInImage;
}

//-----------------------------------
void CPrintHTMLView::InitializeCapture (void)
//-----------------------------------
{
	mElementRects.RemoveItemsAt (mElementRects.GetCount(), LArray::index_First);
	mVPanelRects.RemoveItemsAt (mVPanelRects.GetCount(), LArray::index_First);
}

//-----------------------------------
void CPrintHTMLView::CalculatePageBreaks (void)
//-----------------------------------
{
	Boolean			breakBigElements;	// a large element straddles the boundary; break it
	Rect			pageArea;
	short			pageHeight;
	XP_Rect			vPanelRect,			// page rect
					elementRect;		// element rectangle
	int32			pageMajorLimit,		// refuse to truncate the page past this point
					pageMinorLimit;		// secondary page truncation limit
	ArrayIndexT		element;			// index into list of element rectangles

	CalcPortFrameRect (pageArea);

	pageHeight = pageArea.bottom - pageArea.top;

	vPanelRect.left = pageArea.left;
	vPanelRect.right = pageArea.right;
	vPanelRect.top = 0;
	vPanelRect.bottom = pageHeight;

	do {
		pageMajorLimit = vPanelRect.bottom - pageHeight / 2;
		pageMinorLimit = vPanelRect.bottom - pageHeight / 32;
		element = LArray::index_First;
		breakBigElements = false;
		while (mElementRects.FetchItemAt (element, &elementRect)) {
			element++;
			// if it spans the page boundary, it's trouble:
			if (elementRect.top < vPanelRect.bottom && elementRect.bottom > vPanelRect.bottom)
				// if it's bigger than a whole page, don't even consider it
				if (elementRect.bottom - elementRect.top < pageHeight) {
					// if we've decided to break on a big element, consider only small elements
					if (breakBigElements && elementRect.top < pageMinorLimit)
						continue;
					// bump up page bottom. if it's been bumped too far, reset and punt
					vPanelRect.bottom = elementRect.top;
					element = LArray::index_First;
					if (vPanelRect.bottom < pageMajorLimit) {
						vPanelRect.bottom = vPanelRect.top + pageHeight;
						breakBigElements = true;
					}
				}
		}

		// remove all elements occurring on this page
		element = LArray::index_First;
		while (mElementRects.FetchItemAt (element, &elementRect))
			if (elementRect.top < vPanelRect.bottom)
				mElementRects.RemoveItemsAt (1, element);
			else
				element++;

		// store page rect and prepare for next page
		mVPanelRects.InsertItemsAt (1, LArray::index_Last, &vPanelRect);
		vPanelRect.top = vPanelRect.bottom + 1;
		vPanelRect.bottom = vPanelRect.top + pageHeight;
	} while (mElementRects.GetCount() > 0);
}

//-----------------------------------
void CPrintHTMLView::FinishCreateSelf(void)
//-----------------------------------
{
	/* Printing contexts want to be drawn back-to-front, only, and all in one pass,
	   rather than on a delayed call-back system.  The inherited method creates
	   a compositor; we modify it.  Note that while a CHTMLView's mCompositor
	   is intended to be a _shared_ object, it appears that this one is used only
	   while printing, so it seems safe to make printing-specific changes to it.
	*/
	Inherited::FinishCreateSelf();
	CL_SetCompositorDrawingMethod (*mCompositor, CL_DRAWING_METHOD_BACK_TO_FRONT_ONLY);
	CL_SetCompositorFrameRate (*mCompositor, 0);
}

//-----------------------------------
void CPrintHTMLView::InstallBackgroundColor()
// Overridden to use a white background
//-----------------------------------
{
	if (!CPrefs::GetBoolean(CPrefs::PrintBackground))
		memset(&mBackgroundColor, 0xFF, sizeof(mBackgroundColor)); // white
	// Else do nothing: use the background color that we copied from the view
	// we're printing.
}
//-----------------------------------
void CPrintHTMLView::ResetBackgroundColor() const
//-----------------------------------
{
	// do nothing.
}

//-----------------------------------
void CPrintHTMLView::GetFullGridSize(
	Int32&					outWidth,
	Int32&					outHeight)
//-----------------------------------
{
	// We never have scrollers, but is this right?  I'm fairly clueless...
	// The base class gets the size from mScroller which, of course, is null
	// for a print view...
	SDimension16 theContainerSize;
	this->GetFrameSize(theContainerSize);
	outWidth = theContainerSize.width;
	outHeight = theContainerSize.height;
} // CPrintHTMLView::GetFullGridSize

//-----------------------------------
void CPrintHTMLView::CapturePosition (LO_Element *inElement) {
//-----------------------------------

	XP_Rect			rect;

	CalcAbsoluteElementPosition (inElement, rect);

	try {
		mElementRects.InsertItemsAt (1, LArray::index_Last, &rect);
	} catch (...) {
		// there's really no one to report this error to.  if we're this out of memory,
		// something else is going to fail worse than this.  failure to add this rect
		// to the list will only cause pagination to happen at inopportune positions.
	}
}

//-----------------------------------
// does an object with the given top and height want to be rendered on the current page?
Boolean
CPrintHTMLView::BelongsOnPage (LO_Element *inElement) {
//-----------------------------------

	Rect	frame;
	return CalcElementPosition (inElement, frame);
}

//-----------------------------------
// does an object with the given top and height want to be rendered on the current page?
Boolean
CPrintHTMLView::BelongsOnPage (LO_FormElementStruct *inElement) {
//-----------------------------------

	Rect	frame;
	Boolean	rtnVal;
#ifdef LAYERS
	// hack! HACK! HAAAACK!
	/*	The element position calculation takes layer positions into account.
		That's normally good, except for form elements, which are implemented as PowerPlant
		subpanes.  That's normally good, except that the compositor sticks each form element
		subpane into a layer of its own.  No doubt the compositor thinks that's good, and
		normally we don't have a problem with that.  But here, it results in the calculated
		position of the frame being multiplied by two (since the layer's position is added
		to the element's position).  Here's where we temporarily convince our superclass
		that the current layer is at (0,0):
	*/
	// note: if this ever gives you problems, you can probably replace it with simply
	// "return true" and let clipping decide who to display.
	CDrawable	*drawable = mCurrentDrawable;
	SPoint32	origin = mLayerOrigin;
	mCurrentDrawable = NULL;
	mLayerOrigin.h = 0;
	mLayerOrigin.v = 0;
#endif
	rtnVal = CalcElementPosition ((LO_Element *) inElement, frame);
#ifdef LAYERS
	mCurrentDrawable = drawable;
	mLayerOrigin = origin;
#endif
	return rtnVal;
}


//-----------------------------------
void CPrintHTMLView::DisplaySubtext(
	int 					inLocation,
	LO_TextStruct*			inText,
	Int32 					inStartPos,
	Int32					inEndPos,
	XP_Bool 				inNeedBG)
//-----------------------------------
{
	switch (mPrintMode) {
		case epmBlockDisplay:
			break;
		case epmLayout:
			// do it all in DisplayText. the assumption here is that DisplaySubtext
			// will be called more often, but DisplayText will be called for each line
			break;
		case epmDisplay:
			if (FocusDraw() && BelongsOnPage (inText))
				Inherited::DisplaySubtext(inLocation, inText, inStartPos, inEndPos, inNeedBG);
	}
} // CPrintHTMLView::DisplaySubtext

//-----------------------------------
void CPrintHTMLView::DisplayText(
	int 					inLocation,
	LO_TextStruct*			inText,
	XP_Bool 				inNeedBG) {
//-----------------------------------

	switch (mPrintMode) {
		case epmBlockDisplay:
			break;
		case epmLayout:
			CapturePosition ((LO_Element *) inText);
			break;
		case epmDisplay:
			if (FocusDraw() && BelongsOnPage (inText))
				Inherited::DisplayText (inLocation, inText, inNeedBG);
	}
}

//-----------------------------------
void CPrintHTMLView::DisplayLineFeed(
	int 					inLocation,
	LO_LinefeedStruct*		inLinefeedStruct,
	XP_Bool 				inNeedBG) {
//-----------------------------------

	switch (mPrintMode) {
		case epmBlockDisplay:
			break;
		case epmLayout:
			CapturePosition ((LO_Element *) inLinefeedStruct);
			break;
		case epmDisplay:
			if (FocusDraw() && BelongsOnPage (inLinefeedStruct))
				Inherited::DisplayLineFeed(inLocation, inLinefeedStruct, inNeedBG);
	}
}


//-----------------------------------
void CPrintHTMLView::DisplayHR(
	int 					inLocation,
	LO_HorizRuleStruct*		inRuleStruct)
//-----------------------------------
{
	switch (mPrintMode) {
		case epmBlockDisplay:
			break;
		case epmLayout:
			CapturePosition ((LO_Element *) inRuleStruct);
			break;
		case epmDisplay:
			if (FocusDraw() && BelongsOnPage (inRuleStruct))
				Inherited::DisplayHR (inLocation, inRuleStruct);
	}
} // CPrintHTMLView::DisplayHR

//-----------------------------------
void CPrintHTMLView::DisplayBullet(
	int 					inLocation,
	LO_BullettStruct*		inBulletStruct) {
//-----------------------------------

	switch (mPrintMode) {
		case epmBlockDisplay:
			break;
		case epmLayout:
			CapturePosition ((LO_Element *) inBulletStruct);
			break;
		case epmDisplay:
			if (FocusDraw() && BelongsOnPage (inBulletStruct))
				Inherited::DisplayBullet (inLocation, inBulletStruct);
	}
} // CPrintHTMLView::DisplayBullet

//-----------------------------------
void CPrintHTMLView::DisplayEmbed(
	int 					/*inLocation*/,
	LO_EmbedStruct*			inEmbedStruct) {
//-----------------------------------

	switch (mPrintMode) {
		case epmBlockDisplay:
			break;
		case epmLayout:
			CapturePosition ((LO_Element *) inEmbedStruct);
			break;
		case epmDisplay:
			if (FocusDraw() && BelongsOnPage (inEmbedStruct))
				{
				NPEmbeddedApp* app = (NPEmbeddedApp*) inEmbedStruct->objTag.FE_Data;
				CPluginView* view = (CPluginView*) app->fe_data;
				view->EmbedDisplay(inEmbedStruct, true);
				}
	}
}

//-----------------------------------
void CPrintHTMLView::DisplayFormElement(
	int 					inLocation,
	LO_FormElementStruct* 	inFormElement) {
//-----------------------------------

	switch (mPrintMode) {
		case epmBlockDisplay:
			break;
		case epmLayout:
			CapturePosition ((LO_Element *) inFormElement);
			break;
		case epmDisplay:
			if (FocusDraw() && BelongsOnPage (inFormElement))
				Inherited::DisplayFormElement(inLocation, inFormElement);
	}
}

//-----------------------------------
void CPrintHTMLView::DisplayEdge(
	int 					/*inLocation*/,
	LO_EdgeStruct*			/*inEdgeStruct*/)
//-----------------------------------
{
	// Do nothing for printing.
}

//-----------------------------------
void CPrintHTMLView::DisplayTable(
	int 					inLocation,
	LO_TableStruct*			inTableStruct) {
//-----------------------------------

	switch (mPrintMode) {
		case epmBlockDisplay:
			break;
		case epmLayout:
			CapturePosition ((LO_Element *) inTableStruct);
			break;
		case epmDisplay:
			if (FocusDraw() && BelongsOnPage (inTableStruct))
				Inherited::DisplayTable(inLocation, inTableStruct);
	}
} // CPrintHTMLView::DisplayTable

//-----------------------------------
void CPrintHTMLView::DisplayCell(
	int 					inLocation,
	LO_CellStruct*			inCellStruct) {
//-----------------------------------

	switch (mPrintMode) {
		case epmBlockDisplay:
			break;
		case epmLayout:
			CapturePosition ((LO_Element *) inCellStruct);
			break;
		case epmDisplay:
			if (FocusDraw() && BelongsOnPage (inCellStruct))
				Inherited::DisplayCell (inLocation, inCellStruct);
	}
} // CPrintHTMLView::DisplayCell

//-----------------------------------
void CPrintHTMLView::DrawFrameFocus()
//-----------------------------------
{
	// Do nothing for printing.
} // CPrintHTMLView::DrawFrameFocus

//-----------------------------------
void CPrintHTMLView::DrawBackground(
	const Rect& 			inArea,
	LO_ImageStruct*			inBackdrop)
//-----------------------------------
{
	// ¥ default white background for printing (also black text)
	if (!CPrefs::GetBoolean(CPrefs::PrintBackground))
		return; 
	Inherited::DrawBackground(inArea, inBackdrop);
} // CPrintHTMLView::DrawBackground

//-----------------------------------
void CPrintHTMLView::EraseBackground(
	int						inLocation,
	Int32					inX,
	Int32					inY,
	Uint32					inWidth,
	Uint32					inHeight,
	LO_Color*				inColor)
//-----------------------------------
{
	if (inColor != nil && !CPrefs::GetBoolean(CPrefs::PrintBackground))
		return;
	Inherited::EraseBackground(inLocation, inX, inY, inWidth, inHeight, inColor);
} // CPrintHTMLView::EraseBackground


//-----------------------------------
void CPrintHTMLView::CreateGridView(
	CBrowserContext*		inGridContext,
	Int32					inX,
	Int32 					inY,
	Int32					inWidth,
	Int32					inHeight,
	Int8					inScrollMode,
	Bool 					inNoEdge)
//-----------------------------------
{

	CHyperScroller* theContainer = NULL;

	try
	{
		SetScrollMode(LO_SCROLL_NO, false);
		mShowFocus = false;
		
		FocusDraw();
		LCommander::SetDefaultCommander( this );
		LPane::SetDefaultView( this );

		theContainer = (CHyperScroller*)UReanimator::ReadObjects('PPob', 1002);
		ThrowIfNULL_(theContainer);
		theContainer->FinishCreate();
		
		CPrintHTMLView* theNewView = (CPrintHTMLView*)theContainer->FindPaneByID(2005);
		ThrowIfNULL_(theNewView);
	
		// ¥ position it
		theNewView->mNoBorder = inNoEdge;
		theNewView->SetSuperHTMLView(this);

		Rect theOwningFrame;
		CalcLocalFrameRect(theOwningFrame);

		Rect theNewFrame;
		CropFrameToContainer(inX, inY, inWidth, inHeight, theNewFrame);

		CHyperScroller* theSuperScroller = mScroller;
		if (!theNewView->mNoBorder)
			{
			//	for each side that intersects the container, 
			//	expand the container out one
			if (inX == 0)
				if (theSuperScroller)
					theSuperScroller->ExpandLeft();
				
			if (inY == 0)
				if (theSuperScroller)
					theSuperScroller->ExpandTop();

			if (theNewFrame.right == theOwningFrame.right)
				if (theSuperScroller)
					theSuperScroller->ExpandRight();
				
			if (theNewFrame.bottom == theOwningFrame.bottom)
				if (theSuperScroller)
					theSuperScroller->ExpandBottom();
			
			if (theSuperScroller)
				theSuperScroller->AdjustHyperViewBounds();
			}
		else
			{
			theContainer->ExpandLeft();
			theContainer->ExpandTop();
			theContainer->ExpandRight();
			theContainer->ExpandBottom();
			theContainer->AdjustHyperViewBounds();
			}
									
		CropFrameToContainer(inX, inY, inWidth, inHeight, theNewFrame);

		theContainer->PlaceInSuperFrameAt(theNewFrame.left, theNewFrame.top, false);
		theContainer->ResizeFrameTo(RectWidth(theNewFrame), RectHeight(theNewFrame), false);

		if (theSuperScroller)
			theSuperScroller->AdjustHyperViewBounds();
		theContainer->AdjustHyperViewBounds();

		theNewView->SetScrollMode(inScrollMode);
		theNewView->mShowFocus = true;

		// ¥ so that we call scroller's activate, and it actually enables the scrollbars
//		theContainer->ActivateSelf();
// FIX ME!!! this used to be the line above.  I hope this still works
		theContainer->Activate();
		
		if (theSuperScroller)
			theSuperScroller->Refresh();

		theNewView->SetContext(inGridContext);
		mHasGridCells = true;
		
			// give the grid context the same listeners as the root context
		mContext->CopyListenersToContext(inGridContext);	
		}
	catch (...)
		{
		delete theContainer;
		throw;
		}

} // CPrintHTMLView::CreateGridView

#pragma mark --- CPrintContext ---
//======================================
class CPrintContext : public CBrowserContext
//======================================
{
	public:
		CPrintContext();
		operator MWContext*() { return &mContext; }
		operator MWContext&() { return mContext; }
}; // class CPrintContext

//-----------------------------------
CPrintContext::CPrintContext()
:	CBrowserContext(MWContextPrint)
//-----------------------------------
{
	mContext.fancyFTP = false;
	mContext.fancyNews = false;
	
	mContext.convertPixX = 1;
	mContext.convertPixY = 1;
	mContext.fe.view = nil;
//	mContext.fe.realContext = this;
	mContext.is_grid_cell = false;

	IL_DisplayData	data;
	data.dither_mode = IL_ClosestColor;
	IL_SetDisplayMode(mContext.img_cx, IL_DITHER_MODE, &data);	
}

#pragma mark --- CCustomPageSetup ---

#define NUM_POPUPS			6
#define NUM_CONTROLS		7
#define BACKGROUND_CHECKBOX	6	// member of fControls[] which is treated specially

//======================================
class CCustomPageSetup
//======================================
{
public:
	static void				InitCustomPageSetup();
	static void				OpenPageSetup( THPrint hPrint );
							
	static pascal TPPrDlg	CustomStyleDialogInit( THPrint hPrint );
	PROCDECL( static, CustomStyleDialogInit )

	static long				GetControlValue( long i );
protected:
	struct ControlStruct
	{
		ControlHandle		handle;
		CPrefs::PrefEnum	prefEnum;
	};
	
	static pascal void CustomStyleItems( DialogPtr xdialogPtr, short itemNo );
	PROCDECL( static, CustomStyleItems )

	static TPPrDlg			sPageSetupDialog;	// pointer to style dialog
	static PItemUPP			sItemProc;			// we need to store the old itemProc here
	static long				sFirstItem;			// save our first item here

	static ControlStruct	fControls[ NUM_CONTROLS ];
}; // class CCustomPageSetup

TPPrDlg CCustomPageSetup::sPageSetupDialog = nil;// pointer to style dialog
PItemUPP CCustomPageSetup::sItemProc = nil;		// we need to store the old itemProc here
long CCustomPageSetup::sFirstItem = nil;		// save our first item here
CCustomPageSetup::ControlStruct	CCustomPageSetup::fControls[ NUM_CONTROLS ];

//-----------------------------------
void CCustomPageSetup::InitCustomPageSetup()
//-----------------------------------
{
	fControls[ 0 ].prefEnum = CPrefs::TopLeftHeader;
	fControls[ 1 ].prefEnum = CPrefs::TopMidHeader;
	fControls[ 2 ].prefEnum = CPrefs::TopRightHeader;
	fControls[ 3 ].prefEnum = CPrefs::BottomLeftFooter;
	fControls[ 4 ].prefEnum = CPrefs::BottomMidFooter;
	fControls[ 5 ].prefEnum = CPrefs::BottomRightFooter;
	fControls[ BACKGROUND_CHECKBOX ].prefEnum = CPrefs::PrintBackground;
}

//-----------------------------------
void CCustomPageSetup::OpenPageSetup( THPrint hp )
//-----------------------------------
{
	sPageSetupDialog = ::PrStlInit( hp );
	
	if ( PrError() == noErr )
	{
		if ( ::PrDlgMain( hp, (PDlgInitUPP) &PROCPTR( CCustomPageSetup::CustomStyleDialogInit ) ) )
			;
	}
}

//-----------------------------------
long CCustomPageSetup::GetControlValue( long i )
//-----------------------------------
{
	if (i == BACKGROUND_CHECKBOX)
		return CPrefs::GetBoolean (fControls[BACKGROUND_CHECKBOX].prefEnum);
	return CPrefs::GetLong (fControls[i].prefEnum);
}

//-----------------------------------
pascal void CCustomPageSetup::CustomStyleItems( DialogPtr xdialogPtr, short itemNo )
//-----------------------------------
{
	TPPrDlg dialogPtr = (TPPrDlg)xdialogPtr;
	long value = 0;
	short myItem = itemNo - sFirstItem;	// "localize" current item No
	if ( myItem >= 0 ) 					// if localized item > 0, it's one of ours
	{
		if ( fControls[ myItem ].handle )
		{
			value = ::GetControlValue( fControls[ myItem ].handle );
			if ( myItem == BACKGROUND_CHECKBOX )
			{
				value = !value;
				::SetControlValue( fControls[ myItem ].handle, value );
				CPrefs::SetBoolean( value, fControls[ myItem ].prefEnum );
			}
			else
				CPrefs::SetLong( value, fControls[ myItem ].prefEnum );
		}
	}
	else 
		CallPItemProc( sItemProc, (DialogPtr)dialogPtr, itemNo );
}


PROCEDURE( CCustomPageSetup::CustomStyleItems, uppPItemProcInfo )

//-----------------------------------
pascal TPPrDlg CCustomPageSetup::CustomStyleDialogInit( THPrint /*hPrint*/ )
//	this routine appends items to the standard style dialog and sets up the
//	user fields of the printing dialog record TPRDlg 
//	This routine will be called by PrDlgMain
//-----------------------------------
{
	short			itemType = 0;		// needed for GetDialogItem/SetDItem call
	Handle			itemH = nil;
	Rect			itemBox;
	Handle			ditlList;
	
	// ¥Êappend the items from the resource
	ditlList = GetResource( 'DITL', 256 );
	sFirstItem = CountDITL( (DialogPtr)sPageSetupDialog ) + 1;
	AppendDITL( (DialogPtr)sPageSetupDialog, ditlList, appendDITLBottom );
	ReleaseResource( ditlList );

	// ¥Êinit all the control values	
	for ( long i = 0; i < NUM_CONTROLS; i++ )
	{
		::GetDialogItem( (DialogPtr) sPageSetupDialog, sFirstItem + i, &itemType, &itemH, &itemBox );
		fControls[ i ].handle = (ControlHandle)itemH;
		::SetControlValue( (ControlHandle)itemH, GetControlValue(i) );
	}
	
	// ¥ patch in our item handler
	//		sItemProc is the original item proc,
	//		pItemProc becomes our custom item proc
	sItemProc = sPageSetupDialog->pItemProc;
	sPageSetupDialog->pItemProc = &PROCPTR( CCustomPageSetup::CustomStyleItems );
	
	// ¥ÊPrDlgMain expects a pointer to the modified dialog to be returnedÉ
	return sPageSetupDialog;
} // CCustomPageSetup::CustomStyleDialogInit
PROCEDURE( CCustomPageSetup::CustomStyleDialogInit, uppPDlgInitProcInfo )

//======================================
#pragma mark --- CPrintHeaderCaption ---
//======================================

//======================================
class CPrintHeaderCaption: public LCaption
//======================================
{
public:
	enum			{ class_ID = 'HPPE' };
	
					CPrintHeaderCaption( LStream* inStream );
	virtual void	PrintPanelSelf( const PanelSpec& inPanel );
	
	static void		CreateString( long type, CStr255& string, const PanelSpec& inPanel );
	static void		SetPrintContext( MWContext* cntx )
					{
						sContext = cntx;
					}
	static void		SetTemporaryFile( Boolean b ) { sIsTemporaryFile = b; }
protected:
	static MWContext*		sContext;
	static Boolean			sIsTemporaryFile;
}; // class CPrintHeaderCaption

MWContext* CPrintHeaderCaption::sContext = nil;
Boolean	CPrintHeaderCaption::sIsTemporaryFile = false;

//-----------------------------------
CPrintHeaderCaption::CPrintHeaderCaption( LStream* inStream ): LCaption( inStream )
//-----------------------------------
{
}

//-----------------------------------
void CPrintHeaderCaption::CreateString( long form, CStr255& string, const PanelSpec& panel )
//-----------------------------------
{
	switch ( form )
	{
		case PAGE_NUMBER_FORM:
		{
			char buffer[ 30 ];
			CStr255 format = GetPString( PG_NUM_FORM_RESID );
			PR_snprintf( buffer, 30, format, panel.pageNumber );
			string = buffer;
		}
		break;
		
		case DATE_FORM:
			GetDateTimeString( string );
		break;
	
		case LOCATION_FORM:
			if ( sContext && !sIsTemporaryFile )
			{
				History_entry*		current;
				current = SHIST_GetCurrent( &sContext->hist );
				if ( current && current->address )
				{
					char *urlPath = NULL;
					// if the URL starts with ftp or http then we'll make an attempt to remove password
					if ( (XP_STRNCMP(current->address, "ftp", 3) == 0 || XP_STRNCMP(current->address, "http", 4) == 0 )
						&& NET_ParseUploadURL( current->address, &urlPath, NULL, NULL ) )
					{
						string = urlPath;
						if ( urlPath )
							XP_FREE( urlPath );
					}
					else // unknown URL; use it as is
						string = current->address;
				}
			}
		break;
		
		case TITLE_FORM:
			if ( sContext && !sIsTemporaryFile )
			{
				History_entry*		current;
				current = SHIST_GetCurrent( &sContext->hist );
				if ( current )
					string = current->title;
			}
		break;
	}
}

//-----------------------------------
void CPrintHeaderCaption::PrintPanelSelf( const PanelSpec& inPanel )
//-----------------------------------
{
	long value = CCustomPageSetup::GetControlValue( mUserCon );
	CStr255		string;
	CPrintHeaderCaption::CreateString( value, string, inPanel );
	
	unsigned short win_csid = INTL_DefaultWinCharSetID(sContext);
	if (value == TITLE_FORM && 
	   ((INTL_CharSetNameToID(INTL_ResourceCharSet()) & ~CS_AUTO ) 
	   		!= (win_csid & ~CS_AUTO)))	
	{
		// Save the origional font name and number
		TextTraitsH origTT = UTextTraits::LoadTextTraits(this->GetTextTraitsID());
		Int16 origFontNumber = (*origTT)->fontNumber;
		Str255 origFontName;
		::BlockMoveData(origFontName, (*origTT)->fontName, (*origTT)->fontName[0]+1);
		
		// SetHandleSize- the handle is compressed.
		::SetHandleSize((Handle)origTT, sizeof(TextTraitsRecord));	
		StHandleLocker lock((Handle) origTT );
		
		// Set the Font name and number in the TextTraits to the one in buttonF
		TextTraitsH csidTT
			= UTextTraits::LoadTextTraits(CPrefs::GetButtonFontTextResIDs(win_csid));

		(*origTT)->fontNumber 	= (*csidTT)->fontNumber;
		::BlockMoveData((*origTT)->fontName, (*csidTT)->fontName, (*csidTT)->fontName[0]+1);
		
		// Do the usual job
		this->SetDescriptor(string);
		this->DrawSelf();
		
		// Restore the font name, number to the origional TextTraits
		(*origTT)->fontNumber = origFontNumber;
		::BlockMoveData((*origTT)->fontName, origFontName, origFontName[0]+1);
	}
	else
	{
		this->SetDescriptor( string );
		this->DrawSelf();
	}	
} // CPrintHeaderCaption::PrintPanelSelf

//======================================
#pragma mark --- CHTMLPrintout ---
//======================================

//======================================
class CHTMLPrintout
	: public LPrintout
	, public LListener
//======================================
{
public:
	enum			{ class_ID = 'HPPT' };
	typedef LPrintout super;
	
					CHTMLPrintout( LStream* inStream );
					virtual ~CHTMLPrintout();
									
	virtual void	FinishCreateSelf();
	virtual void	ListenToMessage(
						MessageT				inMessage,
						void*					ioParam);
	
	virtual void	DoProgress( const char* message, int level );
	static void		BeginPrint( THPrint hp, CHTMLView* view );
	static CHTMLPrintout*	Get()	{ return sHTMLPrintout; }
	CPrintHTMLView*	GetPrintHTMLView() const { return mPrintHTMLView; }
	void			LoadPageForPrint(URL_Struct* url, 
							MWContext *printcontext, MWContext *viewContext);
	void			StashPrintRecord(THPrint inPrintRecordH);
	Boolean			PrintFinished(void)
						{ return mPrinted && LMGetTicks() >= mFinishedTime; }

protected:

	void			Paginate (void);
	void			CapturePositions (void);

	static CHTMLPrintout*		sHTMLPrintout;

	CPrintHTMLView*				mPrintHTMLView;
	CPrintContext*				mContext;
	Boolean						mFinishedLayout,
								mConnectionsCompleted,
								mPrinted;
	SInt32						mFinishedTime;
}; // class CHTMLPrintout

CHTMLPrintout* CHTMLPrintout::sHTMLPrintout = nil;

extern void BuildAWindowPalette( MWContext* context, WindowPtr window );

//	***** BEGIN HACK ***** (Bug #83149)
#if defined (JAVA)
static Boolean gAllowJavaAWTDrawingCalled_HACK = false;
static Boolean gCHTMLPrintoutContructed_HACK = false;

//	Do the wrong thing and declare this here instead of in a header file
extern "C" Boolean FE_AllowJavaAWTDrawing_HACK();

//	Disable all Java AWT drawing while the FE is printing
//	(This is actually called from LMacGraphics::BeginDrawing
//	in ns:sun-java:awt:macos:LMacGraphics.cp)

Boolean FE_AllowJavaAWTDrawing_HACK()
{
	if (gCHTMLPrintoutContructed_HACK)
	{
		gAllowJavaAWTDrawingCalled_HACK = true;
		return false;
	}
	return true;
}
#endif //JAVA
//	***** END HACK *****

//-----------------------------------
CHTMLPrintout::CHTMLPrintout( LStream* inStream )
//-----------------------------------
:	LPrintout( inStream )
,	mContext(nil)
,	mPrintHTMLView(nil)
,	mPrinted(false)
{
	sHTMLPrintout = this;
	
	//	***** BEGIN HACK ***** (Bug #83149)
#if defined (JAVA)
	gCHTMLPrintoutContructed_HACK = true;
#endif //JAVA
	//	***** END HACK *****
}

//	***** BEGIN HACK ***** (Bug #83149)
#if defined (JAVA)
static void RefreshJavaSubPanes(LArray& subPanes)
{
	LArrayIterator itererator(subPanes, LArrayIterator::from_Start);
	LPane* pane = NULL;
	
	while (itererator.Next(&pane))
	{
		if (pane->GetPaneID() == CJavaView::class_ID)
			pane->Refresh();
		else
		{
			LView *view = dynamic_cast<LView *>(pane);
			
			if (view)
				RefreshJavaSubPanes(view->GetSubPanes());
		}
	}
}
#endif //JAVA
//	***** END HACK *****

//-----------------------------------
CHTMLPrintout::~CHTMLPrintout()
//-----------------------------------
{
	if ( XP_IsContextBusy( *mContext ) )
		NET_InterruptWindow( *mContext );
	XP_CleanupPrintInfo(*mContext);
	DestroyImageContext( *mContext );
	mContext->RemoveListener(this);
	mContext->RemoveUser(this);
	sHTMLPrintout = nil;
	
	//	***** BEGIN HACK ***** (Bug #83149)
#if defined (JAVA)
	gCHTMLPrintoutContructed_HACK = false;
	
	if (gAllowJavaAWTDrawingCalled_HACK)
	{
		gAllowJavaAWTDrawingCalled_HACK = false;
		
		//	Update all the Java panes in all PowerPlant windows
		for (	WindowPtr macWindow = (WindowPtr)LMGetWindowList();
				macWindow;
				macWindow = (WindowPtr)((WindowPeek)macWindow)->nextWindow)
		{
			if (((WindowPeek)macWindow)->visible)
			{
				LWindow *ppWindow = LWindow::FetchWindowObject(macWindow);
				
				if (ppWindow)
					RefreshJavaSubPanes(ppWindow->GetSubPanes());
			}
		}
	}
#endif //JAVA
	//	***** END HACK *****
}

//-----------------------------------
void CHTMLPrintout::FinishCreateSelf()
//-----------------------------------
{
	LPrintout::FinishCreateSelf();
	mContext = new CPrintContext();
	ThrowIfNil_(mContext);
	mContext->AddUser(this);
	mContext->AddListener(this); // listen for msg_NSCAllConnectionsComplete
	
	CreateImageContext( *mContext );

	mPrintHTMLView = (CPrintHTMLView*)FindPaneByID('PtHt');
	mPrintHTMLView->SetContext(mContext);
}

// editor callback
void EditorPrintingCallback( char *completeURL, void *data )
{
	Try_
	{
		URL_Struct* url = NET_CreateURLStruct( completeURL, NET_DONT_RELOAD );
		ThrowIfNil_(url);
		
		CPrintHeaderCaption::SetTemporaryFile( true );
		CHTMLPrintout::Get()->LoadPageForPrint( url, (MWContext *)data, (MWContext *)data );
	}
	Catch_( err )
	{
		Throw_( err );
	}
	EndCatch_
	
}


//-----------------------------------
void CHTMLPrintout::StashPrintRecord (THPrint inPrintRecordH)
//-----------------------------------
{

/*   StashPrintRecord is based on LPrintout::SetPrintRecord.  You might think we'd override
   that method, but it isn't virtual.  So we changed the name to make it obvious that
   it's a different function, then just called it ourself after LPrintout is finished
   calling its version.
     I don't understand why the LPrintout version of this method is supposed to work.
   It sizes the LPrintout to match the size of the paper, rather than the page.  The
   comments in that method mumble something about the coordinate system working out
   better that way.  You can work around this by building into the PPob describing
   the LPrintout an offset which accounts for the unprintable margin, but that works
   only as long as the size of the printer margins is about the size built into the PPob.
   This breaks down terribly if you print at reduced size, which effectively increases
   the size of the printer margins.
     So I've changed the PPob to completely fill its parent view, and here I effectively
   change PowerPlant to use the page size.  As far as I can tell, this works in all
   cases.
*/

	// do what LPrintout::SetPrintRecord does, but size ourselves to the page,
	// rather than the paper
	UPrintingMgr::ValidatePrintRecord(inPrintRecordH);
	mPrintRecordH = inPrintRecordH;

	Rect	pageRect = (**inPrintRecordH).prInfo.rPage;

	ResizeFrameTo(pageRect.right - pageRect.left,
					pageRect.bottom - pageRect.top, false);
	ResizeImageTo(pageRect.right - pageRect.left,
					pageRect.bottom - pageRect.top, false);
	PlaceInSuperImageAt(pageRect.left, pageRect.top, false);
}

//-----------------------------------
void CHTMLPrintout::LoadPageForPrint(URL_Struct* url, MWContext *printcontext, MWContext *viewContext)
//-----------------------------------
{
	// SHIST_CreateURLStructFromHistoryEntry copies pointers but not
	// the saved data structures, so we must zero them out to prevent
	// double-deletions.  An alternative that would preserve saved data
	// for clients that find it useful (e.g. forms, embeds) would be to
	// deep copy the saved data objects.
	XP_MEMSET( &url->savedData, 0, sizeof( SHIST_SavedData ) );

	NPL_PreparePrint( viewContext, &( url->savedData ) );
	if ( printcontext->prSetup )
		printcontext->prSetup->url = url;
	url->position_tag = 0;
	if ( url->address )
		XP_STRTOK( url->address, "#" );

	CPrintHeaderCaption::SetPrintContext( viewContext );
	EarlManager::StartLoadURL( url, printcontext, FO_PRESENT );
	OutOfFocus( nil );
}


//-----------------------------------
void CHTMLPrintout::Paginate (void) {
//-----------------------------------

	mPrintHTMLView->InitializeCapture();
	CapturePositions();
	mPrintHTMLView->CalculatePageBreaks();
	mPrintHTMLView->SetPrintMode (CPrintHTMLView::epmDisplay);
}

//-----------------------------------
void CHTMLPrintout::CapturePositions (void) {
//-----------------------------------

	Rect			pageArea;
	short			pageWidth,
					pageHeight;
	CL_Compositor	*compositor = mPrintHTMLView->GetCompositor();

	mPrintHTMLView->CalcPortFrameRect (pageArea);
	pageHeight = pageArea.bottom - pageArea.top;
	pageWidth = pageArea.right - pageArea.left;

	//  For capturing, we want all the elements in a document to display.
	//  Since we can't size the compositor window to be the size of the
	//  document (the compositor window must have coordinates that fit into
	//  a 16-bit coordinate space), we take snapshots. In other words,
	//  we make the compositor window the size of the physical page and
	//  keep scrolling down till capture the entire document.

	if (compositor) {
		XP_Rect			drawingRect;
		SDimension32	documentSize;
		int				pass;
		int32			captureScrollOffset;

		// The compositor window is the size of the page (minus margins)
		CL_ResizeCompositorWindow (compositor, pageWidth, pageHeight);

		drawingRect.left = 0;
		drawingRect.top = 0;
		drawingRect.right = pageWidth;
		drawingRect.bottom = pageHeight;

		// We display all the elements twice. This is to deal with the
		// fact that certain elements (images, embeds and applets for
		// instance) are always displayed in a compositor pass after
		// the containing HTML content. We only capture during the
		// second pass.
		mPrintHTMLView->SetPrintMode (CPrintHTMLView::epmBlockDisplay);
		mPrintHTMLView->GetImageSize (documentSize);
		for (pass = 0; pass < 2; pass++) {
			// Till we've covered the entire document, we keep scrolling
			// down and taking snapshots
			for (captureScrollOffset = 0; 
			     captureScrollOffset <= documentSize.height; 
			     captureScrollOffset += pageHeight) {

				CL_ScrollCompositorWindow (compositor, 0, captureScrollOffset);
				CL_RefreshWindowRect (compositor, &drawingRect);
			}
			mPrintHTMLView->SetPrintMode (CPrintHTMLView::epmLayout);
		}

		CL_ScrollCompositorWindow (compositor, 0, 0);
	} else {
		mPrintHTMLView->SetPrintMode (CPrintHTMLView::epmLayout);
		LO_RefreshArea (*mContext, 0, 0, 0x7FFFFFFF, 0x7FFFFFFF);
	}
} // end CapturePositions


//-----------------------------------
void CHTMLPrintout::DoProgress( const char* /*message*/, int /*level*/ )
//-----------------------------------
{
}

#ifdef PRE_JRM
MWContext* CHTMLPrintout::CreateGridContext()
{
	MWContext* newContext = mContext->CreateGridContext();
	newContext->type = MWContextPrint;
	return newContext;
}
#endif

//-----------------------------------
void CHTMLPrintout::BeginPrint(THPrint hp, CHTMLView* inViewToPrint)
//-----------------------------------
{			
	if (!hp)
		return;
	if (!sHTMLPrintout)
		sHTMLPrintout = (CHTMLPrintout*)LPrintout::CreatePrintout(1001);
	ThrowIfNil_(sHTMLPrintout);
	CPrintHTMLView* printHTMLView = sHTMLPrintout->GetPrintHTMLView();
	ThrowIfNil_(printHTMLView);
	
	// ¥ set the printout (unlike PP, we have our own SAVED print record )
	sHTMLPrintout->StashPrintRecord(hp);

	// ¥ setup psfe's structs for measurements

	//
	// Keep a reference in the print context to the list of plugin instances
	// in the on-screen context.  This information is used in mplugin.cp when
	// printing plugins to avoid creating new instantiations of all the plugins.
	//
	((MWContext*)sHTMLPrintout->mContext)->pEmbeddedApp
		= ((MWContext*)inViewToPrint->GetContext())->pluginList;

	// ¥Êset the encoding type
#ifdef PRE_JRM
    sHTMLPrintout->fContext.fe.realContext->default_csid
		= inViewToPrint->fHContext->fe.realContext->default_csid;
#endif

	XP_InitializePrintInfo((MWContext*)*(sHTMLPrintout->mContext));

	sHTMLPrintout->mFinishedLayout = false;
	sHTMLPrintout->mConnectionsCompleted = false;
	sHTMLPrintout->mPrinted = false;

	// ¥ it's very important that mainLedge actually be enabled and
	//		not latent, otherwise controls that are printed do not
	//		behave properly
	printHTMLView->SetPrintMode (CPrintHTMLView::epmBlockDisplay);
	printHTMLView->Enable();
	printHTMLView->CopyCharacteristicsFrom(inViewToPrint);

	// ¥ go print
	Try_
	{
		URL_Struct *url;
		Boolean doSuppressURL = false;
		url = inViewToPrint->GetURLForPrinting( doSuppressURL, *(sHTMLPrintout->mContext) );
		if ( url )
		{
			CPrintHeaderCaption::SetTemporaryFile( doSuppressURL );
			sHTMLPrintout->LoadPageForPrint( url, ((MWContext*)*(sHTMLPrintout->mContext)), (MWContext*)*inViewToPrint->GetContext());
		}
	}
	Catch_( err )
	{
		Throw_( err );
	}
	EndCatch_
} // CHTMLPrintout::BeginPrint

//-----------------------------------
void CHTMLPrintout::ListenToMessage(
	MessageT				inMessage,
	void*					/*ioParam*/)
//-----------------------------------
{
	CHTMLView	*htmlView = ExtractHyperView (*mContext);

	switch (inMessage) {
		case msg_NSCFinishedLayout:
//			htmlView->NoteFinishedLayout();	(seems likely, but protected)
			mFinishedLayout = true;
			break;			
		case msg_NSCAllConnectionsComplete:
			// for frames, don't do print stuff until all contexts are done
//			htmlView->NoteAllConnectionsComplete();
			mConnectionsCompleted = !XP_IsContextBusy((MWContext*)*mContext);
			break;
	}
	// the check for mPrinted is necessary; JavaScript will call FinishedLayout twice
	if (mConnectionsCompleted && mFinishedLayout && !mPrinted) {
		// lay out for printing
		Paginate();
		DoPrintJob();

#if GENERATING68K
		//	***** HACK ***** Fixed weird crash where 68K printer code
		//	spammed us with an invalid port -- a vivid example of why
		//	one should not have an invalid port.
		WindowPtr front = FrontWindow();
		if (front)
			SetPort(front);
		else {
			CGrafPtr windowMgrPort;
			GetCWMgrPort(&windowMgrPort);
			SetPort((GrafPtr)windowMgrPort);
		}
#endif

		mPrinted = true;
	}
	mFinishedTime = LMGetTicks() + 600;	// wait 10 seconds for Java to finish
} // CHTMLPrintout::ListenToMessage

//======================================
#pragma mark --- CPrintingCleanupTimer ---
//======================================
/*	CPrintingCleanupTimer gives periodic attention to sHTMLPrintout, waiting for it
	to finish printing.  When sMTMLPrintout claims it's finished, we delete it.
	This is a (small) change in architecture: originally, sHTMLPrintout deleted itself
	as soon as it was finished printing.  I hacked in this timer thing to support
	printing pages with JavaScript.
	  There's no good way to tell whether JavaScript has finished laying out a page.
	Notably, a page containing JavaScript seems to always get two "finished layout"
	messages.  Before this timer thing was installed, the first such message caused
	the page to be printed and deleted.  The second such message brought the machine
	down.  Now we wait to delete the object until it stops getting messages from
	layout.  We define "stopped getting messages" as "haven't seen one in ten seconds."
	This sucks, and will cause your machine to crash if a big delay happens for some
	reason, but I'm led to believe JavaScript itself must be full of such things,
	and can't come up with a better plan today.
*/

//-----------------------------------
// class definition
class CPrintingCleanupTimer : public LPeriodical {

public:
					CPrintingCleanupTimer();
	virtual	void	SpendTime(const EventRecord &inMacEvent);
	static void		QueueCleanup(void);

private:
	static CPrintingCleanupTimer	sTheTimer;
};

//-----------------------------------
CPrintingCleanupTimer::CPrintingCleanupTimer()
// constructor: like the base class, but don't start repeating until told to
//-----------------------------------
{
	// when we're initialized at app load time, don't yet start asking for time
	StopRepeating();
}

//-----------------------------------
void CPrintingCleanupTimer::QueueCleanup(void)
// public access to turn on the timer; call when an sHTMLPrintout has been allocated
//-----------------------------------
{
	sTheTimer.StartRepeating();
}

//-----------------------------------
void CPrintingCleanupTimer::SpendTime(const EventRecord &)
// internal workings of the timer: wait until sHTMLPrintout says it's finished,
// then delete it and shut yourself down
//-----------------------------------
{
	CHTMLPrintout	*activePrintout = CHTMLPrintout::Get();

	if (activePrintout && activePrintout->PrintFinished()) {
		delete activePrintout;	// which sets sHTMLPrintout to 0 */
		StopRepeating();
	}
}

//-----------------------------------
CPrintingCleanupTimer CPrintingCleanupTimer::sTheTimer;

//======================================
#pragma mark --- UHTMLPrinting ---
//======================================

//-----------------------------------
void UHTMLPrinting::RegisterHTMLPrintClasses()
//-----------------------------------
{
//#define REGISTER_(letter,root) \
//	RegisterClass_(letter##root::class_ID, \
//		(ClassCreatorFunc)letter##root::Create##root##Stream);

#define REGISTER_(letter,root) RegisterClass_(letter##root);
	
#define REGISTERC(root) REGISTER_(C,root)
#define REGISTERL(root) REGISTER_(L,root)
	REGISTERC(HTMLPrintout)						// 97/01/24 jrm
	REGISTERC(PrintHTMLView)					// 97/01/24 jrm
	REGISTERC(PrintHeaderCaption)				// 97/01/24 jrm
} // UHTMLPrinting::RegisterHTMLPrintClasses

//-----------------------------------
void UHTMLPrinting::InitCustomPageSetup()
//-----------------------------------
{
	CCustomPageSetup::InitCustomPageSetup();
}

//-----------------------------------
void UHTMLPrinting::OpenPageSetup( THPrint hPrint )
//-----------------------------------
{
	CCustomPageSetup::OpenPageSetup(hPrint);
}

//-----------------------------------
void UHTMLPrinting::BeginPrint( THPrint hPrint, CHTMLView* inView )
//-----------------------------------
{
	CPrintingCleanupTimer::QueueCleanup();
	CHTMLPrintout::BeginPrint(hPrint, inView);
}