/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _nsMsgDBView_H_
#define _nsMsgDBView_H_

#include "nsIMsgDBView.h"
#include "nsIMsgWindow.h"
#include "nsIMessenger.h"
#include "nsIMsgDatabase.h"
#include "nsIMsgHdr.h"
#include "nsMsgLineBuffer.h" // for nsByteArray
#include "nsMsgKeyArray.h"
#include "nsUint8Array.h"
#include "nsIDBChangeListener.h"
#include "nsIOutlinerView.h"
#include "nsIOutlinerBoxObject.h"
#include "nsIOutlinerSelection.h"
#include "nsVoidArray.h"
#include "nsIMsgFolder.h"
#include "nsIDateTimeFormat.h"
#include "nsIMsgHeaderParser.h"
#include "nsIDOMElement.h"
#include "nsIAtom.h"
#include "nsIImapIncomingServer.h"
#include "nsIMsgSearchNotify.h"

#include "nsIStringBundle.h"

#define MESSENGER_STRING_URL       "chrome://messenger/locale/messenger.properties"

enum eFieldType {
    kCollationKey,
    kU32,
    kPRTime
};

// reserve the top 8 bits in the msg flags for the view-only flags.
#define MSG_VIEW_FLAGS 0xEE000000
#define MSG_VIEW_FLAG_HASCHILDREN 0x40000000
#define MSG_VIEW_FLAG_ISTHREAD 0x8000000

// I think this will be an abstract implementation class.
// The classes that implement the outliner support will probably
// inherit from this class.
class nsMsgDBView : public nsIMsgDBView, public nsIDBChangeListener, public nsIOutlinerView, public nsIMsgCopyServiceListener, public nsIMsgSearchNotify
{
public:
  nsMsgDBView();
  virtual ~nsMsgDBView();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIMSGDBVIEW
  NS_DECL_NSIDBCHANGELISTENER
  NS_DECL_NSIOUTLINERVIEW
  NS_DECL_NSIMSGCOPYSERVICELISTENER
  NS_DECL_NSIMSGSEARCHNOTIFY

protected:
  static nsrefcnt gInstanceCount;
  // atoms used for styling the view. we're going to have a lot of
  // these so i'm going to make them static.
  static nsIAtom* kUnreadMsgAtom;
  static nsIAtom* kNewMsgAtom;
  static nsIAtom* kReadMsgAtom;
  static nsIAtom* kOfflineMsgAtom;
  static nsIAtom* kFlaggedMsgAtom;
  static nsIAtom* kNewsMsgAtom;
  static nsIAtom* kImapDeletedMsgAtom;
  static nsIAtom* kAttachMsgAtom;
  static nsIAtom* kHasUnreadAtom;

  static nsIAtom* kHighestPriorityAtom;
  static nsIAtom* kHighPriorityAtom;
  static nsIAtom* kLowestPriorityAtom;
  static nsIAtom* kLowPriorityAtom;

  static PRUnichar* kHighestPriorityString;
  static PRUnichar* kHighPriorityString;
  static PRUnichar* kLowestPriorityString;
  static PRUnichar* kLowPriorityString;
  static PRUnichar* kNormalPriorityString;
  
  static PRUnichar* kReadString;
  static PRUnichar* kRepliedString;
  static PRUnichar* kForwardedString;
  static PRUnichar* kNewString;

  nsCOMPtr<nsIOutlinerBoxObject> mOutliner;
  nsCOMPtr<nsIOutlinerSelection> mOutlinerSelection;
  PRUint32 mNumSelectedRows; // we cache this to determine when to push command status notifications.
  PRBool   mSupressMsgDisplay; // set when the message pane is collapsed
  PRBool   mIsSearchView; // tells if the search view is loaded.
  PRBool   mRemovingRow; // set when we're telling the outline a row is being removed. used to supress msg loading.
                        // during delete/move operations.
  PRBool  mOfflineMsgSelected;
  virtual const char * GetViewName(void) {return "MsgDBView"; }
  nsresult FetchAuthor(nsIMsgHdr * aHdr, PRUnichar ** aAuthorString);
  nsresult FetchSubject(nsIMsgHdr * aMsgHdr, PRUint32 aFlags, PRUnichar ** aValue);
  nsresult FetchDate(nsIMsgHdr * aHdr, PRUnichar ** aDateString);
  nsresult FetchStatus(PRUint32 aFlags, PRUnichar ** aStatusString);
  nsresult FetchSize(nsIMsgHdr * aHdr, PRUnichar ** aSizeString);
  nsresult FetchPriority(nsIMsgHdr *aHdr, PRUnichar ** aPriorityString);
  nsresult FetchLabel(nsIMsgHdr *aHdr, PRUnichar ** aLabelString);
  nsresult CycleThreadedColumn(nsIDOMElement * aElement);

  // Save and Restore Selection are a pair of routines you should
  // use when performing an operation which is going to change the view
  // and you want to remember the selection. (i.e. for sorting). 
  // Call SaveSelection and we'll give you an array of msg keys for
  // the current selection. We also freeze selection. When  you are done
  // changing the view, call RestoreSelection passing in the same array
  // and we'll restore the selection AND unfreeze selection in the UI.
  nsresult SaveSelection(nsMsgKeyArray * aMsgKeyArray);
  nsresult RestoreSelection(nsMsgKeyArray * aMsgKeyArray);

  // this is not safe to use when you have a selection
  // RowCountChanged() will call AdjustSelection() 
  // it should be called after SaveSelection() and before
  // RestoreSelection()
  nsresult AdjustRowCount(PRInt32 rowCountBeforeSort, PRInt32 rowCountAfterSort);

  nsresult GetSelectedIndices(nsUInt32Array *selection);
  nsresult GenerateURIForMsgKey(nsMsgKey aMsgKey, nsIMsgFolder *folder, char ** aURI);
// routines used in building up view
  virtual nsresult AddKeys(nsMsgKey *pKeys, PRInt32 *pFlags, const char *pLevels, nsMsgViewSortTypeValue sortType, PRInt32 numKeysToAdd);
  virtual PRBool WantsThisThread(nsIMsgThread * thread);
  virtual nsresult	AddHdr(nsIMsgDBHdr *msgHdr);
  PRBool GetShowingIgnored() {return (m_viewFlags & nsMsgViewFlagsType::kShowIgnored) != 0;}
  virtual nsresult OnNewHeader(nsMsgKey newKey, nsMsgKey parentKey, PRBool ensureListed);
  virtual nsMsgViewIndex GetInsertIndex(nsIMsgDBHdr *msgHdr);
  nsMsgViewIndex GetIndexForThread(nsIMsgDBHdr *hdr);
  virtual nsresult GetThreadContainingIndex(nsMsgViewIndex index, nsIMsgThread **thread);
  virtual nsresult GetMsgHdrForViewIndex(nsMsgViewIndex index, nsIMsgDBHdr **msgHdr);

  nsresult ToggleExpansion(nsMsgViewIndex index, PRUint32 *numChanged);
  nsresult ExpandByIndex(nsMsgViewIndex index, PRUint32 *pNumExpanded);
  nsresult CollapseByIndex(nsMsgViewIndex index, PRUint32 *pNumCollapsed);
  nsresult ExpandAll();
  nsresult CollapseAll();
  nsresult ExpandAndSelectThreadByIndex(nsMsgViewIndex index);
  nsresult ExpandAndSelectThread();

  // helper routines for thread expanding and collapsing.
  nsresult		GetThreadCount(nsMsgKey messageKey, PRUint32 *pThreadCount);
  nsMsgViewIndex GetIndexOfFirstDisplayedKeyInThread(nsIMsgThread *threadHdr);
  nsresult GetFirstMessageHdrToDisplayInThread(nsIMsgThread *threadHdr, nsIMsgDBHdr **result);
  nsMsgViewIndex ThreadIndexOfMsg(nsMsgKey msgKey, 
											  nsMsgViewIndex msgIndex = nsMsgViewIndex_None,
											  PRInt32 *pThreadCount = nsnull,
											  PRUint32 *pFlags = nsnull);
  nsMsgKey GetKeyOfFirstMsgInThread(nsMsgKey key);
  PRInt32 CountExpandedThread(nsMsgViewIndex index);
  nsresult ExpansionDelta(nsMsgViewIndex index, PRInt32 *expansionDelta);
  nsresult ReverseSort();
  nsresult ReverseThreads();
  nsresult SaveSortInfo(nsMsgViewSortTypeValue sortType, nsMsgViewSortOrderValue sortOrder);

	nsMsgKey		GetAt(nsMsgViewIndex index) ;
	nsMsgViewIndex	FindViewIndex(nsMsgKey  key) 
						{return (nsMsgViewIndex) (m_keys.FindIndex(key));}
	virtual nsMsgViewIndex	FindKey(nsMsgKey key, PRBool expand);
	virtual nsresult GetDBForViewIndex(nsMsgViewIndex index, nsIMsgDatabase **db);
  virtual nsresult GetFolders(nsISupportsArray **folders);

  nsresult ListIdsInThread(nsIMsgThread *threadHdr, nsMsgViewIndex viewIndex, PRUint32 *pNumListed);
  nsresult ListUnreadIdsInThread(nsIMsgThread *threadHdr, nsMsgViewIndex startOfThreadViewIndex, PRUint32 *pNumListed);
  PRInt32  FindLevelInThread(nsIMsgDBHdr *msgHdr, nsMsgViewIndex startOfThreadViewIndex);
  nsresult ListIdsInThreadOrder(nsIMsgThread *threadHdr, nsMsgKey parentKey, PRInt32 level, nsMsgViewIndex *viewIndex, PRUint32 *pNumListed);
	PRInt32	  GetSize(void) {return(m_keys.GetSize());}

  // notification api's
	void	EnableChangeUpdates();
	void	DisableChangeUpdates();
	void	NoteChange(nsMsgViewIndex firstlineChanged, PRInt32 numChanged, 
							   nsMsgViewNotificationCodeValue changeType);
	void	NoteStartChange(nsMsgViewIndex firstlineChanged, PRInt32 numChanged, 
							   nsMsgViewNotificationCodeValue changeType);
	void	NoteEndChange(nsMsgViewIndex firstlineChanged, PRInt32 numChanged, 
							   nsMsgViewNotificationCodeValue changeType);

  // for commands
  nsresult ApplyCommandToIndices(nsMsgViewCommandTypeValue command, nsMsgViewIndex* indices,
					PRInt32 numIndices);
  nsresult ApplyCommandToIndicesWithFolder(nsMsgViewCommandTypeValue command, nsMsgViewIndex* indices, 
                    PRInt32 numIndices, nsIMsgFolder *destFolder);
  virtual nsresult CopyMessages(nsIMsgWindow *window, nsMsgViewIndex *indices, PRInt32 numIndices, PRBool isMove, nsIMsgFolder *destFolder);
  virtual nsresult DeleteMessages(nsIMsgWindow *window, nsMsgViewIndex *indices, PRInt32 numIndices, PRBool deleteStorage);
  nsresult ToggleReadByIndex(nsMsgViewIndex index);
  nsresult SetReadByIndex(nsMsgViewIndex index, PRBool read);
  nsresult SetThreadOfMsgReadByIndex(nsMsgViewIndex index, nsMsgKeyArray &keysMarkedRead, PRBool read);
  nsresult SetFlaggedByIndex(nsMsgViewIndex index, PRBool mark);
  nsresult SetLabelByIndex(nsMsgViewIndex index, nsMsgLabelValue label);
  nsresult OrExtraFlag(nsMsgViewIndex index, PRUint32 orflag);
  nsresult AndExtraFlag(nsMsgViewIndex index, PRUint32 andflag);
  nsresult SetExtraFlag(nsMsgViewIndex index, PRUint32 extraflag);
	virtual nsresult RemoveByIndex(nsMsgViewIndex index);
  virtual void		OnExtraFlagChanged(nsMsgViewIndex /*index*/, PRUint32 /*extraFlag*/) {}
	virtual void		OnHeaderAddedOrDeleted() {}	
  nsresult ToggleThreadIgnored(nsIMsgThread *thread, nsMsgViewIndex threadIndex);
  nsresult ToggleThreadWatched(nsIMsgThread *thread, nsMsgViewIndex index);
  nsresult ToggleWatched( nsMsgViewIndex* indices,	PRInt32 numIndices);
  nsresult SetThreadWatched(nsIMsgThread *thread, nsMsgViewIndex index, PRBool watched);
  nsresult SetThreadIgnored(nsIMsgThread *thread, nsMsgViewIndex threadIndex, PRBool ignored);
  nsresult DownloadForOffline(nsIMsgWindow *window, nsMsgViewIndex *indices, PRInt32 numIndices);
  nsresult DownloadFlaggedForOffline(nsIMsgWindow *window);


  nsMsgViewIndex	GetThreadFromMsgIndex(nsMsgViewIndex index, 
													 nsIMsgThread **threadHdr);

  // for sorting
  nsresult GetFieldTypeAndLenForSort(nsMsgViewSortTypeValue sortType, PRUint16 *pMaxLen, eFieldType *pFieldType);
  nsresult GetCollationKey(nsIMsgHdr *msgHdr, nsMsgViewSortTypeValue sortType, PRUint8 **result, PRUint32 *len);
  nsresult GetLongField(nsIMsgHdr *msgHdr, nsMsgViewSortTypeValue sortType, PRUint32 *result);
  nsresult GetPRTimeField(nsIMsgHdr *msgHdr, nsMsgViewSortTypeValue sortType, PRTime *result);
  nsresult GetStatusSortValue(nsIMsgHdr *msgHdr, PRUint32 *result);
  nsresult GetLocationCollationKey(nsIMsgHdr *msgHdr, PRUint8 **result, PRUint32 *len);

  // for view navigation
  nsresult NavigateFromPos(nsMsgNavigationTypeValue motion, nsMsgViewIndex startIndex, nsMsgKey *pResultKey, 
              nsMsgViewIndex *pResultIndex, nsMsgViewIndex *pThreadIndex, PRBool wrap);
  nsresult FindNextFlagged(nsMsgViewIndex startIndex, nsMsgViewIndex *pResultIndex);
  nsresult FindFirstNew(nsMsgViewIndex *pResultIndex);
  nsresult FindNextUnread(nsMsgKey startId, nsMsgKey *pResultKey, nsMsgKey *resultThreadId);
  nsresult FindPrevUnread(nsMsgKey startKey, nsMsgKey *pResultKey, nsMsgKey *resultThreadId);
  nsresult FindFirstFlagged(nsMsgViewIndex *pResultIndex);
  nsresult FindPrevFlagged(nsMsgViewIndex startIndex, nsMsgViewIndex *pResultIndex);
  nsresult MarkThreadOfMsgRead(nsMsgKey msgId, nsMsgViewIndex msgIndex, nsMsgKeyArray &idsMarkedRead, PRBool bRead);
  nsresult MarkThreadRead(nsIMsgThread *threadHdr, nsMsgViewIndex threadIndex, nsMsgKeyArray &idsMarkedRead, PRBool bRead);
  PRBool IsValidIndex(nsMsgViewIndex index);
  nsresult ToggleIgnored(nsMsgViewIndex * indices, PRInt32 numIndices, PRBool *resultToggleState);
  nsresult GetKeyForFirstSelectedMessage(nsMsgKey *key);
  PRBool OfflineMsgSelected(nsMsgViewIndex * indices, PRInt32 numIndices);
  PRUnichar * GetString(const PRUnichar *aStringName);
  void InitializeAtomsAndLiterals();
  PRInt32 GetLevelInUnreadView(nsIMsgDBHdr *msgHdr, nsMsgViewIndex startOfThread, nsMsgViewIndex viewIndex);
  nsresult GetImapDeleteModel(nsIMsgFolder *folder);
  nsresult UpdateDisplayMessage(nsMsgKey aMsgKey);

  PRBool AdjustReadFlag(nsIMsgDBHdr *msgHdr, PRUint32 *msgFlags);
  void FreeAll(nsVoidArray *ptrs);
  void ClearHdrCache();
  nsMsgKeyArray m_keys;
  nsUInt32Array m_flags;
  nsUint8Array   m_levels;
  nsMsgImapDeleteModel mDeleteModel;

  // cache the most recently asked for header and corresponding msgKey.
  nsCOMPtr <nsIMsgDBHdr>  m_cachedHdr;
  nsMsgKey                m_cachedMsgKey;

  // we need to store the message key for the message we are currenty displaying to ensure we
  // don't try to redisplay the same message just because the selection changed (i.e. after a sort)
  nsMsgKey                m_currentlyDisplayedMsgKey;
  // if we're deleting messages, we want to hold off loading messages on selection changed until the delete is done
  // and we want to batch notifications.
  PRBool                  m_deletingMsgs;
  // used for batching deletes
  nsUInt32Array           m_removedIndices;

  nsCOMPtr <nsIMsgFolder> m_folder;
  PRBool mIsSpecialFolder; // for special folders, the Sender column really shows recipients.
  PRBool mIsNews;          // we have special icons for news, and for news, we show lines instead of size
  nsCOMPtr <nsIMsgDatabase> m_db;
  PRBool		m_sortValid;
  nsMsgViewSortTypeValue  m_sortType;
  nsMsgViewSortOrderValue m_sortOrder;
  nsMsgViewFlagsTypeValue m_viewFlags;

  // I18N date formater service which we'll want to cache locally.
  nsCOMPtr<nsIDateTimeFormat> mDateFormater;
  nsCOMPtr<nsIMsgHeaderParser> mHeaderParser;
  // i'm not sure if we are going to permamently need a nsIMessenger instance or if we'll be able
  // to phase it out eventually....for now we need it though.
  nsCOMPtr<nsIMessenger> mMessengerInstance;
  nsCOMPtr<nsIMsgWindow> mMsgWindow;
  nsCOMPtr<nsIMsgDBViewCommandUpdater> mCommandUpdater; // we push command update notifications to the UI from this.
  nsCOMPtr<nsIStringBundle> mMessengerStringBundle;  
};

#endif
