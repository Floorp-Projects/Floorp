/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef nsNodeInfoManager_h___
#define nsNodeInfoManager_h___

#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "plhash.h"

class nsIAtom;
class nsIDocument;
class nsINodeInfo;
class nsNodeInfo;
class nsIPrincipal;
class nsIURI;

class nsNodeInfoManager
{
public:
  nsNodeInfoManager();
  ~nsNodeInfoManager();

  nsrefcnt AddRef(void);
  nsrefcnt Release(void);

  /**
   * Initialize the nodeinfo manager with a document.
   */
  nsresult Init(nsIDocument *aDocument);

  /**
   * Release the reference to the document, this will be called when
   * the document is going away.
   */
  void DropDocumentReference();

  /**
   * Methods for creating nodeinfo's from atoms and/or strings.
   */
  nsresult GetNodeInfo(nsIAtom *aName, nsIAtom *aPrefix,
                       PRInt32 aNamespaceID, nsINodeInfo** aNodeInfo);
  nsresult GetNodeInfo(const nsAString& aName, nsIAtom *aPrefix,
                       PRInt32 aNamespaceID, nsINodeInfo** aNodeInfo);
  nsresult GetNodeInfo(const nsAString& aQualifiedName,
                       const nsAString& aNamespaceURI,
                       nsINodeInfo** aNodeInfo);

  /**
   * Retrieve a pointer to the document that owns this node info
   * manager.
   */
  nsIDocument* GetDocument() const
  {
    return mDocument;
  }

  /**
   * Gets the principal of the document associated with this.
   */
  nsIPrincipal *GetDocumentPrincipal();

  /**
   * Sets the principal of the nodeinfo manager. This should only be called
   * when this nodeinfo manager isn't connected to an nsIDocument.
   */
  void SetDocumentPrincipal(nsIPrincipal *aPrincipal);

  /**
   * Populate the given nsCOMArray with all of the nsINodeInfos
   * managed by this manager.
   */
  nsresult GetNodeInfos(nsCOMArray<nsINodeInfo> *aArray);

  void RemoveNodeInfo(nsNodeInfo *aNodeInfo);

private:
  static PRIntn PR_CALLBACK NodeInfoInnerKeyCompare(const void *key1,
                                                    const void *key2);
  static PLHashNumber PR_CALLBACK GetNodeInfoInnerHashValue(const void *key);

  nsAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD

  PLHashTable *mNodeInfoHash;
  nsIDocument *mDocument; // WEAK
  nsCOMPtr<nsIPrincipal> mPrincipal;

  static PRUint32 gNodeManagerCount;
};

#endif /* nsNodeInfoManager_h___ */
