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

// CNSContext.h


#pragma once


#include <LBroadcaster.h>
#include <LSharable.h>

#include "structs.h"
#include "cstring.h"

class CHTMLView;
class CNSContext;


inline CNSContext* ExtractNSContext(MWContext* inContext)
	{	return inContext->fe.newContext;		}

inline const CNSContext* ExtractConstNSContext(const MWContext* inContext)
	{	return inContext->fe.newContext;		}

inline CHTMLView* ExtractHyperView(const MWContext* inContext)
	{	return inContext->fe.newView;				}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	This enumeration contains all of the possible broadcast messages that
//	a CNSContext can give.
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

enum {
		// These messages notify thge clients about the layout state.		
	msg_NSCDocTitleChanged				=	'DTCG',					// cstring* theNewTitle
	msg_NSCInternetKeywordChanged		= 	'IKEY',					// char* keyword
	msg_NSCLayoutNewDocument			=	'LOND',					// URL_Struct* theURL
	msg_NSCFinishedLayout				=	'FNLO',					// < none >

		// These messages notify thge clients about the repagination state.		
	msg_NSCPEmptyRepagination			=	'NLPG',					// < none >
	msg_NSCPAboutToRepaginate			=	'ABPG',					// < none >
	msg_NSCPEditorRepaginate			=	'EDPG',					// < none >

		// These messages are key to the whole process of loading a URL.
		// The start loading and all connections complete notifications are
		// guaranteed to be symmetrical.
	msg_NSCStartLoadURL					=	'SLUB',					// URL_Struct* theURL
	msg_NSCConfirmLoadNewURL			=	'CLNU',					// Boolean*
	msg_NSCAllConnectionsComplete		=	'ACCP',					// < none >

		// A message to all context clients that this grid context is about to die.
		// Clients should clean up and remove their shared references to the context
		// upon receiving this message.
	msg_NSCGridContextPreDispose		=	'GCPD',					// Boolean* isSavingHistory
	
		// A message to all context clients that a child grid context has been
		// created or disposed.  Clients will want to know if a grid is created
		// so that they can add themselves as a listener or add a shared reference
		// to the new context.
	msg_NSCGridContextCreated			=	'GCCR',					// CNSContext* new grid
	msg_NSCGridContextDisposed			=	'GCDP',					// < none >
	
		// Progress notifications, like the url loading notifications are
		// guaranteed to be symmetric.  There will always be one begin,
		// n updates, and one end notification.	
	msg_NSCProgressBegin				=	'PGST',					// CContextProgress*
	msg_NSCProgressUpdate				=	'PGUP',					// CContextProgress*
	msg_NSCProgressEnd					=	'PGED',					// CContextProgress*

		// These are progress messages that are not guaranteed to be sent
		// between bind and end progress notifications.
//	msg_NSCProgressMessageChanged		=	'PGMC',					// cstring* theNewMessage
	msg_NSCProgressMessageChanged		=	'PGMC',					// const char* theNewMessage
	msg_NSCProgressPercentChanged		=	'PGPC'					// Int32* theNewPercent
};

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//  The progress of a particular url loading operation is encapsulated in the
//	following object.  Accessors are provided in the context to support this.
//	This object is only instantiated during the actual load itself, begining
//	with msg_NSCStartLoadURL notification and ending with the
//	msg_NSCAllConnectionsComplete notification.  At all other (inactive) times
//	the accessors for this object will return NULL.  See the accessor comments
//	for further information.
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

class CContextProgress : public LSharable
{
	public:
						CContextProgress();
		
		Int32			mTotal;			// Total bytes tracked
		Int32			mRead;			// How many have been read
		Int32			mUnknownCount;	// How many connections of the unknown length do we have
		Int32			mPercent;		// Percent complete
		Int32			mInitCount;
		Uint32			mStartTime;
		
		cstring			mAction;
		cstring			mMessage;
		cstring			mComment;
};

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

class CNSContext : public LBroadcaster, public LSharable
{
	friend class CNSContextCallbacks;
	friend class CPlainTextConversionContext;
	
	public:
								CNSContext(MWContextType inType);
								CNSContext(const CNSContext& inOriginal);
								
		virtual 				~CNSContext();

		virtual	void			NoMoreUsers();
		
		operator MWContext*();
		operator const MWContext*() const;
		operator MWContext&();
		operator const MWContext&() const;

		virtual	CContextProgress* 	GetContextProgress();

	protected:
		virtual	void			 	SetContextProgress(CContextProgress* inProgress);
	public:
		void						EnsureContextProgress();

		virtual cstring				GetDescriptor() const;
		virtual void				SetDescriptor(const char* inDescriptor);
	
		virtual	Boolean				IsCloneRequired() const;
		virtual	void				SetRequiresClone(Boolean inClone);
	
		virtual	CContextProgress*	GetCurrentProgressStats();
		virtual	void				UpdateCurrentProgressStats();

		virtual void				WaitWhileBusy();

// FIX ME!!! ACCESSOR for unique ID

			// CHARACTER SET ACCESSORS
			
		void						InitDefaultCSID();
		virtual	void				SetDefaultCSID(Int16 inDefaultCSID);
		virtual	Int16				GetDefaultCSID() const;
		virtual	void				SetDocCSID(Int16 inDocCSID);
		virtual	Int16				GetDocCSID() const;
		virtual	void				SetWinCSID(Int16 inWinCSID);
		virtual	Int16				GetWinCSID() const;
		virtual	Int16				GetWCSIDFromDocCSID(
											Int16					inDocCSID);


		class IndexOutOfRangeException { };
		virtual History_entry*		GetCurrentHistoryEntry();
		virtual Int32				GetHistoryListCount();
		virtual cstring*			GetHistoryEntryTitleByIndex(Int32 inIndex); // one-based index
		virtual Int32				GetIndexOfCurrentHistoryEntry();
		virtual void				GetHistoryURLByIndex(cstring& outURL, Int32 inIndex);	// one-based index

			// URL MANIPULATION
		// 97-10-29 pchen -- Fix bug #90892, prefer origin_url in history entry to address
		virtual cstring				GetURLForReferral();
		
		virtual	cstring				GetCurrentURL();
		
		virtual	void				SwitchLoadURL(
											URL_Struct*				inURL,
											FO_Present_Types		inOutputFormat);
											
		virtual	void				ImmediateLoadURL(
											URL_Struct*				inURL,
											FO_Present_Types		inOutputFormat);

		// Need to make Alert public because we need to be able to call it from FE_Alert
		virtual	void 				Alert(
											const char*				inAlertText);
		
			// STATUS		

		virtual const char*			GetDefaultStatus() const;
		virtual void				ClearDefaultStatus();
		virtual void				SetStatus(const char* inStatus);

			// STUFF
		virtual void 				CompleteLoad(URL_Struct* inURL, int inStatus);
		virtual void				ClearMWContextViewPtr();

		virtual void				CopyListenersToContext(CNSContext* aSubContext);		// used when spawning grid contexts

	protected:


			// CALLBACK IMPLEMENTATION

		virtual	MWContext* 			CreateNewDocWindow(
											URL_Struct*				inURL);

		virtual	void 				LayoutNewDocument(
											URL_Struct*				inURL,
											Int32*					inWidth,
											Int32*					inHeight,
											Int32*					inMarginWidth,
											Int32*					inMarginHeight);

		virtual	void 				SetDocTitle(
											char* 					inTitle);
											
		virtual	void 				FinishedLayout();
											
		virtual	char* 				TranslateISOText(
											int 					inCharset,
											char*					inISOText);
											
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
											
		virtual void				CreateEmbedWindow(
											NPEmbeddedApp*			inEmbeddedApp);
											
		virtual void				SaveEmbedWindow(
											NPEmbeddedApp*			inEmbeddedApp);

		virtual void				RestoreEmbedWindow(
											NPEmbeddedApp*			inEmbeddedApp);

		virtual void				DestroyEmbedWindow(
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

		virtual void 				UpdateEnableStates();
		
		virtual void 				DisplayFeedback(
											int 					inLocation,
											LO_Element* 			inElement);

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

		virtual	void 				BeginPreSection();

		virtual	void 				EndPreSection();

		virtual	void 				SetProgressBarPercent(
											Int32 					inPercent);

		virtual	void 				SetBackgroundColor(
											Uint8 					inRed,
											Uint8					inGreen,
											Uint8 					inBlue);

public:
		virtual	void 				Progress(
											const char*				inMessageText);
protected:
		virtual	void 				SetCallNetlibAllTheTime();

		virtual	void 				ClearCallNetlibAllTheTime();

		virtual	void 				GraphProgressInit(
											URL_Struct*				inURL,
											Int32 					inContentLength);

		virtual	void 				GraphProgressDestroy(
											URL_Struct*				inURL,
											Int32 					inContentLength,
											Int32 					inTotalRead);

		virtual	void 				GraphProgress(
											URL_Struct*				inURL,
											Int32 					inBytesReceived,
											Int32 					inBytesSinceLast,
											Int32 					inContentLength);

		virtual	XP_Bool 			UseFancyFTP();

		virtual	XP_Bool 			UseFancyNewsgroupListing();

		virtual	int 				FileSortMethod();

		virtual	XP_Bool 			ShowAllNewsArticles();

		virtual	XP_Bool				Confirm(
											const char* 			inMessage);

		virtual	char* 				Prompt(
											const char* 			inMessage,
											const char*				inDefaultText);

		virtual	char* 				PromptWithCaption(
											const char*				inCaption,
											const char* 			inMessage,
											const char*				inDefaultText);

		virtual	XP_Bool 			PromptUsernameAndPassword(
											const char*				inMessage,
											char**					outUserName,
											char**					outPassword);

		virtual	char* 				PromptPassword(
											const char* 			inMessage);

		virtual	void 				EnableClicking();

		virtual	void 				AllConnectionsComplete();

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

public:

		static	UInt32	sNSCWindowID;		// Unique ID, incremented for each context
		UInt32			fNSCWindowID;		// ID of this window
		Int32			fNSCProgressID;	// 

		Int32			GetTransactionID() { return fNSCProgressID; }
		Int32			GetContextUniqueID() { return fNSCWindowID; }
		CommandT		GetCurrentCommand() const { return mCurrentCommand; }
		void			SetCurrentCommand(CommandT inCommand) { mCurrentCommand = inCommand; }
		// Window ID. Used to identify the context


		// There are listeners that listen to several contexts (eg, in mail windows).
		// This works by reference counting, and such listeners assume calls to
		// SwitchLoadURL and AllConnectionsComplete are balanced.  Each context must
		// therefore ensure that they are, even if it is done artificially.
		Int32						mLoadRefCount;


protected:


		MWContext				mContext;

		Int16 					mDefaultCSID;
		Boolean					mRequiresClone;		
		CContextProgress*		mProgress;
		CommandT				mCurrentCommand; // command being executed
};


inline CNSContext::operator MWContext*()
	{	return &mContext;		};
inline CNSContext::operator const MWContext*() const
	{	return &mContext;		};
inline CNSContext::operator MWContext&()
	{	return mContext;		};
inline CNSContext::operator const MWContext&() const
	{	return mContext;		};
	
