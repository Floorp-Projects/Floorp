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
 * Copyright (C) 1996 Netscape Communications Corporation.  All Rights
 * Reserved.
 */



// CMessageView.h

#pragma once

#define INHERIT_FROM_BROWSERVIEW
#ifdef INHERIT_FROM_BROWSERVIEW
#include "CBrowserView.h"
#else
#include "CHTMLView.h"
#endif

#include "MailNewsCallbacks.h"

typedef struct MSG_ResultElement MSG_ResultElement;
class CMessageAttachmentView;
class CURLDispatchInfo;
class CDeferredCloseTask;

//======================================
class CMessageView
#ifdef INHERIT_FROM_BROWSERVIEW
	:	public CBrowserView
#else
	:	public CHTMLView
#endif
	,	public CMailCallbackListener
//======================================
{
	private:
#ifdef INHERIT_FROM_BROWSERVIEW
		typedef CBrowserView Inherited;
#else
		typedef CHTMLView Inherited;
#endif
	public:
		enum { class_ID = 'MsVw' };
	public:
								CMessageView(LStream* inStream);
		virtual					~CMessageView();
		virtual void			FinishCreateSelf(void);
		
		virtual void			ClickSelf(const SMouseDownEvent& where);
		virtual void			ListenToMessage(MessageT inMessage, void* ioParam);

		void					SetAttachmentView( CMessageAttachmentView* attachmentView)
												{ mAttachmentView = attachmentView; }
		void					SetMasterCommander(LCommander* inCommander)
									{ mMasterCommander = inCommander; }
		LCommander*				GetMasterCommander() const
									{ return mMasterCommander; }
		virtual void			SetContext(
									CBrowserContext* inNewContext);
		MSG_Pane*				GetMessagePane() const { return mMessagePane; }
		
		// Info about the parent folder: must be queried, not cached!
		MSG_FolderInfo*			GetFolderInfo() const;
		uint32					GetFolderFlags() const;

		// Info about the message currently displayed.
		MSG_ViewIndex			GetCurMessageViewIndex() const;
		MessageKey				GetCurMessageKey() const;
		uint32					GetCurMessageFlags() const;
		Boolean					IsDueToCloseLater() const;
		void					SetDueToCloseLater();

		void					ShowMessage(MSG_Master* inMsgMaster,
									MSG_FolderInfo* inMsgFolderInfo,
									MessageKey inMessageKey,
									Boolean inLoadNow = false);
		void					ClearMessageArea();

		void					ShowURLMessage(
									const char* url,
									Boolean inLoadNow = false);
		void					ShowSearchMessage(
									MSG_Master *inMsgMaster,
									MSG_ResultElement *inResult,
									Boolean inNoFolder = false);
		void					FileMessageToSelectedPopupFolder(const char *ioFolderName,
														  		 Boolean inMoveMessages);//¥¥TSM
	

		virtual void 			PaneChanged(
									MSG_Pane* inPane,
									MSG_PANE_CHANGED_NOTIFY_CODE inNotifyCode,
									int32 value);
		
		virtual void  			FindCommandStatus(
									CommandT inCommand,
									Boolean& outEnabled, Boolean& outUsesMark, 
									Char16& outMark,Str255 outName);
		virtual Boolean			ObeyCommand(CommandT inCommand, void *ioParam);
		Boolean					ObeyMotionCommand(MSG_MotionType inCommand);
		virtual Boolean			HandleKeyPress(const EventRecord& inKeyEvent);
		virtual Boolean			SetDefaultCSID(Int16 default_csid, Boolean forceRepaginate = false);

		virtual void			AdaptToSuperFrameSize(
										Int32				inSurrWidthDelta,
										Int32				inSurrHeightDelta,
										Boolean				inRefresh);
		virtual void			AdjustCursorSelf(Point inPortPt, const EventRecord& inMacEvent);
		Boolean					MaybeCloseLater(CommandT inCommand); // check prefs

	protected:

		void					YieldToMaster();
			// An ugly solution, but after trying many, it's the only one that worked.
			// This makes sure this view isn't target if there's a thread view in the same
			// window.

		virtual void			InstallBackgroundColor();
									// Sets mBackgroundColor. Called from ClearBackground().
									// The base class implementation uses the text background
									// preference, but derived classes can override this.
		virtual void			SetBackgroundColor(
										Uint8 					inRed,
										Uint8					inGreen,
										Uint8 					inBlue);
									// Avoids changing the color, cheats and sets it to white, which
									// is what we want for mail messages. 98/01/13.
		virtual void			GetDefaultFileNameForSaveAs(URL_Struct* url, CStr31& defaultName);
									// overridden by CMessageView to use subject.

		virtual void			DispatchURL(
									URL_Struct* inURLStruct,
									CNSContext* inTargetContext,
									Boolean inDelay = false,
									Boolean	inForceCreate = false,
									FO_Present_Types inOutputFormat = FO_CACHE_AND_PRESENT
									);
									
		virtual void			DispatchURL(CURLDispatchInfo* inDispatchInfo);

		void					CloseLater(); // Close on next idle


	protected:
		MSG_Pane*				mMessagePane;
		LCommander*				mMasterCommander; // Not the super commander.  See YieldToMaster.
		Boolean					mLoadingNakedURL;
		CMessageAttachmentView*	mAttachmentView; // the attachment pane
		Boolean					mClosing;
		MSG_MotionType			mMotionPendingCommand;
		CDeferredCloseTask*		mDeferredCloseTask;
		
		friend class CMessageAttachmentView;
};
