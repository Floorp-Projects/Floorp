/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
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

#ifndef nsNodeInfoManager_h___
#define nsNodeInfoManager_h___

#include "nsINodeInfo.h"
#include "nsINameSpaceManager.h"
#include "nsCOMPtr.h"
#include "plhash.h"

class nsNodeInfo;


class nsNodeInfoManager : public nsINodeInfoManager
{
public:
  NS_DECL_ISUPPORTS

  // nsINodeInfoManager
  NS_IMETHOD Init(nsIDocument *aDocument,
                  nsINameSpaceManager *aNameSpaceManager);
  NS_IMETHOD DropDocumentReference();
  NS_IMETHOD GetNodeInfo(nsIAtom *aName, nsIAtom *aPrefix,
                         PRInt32 aNamespaceID, nsINodeInfo*& aNodeInfo);
  NS_IMETHOD GetNodeInfo(const nsAReadableString& aName, nsIAtom *aPrefix,
                         PRInt32 aNamespaceID, nsINodeInfo*& aNodeInfo);
  NS_IMETHOD GetNodeInfo(const nsAReadableString& aName, const nsAReadableString& aPrefix,
                         PRInt32 aNamespaceID, nsINodeInfo*& aNodeInfo);
  NS_IMETHOD GetNodeInfo(const nsAReadableString& aName, const nsAReadableString& aPrefix,
                         const nsAReadableString& aNamespaceURI,
                         nsINodeInfo*& aNodeInfo);
  NS_IMETHOD GetNodeInfo(const nsAReadableString& aQualifiedName,
                         const nsAReadableString& aNamespaceURI,
                         nsINodeInfo*& aNodeInfo); 
  NS_IMETHOD GetNamespaceManager(nsINameSpaceManager*& aNameSpaceManager);
  NS_IMETHOD GetDocument(nsIDocument*& aDocument);

  // nsNodeInfoManager
  nsNodeInfoManager();
  virtual ~nsNodeInfoManager();

  void RemoveNodeInfo(nsNodeInfo *aNodeInfo);

  static nsresult GetAnonymousManager(nsINodeInfoManager*& aNodeInfoManager);

private:
  PLHashTable *mNodeInfoHash;
  nsCOMPtr<nsINameSpaceManager> mNameSpaceManager;
  nsIDocument *mDocument; // WEAK

  /*
   * gAnonymousNodeInfoManager is a global nodeinfo manager used for nodes
   * that are no longer part of a document and for nodes that are created
   * where no document is accessible.
   *
   * gAnonymousNodeInfoManager is allocated when requested for the first time
   * and once the last nodeinfo manager (appart from gAnonymousNodeInfoManager)
   * is destroyed gAnonymousNodeInfoManager is destroyed. If the global
   * nodeinfo manager is the only nodeinfo manager used it can be deleted
   * and later reallocated if all users of the nodeinfo manager drops the
   * referernces to it.
   */
  static nsNodeInfoManager *gAnonymousNodeInfoManager;
  static PRUint32 gNodeManagerCount;
};

#endif /* nsNodeInfoManager_h___ */
