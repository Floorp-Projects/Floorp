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
#include "nsCOMPtr.h"
#include "plhash.h"

class nsNodeInfo;
class nsIPrincipal;
class nsIURI;


class nsNodeInfoManager : public nsINodeInfoManager
{
public:
  NS_DECL_ISUPPORTS

  // nsINodeInfoManager
  virtual nsresult Init(nsIDocument *aDocument);
  virtual void DropDocumentReference();
  virtual nsresult GetNodeInfo(nsIAtom *aName, nsIAtom *aPrefix,
                               PRInt32 aNamespaceID, nsINodeInfo** aNodeInfo);
  virtual nsresult GetNodeInfo(const nsAString& aName, nsIAtom *aPrefix,
                               PRInt32 aNamespaceID, nsINodeInfo** aNodeInfo);
  virtual nsresult GetNodeInfo(const nsAString& aQualifiedName,
                               const nsAString& aNamespaceURI,
                               nsINodeInfo** aNodeInfo);
  virtual nsresult GetNodeInfo(const nsAString& aName, nsIAtom *aPrefix,
                               const nsAString& aNamespaceURI,
                               nsINodeInfo** aNodeInfo);

  virtual nsresult GetDocumentPrincipal(nsIPrincipal** aPrincipal);
  virtual nsresult SetDocumentPrincipal(nsIPrincipal* aPrincipal);

  // nsNodeInfoManager
  nsNodeInfoManager();
  virtual ~nsNodeInfoManager();

  void RemoveNodeInfo(nsNodeInfo *aNodeInfo);

private:
  static PRIntn PR_CALLBACK NodeInfoInnerKeyCompare(const void *key1,
                                                    const void *key2);
  static PLHashNumber PR_CALLBACK GetNodeInfoInnerHashValue(const void *key);

  PLHashTable *mNodeInfoHash;
  nsCOMPtr<nsIPrincipal> mPrincipal;

  static PRUint32 gNodeManagerCount;
};

#endif /* nsNodeInfoManager_h___ */
