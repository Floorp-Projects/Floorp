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

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CHTMLView.h
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#pragma once

#include <LView.h>
#include <LListener.h>
#include <LDragAndDrop.h>
#include <LDragTask.h>
#include <LTabGroup.h>
#include <LPeriodical.h>
#include <URegions.h>
#include <LArray.h>

#include <string>
#include <vector.h>

#include "ntypes.h"
#include "structs.h"
#include "ctxtfunc.h"
#include "uprefd.h"
#include "layers.h"
#include "CDrawable.h"
#include "CBrowserDragTask.h"

#include "Events.h" // need for new fe_EventStruct - mjc

#include "net.h" // for FO_CACHE_AND_PRESENT

// The FE part of a cross-platform event. The event will get filtered
// through the compositor and will be dispatched on a per-layer basis.
/*
typedef struct fe_EventStruct {
	Point portPoint;		// The point (in port coordinates) associated with the event
	void *event;			// The specifics of the event - event dependent
} fe_EventStruct;
*/

// new typedef replaces void* with union for meaningful value on return from event dispatch.
typedef struct fe_EventStruct {
	Point portPoint;
	union event_union {
		SMouseDownEvent mouseDownEvent;
		EventRecord macEvent;
	} event;
} fe_EventStruct;

void SafeSetCursor( ResIDT inCursorID );
void FlushEventHierarchy(LView *javaAppletView);
void FlushEventHierarchyRecursive(LPane *currentPane);

class CHyperScroller;
class CHTMLClickRecord;
class CSharableCompositor;
class CSharedPatternWorld;
class CURLDispatchInfo;

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

class CHTMLView :
		public COnscreenDrawable,
		public LView,
		public LListener,
		public LDragAndDrop,
		public LTabGroup,
		public LPeriodical
{
	friend class CBrowserContext;
	friend class CPluginView;
	friend class CDragURLTask;

	public:
	
		enum { pane_ID = 'html', class_ID = 'HtVw' };
		
								CHTMLView(LStream* inStream);
		virtual					~CHTMLView();

		virtual void			SetContext(
									CBrowserContext*				inNewContext);

			// ACCESSORS

		CHyperScroller*			GetScroller() { return mScroller; }
		
		CBrowserContext*		GetContext() const { return mContext; }
		Boolean 				IsBorderless(void) const;
		Boolean 				IsRootHTMLView(void) const;
		Boolean 				IsFocusedFrame(void) const;
		
		CHTMLView* 				GetSuperHTMLView(void);
		void					SetSuperHTMLView(CHTMLView *inView);
		GrafPtr					GetCachedPort(void);

		void 					SetFormElemBaseModel(LModelObject* inModel);
		LModelObject* 			GetFormElemBaseModel(void);

		virtual void			ResetScrollMode(Boolean inRefresh = false); // resets to default, LO_SCROLL_AUTO
		// Add method to *REALLY* reset scroll mode to default scroll mode.
		void					ResetToDefaultScrollMode();
		
		virtual	void			SetScrollMode(
									Int8 					inScrollMode,
									Boolean					inRefresh = false);
		
		virtual	Boolean			GetScrollMode(void) const;

		void					SetDefaultScrollMode(Int8 inScrollMode) {mDefaultScrollMode = inScrollMode; }
		void					SetEraseBackground(Boolean inErase)	{mEraseBackground = inErase; }

		RGBColor				GetBackgroundColor() const { return mBackgroundColor; }

		virtual Int16			GetWinCSID(void) const;
		virtual Int16			DefaultCSIDForNewWindow(void);
		
		virtual Boolean			SetDefaultCSID(Int16);
		virtual void			SetFontInfo();

		void					SetWindowBackgroundColor();
		virtual void			GetDefaultFileNameForSaveAs(URL_Struct* url, CStr31& defaultName);
									// overridden by CMessageView to use subject.

		static void				CalcStandardSizeForWindowForScreen(
									CHTMLView*			inTopMostHTMLView,
									const LWindow&		inWindow,
									const Rect&			inScreenBounds,
									SDimension16&		outStandardSize);

		void					ClearInFocusCallAlready()
								{ mInFocusCallAlready = false; }

			// COMMANDER STUFF
						
		virtual Boolean			HandleKeyPress( const EventRecord& inKeyEvent );
			
		virtual void 			ShowView(LPane& pane);
			
			// PERIODICAL AND LISTENER STUFF

		virtual	void			SpendTime(const EventRecord& inMacEvent);

		virtual	void			ListenToMessage(
									MessageT				inMessage,
									void*					ioParam);
		
			// LAYER DISPATCH
									
		virtual	void			SetLayerOrigin(
									Int32 					inX,
									Int32 					inY);
									
		virtual	void			GetLayerOrigin(
									Int32*					outX,
									Int32*					outY);
									
		virtual void			SetLayerClip(
									FE_Region 				inRegion);

		virtual void			CopyPixels(
									CDrawable*				inSrcDrawable,
									FE_Region				inCopyRgn);

		PRBool 					HandleLayerEvent(
									CL_Layer*				inLayer,
									CL_Event*				inEvent);

		PRBool 					HandleEmbedEvent(	
									LO_EmbedStruct*			inEmbed, 
									CL_Event*				inEvent);

		void					SetCurrentDrawable(
									CDrawable*				inDrawable);
									
		CGrafPtr				GetCurrentPort(
									Point&					outPortOrigin);

			// DRAWING AND UPDATING

		virtual void			ScrollImageBy(
										Int32				inLeftDelta,
										Int32				inTopDelta,
										Boolean 			inRefresh);

		virtual void			AdaptToSuperFrameSize(
										Int32				inSurrWidthDelta,
										Int32				inSurrHeightDelta,
										Boolean				inRefresh);

		virtual void			ResizeFrameBy(
										Int16 				inWidthDelta,
										Int16 				inHeightDelta,
										Boolean 			inRefresh);

		virtual Boolean			FocusDraw(
										LPane* 				inSubPane = nil);

		virtual Boolean			EstablishPort();

		static	int				PrefUpdateCallback(
										const char			*inPrefString,
										void				*inCHTMLView);
		static	int				PrefInvalidateCachedPreference(
										const char			*inPrefString,
										void				*inCHTMLView);
										
		virtual void			DrawGridEdges(RgnHandle inUpdateRgn);

	protected:
		virtual void			BeTarget();
		void					SetRepaginate(Boolean inSetting);
		Boolean					GetRepaginate();

	protected:
		virtual Boolean IsGrowCachingEnabled() const;

	public:

			// MOUSING
		virtual void			AdjustCursorSelf(
										Point				inPortPt,
										const EventRecord&	inMacEvent );
		void 					AdjustCursorSelfForLayer(
										Point				inPortPt,
										const EventRecord&	inMacEvent,
										CL_Layer			*layer,
										SPoint32			inLayerPt );
		
		virtual void			ImageToAvailScreenPoint(const SPoint32 &inImagePoint, Point &outPoint) const;
			// TIMER URL
		
		virtual void			SetTimerURL(Uint32 inSeconds, const char* inURL);
		virtual void			ClearTimerURL(void);

		inline CCharSet			GetCharSet()	{ return mCharSet; }

			// FIND SUPPORT
		
		virtual void			CreateFindWindow();
		virtual Boolean			DoFind();
		
			// MOUSING AND KEYING

		virtual void 			PostProcessClickSelfLink(
									const SMouseDownEvent&	inMouseDown,
									CHTMLClickRecord&		inClickRecord,
									Boolean					inMakeNewWindow,
									Boolean					inSaveToDisk,
									Boolean					inDelay);

		static void 			SetLastFormKeyPressDispatchTime(UInt32 inTime) { sLastFormKeyPressDispatchTime = inTime; }
		
		virtual void			HandleImageIconClick(CHTMLClickRecord& inClickRecord);

		
	protected:


		virtual void			DrawSelf(void);
		virtual	void			DrawFrameFocus(void);
		virtual	void			CalcFrameFocusMask(RgnHandle outFocusMask);
		virtual	void			InvalFocusArea(void);
		virtual	void 			AdjustScrollBars();

		virtual void			FinishCreateSelf(void);
		virtual	void			EnableSelf(void);
		virtual	void			DisableSelf(void);

			// COMMANDER ISSUES

		virtual void			PutOnDuty(LCommander*);
		virtual void			TakeOffDuty(void);
				void			RegisterCallBackCalls();
				void			UnregisterCallBackCalls();
		
	public:
	
		virtual void			FindCommandStatus(CommandT inCommand,
													Boolean	&outEnabled,
													Boolean	&outUsesMark,
													Char16	&outMark,
													Str255	outName);
		virtual Boolean			ObeyCommand(CommandT inCommand, void* ioParam);
		virtual URL_Struct*		GetURLForPrinting(Boolean& outSuppressURLCaption, 
													MWContext *printingContext);
		
	protected:
	
			// URL DISPATCHING
		virtual void			DispatchURL(
									URL_Struct* inURLStruct,
									CNSContext* inTargetContext,
									Boolean inDelay = false,
									Boolean	inForceCreate = false,
									FO_Present_Types inOutputFormat = FO_CACHE_AND_PRESENT
									);
	
		virtual void			DispatchURL(CURLDispatchInfo* inDispatchInfo);

		Boolean					CanPrint() const;
		void					DoPrintCommand(CommandT);
		
#if 0
			// CONTEXTUAL POPUP
// CContextMenuAttachment will make this obsolete when you decide to use it.
		virtual short			DoPopup(const SMouseDownEvent& event, CHTMLClickRecord& cr);
// CContextMenuAttachment will make this obsolete when you decide to use it.
		virtual void			HandlePopupResult(const SMouseDownEvent& where,
													CHTMLClickRecord& cr,
													short result);
#endif				
			// MOUSING AND KEYING
			
		virtual void			Click(SMouseDownEvent &inMouseDown);
		
		virtual	void			ClickSelf(const SMouseDownEvent& inMouseDown);

		virtual	void			ClickSelfLayer(
									const SMouseDownEvent&	inMouseDown,
									CL_Layer*				inLayer,
									SPoint32 				inLayerWhere);
		
		virtual	void			ClickSelfLink(
									const SMouseDownEvent&	inMouseDown,
									CHTMLClickRecord&		inClickRecord,
									Boolean					inMakeNewWindow);
									
		virtual	void			ClickDragLink(
									const SMouseDownEvent& 	inMouseDown,
									LO_Element* 			inElement);
		
		virtual void			ClickTrackEdge(
									const SMouseDownEvent&	inMouseDown,
									CHTMLClickRecord&		inClickRecord);
									
		virtual	Boolean			ClickTrackSelection(
									const SMouseDownEvent&	inMouseDown,
									CHTMLClickRecord&		inClickRecord);
	
		virtual void			EventMouseUp(const EventRecord& inMouseUp);
		
		virtual Boolean			HandleKeyPressLayer( const EventRecord& inKeyEvent, CL_Layer* inLayer, SPoint32 inLayerWhere );
		
			// Drag and Drop support
		
		virtual	void 			DoDragSendData(
									FlavorType				inFlavor,
									ItemReference			inItemRef,
									DragReference			inDragRef);
				
			// NOTIFICATION RESPONSE

		virtual	void 			NoteFinishedLayout(void);
		virtual void			NoteAllConnectionsComplete(void);
		virtual	void			NoteStartRepagination(void);
		virtual	void			NoteEmptyRepagination(void);
		virtual	void			NoteConfirmLoadNewURL(Boolean& ioCanLoad);
		virtual	void			NoteStartLoadURL(void);
		virtual	void			NoteGridContextPreDispose(Boolean inSavingHistory);
		virtual	void			NoteGridContextDisposed(void);
		
			// DEFERRED LOADING
	
		virtual	void			PostDeferredImage(const char* inImageURL);
		virtual	Boolean			IsImageInDeferredQueue(const char* inImageURL) const;
		virtual	void			ClearDeferredImageQueue(void);
		
			// CONTEXT DISPATCH

		virtual void 			LayoutNewDocument(
									URL_Struct*				inURL,
									Int32*					inWidth,
									Int32*					inHeight,
									Int32*					inMarginWidth,
									Int32*					inMarginHeight);

		virtual	void			ClearView(
									int 					inWhich = 0);
	
		virtual	void 			ClearBackground(void);

		virtual	void 			DrawBackground(
									const Rect& 			inArea,
									LO_ImageStruct*			inBackdrop = NULL);
									
		virtual	void 			DrawBackgroundSelf(
									const Rect& 			inArea,
									LO_ImageStruct*			inBackdrop);

		virtual void 			EraseBackground(
									int						inLocation,
									Int32					inX,
									Int32					inY,
									Uint32					inWidth,
									Uint32					inHeight,
									LO_Color*				inColor);

		virtual int 			SetColormap(
									IL_IRGB*				inMap,
									int						inRequested);

		virtual void 			SetBackgroundColor(
									Uint8 					inRed,
									Uint8					inGreen,
									Uint8 					inBlue);

		virtual	void			SetBackgroundImage(
									LO_ImageStruct* 		inImageStruct,
									Boolean 				inRefresh = true);


		virtual	void			CalcPluginMask(RgnHandle ioRgn);

//-----------------------

	public:
		// needs to be public for FE_ScrollTo and FE_ScrollBy
		// added default Boolean arg to scroll even if position is visible - mjc 97-9-12
		virtual void 			SetDocPosition(
									int 					inLocation,
									Int32 					inX,
									Int32 					inY,
									Boolean					inScrollEvenIfVisible = false);

	protected:
	
		virtual void 			SetDocDimension(
									int 					inLocation,
									Int32 					inWidth,
									Int32 					inHeight);

#if 0
		virtual void 			SetDocDimensionSelf(
									Int32 					inWidth,
									Int32 					inHeight);
#endif

		virtual void 			GetDocPosition(
									int 					inLocation,
									Int32*					outX,
									Int32*					outY);
		
		virtual	void 			FlushPendingDocResize(void);

//------------------------									

		virtual int				GetTextInfo(
									LO_TextStruct*			inText,
									LO_TextInfo*			outTextInfo);

		virtual int				MeasureText(
									LO_TextStruct*			inText,
									short*					outCharLocs);
			
		virtual void			GetTextFrame(
									LO_TextStruct*			inTextStruct,
									Int32					inStartPos,
									Int32					inEndPos,
									XP_Rect*				outFrame);

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

//-----------------------
			
	protected:
	
		virtual void 			GetEmbedSize(
									LO_EmbedStruct*			inEmbedStruct,
									NET_ReloadMethod		inReloadMethod);
									
		virtual void			FreeEmbedElement(
									LO_EmbedStruct*			inEmbedStruct);

		virtual void 			CreateEmbedWindow(
									NPEmbeddedApp*			inEmbeddedApp);

		virtual void 			SaveEmbedWindow(
									NPEmbeddedApp*			inEmbeddedApp);

		virtual void 			RestoreEmbedWindow(
									NPEmbeddedApp*			inEmbeddedApp);

		virtual void 			DestroyEmbedWindow(
									NPEmbeddedApp*			inEmbeddedApp);

		virtual void 			DisplayEmbed(
									int 					inLocation,
									LO_EmbedStruct*			inEmbedStruct);

//-----------------------
			
	protected:
	
		virtual void 			GetJavaAppSize(
									LO_JavaAppStruct*		inJavaAppStruct,
									NET_ReloadMethod		inReloadMethod);

		virtual void 			FreeJavaAppElement(
									LJAppletData*			inAppletData);

		virtual void 			HideJavaAppElement(
									LJAppletData*			inAppletData);
									
		virtual void 			DisplayJavaApp(
									int 					inLocation,
									LO_JavaAppStruct*		inJavaAppStruct);

		virtual	void 			DrawJavaApp(
									int 					inLocation,
									LO_JavaAppStruct*		inJavaAppStruct);
											
		virtual	void 			HandleClippingView(
									struct LJAppletData 	*appletD, 
									int 					x, 
									int 					y, 
									int 					width, 
									int 					height);
//-----------------------

	protected:
	
		virtual void 			GetFormElementInfo(
									LO_FormElementStruct* 	inElement);

/*
	nothing view-specific about these routines, so the BrowserContext just calls thru
	to the status functions in UFormElementFactory.
	deeje 97-02-13
	
		virtual void 			GetFormElementValue(
									LO_FormElementStruct* 	inElement,
									XP_Bool 				inHide);

		virtual void 			ResetFormElement(
									LO_FormElementStruct* 	inElement);

		virtual void 			SetFormElementToggle(
									LO_FormElementStruct* 	inElement,
									XP_Bool 				inToggle);

		virtual void 			FormTextIsSubmit(
									LO_FormElementStruct* 	inElement);
*/

		virtual	void 			ResetFormElementData(
									LO_FormElementStruct*	inElement,
									Boolean 				inRefresh,
									Boolean					inFromDefaults);

		virtual void 			DisplayFormElement(
									int 					inLocation,
									LO_FormElementStruct* 	inFormElement);

		virtual void 			DisplayBorder(
									int 					inLocation,
									int						inX,
									int						inY,
									int						inWidth,
									int						inHeight,
									int						inBW,
									LO_Color*	 			inColor,
									LO_LineStyle			inStyle);

		virtual void			UpdateEnableStates();
		
		virtual void 			DisplayFeedback(
									int 					inLocation,
									LO_Element*				inElement);

//-----------------------
			
	protected:
	
		virtual void 			FreeEdgeElement(
									LO_EdgeStruct*			inEdgeStruct);

		virtual void 			DisplayEdge(
									int 					inLocation,
									LO_EdgeStruct*			inEdgeStruct);

//-----------------------

	protected:
	
		virtual void 			DisplayTable(
									int 					inLocation,
									LO_TableStruct*			inTableStruct);

		virtual void 			DisplayCell(
									int 					inLocation,
									LO_CellStruct*			inCellStruct);

		virtual	void 			InvalidateEntireTableOrCell(
									LO_Element*				inElement);

		virtual	void 			DisplayAddRowOrColBorder(
									XP_Rect*				inRect,
									XP_Bool					inDoErase);


		virtual	Boolean			CalcElementPosition(
									LO_Element* 			inElement,
									Rect& 					outFrame);

		virtual void			CalcAbsoluteElementPosition(
									LO_Element*				inElement,
									XP_Rect&				outFrame);

		// Support for style sheet borders
		virtual void			DisplaySolidBorder(
									const Rect&				inFrame,
									const RGBColor&			inBorderColor,
									Int32					inTopWidth,
									Int32					inLeftWidth,
									Int32					inBottomWidth,
									Int32					inRightWidth);

		virtual void			DisplayBevelBorder(
									const Rect&				inFrame,
									const RGBColor&			inBorderColor,
									Boolean					inRaised,
									Int32					inTopWidth,
									Int32					inLeftWidth,
									Int32					inBottomWidth,
									Int32					inRightWidth);

		virtual void			DisplayGrooveRidgeBorder(
									const Rect&				inFrame,
									const RGBColor&			inBorderColor,
									Boolean					inIsGroove,
									Int32					inTopWidth,
									Int32					inLeftWidth,
									Int32					inBottomWidth,
									Int32					inRightWidth);
//-----------------------

	protected:
	
		virtual	void 			CreateGridView(
									CBrowserContext*		inGridContext,
									Int32					inX,
									Int32 					inY,
									Int32					inWidth,
									Int32					inHeight,
									Int8					inScrollMode,
									Bool 					inNoEdge);

		virtual	void 			CropFrameToContainer(
									Int32					inImageLeft,
									Int32					inImageTop,
									Int32					inImageWidth,
									Int32					inImageHeight,
									Rect&					outLocalFrame) const;

		virtual	void			RestructureGridView(
									Int32					inX,
									Int32 					inY,
									Int32					inWidth,
									Int32					inHeight);	

		virtual	void			GetFullGridSize(
									Int32&					outWidth,
									Int32&					outHeight);
	
//-----------------------

	protected:
	
		virtual void			InstallBackgroundColor(void);
									// Sets mBackgroundColor. Called from ClearBackground().
									// The base class implementation uses the text background
									// preference, but derived classes can override this.

		virtual void			GetDefaultBackgroundColor(LO_Color* outColor) const;
									// Called by layout before setting the background color
									// of a context.  The view can leave it alone (which will
									// use the global default background color) or override it.

		virtual void			ResetBackgroundColor() const;
									// Calls RGBBackColor(mBackgroundColor).  Printview overrides.

		Boolean					ContextMenuPopupsEnabled (void);
									
		inline void				LocalToLayerCoordinates(
									const XP_Rect&			inBoundingBox,
									Point					inWhereLocal,
									SPoint32&				outWhereLayer) const;

//	char *			fURLTimer;		// URL to load when timer goes off
//	UInt32			fURLFireTime;	// When this url should be fired up


//-----------------------
//	Data

	protected:
	
		CBrowserContext*		mContext;

		RGBColor				mBackgroundColor;
		LO_ImageStruct* 		mBackgroundImage;
		Boolean					mEraseBackground;
		
		Point					mOriginalOrigin;

	private:
		CHTMLView*				mSuperHTMLView;

	protected:

		Boolean					mNoBorder;
		Boolean					mHasGridCells;
		Boolean					mShowFocus;
		GrafPtr					mCachedPort;
	
		string					mTimerURL;
		vector<string>			mImageQueue;

		LModelObject* 			mElemBaseModel;
		
		Int8					mScrollMode;	// LO_SCROLL_YES, LO_SCROLL_NO, and LO_SCROLL_AUTO
		Int8					mDefaultScrollMode;	// same
		CCharSet				mCharSet;

		SDimension32			mPendingDocDimension;
		Boolean						mPendingDocDimension_IsValid;

		StRegion				mSaveLayerClip;
		RgnHandle				mLayerClip;
		SPoint32				mLayerOrigin;
		CSharableCompositor*	mCompositor;

		DragSendDataUPP 		mSendDataUPP;

		LO_Element*				mDragElement;
		
		Uint32					mTimerURLFireTime;
		char*					mTimerURLString;

		CRouterDrawable*		mOnscreenDrawable;
		COffscreenDrawable*		mOffscreenDrawable;
		CDrawable*				mCurrentDrawable;

		CSharedPatternWorld* 	mPatternWorld;
		Boolean					mNeedToRepaginate;
		
		Point					mOldPoint;	// Last place cursor was adjusted. No initializing
		long					mOldEleID;	// Last anchor text block whose URL we displayed. No initializing

		// 97-06-11 pkc -- Add vector to keep track of grid edges and flag that tells us
		// not to add LO_EdgeStruct* to vector because it's already in vector
		vector<LO_EdgeStruct*>	mGridEdgeList;
		Boolean					mDontAddGridEdgeToList;

		Boolean					mLoadingURL;
		Boolean					mStopEnablerHackExecuted;
		
		Boolean					mInFocusCallAlready;
		static Boolean			sCachedAlwaysLoadImages; // Caches general.always_load_images
		
// FIX ME!!! the following things need to be removed in another pass

		CHyperScroller*			mScroller;
		// Set this when we dispatch a key event to a form, to prevent an infinite loop
		// (otherwise, if the form doesn't handle the key, the event will get passed up
		// the chain, and the html view will dispatch the same event to layers).
		static UInt32			sLastFormKeyPressDispatchTime;
		// Set when a mouse down is dispatched, so the view's mouse up handlers don't
		// otherwise get called.
		Boolean					mWaitMouseUp;
		CHTMLClickRecord*		mCurrentClickRecord;
			// So that FindCommandStatus can use it for disabling context menu items.
			// This will be non-null only when testing or executing context menu commands.
}; // class CHTMLView


inline GrafPtr CHTMLView::GetCachedPort(void)
	{	return  mCachedPort;		}
	
	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

class CDragURLTask : public CBrowserDragTask
{
public:
	typedef CBrowserDragTask super;
	
					CDragURLTask(
									const EventRecord&	inEventRecord,
									const Rect& 		inGlobalFrame,
									CHTMLView& 			inHTMLView);

	virtual void	AddFlavors(
									DragReference		inDragRef);
	virtual void	MakeDragRegion(
									DragReference		inDragRef,
									RgnHandle			inDragRegion);
									
protected:
	Rect			mGlobalFrame;
	CHTMLView&		mHTMLView;
};



