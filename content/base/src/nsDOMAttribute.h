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
 * Implementation of DOM Core's nsIDOMAttr node.
 */

#ifndef nsDOMAttribute_h___
#define nsDOMAttribute_h___

#include "nsIAttribute.h"
#include "nsIDOMAttr.h"
#include "nsIDOMText.h"
#include "nsIDOMNodeList.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsINodeInfo.h"
#include "nsDOMAttributeMap.h"
#include "nsCycleCollectionParticipant.h"
#include "nsStubMutationObserver.h"

// Attribute helper class used to wrap up an attribute with a dom
// object that implements nsIDOMAttr and nsIDOMNode
class nsDOMAttribute : public nsIAttribute,
                       public nsIDOMAttr,
                       public nsStubMutationObserver
{
public:
  nsDOMAttribute(nsDOMAttributeMap* aAttrMap,
                 already_AddRefed<nsINodeInfo> aNodeInfo,
                 const nsAString& aValue,
                 PRBool aNsAware);
  virtual ~nsDOMAttribute();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  // nsIDOMNode interface
  NS_DECL_NSIDOMNODE

  // nsIDOMAttr interface
  NS_DECL_NSIDOMATTR

  virtual nsresult PreHandleEvent(nsEventChainPreVisitor& aVisitor);

  // nsIAttribute interface
  void SetMap(nsDOMAttributeMap *aMap);
  nsIContent *GetContent() const;
  nsresult SetOwnerDocument(nsIDocument* aDocument);

  // nsINode interface
  virtual PRBool IsNodeOfType(PRUint32 aFlags) const;
  virtual PRUint32 GetChildCount() const;
  virtual nsIContent *GetChildAt(PRUint32 aIndex) const;
  virtual nsIContent * const * GetChildArray(PRUint32* aChildCount) const;
  virtual PRInt32 IndexOf(nsINode* aPossibleChild) const;
  virtual nsresult InsertChildAt(nsIContent* aKid, PRUint32 aIndex,
                                 PRBool aNotify);
  virtual nsresult AppendChildTo(nsIContent* aKid, PRBool aNotify);
  virtual nsresult RemoveChildAt(PRUint32 aIndex, PRBool aNotify);
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;
  virtual already_AddRefed<nsIURI> GetBaseURI() const;

  static void Initialize();
  static void Shutdown();

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(nsDOMAttribute,
                                                         nsIAttribute)

  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED

  virtual nsXPCClassInfo* GetClassInfo();
protected:
  virtual mozilla::dom::Element* GetNameSpaceElement()
  {
    return GetContentInternal();
  }

  static PRBool sInitialized;

private:
  already_AddRefed<nsIAtom> GetNameAtom(nsIContent* aContent);

  void EnsureChildState();

  /**
   * Really removing the attribute child (unbind and release).
   */
  void doRemoveChild(bool aNotify);

  nsString mValue;
  // XXX For now, there's only a single child - a text element
  // representing the value.  This is strong ref, but we use a raw
  // pointer so we can implement GetChildArray().
  nsIContent* mChild;

  mozilla::dom::Element *GetContentInternal() const
  {
    return mAttrMap ? mAttrMap->GetContent() : nsnull;
  }
};


#endif /* nsDOMAttribute_h___ */
