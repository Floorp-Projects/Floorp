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

//	CURLDispatcher.h


#ifndef CURLDispatcher_H
#define CURLDispatcher_H
#pragma once

#include <LPeriodical.h>
#include <LListener.h>

#include "structs.h"
#include "CAutoPtr.h"
#include "ntypes.h"
#include "net.h" // for FO_CACHE_AND_PRESENT

class CNSContext;
class CBrowserContext;
class CBrowserWindow;

class CURLDispatchInfo;

// Dispatch function prototype

const ResIDT BrowserWindow_ResID = 1010;

typedef void (*DispatchProcPtr)(CURLDispatchInfo* inDispatchInfo);

class CURLDispatcher : 
	public LPeriodical,
	public LListener
{
	public:
	
		static void				DispatchToStorage(
										URL_Struct*				inURL,
										const FSSpec&			inDestSpec,
										FO_Present_Types		inOutputFormat = FO_SAVE_AS,
										Boolean					inDelay = false);
		
		static void				DispatchToStorage(CURLDispatchInfo* inDispatchInfo);
		
		
		virtual	void			SpendTime(const EventRecord&	inMacEvent);
		
		virtual void			ListenToMessage(
										MessageT				inMessage,
										void*					ioParam);
						
		static Uint32 			CountDelayedURLs() { return GetURLDispatcher()->GetCountDelayedURLs(); }
		
		// 97-05-13 pkc -- New URL dispatch mechanism
		
		static void					DispatchURL(
										const char*				inURL,
										CNSContext*				inTargetContext,
										Boolean					inDelay = false,
										Boolean					inForceCreate = false,
										ResIDT					inWindowResID = BrowserWindow_ResID,
										Boolean					inInitiallyVisible = true,
										FO_Present_Types		inOutputFormat = FO_CACHE_AND_PRESENT,
										NET_ReloadMethod		inReloadMethod = NET_DONT_RELOAD);
										
		static void					DispatchURL(
										URL_Struct*				inURLStruct,
										CNSContext*				inTargetContext,
										Boolean					inDelay = false,
										Boolean					inForceCreate = false,
										ResIDT					inWindowResID = BrowserWindow_ResID,
										Boolean					inInitiallyVisible = true,
										FO_Present_Types		inOutputFormat = FO_CACHE_AND_PRESENT,
										Boolean					inWaitingForMochaImageLoad = false);

		static void					DispatchURL(CURLDispatchInfo* inDispatchInfo);

		// Dispatch procs
		
		static void				DispatchToLibNet(CURLDispatchInfo* inDispatchInfo);
		static void				DispatchToBrowserWindow(CURLDispatchInfo* inDispatchInfo);
		static void				DispatchMailboxURL(CURLDispatchInfo* inDispatchInfo);
		static void				DispatchToMailNewsWindow(CURLDispatchInfo* inDispatchInfo);

		// Utility functions
		
		static void				DispatchToNewBrowserWindow(CURLDispatchInfo* inDispatchInfo);
		static CBrowserWindow*	CreateNewBrowserWindow(
										ResIDT inWindowResID = BrowserWindow_ResID,
										Boolean inInitiallyVisible = true);

		// Return the browser window created by the last call to DispatchToView. Note that
		// if the dispatch was delayed, this will be null until the pending dispatch is processed.
		static CBrowserWindow*	GetLastBrowserWindowCreated() { return sLastBrowserWindowCreated; }
		
	protected:

		static CURLDispatcher* GetURLDispatcher();	// singleton class

		static void				DispatchToDisk(CURLDispatchInfo* inDispatchInfo);

		static void				DispatchToDiskAsText(CURLDispatchInfo* inDispatchInfo);
			
		Uint32 					GetCountDelayedURLs() const { return mDelayedURLs.GetCount(); }

		virtual	void			PostPendingDispatch(CURLDispatchInfo* inDispatchInfo);
										
		virtual void			UpdatePendingDispatch(
										CNSContext*				inForContext);
										
		virtual	void			ProcessPendingDispatch(void);

		LArray					mDelayedURLs;
		
		// reset to NULL on entry in DispatchToView(), set in DispatchToNewBrowserWindow()
		static CBrowserWindow*	sLastBrowserWindowCreated;

		static CAutoPtr<CURLDispatcher>	sDispatcher;
		static CAutoPtr<CBrowserContext>	sDispatchContext;
		

private:	
		friend class CAutoPtr<CURLDispatcher>;
		friend class CAutoPtr<CBrowserContext>;
		
								CURLDispatcher();
		virtual					~CURLDispatcher();
		
};

// Info needed to dispatch a URL
class CURLDispatchInfo
{
	public:
								CURLDispatchInfo();

								CURLDispatchInfo(
										const char*				inURL,
										CNSContext*				inTargetContext,
										FO_Present_Types		inOutputFormat,
										NET_ReloadMethod		inReloadMethod = NET_DONT_RELOAD,
										Boolean					inDelay = false /* BLECH! */,
										Boolean					inForceCreate = false,
										Boolean					inIsSaving = false,
										ResIDT					inWindowResID = BrowserWindow_ResID,
										Boolean					inInitiallyVisible = true);

								CURLDispatchInfo(
										URL_Struct*				inURLStruct,
										CNSContext*				inTargetContext,
										FO_Present_Types		inOutputFormat,
										Boolean					inDelay = false /* BLECH! */,
										Boolean					inForceCreate = false,
										Boolean					inIsSaving = false,
										ResIDT					inWindowResID = BrowserWindow_ResID,
										Boolean					inInitiallyVisible = true,
										Boolean					inWaitingForMochaImageLoad = false);

		virtual					~CURLDispatchInfo();
		
		Int32					GetURLType() { return mURLType; }
		char*					GetURL();
		URL_Struct*				GetURLStruct() { return mURLStruct; }
		CNSContext*				GetTargetContext() { return mTargetContext; }
		FO_Present_Types		GetOutputFormat() { return mOutputFormat; }
		Boolean					GetDelay() { return mDelayDispatch; }
		Boolean					GetIsSaving() { return mIsSaving; }
		Boolean					GetInitiallyVisible() { return mInitiallyVisible; }
		FSSpec&					GetFileSpec() { return mFileSpec; }
		ResIDT					GetWindowResID() { return mWindowResID; }
		Boolean					GetForceCreate() { return mForceCreate; }
		Boolean					GetIsWaitingForMochaImageLoad()
								{ return mIsWaitingForMochaImageLoad; }
		
		URL_Struct*				ReleaseURLStruct();

		void					ClearDelay() { mDelayDispatch = false; }		
		void					SetFileSpec(const FSSpec& inFileSpec);
		void					SetTargetContext(CNSContext* inTargetContext)
								{ mTargetContext = inTargetContext; }
		
	protected:
		Int32					mURLType;
		URL_Struct*				mURLStruct;
		CNSContext*				mTargetContext;
		FO_Present_Types		mOutputFormat;
		Boolean					mDelayDispatch;
		Boolean					mForceCreate;
		Boolean					mIsSaving;
		Boolean					mInitiallyVisible;
		Boolean					mIsWaitingForMochaImageLoad;
		FSSpec					mFileSpec;
		ResIDT					mWindowResID;
};
#endif
