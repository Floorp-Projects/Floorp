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

// CBrowserContext.h

#pragma once


#include <LBroadcaster.h>
#include <LSharable.h>

#include "CNSContext.h"

#include "structs.h"
#include "ctxtfunc.h"
#include "cstring.h"

class CHTMLView;
class CSharableCompositor;

const MessageT msg_SecurityState = 'SECS';	// ESecurityState

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

class CBrowserContext : public CNSContext
{
	friend class CNSContextCallbacks;
	friend class CPlainTextConversionContext;
	
	public:
		// history navigation
		// these are special indices for LoadHistoryEntry
		enum {
			index_Reload		=	-2,
			index_GoBack		=	-1,
			index_GoForward		= 	0
		};
			
								CBrowserContext();
								CBrowserContext(MWContextType inType);
								CBrowserContext(const CBrowserContext& inOriginal);
								
		virtual 				~CBrowserContext();

		virtual void				NoMoreUsers(void);

		operator MWContext*();
		operator MWContext&();

		virtual	void				SetCurrentView(CHTMLView* inView);

		virtual	CBrowserContext*	GetTopContext();
		
		virtual	Boolean				HasColorSpace(void) const;
		virtual	Boolean				HasGridParent(void) const;
		virtual	Boolean				HasFullPagePlugin(void) const;
		
		virtual	void				SetLoadImagesOverride(Boolean inOverride);
		virtual Boolean				IsLoadImagesOverride(void) const;

		virtual void				SetDelayImages(Boolean inDelay);
		virtual	Boolean				IsImageLoadingDelayed(void) const;

		virtual Boolean				IsRestrictedTarget(void) const;
		virtual void				SetRestrictedTarget(Boolean inIsRestricted);
		
		virtual Boolean				IsRootDocInfoContext();
		virtual Boolean				IsViewSourceContext();
		
		virtual Boolean				IsSpecialBrowserContext();
		
		virtual Boolean				SupportsPageServices();
		
		void						SetCloseCallback ( void (* close_callback)(void *close_arg), void* close_arg );

// FIX ME!!! ACCESSOR for unique ID

			// LAYERS / COMPOSITOR

		virtual Boolean				HasCompositor(void) const;
	
		virtual CL_Compositor*		GetCompositor(void) const;
	
		virtual	void				SetCompositor(
											CSharableCompositor*	inCompositor);

		virtual	PRBool				HandleLayerEvent(	
											CL_Layer*				inLayer, 
											CL_Event*				inEvent);
											
		virtual PRBool				HandleEmbedEvent(	
											LO_EmbedStruct*			inEmbed, 
											CL_Event*				inEvent);
														

			// HISTORY
		virtual	void				RememberHistoryPosition(
											Int32					inX,
											Int32					inY);
	
		virtual void 				InitHistoryFromContext( CBrowserContext *parentContext);


			// Image Observer
			
		virtual Boolean				IsContextLooping();
		Boolean						IsMochaLoadingImages() { return mMochaImagesLoading; }

		void						SetImagesLoading(Boolean inValue);
		void						SetImagesLooping(Boolean inValue);
	    void						SetImagesDelayed(Boolean inValue);
		void						SetMochaImagesLoading(Boolean inValue);
		void						SetMochaImagesLooping(Boolean inValue);
	    void						SetMochaImagesDelayed(Boolean inValue);

	protected:
		// we don't need to expose these
		virtual History_entry*		GetNextHistoryEntry(void);
		virtual History_entry*		GetPreviousHistoryEntry(void);
		virtual Boolean				IsContextLoopingRecurse();
	
	public:
		virtual Boolean				CanGoForward(void);
		virtual Boolean				CanGoBack(void);

		virtual Boolean				HasGridChildren(void);
		virtual Boolean				IsGridChild(void);
		virtual Boolean				IsGridCell();

		virtual void				GoForwardOneHost();
		virtual void				GoBackOneHost();

		virtual void				GoForward(void);
		virtual void				GoBack(void);
		virtual void				LoadHistoryEntry(	// one-based
											Int32 					inIndex,
											Boolean					inSuperReload = false);
		virtual Boolean				GoForwardInGrid(void);
		virtual Boolean				GoBackInGrid(void);		
		
/*
			// URL MANIPULATION
		virtual	cstring				GetCurrentURL(void);
		
		virtual	void				SwitchLoadURL(
											URL_Struct*				inURL,
											FO_Present_Types		inOutputFormat);
											
		virtual	void				ImmediateLoadURL(
											URL_Struct*				inURL,
											FO_Present_Types		inOutputFormat);
*/
			// REPAGINTAION

		virtual	void 				Repaginate(NET_ReloadMethod repage = NET_RESIZE_RELOAD);
		virtual	Boolean				IsRepaginating(void) const;
		virtual	Boolean				IsRepagintaitonPending(void) const;

			// FRAME MANAGEMENT

		virtual	MWContext* 			CreateGridContext(
											void* 					inHistList,
											void* 					inHistEntry,
											Int32					inX,
											Int32 					inY,
											Int32					inWidth,
											Int32					inHeight,
											char* 					inURLString,
											char* 					inWindowTarget,
											Int8					inScrollMode,
											NET_ReloadMethod 		inForceReload,
											Bool 					inNoEdge);
											
		virtual void*				DisposeGridContext(
											XP_Bool					inSaveHistory);
											
		virtual	void				DisposeGridChild(
											CBrowserContext*				inChildContext);
											
		virtual	void				RestructureGridContext(
											Int32					inX,
											Int32 					inY,
											Int32					inWidth,
											Int32					inHeight);

		virtual	void				GetFullGridSize(
											Int32&					outWidth,
											Int32&					outHeight);

		virtual	void				ReloadGridFromHistory(
											void* 					inHistEntry,
											NET_ReloadMethod 		inReload);


		virtual	Int32				CountGridChildren(void) const;

		// save dialog for editor--Paul will fix some time
		virtual CSaveProgress*		GetSaveDialog() { return fSaveDialog; };
		virtual void				SetSaveDialog( CSaveProgress* theDialog ) { fSaveDialog = theDialog; };

		// override for JavaScript foolishness
		virtual	void 				Alert(
											const char*				inAlertText);

		virtual	XP_Bool				Confirm(
											const char* 			inMessage);
	protected:
		CSaveProgress*				fSaveDialog;

		void						ConstructJSDialogTitle(LStr255& outTitle);


			// CALLBACK IMPLEMENTATION

// FIX ME!!! this needs to become an apple event
//		virtual	MWContext* 			CreateNewDocWindow(
//											URL_Struct*				inURL);

		virtual	void 				LayoutNewDocument(
											URL_Struct*				inURL,
											Int32*					inWidth,
											Int32*					inHeight,
											Int32*					inMarginWidth,
											Int32*					inMarginHeight);

		virtual	void 				SetDocTitle(
											char* 					inTitle);
											
		virtual	void 				FinishedLayout(void);
											
		virtual	int 				GetTextInfo(
											LO_TextStruct*			inText,
											LO_TextInfo*			inTextInfo);
											
		virtual	int 				MeasureText(
											LO_TextStruct*			inText,
											short*					outCharLocs);
											
		virtual	void 				GetEmbedSize(
											LO_EmbedStruct*			inEmbedStruct,
											NET_ReloadMethod		inReloadMethod);
											
		virtual	void 				GetJavaAppSize(
											LO_JavaAppStruct*		inJavaAppStruct,
											NET_ReloadMethod		inReloadMethod);
											
		virtual	void 				GetFormElementInfo(
											LO_FormElementStruct* 	inElement);

		virtual	void 				GetFormElementValue(
											LO_FormElementStruct* 	inElement,
											XP_Bool 				inHide);

		virtual	void 				ResetFormElement(
											LO_FormElementStruct* 	inElement);

		virtual	void 				SetFormElementToggle(
											LO_FormElementStruct* 	inElement,
											XP_Bool 				inToggle);

		virtual	void 				FreeEmbedElement(
											LO_EmbedStruct*			inEmbedStruct);

		virtual	void 				CreateEmbedWindow(
											NPEmbeddedApp*			inEmbeddedApp);

		virtual	void 				SaveEmbedWindow(
											NPEmbeddedApp*			inEmbeddedApp);

		virtual	void 				RestoreEmbedWindow(
											NPEmbeddedApp*			inEmbeddedApp);

		virtual	void 				DestroyEmbedWindow(
											NPEmbeddedApp*			inEmbeddedApp);

		virtual	void 				FreeJavaAppElement(
											LJAppletData*			inAppletData);

		virtual	void 				HideJavaAppElement(
											LJAppletData*				inAppletData);

		virtual	void 				FreeEdgeElement(
											LO_EdgeStruct*			inEdgeStruct);

		virtual	void 				FormTextIsSubmit(
											LO_FormElementStruct* 	inElement);

		virtual	void 				DisplaySubtext(
											int 					inLocation,
											LO_TextStruct*			inText,
											Int32 					inStartPos,
											Int32					inEndPos,
											XP_Bool 				inNeedBG);

		virtual	void 				DisplayText(
											int 					inLocation,
											LO_TextStruct*			inText,
											XP_Bool 				inNeedBG);

		virtual	void 				DisplayEmbed(
											int 					inLocation,
											LO_EmbedStruct*			inEmbedStruct);

		virtual	void 				DisplayJavaApp(
											int 					inLocation,
											LO_JavaAppStruct*		inJavaAppStruct);

		virtual	void 				DisplayEdge (
											int 					inLocation,
											LO_EdgeStruct*			inEdgeStruct);

		virtual	void 				DisplayTable(
											int 					inLocation,
											LO_TableStruct*			inTableStruct);

		virtual	void 				DisplayCell(
											int 					inLocation,
											LO_CellStruct*			inCellStruct);

		virtual	void 				InvalidateEntireTableOrCell(
											LO_Element*				inElement);

		virtual	void 				DisplayAddRowOrColBorder(
											XP_Rect*				inRect,
											XP_Bool					inDoErase);

		virtual	void 				DisplaySubDoc(
											int 					inLocation,
											LO_SubDocStruct*		inSubdocStruct);

		virtual	void 				DisplayLineFeed(
											int 					inLocation,
											LO_LinefeedStruct*		inLinefeedStruct,
											XP_Bool 				inNeedBG);

		virtual	void 				DisplayHR(
											int 					inLocation,
											LO_HorizRuleStruct*		inRuleStruct);

		virtual	void 				DisplayBullet(
											int 					inLocation,
											LO_BullettStruct*		inBullettStruct);

		virtual	void 				DisplayFormElement(
											int 					inLocation,
											LO_FormElementStruct* 	inFormElement);

		virtual	void 				DisplayBorder(
											int 					inLocation,
											int						inX,
											int						inY,
											int						inWidth,
											int						inHeight,
											int						inBW,
											LO_Color*	 			inColor,
											LO_LineStyle			inStyle);

		virtual void				UpdateEnableStates();
		
		virtual void				DisplayFeedback(
											int 					inLocation,
											LO_Element_struct		*inElement);

		virtual	void 				ClearView(
											int 					inWhich);

		virtual	void 				SetDocDimension(
											int 					inLocation,
											Int32 					inWidth,
											Int32 					inLength);

		virtual	void 				SetDocPosition(
											int 					inLocation,
											Int32 					inX,
											Int32 					inY);

		virtual	void 				GetDocPosition(
											int 					inLocation,
											Int32*					outX,
											Int32*					outY);

		virtual	void 				SetBackgroundColor(
											Uint8 					inRed,
											Uint8					inGreen,
											Uint8 					inBlue);

		virtual	void 				AllConnectionsComplete(void);

		virtual	void 				EraseBackground(
											int						inLocation,
											Int32					inX,
											Int32					inY,
											Uint32					inWidth,
											Uint32					inHieght,
											LO_Color*				inColor);

		virtual	void 				SetDrawable(
											CL_Drawable*			inDrawable);

		virtual	void 				GetTextFrame(
											LO_TextStruct*			inTextStruct,
											Int32					inStartPos,
											Int32					inEndPos,
											XP_Rect*				outFrame);

		virtual	void 				GetDefaultBackgroundColor(
											LO_Color*				outColor) const;

		virtual	void 				DrawJavaApp(
											int 					inLocation,
											LO_JavaAppStruct*		inJavaAppStruct);
											
		virtual	void 				HandleClippingView(
											struct LJAppletData 	*appletD, 
											int 					x, 
											int 					y, 
											int 					width, 
											int 					height);

		virtual	char* 				Prompt(
											const char* 			inMessage,
											const char*				inDefaultText);

#if 0
		Int32			GetTransactionID() { return fProgressID; }
		Int32			GetContextUniqueID() { return fWindowID; }
		// Window ID. Used to identify the context
		static	Int32	sWindowID;		// Unique ID, incremented for each context
		Int32			fWindowID;		// ID of this window


	private:
#endif

		Boolean					mIsRepaginating;
		Boolean					mIsRepaginationPending;

		Boolean					mLoadImagesOverride;		
		Boolean					mDelayImages;
		CSharableCompositor*	mCompositor;

		IL_GroupContext*		mImageGroupContext;
		
		Boolean					mImagesLoading;
		Boolean					mImagesLooping;
	    Boolean					mImagesDelayed;
		Boolean					mMochaImagesLoading;
		Boolean					mMochaImagesLooping;
	    Boolean					mMochaImagesDelayed;
	    
	    Boolean					mInNoMoreUsers;

		void (* mCloseCallback)(void *);				// called on window close
		void*					mCloseCallbackArg;

}; // class CBrowserContext

inline CBrowserContext::operator MWContext*()
	{	return &mContext;		};
inline CBrowserContext::operator MWContext&()
	{	return mContext;		};
	
inline CBrowserContext* ExtractBrowserContext(MWContext* inContext)
	{	return dynamic_cast<CBrowserContext*>(inContext->fe.newContext);		}

class CSharableCompositor : public LSharable
{
public:
	CSharableCompositor(CL_Compositor* c = nil) : mCompositor(c) {}
	void SetCompositor(CL_Compositor* c) { mCompositor = c; }
	virtual ~CSharableCompositor();
	operator CL_Compositor*() { return mCompositor; }
	CL_Compositor* mCompositor;
}; // class CSharableCompositor
