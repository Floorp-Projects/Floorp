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

#ifndef nsLocalUndoTxn_h__
#define nsLocalUndoTxn_h__

#include "msgCore.h"
#include "nsIMsgFolder.h"
#include "nsMailboxService.h"
#include "nsMsgTxn.h"
#include "nsMsgKeyArray.h"
#include "nsCOMPtr.h"
#include "nsIUrlListener.h"

#define NS_LOCALMOVECOPYMSGTXN_IID \
{ /* 874363b4-242e-11d3-afad-001083002da8 */ \
	0x874363b4, 0x242e, 0x11d3, \
	{ 0xaf, 0xad, 0x00, 0x10, 0x83, 0x00, 0x2d, 0xa8 } }

class nsLocalMoveCopyMsgTxn : public nsMsgTxn, public nsIUrlListener
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_LOCALMOVECOPYMSGTXN_IID)

	nsLocalMoveCopyMsgTxn();
    nsLocalMoveCopyMsgTxn(nsIMsgFolder *srcFolder,
						  nsIMsgFolder *dstFolder,
						  PRBool isMove);
    virtual ~nsLocalMoveCopyMsgTxn();

    NS_DECL_ISUPPORTS_INHERITED

    // nsIUrlListener methods
	NS_IMETHOD OnStartRunningUrl(nsIURI * aUrl);
	NS_IMETHOD OnStopRunningUrl(nsIURI * aUrl, nsresult aExitCode);

    // overloading nsITransaction methods
	NS_IMETHOD Undo(void);
    NS_IMETHOD Redo(void);
    NS_IMETHOD GetUndoString(nsString *aString);
    NS_IMETHOD GetRedoString(nsString *aString);

    // helper
    nsresult SetUndoString(nsString *aString);
    nsresult SetRedoString(nsString *aString);
    nsresult AddSrcKey(nsMsgKey aKey);
    nsresult AddDstKey(nsMsgKey aKey);
    nsresult AddDstMsgSize(PRUint32 msgSize);
    nsresult SetSrcFolder(nsIMsgFolder* srcFolder);
    nsresult SetDstFolder(nsIMsgFolder* dstFolder);
    nsresult Init(nsIMsgFolder* srcFolder,
				  nsIMsgFolder* dstFolder,
				  PRBool isMove);
    nsresult UndoImapDeleteFlag(nsIMsgFolder* aFolder, 
                                nsMsgKeyArray& aKeyArray,
                                PRBool addFlag);
  
private:
    nsCOMPtr<nsIMsgFolder> m_srcFolder;
    nsMsgKeyArray m_srcKeyArray;
    nsCOMPtr<nsIMsgFolder> m_dstFolder;
    nsMsgKeyArray m_dstKeyArray;
    nsString m_undoString;
    nsString m_redoString;
    PRBool m_isMove;
    PRBool m_srcIsImap4;
    nsUInt32Array m_dstSizeArray;
};

#endif
