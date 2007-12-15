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

/*
 * A class for handing out nodeinfos and ensuring sharing of them as needed.
 */

#ifndef nsNodeInfoManager_h___
#define nsNodeInfoManager_h___

#include "nsCOMPtr.h" // for already_AddRefed
#include "plhash.h"
#include "prlog.h"
#include "nsAutoPtr.h"

class nsIAtom;
class nsIDocument;
class nsINodeInfo;
class nsNodeInfo;
class nsIPrincipal;
class nsIURI;
class nsDocument;
class nsIDOMDocumentType;
class nsIDOMDocument;
class nsAString;
class nsIDOMNamedNodeMap;
class nsXULPrototypeDocument;
class nsNodeInfoManager;
struct PLArenaPool;

// The size of mRecyclers array. The max size of recycled memory is
// sizeof(void*) * NS_NODE_RECYCLER_SIZE.
#define NS_NODE_RECYCLER_SIZE 64

class nsDOMNodeAllocator
{
public:
  nsDOMNodeAllocator() : mPool(nsnull) {}
  ~nsDOMNodeAllocator();

  nsrefcnt AddRef()
  {
    NS_ASSERTION(PRInt32(mRefCnt) >= 0, "illegal refcnt");
    ++mRefCnt;
    NS_LOG_ADDREF(this, mRefCnt, "nsDOMNodeAllocator", sizeof(*this));
    return mRefCnt;
  }
  nsrefcnt Release();

  void* Alloc(size_t aSize);
  void Free(size_t aSize, void* aPtr);
protected:
  friend class nsNodeInfoManager;
  nsresult Init();
  nsAutoRefCnt mRefCnt;
  PLArenaPool* mPool;
  // The recycler array points to recycled memory, where the size of
  // block is index*sizeof(void*), i.e., 0, 4, 8, 12, 16, ... or 0, 8, 16, ...
  void*        mRecyclers[NS_NODE_RECYCLER_SIZE];
};

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
   * Returns the nodeinfo for text nodes. Can return null if OOM.
   */
  already_AddRefed<nsINodeInfo> GetTextNodeInfo();

  /**
   * Returns the nodeinfo for comment nodes. Can return null if OOM.
   */
  already_AddRefed<nsINodeInfo> GetCommentNodeInfo();

  /**
   * Returns the nodeinfo for the document node. Can return null if OOM.
   */
  already_AddRefed<nsINodeInfo> GetDocumentNodeInfo();     

  /**
   * Retrieve a pointer to the document that owns this node info
   * manager.
   */
  nsIDocument* GetDocument() const
  {
    return mDocument;
  }

  /**
   * Gets the principal of the document this nodeinfo manager belongs to.
   */
  nsIPrincipal *DocumentPrincipal() const {
    NS_ASSERTION(mPrincipal, "How'd that happen?");
    return mPrincipal;
  }

  void RemoveNodeInfo(nsNodeInfo *aNodeInfo);

  nsDOMNodeAllocator* NodeAllocator() { return mNodeAllocator; }
protected:
  friend class nsDocument;
  friend class nsXULPrototypeDocument;
  friend nsresult NS_NewDOMDocumentType(nsIDOMDocumentType** ,
                                        nsNodeInfoManager *,
                                        nsIPrincipal *,
                                        nsIAtom *,
                                        nsIDOMNamedNodeMap *,
                                        nsIDOMNamedNodeMap *,
                                        const nsAString& ,
                                        const nsAString& ,
                                        const nsAString& );

  /**
   * Sets the principal of the document this nodeinfo manager belongs to.
   */
  void SetDocumentPrincipal(nsIPrincipal *aPrincipal);

private:
  static PRIntn PR_CALLBACK NodeInfoInnerKeyCompare(const void *key1,
                                                    const void *key2);
  static PLHashNumber PR_CALLBACK GetNodeInfoInnerHashValue(const void *key);

  nsAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD

  PLHashTable *mNodeInfoHash;
  nsIDocument *mDocument; // WEAK
  nsIPrincipal *mPrincipal; // STRONG, but not nsCOMPtr to avoid include hell
                            // while inlining DocumentPrincipal().  Never null
                            // after Init() succeeds.
  nsCOMPtr<nsIPrincipal> mDefaultPrincipal; // Never null after Init() succeeds
  nsINodeInfo *mTextNodeInfo; // WEAK to avoid circular ownership
  nsINodeInfo *mCommentNodeInfo; // WEAK to avoid circular ownership
  nsINodeInfo *mDocumentNodeInfo; // WEAK to avoid circular ownership

  nsRefPtr<nsDOMNodeAllocator> mNodeAllocator;

  static PRUint32 gNodeManagerCount;
};

#endif /* nsNodeInfoManager_h___ */
