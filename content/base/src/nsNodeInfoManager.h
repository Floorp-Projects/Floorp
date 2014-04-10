/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A class for handing out nodeinfos and ensuring sharing of them as needed.
 */

#ifndef nsNodeInfoManager_h___
#define nsNodeInfoManager_h___

#include "mozilla/Attributes.h"           // for MOZ_FINAL
#include "nsCOMPtr.h"                     // for member
#include "nsAutoPtr.h"                    // for nsRefPtr
#include "nsCycleCollectionParticipant.h" // for NS_DECL_CYCLE_*
#include "plhash.h"                       // for typedef PLHashNumber

class nsAString;
class nsBindingManager;
class nsIAtom;
class nsIDocument;
class nsIDOMDocumentType;
class nsINodeInfo;
class nsIPrincipal;
class nsNodeInfo;
struct PLHashEntry;
struct PLHashTable;
template<class T> struct already_AddRefed;

class nsNodeInfoManager MOZ_FINAL
{
public:
  nsNodeInfoManager();
  ~nsNodeInfoManager();

  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(nsNodeInfoManager)

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(nsNodeInfoManager)

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
  already_AddRefed<nsINodeInfo> GetNodeInfo(nsIAtom *aName, nsIAtom *aPrefix,
                                            int32_t aNamespaceID,
                                            uint16_t aNodeType,
                                            nsIAtom* aExtraName = nullptr);
  nsresult GetNodeInfo(const nsAString& aName, nsIAtom *aPrefix,
                       int32_t aNamespaceID, uint16_t aNodeType,
                       nsINodeInfo** aNodeInfo);
  nsresult GetNodeInfo(const nsAString& aName, nsIAtom *aPrefix,
                       const nsAString& aNamespaceURI, uint16_t aNodeType,
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

  nsBindingManager* GetBindingManager() const
  {
    return mBindingManager;
  }

protected:
  friend class nsDocument;
  friend class nsXULPrototypeDocument;
  friend nsresult NS_NewDOMDocumentType(nsIDOMDocumentType** ,
                                        nsNodeInfoManager *,
                                        nsIAtom *,
                                        const nsAString& ,
                                        const nsAString& ,
                                        const nsAString& );

  /**
   * Sets the principal of the document this nodeinfo manager belongs to.
   */
  void SetDocumentPrincipal(nsIPrincipal *aPrincipal);

private:
  static int NodeInfoInnerKeyCompare(const void *key1, const void *key2);
  static PLHashNumber GetNodeInfoInnerHashValue(const void *key);
  static int DropNodeInfoDocument(PLHashEntry *he, int hashIndex,
                                     void *arg);

  PLHashTable *mNodeInfoHash;
  nsIDocument *mDocument; // WEAK
  uint32_t mNonDocumentNodeInfos;
  nsIPrincipal *mPrincipal; // STRONG, but not nsCOMPtr to avoid include hell
                            // while inlining DocumentPrincipal().  Never null
                            // after Init() succeeds.
  nsCOMPtr<nsIPrincipal> mDefaultPrincipal; // Never null after Init() succeeds
  nsINodeInfo *mTextNodeInfo; // WEAK to avoid circular ownership
  nsINodeInfo *mCommentNodeInfo; // WEAK to avoid circular ownership
  nsINodeInfo *mDocumentNodeInfo; // WEAK to avoid circular ownership
  nsRefPtr<nsBindingManager> mBindingManager;
};

#endif /* nsNodeInfoManager_h___ */
