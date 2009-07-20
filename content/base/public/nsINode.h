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

#include "nsPIDOMEventTarget.h"
#include "nsEvent.h"
#include "nsPropertyTable.h"
#include "nsTObserverArray.h"
#include "nsINodeInfo.h"
#include "nsCOMPtr.h"
#include "nsWrapperCache.h"
#include "nsIProgrammingLanguage.h" // for ::JAVASCRIPT

class nsIContent;
class nsIDocument;
class nsIDOMEvent;
class nsIDOMNode;
class nsIDOMNodeList;
class nsINodeList;
class nsIPresShell;
class nsPresContext;
class nsEventChainVisitor;
class nsEventChainPreVisitor;
class nsEventChainPostVisitor;
class nsIEventListenerManager;
class nsIPrincipal;
class nsIMutationObserver;
class nsChildContentList;
class nsNodeWeakReference;
class nsNodeSupportsWeakRefTearoff;
class nsIEditor;

enum {
  // This bit will be set if the node doesn't have nsSlots
  NODE_DOESNT_HAVE_SLOTS =       0x00000001U,

  // This bit will be set if the node has a listener manager in the listener
  // manager hash
  NODE_HAS_LISTENERMANAGER =     0x00000002U,

  // Whether this node has had any properties set on it
  NODE_HAS_PROPERTIES =          0x00000004U,

  // Whether this node is the root of an anonymous subtree.  Note that this
  // need not be a native anonymous subtree.  Any anonymous subtree, including
  // XBL-generated ones, will do.  This flag is set-once: once a node has it,
  // it must not be removed.
  // NOTE: Should only be used on nsIContent nodes
  NODE_IS_ANONYMOUS =            0x00000008U,

  // Whether the node has some ancestor, possibly itself, that is native
  // anonymous.  This includes ancestors crossing XBL scopes, in cases when an
  // XBL binding is attached to an element which has a native anonymous
  // ancestor.  This flag is set-once: once a node has it, it must not be
  // removed.
  // NOTE: Should only be used on nsIContent nodes
  NODE_IS_IN_ANONYMOUS_SUBTREE = 0x00000010U,

  // Whether this node is the root of a native anonymous (from the perspective
  // of its parent) subtree.  This flag is set-once: once a node has it, it
  // must not be removed.
  // NOTE: Should only be used on nsIContent nodes
  NODE_IS_NATIVE_ANONYMOUS_ROOT = 0x00000020U,

  // Whether this node may have a frame
  // NOTE: Should only be used on nsIContent nodes
  NODE_MAY_HAVE_FRAME =          0x00000040U,

  // Forces the XBL code to treat this node as if it were
  // in the document and therefore should get bindings attached.
  NODE_FORCE_XBL_BINDINGS =      0x00000080U,

  // Whether a binding manager may have a pointer to this
  NODE_MAY_BE_IN_BINDING_MNGR =  0x00000100U,

  NODE_IS_EDITABLE =             0x00000200U,

  // Optimizations to quickly check whether element may have ID, class or style
  // attributes. Not all element implementations may use these!
  NODE_MAY_HAVE_ID =             0x00000400U,
  // For all Element nodes, NODE_MAY_HAVE_CLASS is guaranteed to be set if the
  // node in fact has a class, but may be set even if it doesn't.
  NODE_MAY_HAVE_CLASS =          0x00000800U,
  NODE_MAY_HAVE_STYLE =          0x00001000U,

  NODE_IS_INSERTION_PARENT =     0x00002000U,

  // Node has an :empty or :-moz-only-whitespace selector
  NODE_HAS_EMPTY_SELECTOR =      0x00004000U,

  // A child of the node has a selector such that any insertion,
  // removal, or appending of children requires restyling the parent.
  NODE_HAS_SLOW_SELECTOR =       0x00008000U,

  // A child of the node has a :first-child, :-moz-first-node,
  // :only-child, :last-child or :-moz-last-node selector.
  NODE_HAS_EDGE_CHILD_SELECTOR = 0x00010000U,

  // A child of the node has a selector such that any insertion or
  // removal of children requires restyling the parent (but append is
  // OK).
  NODE_HAS_SLOW_SELECTOR_NOAPPEND
                               = 0x00020000U,

  NODE_ALL_SELECTOR_FLAGS =      NODE_HAS_EMPTY_SELECTOR |
                                 NODE_HAS_SLOW_SELECTOR |
                                 NODE_HAS_EDGE_CHILD_SELECTOR |
                                 NODE_HAS_SLOW_SELECTOR_NOAPPEND,

  NODE_MAY_HAVE_CONTENT_EDITABLE_ATTR
                               = 0x00040000U,

  NODE_ATTACH_BINDING_ON_POSTCREATE
                               = 0x00080000U,

  // Four bits for the script-type ID
  NODE_SCRIPT_TYPE_OFFSET =               20,

  NODE_SCRIPT_TYPE_SIZE =                  4,

  // Remaining bits are node type specific.
  NODE_TYPE_SPECIFIC_BITS_OFFSET =
    NODE_SCRIPT_TYPE_OFFSET + NODE_SCRIPT_TYPE_SIZE
};

// Useful inline function for getting a node given an nsIContent and an
// nsIDocument.  Returns the first argument cast to nsINode if it is non-null,
// otherwise returns the second (which may be null).  We use type variables
// instead of nsIContent* and nsIDocument* because the actual types must be
// known for the cast to work.
template<class C, class D>
inline nsINode* NODE_FROM(C& aContent, D& aDocument)
{
  if (aContent)
    return static_cast<nsINode*>(aContent);
  return static_cast<nsINode*>(aDocument);
}

/**
 * Class used to detect unexpected mutations. To use the class create an
 * nsMutationGuard on the stack before unexpected mutations could occur.
 * You can then at any time call Mutated to check if any unexpected mutations
 * have occured.
 *
 * When a guard is instantiated sMutationCount is set to 300. It is then
 * decremented by every mutation (capped at 0). This means that we can only
 * detect 300 mutations during the lifetime of a single guard, however that
 * should be more then we ever care about as we usually only care if more then
 * one mutation has occured.
 *
 * When the guard goes out of scope it will adjust sMutationCount so that over
 * the lifetime of the guard the guard itself has not affected sMutationCount,
 * while mutations that happened while the guard was alive still will. This
 * allows a guard to be instantiated even if there is another guard higher up
 * on the callstack watching for mutations.
 *
 * The only thing that has to be avoided is for an outer guard to be used
 * while an inner guard is alive. This can be avoided by only ever
 * instantiating a single guard per scope and only using the guard in the
 * current scope.
 */
class nsMutationGuard {
public:
  nsMutationGuard()
  {
    mDelta = eMaxMutations - sMutationCount;
    sMutationCount = eMaxMutations;
  }
  ~nsMutationGuard()
  {
    sMutationCount =
      mDelta > sMutationCount ? 0 : sMutationCount - mDelta;
  }

  /**
   * Returns true if any unexpected mutations have occured. You can pass in
   * an 8-bit ignore count to ignore a number of expected mutations.
   */
  PRBool Mutated(PRUint8 aIgnoreCount)
  {
    return sMutationCount < static_cast<PRUint32>(eMaxMutations - aIgnoreCount);
  }

  // This function should be called whenever a mutation that we want to keep
  // track of happen. For now this is only done when children are added or
  // removed, but we might do it for attribute changes too in the future.
  static void DidMutate()
  {
    if (sMutationCount) {
      --sMutationCount;
    }
  }

private:
  // mDelta is the amount sMutationCount was adjusted when the guard was
  // initialized. It is needed so that we can undo that adjustment once
  // the guard dies.
  PRUint32 mDelta;

  // The value 300 is not important, as long as it is bigger then anything
  // ever passed to Mutated().
  enum { eMaxMutations = 300 };

  
  // sMutationCount is a global mutation counter which is decreased by one at
  // every mutation. It is capped at 0 to avoid wrapping.
  // Its value is always between 0 and 300, inclusive.
  static PRUint32 sMutationCount;
};

// IID for the nsINode interface
#define NS_INODE_IID \
{ 0xfc22c6df, 0x3e8e, 0x47c3, \
  { 0x96, 0xa6, 0xaf, 0x14, 0x3c, 0x05, 0x88, 0x68 } }
 
/**
 * An internal interface that abstracts some DOMNode-related parts that both
 * nsIContent and nsIDocument share.  An instance of this interface has a list
 * of nsIContent children and provides access to them.
 */
class nsINode : public nsPIDOMEventTarget,
                public nsWrapperCache
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_INODE_IID)

  friend class nsNodeUtils;
  friend class nsNodeWeakReference;
  friend class nsNodeSupportsWeakRefTearoff;

#ifdef MOZILLA_INTERNAL_API
  nsINode(nsINodeInfo* aNodeInfo)
    : mNodeInfo(aNodeInfo),
      mParentPtrBits(0),
      mFlagsOrSlots(NODE_DOESNT_HAVE_SLOTS)
  {
  }
#endif

  virtual ~nsINode();

  /**
   * Bit-flags to pass (or'ed together) to IsNodeOfType()
   */
  enum {
    /** nsIContent nodes */
    eCONTENT             = 1 << 0,
    /** nsIDocument nodes */
    eDOCUMENT            = 1 << 1,
    /** nsIAttribute nodes */
    eATTRIBUTE           = 1 << 2,
    /** elements */
    eELEMENT             = 1 << 3,
    /** text nodes */
    eTEXT                = 1 << 4,
    /** xml processing instructions */
    ePROCESSING_INSTRUCTION = 1 << 5,
    /** comment nodes */
    eCOMMENT             = 1 << 6,
    /** html elements */
    eHTML                = 1 << 7,
    /** form control elements */
    eHTML_FORM_CONTROL   = 1 << 8,
    /** XUL elements */
    eXUL                 = 1 << 9,
    /** svg elements */
    eSVG                 = 1 << 10,
    /** document fragments */
    eDOCUMENT_FRAGMENT   = 1 << 11,
    /** data nodes (comments, PIs, text). Nodes of this type always
     returns a non-null value for nsIContent::GetText() */
    eDATA_NODE           = 1 << 12,
    /** nsMathMLElement */
    eMATHML              = 1 << 13,
    /** nsHTMLMediaElement */
    eMEDIA               = 1 << 14
  };

  /**
   * API for doing a quick check if a content is of a given
   * type, such as HTML, XUL, Text, ...  Use this when you can instead of
   * checking the tag.
   *
   * @param aFlags what types you want to test for (see above)
   * @return whether the content matches ALL flags passed in
   */
  virtual PRBool IsNodeOfType(PRUint32 aFlags) const = 0;

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
   * Get a raw pointer to the child array.  This should only be used if you
   * plan to walk a bunch of the kids, promise to make sure that nothing ever
   * mutates (no attribute changes, not DOM tree changes, no script execution,
   * NOTHING), and will never ever peform an out-of-bounds access here.  This
   * method may return null if there are no children, or it may return a
   * garbage pointer.  In all cases the out param will be set to the number of
   * children.
   */
  virtual nsIContent * const * GetChildArray(PRUint32* aChildCount) const = 0;

  /**
   * Get the index of a child within this content
   * @param aPossibleChild the child to get the index of.
   * @return the index of the child, or -1 if not a child
   *
   * If the return value is not -1, then calling GetChildAt() with that value
   * will return aPossibleChild.
   */
  virtual PRInt32 IndexOf(nsINode* aPossibleChild) const = 0;

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
   * Returns true if the content has an ancestor that is a document.
   *
   * @return whether this content is in a document tree
   */
  PRBool IsInDoc() const
  {
    return mParentPtrBits & PARENT_BIT_INDOCUMENT;
  }

  /**
   * Get the document that this content is currently in, if any. This will be
   * null if the content has no ancestor that is a document.
   *
   * @return the current document
   */
  nsIDocument *GetCurrentDoc() const
  {
    return IsInDoc() ? GetOwnerDoc() : nsnull;
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
  nsresult AppendChildTo(nsIContent* aKid, PRBool aNotify)
  {
    return InsertChildAt(aKid, GetChildCount(), aNotify);
  }
  
  /**
   * Remove a child from this node.  This method handles calling UnbindFromTree
   * on the child appropriately.
   *
   * @param aIndex the index of the child to remove
   * @param aNotify whether to notify the document (current document for
   *        nsIContent, and |this| for nsIDocument) that the remove has
   *        occurred
   * @param aMutationEvent whether to fire a mutation event
   *
   * Note: If there is no child at aIndex, this method will simply do nothing.
   */
  virtual nsresult RemoveChildAt(PRUint32 aIndex, 
                                 PRBool aNotify, 
                                 PRBool aMutationEvent = PR_TRUE) = 0;

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
  void* GetProperty(nsIAtom *aPropertyName,
                    nsresult *aStatus = nsnull) const
  {
    return GetProperty(0, aPropertyName, aStatus);
  }

  /**
   * Get a property associated with this node.
   *
   * @param aCategory      category of property to get.
   * @param aPropertyName  name of property to get.
   * @param aStatus        out parameter for storing resulting status.
   *                       Set to NS_PROPTABLE_PROP_NOT_THERE if the property
   *                       is not set.
   * @return               the property. Null if the property is not set
   *                       (though a null return value does not imply the
   *                       property was not set, i.e. it can be set to null).
   */
  virtual void* GetProperty(PRUint16 aCategory,
                            nsIAtom *aPropertyName,
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
   * @param aTransfer      if PR_TRUE the property will not be deleted when the
   *                       ownerDocument of the node changes, if PR_FALSE it
   *                       will be deleted.
   *
   * @return NS_PROPTABLE_PROP_OVERWRITTEN (success value) if the property
   *                                       was already set
   * @throws NS_ERROR_OUT_OF_MEMORY if that occurs
   */
  nsresult SetProperty(nsIAtom *aPropertyName,
                       void *aValue,
                       NSPropertyDtorFunc aDtor = nsnull,
                       PRBool aTransfer = PR_FALSE)
  {
    return SetProperty(0, aPropertyName, aValue, aDtor, aTransfer);
  }

  /**
   * Set a property to be associated with this node. This will overwrite an
   * existing value if one exists. The existing value is destroyed using the
   * destructor function given when that value was set.
   *
   * @param aCategory       category of property to set.
   * @param aPropertyName   name of property to set.
   * @param aValue          new value of property.
   * @param aDtor           destructor function to be used when this property
   *                        is destroyed.
   * @param aTransfer       if PR_TRUE the property will not be deleted when the
   *                        ownerDocument of the node changes, if PR_FALSE it
   *                        will be deleted.
   * @param aOldValue [out] previous value of property.
   *
   * @return NS_PROPTABLE_PROP_OVERWRITTEN (success value) if the property
   *                                       was already set
   * @throws NS_ERROR_OUT_OF_MEMORY if that occurs
   */
  virtual nsresult SetProperty(PRUint16 aCategory,
                               nsIAtom *aPropertyName,
                               void *aValue,
                               NSPropertyDtorFunc aDtor = nsnull,
                               PRBool aTransfer = PR_FALSE,
                               void **aOldValue = nsnull);

  /**
   * Destroys a property associated with this node. The value is destroyed
   * using the destruction function given when that value was set.
   *
   * @param aPropertyName  name of property to destroy.
   *
   * @throws NS_PROPTABLE_PROP_NOT_THERE if the property was not set
   */
  nsresult DeleteProperty(nsIAtom *aPropertyName)
  {
    return DeleteProperty(0, aPropertyName);
  }

  /**
   * Destroys a property associated with this node. The value is destroyed
   * using the destruction function given when that value was set.
   *
   * @param aCategory      category of property to destroy.
   * @param aPropertyName  name of property to destroy.
   *
   * @throws NS_PROPTABLE_PROP_NOT_THERE if the property was not set
   */
  virtual nsresult DeleteProperty(PRUint16 aCategory, nsIAtom *aPropertyName);

  /**
   * Unset a property associated with this node. The value will not be
   * destroyed but rather returned. It is the caller's responsibility to
   * destroy the value after that point.
   *
   * @param aPropertyName  name of property to unset.
   * @param aStatus        out parameter for storing resulting status.
   *                       Set to NS_PROPTABLE_PROP_NOT_THERE if the property
   *                       is not set.
   * @return               the property. Null if the property is not set
   *                       (though a null return value does not imply the
   *                       property was not set, i.e. it can be set to null).
   */
  void* UnsetProperty(nsIAtom  *aPropertyName,
                      nsresult *aStatus = nsnull)
  {
    return UnsetProperty(0, aPropertyName, aStatus);
  }

  /**
   * Unset a property associated with this node. The value will not be
   * destroyed but rather returned. It is the caller's responsibility to
   * destroy the value after that point.
   *
   * @param aCategory      category of property to unset.
   * @param aPropertyName  name of property to unset.
   * @param aStatus        out parameter for storing resulting status.
   *                       Set to NS_PROPTABLE_PROP_NOT_THERE if the property
   *                       is not set.
   * @return               the property. Null if the property is not set
   *                       (though a null return value does not imply the
   *                       property was not set, i.e. it can be set to null).
   */
  virtual void* UnsetProperty(PRUint16 aCategory,
                              nsIAtom *aPropertyName,
                              nsresult *aStatus = nsnull);
  
  PRBool HasProperties() const
  {
    return HasFlag(NODE_HAS_PROPERTIES);
  }

  /**
   * Return the principal of this node.  This is guaranteed to never be a null
   * pointer.
   */
  nsIPrincipal* NodePrincipal() const {
    return mNodeInfo->NodeInfoManager()->DocumentPrincipal();
  }

  /**
   * Get the parent nsIContent for this node.
   * @return the parent, or null if no parent or the parent is not an nsIContent
   */
  nsIContent* GetParent() const
  {
    return NS_LIKELY(mParentPtrBits & PARENT_BIT_PARENT_IS_CONTENT) ?
           reinterpret_cast<nsIContent*>
                           (mParentPtrBits & ~kParentBitMask) :
           nsnull;
  }

  /**
   * Get the parent nsINode for this node. This can be either an nsIContent,
   * an nsIDocument or an nsIAttribute.
   * @return the parent node
   */
  nsINode* GetNodeParent() const
  {
    return reinterpret_cast<nsINode*>(mParentPtrBits & ~kParentBitMask);
  }

  /**
   * Adds a mutation observer to be notified when this node, or any of its
   * descendants, are modified. The node will hold a weak reference to the
   * observer, which means that it is the responsibility of the observer to
   * remove itself in case it dies before the node.  If an observer is added
   * while observers are being notified, it may also be notified.  In general,
   * adding observers while inside a notification is not a good idea.
   */
  virtual void AddMutationObserver(nsIMutationObserver* aMutationObserver);

  /**
   * Removes a mutation observer.
   */
  virtual void RemoveMutationObserver(nsIMutationObserver* aMutationObserver);

  /**
   * Clones this node. This needs to be overriden by all node classes. aNodeInfo
   * should be identical to this node's nodeInfo, except for the document which
   * may be different. When cloning an element, all attributes of the element
   * will be cloned. The children of the node will not be cloned.
   *
   * @param aNodeInfo the nodeinfo to use for the clone
   * @param aResult the clone
   */
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const = 0;

  /**
   * Checks if a node has the same ownerDocument as this one. Note that this
   * actually compares nodeinfo managers because nodes always have one, even
   * when they don't have an ownerDocument. If this function returns PR_TRUE
   * it doesn't mean that the nodes actually have an ownerDocument.
   *
   * @param aOther Other node to check
   * @return Whether the owner documents of this node and of aOther are the
   *         same.
   */
  PRBool HasSameOwnerDoc(nsINode *aOther)
  {
    // We compare nodeinfo managers because nodes always have one, even when
    // they don't have an ownerDocument.
    return mNodeInfo->NodeInfoManager() == aOther->mNodeInfo->NodeInfoManager();
  }

  // This class can be extended by subclasses that wish to store more
  // information in the slots.
  class nsSlots
  {
  public:
    nsSlots(PtrBits aFlags)
      : mFlags(aFlags),
        mChildNodes(nsnull),
        mWeakReference(nsnull)
    {
    }

    // If needed we could remove the vtable pointer this dtor causes by
    // putting a DestroySlots function on nsINode
    virtual ~nsSlots();

    /**
     * Storage for flags for this node. These are the same flags as the
     * mFlagsOrSlots member, but these are used when the slots class
     * is allocated.
     */
    PtrBits mFlags;

    /**
     * A list of mutation observers
     */
    nsTObserverArray<nsIMutationObserver*> mMutationObservers;

    /**
     * An object implementing nsIDOMNodeList for this content (childNodes)
     * @see nsIDOMNodeList
     * @see nsGenericHTMLElement::GetChildNodes
     *
     * MSVC 7 doesn't like this as an nsRefPtr
     */
    nsChildContentList* mChildNodes;

    /**
     * Weak reference to this node
     */
    nsNodeWeakReference* mWeakReference;
  };

  /**
   * Functions for managing flags and slots
   */
#ifdef DEBUG
  nsSlots* DebugGetSlots()
  {
    return GetSlots();
  }
#endif

  PRBool HasFlag(PtrBits aFlag) const
  {
    return !!(GetFlags() & aFlag);
  }

  PtrBits GetFlags() const
  {
    return NS_UNLIKELY(HasSlots()) ? FlagsAsSlots()->mFlags : mFlagsOrSlots;
  }

  void SetFlags(PtrBits aFlagsToSet)
  {
    NS_ASSERTION(!(aFlagsToSet & (NODE_IS_ANONYMOUS |
                                  NODE_MAY_HAVE_FRAME |
                                  NODE_IS_NATIVE_ANONYMOUS_ROOT |
                                  NODE_IS_IN_ANONYMOUS_SUBTREE |
                                  NODE_ATTACH_BINDING_ON_POSTCREATE)) ||
                 IsNodeOfType(eCONTENT),
                 "Flag only permitted on nsIContent nodes");
    PtrBits* flags = HasSlots() ? &FlagsAsSlots()->mFlags :
                                  &mFlagsOrSlots;
    *flags |= aFlagsToSet;
  }

  void UnsetFlags(PtrBits aFlagsToUnset)
  {
    NS_ASSERTION(!(aFlagsToUnset &
                   (NODE_IS_ANONYMOUS |
                    NODE_IS_IN_ANONYMOUS_SUBTREE |
                    NODE_IS_NATIVE_ANONYMOUS_ROOT)),
                 "Trying to unset write-only flags");
    PtrBits* flags = HasSlots() ? &FlagsAsSlots()->mFlags :
                                  &mFlagsOrSlots;
    *flags &= ~aFlagsToUnset;
  }

  void SetEditableFlag(PRBool aEditable)
  {
    if (aEditable) {
      SetFlags(NODE_IS_EDITABLE);
    }
    else {
      UnsetFlags(NODE_IS_EDITABLE);
    }
  }

  PRBool IsEditable() const
  {
#ifdef _IMPL_NS_LAYOUT
    return IsEditableInternal();
#else
    return IsEditableExternal();
#endif
  }

  /**
   * Returns PR_TRUE if |this| or any of its ancestors is native anonymous.
   */
  PRBool IsInNativeAnonymousSubtree() const
  {
#ifdef DEBUG
    if (HasFlag(NODE_IS_IN_ANONYMOUS_SUBTREE)) {
      return PR_TRUE;
    }
    CheckNotNativeAnonymous();
    return PR_FALSE;
#else
    return HasFlag(NODE_IS_IN_ANONYMOUS_SUBTREE);
#endif
  }

  /**
   * Get the root content of an editor. So, this node must be a descendant of
   * an editor. Note that this should be only used for getting input or textarea
   * editor's root content. This method doesn't support HTML editors.
   */
  nsIContent* GetTextEditorRootContent(nsIEditor** aEditor = nsnull);

  /**
   * Get the nearest selection root, ie. the node that will be selected if the
   * user does "Select All" while the focus is in this node. Note that if this
   * node is not in an editor, the result comes from the nsFrameSelection that
   * is related to aPresShell, so the result might not be the ancestor of this
   * node.
   */
  nsIContent* GetSelectionRootContent(nsIPresShell* aPresShell);

  virtual nsINodeList* GetChildNodesList();
  nsIContent* GetSibling(PRInt32 aOffset)
  {
    nsINode *parent = GetNodeParent();
    if (!parent) {
      return nsnull;
    }

    return parent->GetChildAt(parent->IndexOf(this) + aOffset);
  }
  nsIContent* GetLastChild() const
  {
    PRUint32 count;
    nsIContent* const* children = GetChildArray(&count);

    return count > 0 ? children[count - 1] : nsnull;
  }

  /**
   * Implementation is in nsIDocument.h, because it needs to cast from
   * nsIDocument* to nsINode*.
   */
  nsIDocument* GetOwnerDocument() const;

  /**
   * Iterator that can be used to easily iterate over the children.  This has
   * the same restrictions on its use as GetChildArray does.
   */
  class ChildIterator {
  public:
    ChildIterator(const nsINode* aNode) { Init(aNode); }
    ChildIterator(const nsINode* aNode, PRUint32 aOffset) {
      Init(aNode);
      Advance(aOffset);
    }
    ~ChildIterator() {
      NS_ASSERTION(!mGuard.Mutated(0), "Unexpected mutations happened");
    }

    PRBool IsDone() const { return mCur == mEnd; }
    operator nsIContent* const () { return *mCur; }
    void Next() { NS_PRECONDITION(mCur != mEnd, "Check IsDone"); ++mCur; }
    void Advance(PRUint32 aOffset) {
      NS_ASSERTION(mCur + aOffset <= mEnd, "Unexpected offset");
      mCur += aOffset;
    }
  private:
    void Init(const nsINode* aNode) {
      NS_PRECONDITION(aNode, "Must have node here!");
      PRUint32 childCount;
      mCur = aNode->GetChildArray(&childCount);
      mEnd = mCur + childCount;
    }
#ifdef DEBUG
    nsMutationGuard mGuard;
#endif
    nsIContent* const * mCur;
    nsIContent* const * mEnd;
  };

  /**
   * The default script type (language) ID for this node.
   * All nodes must support fetching the default script language.
   */
  virtual PRUint32 GetScriptTypeID() const
  { return nsIProgrammingLanguage::JAVASCRIPT; }

  /**
   * Not all nodes support setting a new default language.
   */
  NS_IMETHOD SetScriptTypeID(PRUint32 aLang)
  {
    NS_NOTREACHED("SetScriptTypeID not implemented");
    return NS_ERROR_NOT_IMPLEMENTED;
  }
protected:

  // Override this function to create a custom slots class.
  virtual nsINode::nsSlots* CreateSlots();

  PRBool HasSlots() const
  {
    return !(mFlagsOrSlots & NODE_DOESNT_HAVE_SLOTS);
  }

  nsSlots* FlagsAsSlots() const
  {
    NS_ASSERTION(HasSlots(), "check HasSlots first");
    return reinterpret_cast<nsSlots*>(mFlagsOrSlots);
  }

  nsSlots* GetExistingSlots() const
  {
    return HasSlots() ? FlagsAsSlots() : nsnull;
  }

  nsSlots* GetSlots()
  {
    if (HasSlots()) {
      return FlagsAsSlots();
    }

    nsSlots* slots = CreateSlots();
    if (slots) {
      mFlagsOrSlots = reinterpret_cast<PtrBits>(slots);
    }

    return slots;
  }

  nsTObserverArray<nsIMutationObserver*> *GetMutationObservers()
  {
    return HasSlots() ? &FlagsAsSlots()->mMutationObservers : nsnull;
  }

  PRBool IsEditableInternal() const;
  virtual PRBool IsEditableExternal() const
  {
    return IsEditableInternal();
  }

#ifdef DEBUG
  // Note: virtual so that IsInNativeAnonymousSubtree can be called accross
  // module boundaries.
  virtual void CheckNotNativeAnonymous() const;
#endif

  nsresult GetParentNode(nsIDOMNode** aParentNode);
  nsresult GetChildNodes(nsIDOMNodeList** aChildNodes);
  nsresult GetFirstChild(nsIDOMNode** aFirstChild);
  nsresult GetLastChild(nsIDOMNode** aLastChild);
  nsresult GetPreviousSibling(nsIDOMNode** aPrevSibling);
  nsresult GetNextSibling(nsIDOMNode** aNextSibling);
  nsresult GetOwnerDocument(nsIDOMDocument** aOwnerDocument);

  nsCOMPtr<nsINodeInfo> mNodeInfo;

  enum { PARENT_BIT_INDOCUMENT = 1 << 0, PARENT_BIT_PARENT_IS_CONTENT = 1 << 1 };
  enum { kParentBitMask = 0x3 };

  PtrBits mParentPtrBits;

  /**
   * Used for either storing flags for this node or a pointer to
   * this contents nsContentSlots. See the definition of the
   * NODE_* macros for the layout of the bits in this
   * member.
   */
  PtrBits mFlagsOrSlots;
};


extern const nsIID kThisPtrOffsetsSID;

// _implClass is the class to use to cast to nsISupports
#define NS_OFFSET_AND_INTERFACE_TABLE_BEGIN_AMBIGUOUS(_class, _implClass)     \
  static const QITableEntry offsetAndQITable[] = {                            \
    NS_INTERFACE_TABLE_ENTRY_AMBIGUOUS(_class, nsISupports, _implClass)

#define NS_OFFSET_AND_INTERFACE_TABLE_BEGIN(_class)                           \
  NS_OFFSET_AND_INTERFACE_TABLE_BEGIN_AMBIGUOUS(_class, _class)

#define NS_OFFSET_AND_INTERFACE_TABLE_END                                     \
  { nsnull, 0 } };                                                            \
  if (aIID.Equals(kThisPtrOffsetsSID)) {                                      \
    *aInstancePtr =                                                           \
      const_cast<void*>(static_cast<const void*>(&offsetAndQITable));         \
    return NS_OK;                                                             \
  }

#define NS_OFFSET_AND_INTERFACE_TABLE_TO_MAP_SEGUE                            \
  rv = NS_TableDrivenQI(this, offsetAndQITable, aIID, aInstancePtr);          \
  NS_INTERFACE_TABLE_TO_MAP_SEGUE

// nsNodeSH::PreCreate() depends on the identity pointer being the same as
// nsINode, so if you change the nsISupports line  below, make sure
// nsNodeSH::PreCreate() still does the right thing!
#define NS_NODE_OFFSET_AND_INTERFACE_TABLE_BEGIN(_class)                      \
  NS_OFFSET_AND_INTERFACE_TABLE_BEGIN_AMBIGUOUS(_class, nsINode)              \
    NS_INTERFACE_TABLE_ENTRY(_class, nsINode)                       

#define NS_NODE_INTERFACE_TABLE2(_class, _i1, _i2)                            \
  NS_NODE_OFFSET_AND_INTERFACE_TABLE_BEGIN(_class)                            \
    NS_INTERFACE_TABLE_ENTRY(_class, _i1)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i2)                                     \
  NS_OFFSET_AND_INTERFACE_TABLE_END                                           \
  NS_OFFSET_AND_INTERFACE_TABLE_TO_MAP_SEGUE

#define NS_NODE_INTERFACE_TABLE3(_class, _i1, _i2, _i3)                       \
  NS_NODE_OFFSET_AND_INTERFACE_TABLE_BEGIN(_class)                            \
    NS_INTERFACE_TABLE_ENTRY(_class, _i1)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i2)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i3)                                     \
  NS_OFFSET_AND_INTERFACE_TABLE_END                                           \
  NS_OFFSET_AND_INTERFACE_TABLE_TO_MAP_SEGUE

#define NS_NODE_INTERFACE_TABLE4(_class, _i1, _i2, _i3, _i4)                  \
  NS_NODE_OFFSET_AND_INTERFACE_TABLE_BEGIN(_class)                            \
    NS_INTERFACE_TABLE_ENTRY(_class, _i1)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i2)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i3)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i4)                                     \
  NS_OFFSET_AND_INTERFACE_TABLE_END                                           \
  NS_OFFSET_AND_INTERFACE_TABLE_TO_MAP_SEGUE

#define NS_NODE_INTERFACE_TABLE5(_class, _i1, _i2, _i3, _i4, _i5)             \
  NS_NODE_OFFSET_AND_INTERFACE_TABLE_BEGIN(_class)                            \
    NS_INTERFACE_TABLE_ENTRY(_class, _i1)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i2)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i3)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i4)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i5)                                     \
  NS_OFFSET_AND_INTERFACE_TABLE_END                                           \
  NS_OFFSET_AND_INTERFACE_TABLE_TO_MAP_SEGUE

#define NS_NODE_INTERFACE_TABLE6(_class, _i1, _i2, _i3, _i4, _i5, _i6)        \
  NS_NODE_OFFSET_AND_INTERFACE_TABLE_BEGIN(_class)                            \
    NS_INTERFACE_TABLE_ENTRY(_class, _i1)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i2)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i3)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i4)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i5)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i6)                                     \
  NS_OFFSET_AND_INTERFACE_TABLE_END                                           \
  NS_OFFSET_AND_INTERFACE_TABLE_TO_MAP_SEGUE

#define NS_NODE_INTERFACE_TABLE7(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7)   \
  NS_NODE_OFFSET_AND_INTERFACE_TABLE_BEGIN(_class)                            \
    NS_INTERFACE_TABLE_ENTRY(_class, _i1)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i2)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i3)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i4)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i5)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i6)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i7)                                     \
  NS_OFFSET_AND_INTERFACE_TABLE_END                                           \
  NS_OFFSET_AND_INTERFACE_TABLE_TO_MAP_SEGUE

#define NS_NODE_INTERFACE_TABLE8(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7,   \
                                 _i8)                                         \
  NS_NODE_OFFSET_AND_INTERFACE_TABLE_BEGIN(_class)                            \
    NS_INTERFACE_TABLE_ENTRY(_class, _i1)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i2)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i3)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i4)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i5)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i6)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i7)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i8)                                     \
  NS_OFFSET_AND_INTERFACE_TABLE_END                                           \
  NS_OFFSET_AND_INTERFACE_TABLE_TO_MAP_SEGUE


NS_DEFINE_STATIC_IID_ACCESSOR(nsINode, NS_INODE_IID)


#define NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER \
  nsContentUtils::TraceWrapper(tmp, aCallback, aClosure);

#define NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER \
  nsContentUtils::ReleaseWrapper(s, tmp);


#endif /* nsINode_h___ */
