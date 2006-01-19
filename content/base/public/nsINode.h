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
 * The Original Code is Mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla.com.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *         Boris Zbarsky <bzbarsky@mit.edu> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsINode_h___
#define nsINode_h___

#include "nsIDOMGCParticipant.h"
#include "nsCOMPtr.h"

#ifdef MOZILLA_INTERNAL_API
#include "nsINodeInfo.h"
#endif

class nsIContent;
class nsIDocument;

// IID for the nsINode interface
// a22b59d9-cc21-402c-bdd2-2780e4cd5883
#define NS_INODE_IID \
{ 0xa22b59d9, 0xcc21, 0x402c, \
 { 0xbd, 0xd2, 0x27, 0x80, 0xe4, 0xcd, 0x58, 0x83 } }

/**
 * An internal interface that abstracts some DOMNode-related parts that both
 * nsIContent and nsIDocument share.  An instance of this interface has a list
 * of nsIContent children and provides access to them.
 */
class nsINode : public nsIDOMGCParticipant {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_INODE_IID)

#ifdef MOZILLA_INTERNAL_API
  // If you're using the external API, the only thing you can know about
  // nsINode is that it exists with an IID, if that....
    
  nsINode(nsINodeInfo* aNodeInfo)
    : mNodeInfo(aNodeInfo)
  {
  }
    
  /**
   * Get the number of children
   * @return the number of children
   */
  virtual PRUint32 GetChildCount() const = 0;

  /**
   * Get a child by index
   * @param aIndex the index of the child to get
   * @return the child, or null if index out of bounds
   */
  virtual nsIContent* GetChildAt(PRUint32 aIndex) const = 0;

  /**
   * Get the index of a child within this content
   * @param aPossibleChild the child to get the index of.
   * @return the index of the child, or -1 if not a child
   *
   * If the return value is not -1, then calling GetChildAt() with that value
   * will return aPossibleChild.
   */
  virtual PRInt32 IndexOf(nsIContent* aPossibleChild) const = 0;

  /**
   * Do we need a GetCurrentDoc of some sort?  I don't think we do...
   */

  /** 
   * Return the "owner document" of this node.  Note that this is not the same
   * as the DOM ownerDocument -- that's null for Document nodes, whereas for a
   * nsIDocument GetOwnerDocument returns the document itself.  For nsIContent
   * implementations the two are the same.
   */
  nsIDocument *GetOwnerDoc() const
  {
    return mNodeInfo->GetDocument();
  }

  /**
   * Insert a content node at a particular index.  This method handles calling
   * BindToTree on the child appropriately.
   *
   * @param aKid the content to insert
   * @param aIndex the index it is being inserted at (the index it will have
   *        after it is inserted)
   * @param aNotify whether to notify the document (current document for
   *        nsIContent, and |this| for nsIDocument) that the insert has
   *        occurred
   *
   * @throws NS_ERROR_DOM_HIERARCHY_REQUEST_ERR if one attempts to have more
   * than one element node as a child of a document.  Doing this will also
   * assert -- you shouldn't be doing it!  Check with
   * nsIDocument::GetRootContent() first if you're not sure.  Apart from this
   * one constraint, this doesn't do any checking on whether aKid is a valid
   * child of |this|.
   *
   * @throws NS_ERROR_OUT_OF_MEMORY in some cases (from BindToTree).
   */
  virtual nsresult InsertChildAt(nsIContent* aKid, PRUint32 aIndex,
                                 PRBool aNotify) = 0;

  /**
   * Append a content node to the end of the child list.  This method handles
   * calling BindToTree on the child appropriately.
   *
   * @param aKid the content to append
   * @param aNotify whether to notify the document (current document for
   *        nsIContent, and |this| for nsIDocument) that the append has
   *        occurred
   *
   * @throws NS_ERROR_DOM_HIERARCHY_REQUEST_ERR if one attempts to have more
   * than one element node as a child of a document.  Doing this will also
   * assert -- you shouldn't be doing it!  Check with
   * nsIDocument::GetRootContent() first if you're not sure.  Apart from this
   * one constraint, this doesn't do any checking on whether aKid is a valid
   * child of |this|.
   *
   * @throws NS_ERROR_OUT_OF_MEMORY in some cases (from BindToTree).
   */
  virtual nsresult AppendChildTo(nsIContent* aKid, PRBool aNotify) = 0;
  
  /**
   * Remove a child from this node.  This method handles calling UnbindFromTree
   * on the child appropriately.
   *
   * @param aIndex the index of the child to remove
   * @param aNotify whether to notify the document (current document for
   *        nsIContent, and |this| for nsIDocument) that the remove has
   *        occurred
   *
   * Note: If there is no child at aIndex, this method will simply do nothing.
   */
  virtual nsresult RemoveChildAt(PRUint32 aIndex, PRBool aNotify) = 0;

  /**
   * IsNodeOfType()?  Do we need a non-QI way to tell apart documents and
   * content?
   */
  
protected:

  nsCOMPtr<nsINodeInfo> mNodeInfo;

#endif // MOZILLA_INTERNAL_API
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsINode, NS_INODE_IID)

#endif /* nsINode_h___ */
