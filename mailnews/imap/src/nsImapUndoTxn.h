/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1998, 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsImapUndoTxn_h__
#define nsImapUndoTxn_h__

#include "nsIMsgFolder.h"
#include "nsImapCore.h"
#include "nsIImapService.h"
#include "nsIUrlListener.h"
#include "nsIEventQueue.h"
#include "nsMsgTxn.h"
#include "nsMsgKeyArray.h"
#include "nsCOMPtr.h"

#define NS_IMAPMOVECOPYMSGTXN_IID \
{ /* 51c925b0-208e-11d3-abea-00805f8ac968 */ \
	0x51c925b0, 0x208e, 0x11d3, \
    { 0xab, 0xea, 0x00, 0x80, 0x5f, 0x8a, 0xc9, 0x68 } }

class nsImapMoveCopyMsgTxn : public nsMsgTxn, public nsIUrlListener
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

    NS_IMETHOD Undo(void);
    NS_IMETHOD Redo(void);
    NS_IMETHOD GetUndoString(nsString *aString);
    NS_IMETHOD GetRedoString(nsString *aString);

    // nsIUrlListener methods
	NS_IMETHOD OnStartRunningUrl(nsIURI * aUrl);
	NS_IMETHOD OnStopRunningUrl(nsIURI * aUrl, nsresult aExitCode);
    
    // helper
    nsresult SetUndoString(nsString *aString);
    nsresult SetRedoString(nsString *aString);
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

private:

    nsCOMPtr<nsIMsgFolder> m_srcFolder;
    nsMsgKeyArray m_srcKeyArray;
    nsString m_srcMsgIdString;
    nsCOMPtr<nsIMsgFolder> m_dstFolder;
    nsMsgKeyArray m_dstKeyArray;
    nsString m_dstMsgIdString;
    nsCOMPtr<nsIEventQueue> m_eventQueue;
    nsCOMPtr<nsIUrlListener> m_urlListener;
    nsString m_undoString;
    nsString m_redoString;
    PRBool m_idsAreUids;
    PRBool m_isMove;
    PRBool m_srcIsPop3;
    nsUInt32Array m_srcSizeArray;
};

#endif
