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
 * Portions created by the Initial Developer are Copyright (C) 1998-2001
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

#ifndef nsImapUndoTxn_h__
#define nsImapUndoTxn_h__

#include "nsIMsgFolder.h"
#include "nsImapCore.h"
#include "nsIImapService.h"
#include "nsIUrlListener.h"
#include "nsIEventQueue.h"
#include "nsMsgTxn.h"
#include "nsMsgKeyArray.h"
#include "nsIMsgOfflineImapOperation.h"
#include "nsCOMPtr.h"
#include "nsWeakReference.h"

#define NS_IMAPMOVECOPYMSGTXN_IID \
{ /* 51c925b0-208e-11d3-abea-00805f8ac968 */ \
	0x51c925b0, 0x208e, 0x11d3, \
    { 0xab, 0xea, 0x00, 0x80, 0x5f, 0x8a, 0xc9, 0x68 } }

class nsImapMoveCopyMsgTxn : public nsMsgTxn
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IMAPMOVECOPYMSGTXN_IID)

  nsImapMoveCopyMsgTxn();
  nsImapMoveCopyMsgTxn(nsIMsgFolder* srcFolder, nsMsgKeyArray* srcKeyArray,
                       const char* srcMsgIdString, nsIMsgFolder* dstFolder,
                       PRBool idsAreUids, PRBool isMove, 
                       nsIEventQueue *eventQueue, 
                       nsIUrlListener *urlListener);
  virtual ~nsImapMoveCopyMsgTxn();

  NS_DECL_ISUPPORTS_INHERITED 

  NS_IMETHOD UndoTransaction(void);
  NS_IMETHOD RedoTransaction(void);

  // helper
  nsresult SetCopyResponseUid(nsMsgKeyArray* keyArray,
                              const char *msgIdString);
  nsresult GetSrcKeyArray(nsMsgKeyArray& srcKeyArray);
  nsresult GetDstKeyArray(nsMsgKeyArray& dstKeyArray);
  nsresult AddDstKey(nsMsgKey aKey);
  nsresult UndoMailboxDelete();
  nsresult RedoMailboxDelete();
  nsresult Init(nsIMsgFolder* srcFolder, nsMsgKeyArray* srcKeyArray,
                const char* srcMsgIdString, nsIMsgFolder* dstFolder,
                PRBool idsAreUids, PRBool isMove, 
                nsIEventQueue *eventQueue, 
                nsIUrlListener *urlListener);
  PRBool DeleteIsMoveToTrash(nsIMsgFolder* folder);

protected:

  nsWeakPtr m_srcFolder;
  nsCOMPtr<nsISupportsArray> m_srcHdrs;
  nsMsgKeyArray m_dupKeyArray;
  nsMsgKeyArray m_srcKeyArray;
  nsCString m_srcMsgIdString;
  nsWeakPtr m_dstFolder;
  nsMsgKeyArray m_dstKeyArray;
  nsCString m_dstMsgIdString;
  nsCOMPtr<nsIEventQueue> m_eventQueue;
  nsCOMPtr<nsIUrlListener> m_urlListener;
  PRBool m_idsAreUids;
  PRBool m_isMove;
  PRBool m_srcIsPop3;
  nsUInt32Array m_srcSizeArray;
};

class nsImapOfflineTxn : public nsImapMoveCopyMsgTxn
{
public:
  nsImapOfflineTxn(nsIMsgFolder* srcFolder, nsMsgKeyArray* srcKeyArray,
                       nsIMsgFolder* dstFolder,
                       PRBool isMove,
                       nsOfflineImapOperationType opType,
                       nsIMsgDBHdr *srcHdr,
                       nsIEventQueue *eventQueue, 
                       nsIUrlListener *urlListener);
  virtual ~nsImapOfflineTxn();

  NS_IMETHOD UndoTransaction(void);
  NS_IMETHOD RedoTransaction(void);
  void SetAddFlags(PRBool addFlags) {m_addFlags = addFlags;}
  void SetFlags(PRUint32 flags) {m_flags = flags;}
protected:
  nsOfflineImapOperationType m_opType;
  nsCOMPtr <nsIMsgDBHdr> m_header;
  // these two are used to undo flag changes, which we don't currently do.
  PRBool m_addFlags;
  PRUint32 m_flags;
};


#endif
