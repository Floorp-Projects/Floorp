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
#include "nsEvent.h"
#include "nsPropertyTable.h"

#ifdef MOZILLA_INTERNAL_API
#include "nsINodeInfo.h"
#include "nsCOMPtr.h"
#endif

class nsIContent;
class nsIDocument;
class nsIDOMEvent;
class nsPresContext;
class nsEventChainPreVisitor;
class nsEventChainPostVisitor;
class nsIEventListenerManager;
class nsIPrincipal;

// IID for the nsINode interface
// f96eef82-43fc-4eee-9784-4259415e98a9
#define NS_INODE_IID \
{ 0xf96eef82, 0x43fc, 0x4eee, \
 { 0x97, 0x84, 0x42, 0x59, 0x41, 0x5e, 0x98, 0xa9 } }

// hack to make egcs / gcc 2.95.2 happy
class nsINode_base : public nsIDOMGCParticipant {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_INODE_IID)
};

/**
 * An internal interface that abstracts some DOMNode-related parts that both
 * nsIContent and nsIDocument share.  An instance of this interface has a list
 * of nsIContent children and provides access to them.
 */
class nsINode : public nsINode_base {
public:
#ifdef MOZILLA_INTERNAL_API
  // If you're using the external API, the only thing you can know about
  // nsINode is that it exists with an IID, if that....
    
  nsINode(nsINodeInfo* aNodeInfo)
    : mNodeInfo(aNodeInfo),
      mParentPtrBits(0)
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
   * Get a property associated with this node.
   *
   * @param aPropertyName  name of property to get.
   * @param aStatus        out parameter for storing resulting status.
   *                       Set to NS_PROPTABLE_PROP_NOT_THERE if the property
   *                       is not set.
   * @return               the property. Null if the property is not set
   *                       (though a null return value does not imply the
   *                       property was not set, i.e. it can be set to null).
   */
  virtual void* GetProperty(nsIAtom *aPropertyName,
                            nsresult *aStatus = nsnull) const;

  /**
   * Set a property to be associated with this node. This will overwrite an
   * existing value if one exists. The existing value is destroyed using the
   * destructor function given when that value was set.
   *
   * @param aPropertyName  name of property to set.
   * @param aValue         new value of property.
   * @param aDtor          destructor function to be used when this property
   *                       is destroyed.
   *
   * @return NS_PROPTABLE_PROP_OVERWRITTEN (success value) if the property
   *                                       was already set
   * @throws NS_ERROR_OUT_OF_MEMORY if that occurs
   */
  virtual nsresult SetProperty(nsIAtom *aPropertyName,
                               void *aValue,
                               NSPropertyDtorFunc aDtor = nsnull);

  /**
   * Destroys a property associated with this node. The value is destroyed
   * using the destruction function given when that value was set.
   *
   * @param aPropertyName  name of property to destroy.
   *
   * @throws NS_PROPTABLE_PROP_NOT_THERE if the property was not set
   */
  virtual nsresult DeleteProperty(nsIAtom *aPropertyName);

  /**
   * Unset a property associated with this node. The value will not be
   * destroyed but rather returned. It is the caller's responsibility to
   * destroy the value after that point
   *
   * @param aPropertyName  name of property to unset.
   * @param aStatus        out parameter for storing resulting status.
   *                       Set to NS_PROPTABLE_PROP_NOT_THERE if the property
   *                       is not set.
   * @return               the property. Null if the property is not set
   *                       (though a null return value does not imply the
   *                       property was not set, i.e. it can be set to null).
   */
  virtual void* UnsetProperty(nsIAtom  *aPropertyName,
                              nsresult *aStatus = nsnull);
  
  /**
   * Return the principal of this node.  This is guaranteed to never be a null
   * pointer.
   */
  nsIPrincipal* NodePrincipal() const {
    return mNodeInfo->NodeInfoManager()->DocumentPrincipal();
  }

  /**
   * IsNodeOfType()?  Do we need a non-QI way to tell apart documents and
   * content?
   */

  /**
   * Called before the capture phase of the event flow.
   * This is used to create the event target chain and implementations
   * should set the necessary members of nsEventChainPreVisitor.
   * At least aVisitor.mCanHandle must be set,
   * usually also aVisitor.mParentTarget if mCanHandle is PR_TRUE.
   * First one tells that this object can handle the aVisitor.mEvent event and
   * the latter one is the possible parent object for the event target chain.
   * @see nsEventDispatcher.h for more documentation about aVisitor.
   *
   * @param aVisitor the visitor object which is used to create the
   *                 event target chain for event dispatching.
   *
   * @note Only nsEventDispatcher should call this method.
   */
  virtual nsresult PreHandleEvent(nsEventChainPreVisitor& aVisitor) = 0;

  /**
   * Called after the bubble phase of the system event group.
   * The default handling of the event should happen here.
   * @param aVisitor the visitor object which is used during post handling.
   *
   * @see nsEventDispatcher.h for documentation about aVisitor.
   * @note Only nsEventDispatcher should call this method.
   */
  virtual nsresult PostHandleEvent(nsEventChainPostVisitor& aVisitor) = 0;

  /**
   * Dispatch an event.
   * @param aEvent the event that is being dispatched.
   * @param aDOMEvent the event that is being dispatched, use if you want to
   *                  dispatch nsIDOMEvent, not only nsEvent.
   * @param aPresContext the current presentation context, can be nsnull.
   * @param aEventStatus the status returned from the function, can be nsnull.
   *
   * @note If both aEvent and aDOMEvent are used, aEvent must be the internal
   *       event of the aDOMEvent.
   *
   * If aDOMEvent is not nsnull (in which case aEvent can be nsnull) it is used
   * for dispatching, otherwise aEvent is used.
   *
   * @deprecated This method is here just until all the callers outside Gecko
   *             have been converted to use nsIDOMEventTarget::dispatchEvent.
   */
  virtual nsresult DispatchDOMEvent(nsEvent* aEvent,
                                    nsIDOMEvent* aDOMEvent,
                                    nsPresContext* aPresContext,
                                    nsEventStatus* aEventStatus) = 0;

  /**
   * Get the event listener manager, the guy you talk to to register for events
   * on this node.
   * @param aCreateIfNotFound If PR_FALSE, returns a listener manager only if
   *                          one already exists. [IN]
   * @param aResult           The event listener manager [OUT]
   */
  virtual nsresult GetEventListenerManager(PRBool aCreateIfNotFound,
                                           nsIEventListenerManager** aResult)
                                           = 0;

  /**
   * Get the parent nsIContent for this node.
   * @return the parent, or null if no parent or the parent is not an nsIContent
   */
  nsIContent* GetParent() const
  {
    return NS_LIKELY(mParentPtrBits & PARENT_BIT_PARENT_IS_CONTENT) ?
           NS_REINTERPRET_CAST(nsIContent*,
                               mParentPtrBits & ~kParentBitMask) :
           nsnull;
  }

  /**
   * Get the parent nsINode for this node. This can be either an nsIContent,
   * an nsIDocument or an nsIAttribute.
   * @return the parent node
   */
  nsINode* GetNodeParent() const
  {
    return NS_REINTERPRET_CAST(nsINode*, mParentPtrBits & ~kParentBitMask);
  }

protected:

  nsCOMPtr<nsINodeInfo> mNodeInfo;

  typedef PRUword PtrBits;

  enum { PARENT_BIT_INDOCUMENT = 1 << 0, PARENT_BIT_PARENT_IS_CONTENT = 1 << 1 };
  enum { kParentBitMask = 0x3 };

  PtrBits mParentPtrBits;

#endif // MOZILLA_INTERNAL_API
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsINode_base, NS_INODE_IID)

#endif /* nsINode_h___ */
