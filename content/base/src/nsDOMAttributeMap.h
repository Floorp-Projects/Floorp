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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Allan Beaufour <allan@beaufour.dk>
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
 * Implementation of the |attributes| property of DOM Core's nsIDOMNode object.
 */

#ifndef nsDOMAttributeMap_h___
#define nsDOMAttributeMap_h___

#include "nsIDOMNamedNodeMap.h"
#include "nsString.h"
#include "nsRefPtrHashtable.h"
#include "nsCycleCollectionParticipant.h"
#include "prbit.h"
#include "nsIDOMNode.h"

class nsIAtom;
class nsIContent;
class nsDOMAttribute;
class nsINodeInfo;
class nsIDocument;

namespace mozilla {
namespace dom {
class Element;
} // namespace dom
} // namespace mozilla

/**
 * Structure used as a key for caching nsDOMAttributes in nsDOMAttributeMap's mAttributeCache.
 */
class nsAttrKey
{
public:
  /**
   * The namespace of the attribute
   */
  PRInt32  mNamespaceID;

  /**
   * The atom for attribute, weak ref. is fine as we only use it for the
   * hashcode, we never dereference it.
   */
  nsIAtom* mLocalName;

  nsAttrKey(PRInt32 aNs, nsIAtom* aName)
    : mNamespaceID(aNs), mLocalName(aName) {}

  nsAttrKey(const nsAttrKey& aAttr)
    : mNamespaceID(aAttr.mNamespaceID), mLocalName(aAttr.mLocalName) {}
};

/**
 * PLDHashEntryHdr implementation for nsAttrKey.
 */
class nsAttrHashKey : public PLDHashEntryHdr
{
public:
  typedef const nsAttrKey& KeyType;
  typedef const nsAttrKey* KeyTypePointer;

  nsAttrHashKey(KeyTypePointer aKey) : mKey(*aKey) {}
  nsAttrHashKey(const nsAttrHashKey& aCopy) : mKey(aCopy.mKey) {}
  ~nsAttrHashKey() {}

  KeyType GetKey() const { return mKey; }
  PRBool KeyEquals(KeyTypePointer aKey) const
    {
      return mKey.mLocalName == aKey->mLocalName &&
             mKey.mNamespaceID == aKey->mNamespaceID;
    }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey)
    {
      if (!aKey)
        return 0;

      return PR_ROTATE_LEFT32(static_cast<PRUint32>(aKey->mNamespaceID), 4) ^
             NS_PTR_TO_INT32(aKey->mLocalName);
    }
  enum { ALLOW_MEMMOVE = PR_TRUE };

private:
  nsAttrKey mKey;
};

// Helper class that implements the nsIDOMNamedNodeMap interface.
class nsDOMAttributeMap : public nsIDOMNamedNodeMap
{
public:
  typedef mozilla::dom::Element Element;

  nsDOMAttributeMap(Element *aContent);
  virtual ~nsDOMAttributeMap();

  /**
   * Initialize the map. Must be called before the map is used.
   */
  PRBool Init();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  // nsIDOMNamedNodeMap interface
  NS_DECL_NSIDOMNAMEDNODEMAP

  void DropReference();

  Element* GetContent()
  {
    return mContent;
  }

  /**
   * Called when mContent is moved into a new document.
   * Updates the nodeinfos of all owned nodes.
   */
  nsresult SetOwnerDocument(nsIDocument* aDocument);

  /**
   * Drop an attribute from the map's cache (does not remove the attribute
   * from the node!)
   */
  void DropAttribute(PRInt32 aNamespaceID, nsIAtom* aLocalName);

  /**
   * Returns the number of attribute nodes currently in the map.
   * Note: this is just the number of cached attribute nodes, not the number of
   * attributes in mContent.
   *
   * @return The number of attribute nodes in the map.
   */
  PRUint32 Count() const;

  typedef nsRefPtrHashtable<nsAttrHashKey, nsDOMAttribute> AttrCache;

  /**
   * Enumerates over the attribute nodess in the map and calls aFunc for each
   * one. If aFunc returns PL_DHASH_STOP we'll stop enumerating at that point.
   *
   * @return The number of attribute nodes that aFunc was called for.
   */
  PRUint32 Enumerate(AttrCache::EnumReadFunction aFunc, void *aUserArg) const;

  nsDOMAttribute* GetItemAt(PRUint32 aIndex, nsresult *rv);
  nsDOMAttribute* GetNamedItem(const nsAString& aAttrName, nsresult *rv);

  static nsDOMAttributeMap* FromSupports(nsISupports* aSupports)
  {
#ifdef DEBUG
    {
      nsCOMPtr<nsIDOMNamedNodeMap> map_qi = do_QueryInterface(aSupports);

      // If this assertion fires the QI implementation for the object in
      // question doesn't use the nsIDOMNamedNodeMap pointer as the nsISupports
      // pointer. That must be fixed, or we'll crash...
      NS_ASSERTION(map_qi == static_cast<nsIDOMNamedNodeMap*>(aSupports),
                   "Uh, fix QI!");
    }
#endif

    return static_cast<nsDOMAttributeMap*>(aSupports);
  }

  NS_DECL_CYCLE_COLLECTION_CLASS(nsDOMAttributeMap)

private:
  Element *mContent; // Weak reference

  /**
   * Cache of nsDOMAttributes.
   */
  AttrCache mAttributeCache;

  /**
   * SetNamedItem() (aWithNS = PR_FALSE) and SetNamedItemNS() (aWithNS =
   * PR_TRUE) implementation.
   */
  nsresult SetNamedItemInternal(nsIDOMNode *aNode,
                                nsIDOMNode **aReturn,
                                PRBool aWithNS);

  /**
   * GetNamedItemNS() implementation taking |aRemove| for GetAttribute(),
   * which is used by RemoveNamedItemNS().
   */
  nsresult GetNamedItemNSInternal(const nsAString& aNamespaceURI,
                                  const nsAString& aLocalName,
                                  nsIDOMNode** aReturn,
                                  PRBool aRemove = PR_FALSE);

  nsDOMAttribute* GetAttribute(nsINodeInfo* aNodeInfo, PRBool aNsAware);

  /**
   * Remove an attribute, returns the removed node.
   */
  nsresult RemoveAttribute(nsINodeInfo*     aNodeInfo,
                           nsIDOMNode**     aReturn);
};


#endif /* nsDOMAttributeMap_h___ */
