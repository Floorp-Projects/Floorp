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
//
// 
// 

#ifndef _MSGURLQ_H_
#define _MSGURLQ_H_

#include "ptrarray.h"
#include "net.h"

class MSG_Pane;
class ABook;
class MSG_UrlQueue;


/********************************************************************************************************************
Notes: Each URL in a MSG_UrlQueue is represented by a queue element for any instance data it 
	needs. Since a pane can have one and only one URL queue, it would be nice to be able to have queue elements
	for different types of URLs (local copy message urls, LDAP to AB, etc.) To support this effort, it will be common
	practice to subclass MSG_UrlQueueElement with a queue element capable of handling your new type of url. In addition,
	your new subclass should support a virtual method called PrepareToRun() if you need to make any specific changes to
	things like the current context for your new url before it is run. MSG_UrlQueue will always call the element's 
	PrepareToRun method before actually running that URL in the queue. You will also need to modify MSG_UrlQueue to 
	add methods for adding urls of your new type. 

*********************************************************************************************************************/

//*****************************************************************************
// MSG_UrlQueueElement -- Each URL in a MSG_UrlQueue is represented by a
//                        MSG_UrlQueueElement for any instance data it needs
//*****************************************************************************

class MSG_UrlQueueElement
{
	friend MSG_UrlQueue;

public:
	MSG_UrlQueueElement (const char *, MSG_UrlQueue *, Net_GetUrlExitFunc *, MSG_Pane *, NET_ReloadMethod reloadMethod = NET_DONT_RELOAD, FO_Present_Types outputFormat = FO_CACHE_AND_PRESENT);
	MSG_UrlQueueElement (URL_Struct *, MSG_UrlQueue *, Net_GetUrlExitFunc *, MSG_Pane *, XP_Bool skipFE = FALSE, FO_Present_Types outputFormat = FO_CACHE_AND_PRESENT);
	MSG_UrlQueueElement (URL_Struct * urls, MSG_UrlQueue *q);
	virtual ~MSG_UrlQueueElement ();
	virtual URL_Struct* GetURLStruct();
	virtual char* GetURLString();

	virtual void PrepareToRun();   // the queue element is about to be run....(added in particular for subclasses)

protected:
	char *m_urlString;
	XP_Bool m_callGetURLDirectly;
	MSG_Pane *m_pane;
	URL_Struct *m_url;
	MSG_UrlQueue *m_queue;
	Net_GetUrlExitFunc *m_exitFunction;
	NET_ReloadMethod	m_reloadMethod;
	FO_Present_Types m_outputFormat;
};


//************************************************************************************
// MSG_UrlLocalMsgCopyQueueElement - we want a queue element type which can handle
//									 LOCAL message copy urls. Don't try this with IMAP
//									 copy URLs.
//************************************************************************************

class MSG_UrlLocalMsgCopyQueueElement : public MSG_UrlQueueElement
{
public:
	MSG_UrlLocalMsgCopyQueueElement (MessageCopyInfo *, const char *, MSG_UrlQueue *, Net_GetUrlExitFunc *, 
		MSG_Pane *, NET_ReloadMethod reloadMethod = NET_DONT_RELOAD);
	MSG_UrlLocalMsgCopyQueueElement (MessageCopyInfo *, URL_Struct *, MSG_UrlQueue *, Net_GetUrlExitFunc *, 
		MSG_Pane *, XP_Bool skipFE = FALSE);
	MSG_UrlLocalMsgCopyQueueElement (MessageCopyInfo *, URL_Struct * urls, MSG_UrlQueue * q);
	
	virtual ~MSG_UrlLocalMsgCopyQueueElement();
	virtual void PrepareToRun();	// element is about to be run, we need to clobber context's copy info and replace with ours

protected:
	MessageCopyInfo * m_copyInfo;
};


//******************************************************************************************************
// MSG_UrlQueue -- Handles a queue of URLs which get fired in serial. This is
//                 a stopgap measure to compensate for the lack of multiple running
//                 URLs per MWContext
//
// Note: Must always call the queue element's PrepareToRun method before running a URL for that element.
//*******************************************************************************************************

typedef void MSG_UrlQueueInterruptFunc (MSG_UrlQueue *queue, URL_Struct *URL_s, int status, MWContext *window_id);


class MSG_UrlQueue : public XPPtrArray
{
public:
	MSG_UrlQueue (MSG_Pane *pane);
	virtual ~MSG_UrlQueue ();
	
	// it would be nice to use signature overloading for adding urls. Later, we might want to do this!!!
	
	// Use this for regular add-to-tail functionality
	static MSG_UrlQueue *AddUrlToPane (const char *url, Net_GetUrlExitFunc *exitFunction = NULL, MSG_Pane *pane = NULL, NET_ReloadMethod reloadMethod = NET_DONT_RELOAD);
	static MSG_UrlQueue * AddLocalMsgCopyUrlToPane (MessageCopyInfo * info, const char *url, Net_GetUrlExitFunc *exitFunction = NULL, MSG_Pane *pane = NULL, NET_ReloadMethod reloadMethod = NET_DONT_RELOAD);

	// Use this for regular add-to-tail functionality with URL_Struct filled in
	static MSG_UrlQueue * AddUrlToPane (URL_Struct *url, Net_GetUrlExitFunc *exitFunction = NULL, MSG_Pane *pane = NULL, XP_Bool skipFE = FALSE, FO_Present_Types outputFormat = FO_CACHE_AND_PRESENT);
	static MSG_UrlQueue * AddLocalMsgCopyUrlToPane (MessageCopyInfo * info, URL_Struct * url, Net_GetUrlExitFunc * exitFunction = NULL, MSG_Pane * pane = NULL, XP_Bool skipFE = FALSE);

	void AddUrl (const char *url, Net_GetUrlExitFunc *exitFunction = NULL, MSG_Pane *pane = NULL, NET_ReloadMethod reloadMethod = NET_DONT_RELOAD);
	void AddUrl(URL_Struct *url, Net_GetUrlExitFunc *exitFunction = NULL, MSG_Pane *pane = NULL, XP_Bool skipFE = FALSE, FO_Present_Types outputFormat = FO_CACHE_AND_PRESENT);
	void AddLocalMsgCopyUrl(MessageCopyInfo * info, const char *url, Net_GetUrlExitFunc *exitFunction = NULL, MSG_Pane * pane = NULL, NET_ReloadMethod reloadMethod = NET_DONT_RELOAD);

	// Use this if you need to insert a URL in the middle of the queue
	virtual void AddUrlAt (int where, const char *url, Net_GetUrlExitFunc *exitFunction = NULL, MSG_Pane *pane = NULL, NET_ReloadMethod reloadMethod = NET_DONT_RELOAD);
	virtual void AddLocalMsgCopyUrlAt (MessageCopyInfo * info, int where, const char * url, Net_GetUrlExitFunc * exitFunction = NULL, MSG_Pane * pane = NULL, NET_ReloadMethod reloadmethod = NET_DONT_RELOAD);

	// Use this if you need to know which URL is running
	virtual int GetCursor () { return m_runningUrl; }

	// start the queue by calling this
	virtual void GetNextUrl ();
	
	// which pane is this queue running url's in?
	virtual MSG_Pane* GetPane() { return m_pane; }

	// Use this if you need to know which queue a URL is on
	static MSG_UrlQueue *FindQueue (const char *url, MWContext *context);
	static MSG_UrlQueue *FindQueue (MSG_Pane *pane);
	static MSG_UrlQueue *FindQueueWithSameContext(MSG_Pane *pane);
	
	// Use this if you need to know the queue for a pane
	static MSG_UrlQueue *FindQueue (const char *url, MSG_Pane *pane);
	// used by subclasses who are designed to use other meta data for ordering urls.
	// an example is MSG_ImapLoadFolderUrlQueue
	static const int kNoSpecialIndex;
	void	SetSpecialIndexOfNextUrl(int index) { m_IndexOfNextUrl = index; }
	virtual XP_Bool IsIMAPLoadFolderUrlQueue();
	static void		HandleFolderLoadInterrupt(MSG_UrlQueue *queue, URL_Struct *URL_s, int status, MWContext *window_id);
	virtual void AddInterruptCallback(MSG_UrlQueueInterruptFunc *interruptFunc);
protected:

	MSG_UrlQueueElement *GetAt(int i) { return (MSG_UrlQueueElement*) XPPtrArray::GetAt(i); }

	// called if ExitFunction status == MK_INTERRUPTED
	virtual void HandleUrlQueueInterrupt(URL_Struct *URL_s, int status, MWContext *window_id);
	
	static void ExitFunction (URL_Struct *URL_s, int status, MWContext *window_id);
	void CallExitAndChain (URL_Struct *URL_s, int status, MWContext *window_id);

	static MSG_UrlQueue * GetOrCreateUrlQueue (MSG_Pane * pane, XP_Bool * newQueue);

	int m_runningUrl;
	MSG_Pane *m_pane;
	static XPPtrArray *m_queueArray;
	int	m_IndexOfNextUrl;
	XP_Bool			m_inExitFunc;
	XPPtrArray		m_interruptCallbacks;
	static XPPtrArray *GetQueueArray();
};


class MSG_AddLdapToAddressBookQueue : public MSG_UrlQueue
{
public:

	MSG_AddLdapToAddressBookQueue (MSG_Pane *);

	virtual void GetNextUrl ();

	ABook *m_addressBook;
};


class MSG_ImapLoadFolderUrlQueue : public MSG_UrlQueue
{
public:
	MSG_ImapLoadFolderUrlQueue(MSG_Pane *pane);
	
	// Use this for regular add-to-tail functionality, or use the index of it has been set.
	// Used to ensure ordering of message copy urls when going offline
	virtual void AddUrl (const char *url, Net_GetUrlExitFunc *exitFunction = NULL, MSG_Pane *pane = NULL, NET_ReloadMethod reloadMethod = NET_DONT_RELOAD);
	virtual XP_Bool IsIMAPLoadFolderUrlQueue();
protected:
	// called if ExitFunction status == MK_INTERRUPTED
//	virtual void HandleUrlQueueInterrupt(URL_Struct *URL_s, int status, MWContext *window_id);
	
};

#endif
