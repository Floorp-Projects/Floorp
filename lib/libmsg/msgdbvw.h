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
#ifndef _MsgView_H_
#define _MsgView_H_

typedef uint32 MSG_ViewIndex;

const uint32 kViewIndexNone = MSG_VIEWINDEXNONE;

#include "msg.h"
#include "msgdb.h"
#include "idarray.h"
#include "bytearr.h"
#include "errcode.h"
#include "chngntfy.h"
#include "abcom.h"

class XPStringObj;

class ViewChangeListener : public ChangeListener
{
public:
	ViewChangeListener(MessageDBView *view);
	virtual ~ViewChangeListener();
	virtual void OnViewChange(MSG_ViewIndex startIndex, int32 numChanged, 
								MSG_NOTIFY_CODE changeType, 
								ChangeListener *instigator);
	virtual void OnViewStartChange(MSG_ViewIndex startIndex, int32 numChanged, 
								MSG_NOTIFY_CODE changeType, 
								ChangeListener *instigator);
	virtual void OnViewEndChange(MSG_ViewIndex startIndex, int32 numChanged, 
								MSG_NOTIFY_CODE changeType, 
								ChangeListener *instigator);
	virtual void OnKeyChange(MessageKey keyChanged, int32 flags, 
								ChangeListener *instigator);
	virtual void OnAnnouncerGoingAway (ChangeAnnouncer *instigator);
protected:
	MessageDBView *m_dbView;
};

typedef int32 ViewFlags;
	// flags for GetViewFlags
const int kOutlineDisplay = 0x1;
const int kFlatDisplay = 0x2;
const int kShowIgnored = 0x8;
const int kUnreadOnly = 0x10;

class MessageDBView : public ChangeAnnouncer
{
	friend class ViewChangeListener;
public:
	MessageDBView();

	// Open and close methods
static MsgERR OpenURL(const char * url, MSG_Master *master,
					  ViewType viewType, MessageDBView **view, XP_Bool openInForeground);
static MsgERR OpenViewOnDB(MessageDB *msgDB, ViewType viewType, 
						   MessageDBView ** pRetView,
						   XP_Bool runInForeground = TRUE);

	virtual MsgERR Open(MessageDB *messageDB, ViewType viewType,
						uint32* pCount, XP_Bool runInForeground = TRUE);
	virtual MsgERR Close();

	virtual MsgERR Init(uint32 *pCount, XP_Bool runInForeground = TRUE);
	virtual MsgERR		InitSort(SortType sortType, SortOrder sortOrder);
	virtual int32		AddKeys(MessageKey *pOutput, int32 *pFlags, char *pLevels, SortType sortType, int numListed);

	// Methods dealing with adding headers

	// this is for background loading and indicates to the view that no
	// more headers are coming. For example, the view of only threads with
	// new documents could throw away all threads not having new at this point,
	// and whatever data structures it no longer needs
	virtual MsgERR		FinishedAddingHeaders();
	// this tells the view that we've added a chunk and we need to add them to
	//  the db and view. For threaded views, we want to do threading in chunks.
	virtual MsgERR		AddNewMessages() = 0;
	// for news, xover line, potentially, for IMAP, imap line...
	virtual MsgERR		AddHdrFromServerLine(char *line, MessageKey *msgId) = 0;
	virtual MsgERR		AddHdr(DBMessageHdr *msgHdr);
	virtual MsgERR		InsertHdrAt(DBMessageHdr *msgHdr, MSG_ViewIndex index);
	virtual MsgERR		OnNewHeader(MessageKey newKey, XP_Bool ensureListed);
	virtual XP_Bool		WantsThisThread(DBThreadMessageHdr *threadHdr);
public:
	// accessors
	virtual ViewType	GetViewType(void) {return m_viewType;}
			XP_Bool		GetShowingIgnored();
			void		SetShowingIgnored(XP_Bool bShowIgnored);
	MessageDB			*GetDB() {return m_messageDB;}
	virtual void 		SetInitialSortState(void) = 0;
	virtual const char * GetViewName(void) = 0;

	MsgERR		ListThreads(MessageKey *pMessageNums, int numToList, 
					MessageHdrStruct *pOutput, int *pNumListed);
	MsgERR		ListThreadsShort(MessageKey *pMessageNums, int numToList, 
					MSG_MessageLine *pOutput, int *pNumListed);
	// list the headers of the top-level thread ids 
	MsgERR		ListThreadIds(MessageKey *startMsg, MessageKey *pOutput,
							  int numToList, int *numListed);
	MsgERR		ListThreadIds(ListContext *context, MessageKey *pOutput,
							  int numToList, int *numListed);
	// return the list header information for the documents in a thread.
	MsgERR		ListThread(MessageKey threadId, MessageKey startMsg,
						   int numToList, MessageHdrStruct *pOutput,
						   int *pNumListed);
	MsgERR		ListThreadShort(MessageKey threadId, MessageKey startMsg,
								int numToList, MSG_MessageLine *pOutput,
								int *pNumListed);
	// list headers by index
	virtual MsgERR		ListShortMsgHdrByIndex(MSG_ViewIndex startIndex,
											   int numToList,
											   MSG_MessageLine *pOutput,
											   int *pNumListed);
	virtual MsgERR		ListMsgHdrByIndex(MSG_ViewIndex startIndex,
										  int numToList, MessageHdrStruct *pOutput,
										  int *pNumListed);
	MsgERR		GetMsgLevelByIndex(MSG_ViewIndex index, int &level);
	// view modification by index.
	MsgERR		ToggleIgnored(MSG_ViewIndex* indices,	int32 numIndices, XP_Bool *resultToggleState);
	MsgERR		ToggleWatched(MSG_ViewIndex* indices,	int32 numIndices);
	MsgERR		ExpansionDelta(MSG_ViewIndex index, int32 *expansionDelta);
	MsgERR		ToggleExpansion(MSG_ViewIndex index, uint32 *numChanged);
	MsgERR 		ExpandByIndex(MSG_ViewIndex index, uint32 *numExpanded);
	MsgERR 		CollapseByIndex(MSG_ViewIndex index, uint32 *numCollapsed);
	MsgERR		ExpandAll();
	MsgERR		CollapseAll();

	// navigation routines
	// This can cause a thread to be expanded. Maybe caller doesn't care...
	// But we'll return the index of any expanded thread, if asked, and the
	// index where we landed..
	MsgERR Navigate(MSG_ViewIndex startIndex, MSG_MotionType motion, 
					MessageKey *pResultKey, MSG_ViewIndex *resultIndex, 
					MSG_ViewIndex *pThreadIndex, XP_Bool wrap = TRUE);
	// Data navigate has no side effects - could be used to see if navigate
	// is valid...
	MsgERR DataNavigate(MessageKey startKey, MSG_MotionType motion, 
					MessageKey *pResultKey, MessageKey *pThreadKey);

	MsgERR GetNavigateStatus(MSG_MotionType motion, MSG_ViewIndex index, 
							  XP_Bool *selectable_p,
							  const char **display_string);

	MsgERR FindNextUnread(MessageKey startId, MessageKey *resultId, 
						MessageKey *resultThreadId);

	MsgERR FindPrevUnread(MessageKey startKey, MessageKey *pResultKey,
									 MessageKey *resultThreadId = NULL);
	MsgERR FindFirstFlagged(MSG_ViewIndex * pResultIndex);
	MsgERR FindPrevFlagged(MSG_ViewIndex startIndex, MSG_ViewIndex *pResultIndex);
	MsgERR FindNextFlagged(MSG_ViewIndex startIndex, MSG_ViewIndex *pResultIndex);

	MsgERR FindFirstNew(MSG_ViewIndex *pResultIndex);
// view modifications methods by index
	MsgERR ToggleReadByIndex(MSG_ViewIndex index);
	MsgERR SetReadByIndex(MSG_ViewIndex index, XP_Bool read);
	MsgERR SetThreadOfMsgReadByIndex(MSG_ViewIndex index, XP_Bool read);
	MsgERR ToggleThreadIgnored(DBThreadMessageHdr *thread, MSG_ViewIndex index);
	MsgERR SetThreadIgnored(DBThreadMessageHdr *thread, MSG_ViewIndex index, 
							XP_Bool ignored);

	MsgERR MarkMarkedByIndex(MSG_ViewIndex index, XP_Bool mark);
	MsgERR ToggleThreadWatched(DBThreadMessageHdr *thread, MSG_ViewIndex index);
	MsgERR SetThreadWatched(DBThreadMessageHdr *thread, MSG_ViewIndex index,
							XP_Bool watched);
	MsgERR SetKeyByIndex(MSG_ViewIndex index, MessageKey id);
	virtual MsgERR RemoveByIndex(MSG_ViewIndex index);
	MsgERR InsertByIndex(MSG_ViewIndex index, MessageKey id);
	MsgERR DeleteMessagesByIndex(MSG_ViewIndex *indices, int32 numIndices, XP_Bool removeFromDB);
	virtual MsgERR DeleteMsgByIndex(MSG_ViewIndex index, XP_Bool removeFromDB);

	/* these are the old methods....delete these once everyone in Nova is using the new address book */
	MsgERR AddSenderToABByIndex(MWContext* context, MSG_ViewIndex index, XP_Bool lastOneToAdd, XP_Bool displayRecip);
	MsgERR AddAllToABByIndex(MWContext* context, MSG_ViewIndex index, XP_Bool lastOneToAdd);

	/* new address book APIs require a destination container */
	MsgERR AddSenderToABByIndex(MSG_Pane * pane, MWContext* context, MSG_ViewIndex index, XP_Bool lastOneToAdd, XP_Bool displayRecip, AB_ContainerInfo * destAB);
	MsgERR AddAllToABByIndex(MSG_Pane * pane, MWContext* context, MSG_ViewIndex index, XP_Bool lastOneToAdd, AB_ContainerInfo * destAB);



	// view modifications methods by ID	- returned index can be useful to
	// caller.
	MsgERR SetReadByID(MessageKey  id, XP_Bool read, MSG_ViewIndex* pIndex);
	MsgERR InsertByID(MessageKey  id, MessageKey  newId, MSG_ViewIndex* pIndex);

	// make sure the passed key is "in" the view (e.g., for a threaded sort, this
	// may just mean the parent thread is in the view).
	virtual void	EnsureListed(MessageKey key);

	void	EnableChangeUpdates();
	void	DisableChangeUpdates();
	void	NoteChange(MSG_ViewIndex firstlineChanged, int numChanged, 
							   MSG_NOTIFY_CODE changeType);
	void	NoteStartChange(MSG_ViewIndex firstlineChanged, int numChanged, 
							   MSG_NOTIFY_CODE changeType);
	void	NoteEndChange(MSG_ViewIndex firstlineChanged, int numChanged, 
							   MSG_NOTIFY_CODE changeType);

	// array accessors
	MessageKey		GetAt(MSG_ViewIndex index) ;
	MSG_ViewIndex	FindViewIndex(MessageKey  key) 
						{return (MSG_ViewIndex) (m_idArray.FindIndex(key));}
	virtual MSG_ViewIndex	FindKey(MessageKey key, XP_Bool expand);
	int	  GetSize(void) {return(m_idArray.GetSize());}

	virtual MsgViewIndex		GetThreadFromMsgIndex(MsgViewIndex index,
									DBThreadMessageHdr **threadHdr);
	virtual MSG_ViewIndex		ThreadIndexOfMsg(MessageKey msgKey, 
											  MSG_ViewIndex msgIndex = kViewIndexNone,
											  int32 *pThreadCount = NULL,
											  uint32 *pFlags = NULL);

	// public methods dealing with sorting
	virtual MsgERR	Sort(SortType sortType, SortOrder sortOrder);
	virtual MsgERR	ExternalSort(SortType sort_key,
								XP_Bool sort_forward_p);
	SortType GetSortType(void) {return(m_sortType);}
	SortOrder GetSortOrder(void) {return m_sortOrder;}
	enum eFieldType {
		kString,
		kU16,
		kU32
	};
	MSG_ViewIndex		GetInsertIndex(DBMessageHdr *msgHdr);

	// methods dealing with view flags
	virtual MsgERR		OrExtraFlag(MSG_ViewIndex index, char orflag);
	virtual MsgERR		AndExtraFlag(MSG_ViewIndex index, char andflag);
	virtual MsgERR		SetExtraFlag(MSG_ViewIndex index, char setflag);
	virtual MsgERR		GetExtraFlag(MSG_ViewIndex, char *extraFlag);

	virtual void		SetExtraFlagsFromDBFlags(uint32 messageFlags, 
												MSG_ViewIndex index);
	static  void		CopyExtraFlagsToDBFlags(char flags,
												 uint32 *messageFlags);
	static  void		CopyDBFlagsToExtraFlags(uint32 messageFlags, 
												char *extraFlags);
	static	void		CopyExtraFlagsToPublicFlags(char flags, 
													uint32 *messageFlags);

	virtual XPByteArray *GetFlagsArray() {return &m_flags;}
	int32 AddReference ();
protected:
	static	void		InitNavigateCommands();
	virtual void		OnHeaderAddedOrDeleted() {}	
	virtual void		OnExtraFlagChanged(MSG_ViewIndex /*index*/, char /*extraFlag*/) {}
	virtual XP_Bool		IsValidIndex(MSG_ViewIndex index);
	virtual MSG_ViewIndex	GetIndexOfFirstDisplayedKeyInThread(DBThreadMessageHdr *threadHdr);
	virtual DBMessageHdr *GetFirstDisplayedHdrInThread(DBThreadMessageHdr *threadHdr);
	virtual DBMessageHdr *GetFirstMessageHdrToDisplayInThread(DBThreadMessageHdr *threadHdr);
	virtual int32		CountExpandedThread(MSG_ViewIndex index);

	virtual	MsgERR		MarkThreadOfMsgRead(MessageKey msgId, MSG_ViewIndex msgIndex,
									   XP_Bool bRead);
	virtual MsgERR		MarkThreadRead(DBThreadMessageHdr *threadHdr, 
									 MSG_ViewIndex threadIndex, XP_Bool bRead);
	virtual MsgERR		SortInternal(SortType sortType, SortOrder sortOrder);
			void		ReverseSort();
	virtual MsgERR		ReverseThreads();
	// helper routines for internal sort
			char		*GetStringField(DBMessageHdr *msgHdr, SortType sortType, int16 csid, XPStringObj &string);
			uint32		GetLongField(DBMessageHdr *msgHdr, SortType sortType);
	eFieldType			GetFieldTypeAndLenForSort(SortType sortType, uint16 *pMaxLen);
	uint32				GetStatusSortValue(DBMessageHdr *msgHdr);

	static MessageDBView *CacheLookup (MessageDB *, ViewType);
	static XP_List *m_viewCache;
	void CacheAdd ();
	void CacheRemove ();

	virtual ~MessageDBView();
	int32 m_refCount;

protected:
static uint32	m_publicEquivOfExtraFlags;
	SortType	m_sortType;
	SortOrder	m_sortOrder;
	ViewType	m_viewType;
	ViewFlags	m_viewFlags;
	IDArray	m_idArray;
	XPByteArray m_flags;
	XPByteArray m_levels;
	MessageDB	*m_messageDB;
	ViewChangeListener	m_changeListener;
	XP_Bool		m_sortValid;	// has id array been added to since we sorted?
} ;

#endif
