/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * Contributor(s):
 *   David Bienvenu <bienvenu@nventure.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "msgCore.h"
#include "nsCOMArray.h"
#include "nsIMsgThread.h"
#include "nsMsgKeyArray.h"
#include "nsIMsgDatabase.h"
#include "nsIMsgHdr.h"

class nsMsgGroupView;

class nsMsgGroupThread : public nsIMsgThread
{
public:
  friend class nsMsgGroupView;

  nsMsgGroupThread();
  nsMsgGroupThread(nsIMsgDatabase *db);
  virtual ~nsMsgGroupThread();

  NS_DECL_NSIMSGTHREAD
  NS_DECL_ISUPPORTS

protected:
  void      Init();
  nsresult  RemoveChild(nsMsgKey msgKey);
  nsresult  RerootThread(nsIMsgDBHdr *newParentOfOldRoot, nsIMsgDBHdr *oldRoot, nsIDBChangeAnnouncer *announcer);

  nsresult AddMsgHdrInDateOrder(nsIMsgDBHdr *child);
  nsresult ReparentNonReferenceChildrenOf(nsIMsgDBHdr *topLevelHdr, nsMsgKey newParentKey,
                                                            nsIDBChangeAnnouncer *announcer);

  nsresult ReparentChildrenOf(nsMsgKey oldParent, nsMsgKey newParent, nsIDBChangeAnnouncer *announcer);
  nsresult ChangeUnreadChildCount(PRInt32 delta);
  nsresult GetChildHdrForKey(nsMsgKey desiredKey, nsIMsgDBHdr **result, PRInt32 *resultIndex);
  PRUint32 NumRealChildren();

  nsMsgKey        m_threadKey; 
  PRUint32        m_numUnreadChildren;	
  PRUint32        m_flags;
  nsMsgKey        m_threadRootKey;
  PRUint32        m_newestMsgDate;
  nsMsgKeyArray   m_keys;
  PRBool          m_dummy; // top level msg is a dummy, e.g., grouped by age.
  nsCOMPtr <nsIMsgDatabase> m_db; // should we make a weak ref or just a ptr?
};

class nsMsgXFGroupThread : public nsMsgGroupThread
{
public:
  nsMsgXFGroupThread();
  virtual ~nsMsgXFGroupThread();

  NS_IMETHOD GetNumChildren(PRUint32 *aNumChildren);
  NS_IMETHOD GetChildKeyAt(PRInt32 aIndex, nsMsgKey *aResult);
protected:
  nsCOMArray <nsIMsgDBHdr> m_hdrs;
};

