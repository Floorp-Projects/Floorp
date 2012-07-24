/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsINode_h___
#define nsINode_h___

#include "nsIDOMEventTarget.h"
#include "nsEvent.h"
#include "nsPropertyTable.h"
#include "nsTObserverArray.h"
#include "nsINodeInfo.h"
#include "nsCOMPtr.h"
#include "nsWrapperCache.h"
#include "nsIProgrammingLanguage.h" // for ::JAVASCRIPT
#include "nsDOMError.h"
#include "nsDOMString.h"
#include "jspubtd.h"
#include "nsWindowMemoryReporter.h"
#include "nsIVariant.h"
#include "nsGkAtoms.h"

// Including 'windows.h' will #define GetClassInfo to something else.
#ifdef XP_WIN
#ifdef GetClassInfo
#undef GetClassInfo
#endif
#endif

class nsIContent;
class nsIDocument;
class nsIDOMEvent;
class nsIDOMNode;
class nsIDOMElement;
class nsIDOMNodeList;
class nsINodeList;
class nsIPresShell;
class nsEventChainVisitor;
class nsEventChainPreVisitor;
class nsEventChainPostVisitor;
class nsEventListenerManager;
class nsIPrincipal;
class nsIMutationObserver;
class nsChildContentList;
class nsNodeWeakReference;
class nsNodeSupportsWeakRefTearoff;
class nsIEditor;
class nsIDOMUserDataHandler;
class nsAttrAndChildArray;
class nsXPCClassInfo;

namespace mozilla {
namespace dom {
class Element;
} // namespace dom
} // namespace mozilla

enum {
  // This bit will be set if the node has a listener manager.
  NODE_HAS_LISTENERMANAGER =     0x00000001U,

  // Whether this node has had any properties set on it
  NODE_HAS_PROPERTIES =          0x00000002U,

  // Whether this node is the root of an anonymous subtree.  Note that this
  // need not be a native anonymous subtree.  Any anonymous subtree, including
  // XBL-generated ones, will do.  This flag is set-once: once a node has it,
  // it must not be removed.
  // NOTE: Should only be used on nsIContent nodes
  NODE_IS_ANONYMOUS =            0x00000004U,

  // Whether the node has some ancestor, possibly itself, that is native
  // anonymous.  This includes ancestors crossing XBL scopes, in cases when an
  // XBL binding is attached to an element which has a native anonymous
  // ancestor.  This flag is set-once: once a node has it, it must not be
  // removed.
  // NOTE: Should only be used on nsIContent nodes
  NODE_IS_IN_ANONYMOUS_SUBTREE = 0x00000008U,

  // Whether this node is the root of a native anonymous (from the perspective
  // of its parent) subtree.  This flag is set-once: once a node has it, it
  // must not be removed.
  // NOTE: Should only be used on nsIContent nodes
  NODE_IS_NATIVE_ANONYMOUS_ROOT = 0x00000010U,

  // Forces the XBL code to treat this node as if it were
  // in the document and therefore should get bindings attached.
  NODE_FORCE_XBL_BINDINGS =      0x00000020U,

  // Whether a binding manager may have a pointer to this
  NODE_MAY_BE_IN_BINDING_MNGR =  0x00000040U,

  NODE_IS_EDITABLE =             0x00000080U,

  // For all Element nodes, NODE_MAY_HAVE_CLASS is guaranteed to be set if the
  // node in fact has a class, but may be set even if it doesn't.
  NODE_MAY_HAVE_CLASS =          0x00000100U,

  NODE_IS_INSERTION_PARENT =     0x00000200U,

  // Node has an :empty or :-moz-only-whitespace selector
  NODE_HAS_EMPTY_SELECTOR =      0x00000400U,

  // A child of the node has a selector such that any insertion,
  // removal, or appending of children requires restyling the parent.
  NODE_HAS_SLOW_SELECTOR =       0x00000800U,

  // A child of the node has a :first-child, :-moz-first-node,
  // :only-child, :last-child or :-moz-last-node selector.
  NODE_HAS_EDGE_CHILD_SELECTOR = 0x00001000U,

  // A child of the node has a selector such that any insertion or
  // removal of children requires restyling later siblings of that
  // element.  Additionally (in this manner it is stronger than
  // NODE_HAS_SLOW_SELECTOR), if a child's style changes due to any
  // other content tree changes (e.g., the child changes to or from
  // matching :empty due to a grandchild insertion or removal), the
  // child's later siblings must also be restyled.
  NODE_HAS_SLOW_SELECTOR_LATER_SIBLINGS
                               = 0x00002000U,

  NODE_ALL_SELECTOR_FLAGS =      NODE_HAS_EMPTY_SELECTOR |
                                 NODE_HAS_SLOW_SELECTOR |
                                 NODE_HAS_EDGE_CHILD_SELECTOR |
                                 NODE_HAS_SLOW_SELECTOR_LATER_SIBLINGS,

  NODE_ATTACH_BINDING_ON_POSTCREATE
                               = 0x00004000U,

  // This node needs to go through frame construction to get a frame (or
  // undisplayed entry).
  NODE_NEEDS_FRAME =             0x00008000U,

  // At least one descendant in the flattened tree has NODE_NEEDS_FRAME set.
  // This should be set on every node on the flattened tree path between the
  // node(s) with NODE_NEEDS_FRAME and the root content.
  NODE_DESCENDANTS_NEED_FRAMES = 0x00010000U,

  // Set if the node has the accesskey attribute set.
  NODE_HAS_ACCESSKEY           = 0x00020000U,

  // Set if the node is handling a click.
  NODE_HANDLING_CLICK          = 0x00040000U,

  // Set if the node has had :hover selectors matched against it
  NODE_HAS_RELEVANT_HOVER_RULES = 0x00080000U,

  // Remaining bits are node type specific.
  NODE_TYPE_SPECIFIC_BITS_OFFSET =        20
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
 * have occurred.
 *
 * When a guard is instantiated sMutationCount is set to 300. It is then
 * decremented by every mutation (capped at 0). This means that we can only
 * detect 300 mutations during the lifetime of a single guard, however that
 * should be more then we ever care about as we usually only care if more then
 * one mutation has occurred.
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
   * Returns true if any unexpected mutations have occurred. You can pass in
   * an 8-bit ignore count to ignore a number of expected mutations.
   */
  bool Mutated(PRUint8 aIgnoreCount)
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

// Categories of node properties
// 0 is global.
#define DOM_USER_DATA         1
#define DOM_USER_DATA_HANDLER 2
#define SMIL_MAPPED_ATTR_ANIMVAL 3

// IID for the nsINode interface
#define NS_INODE_IID \
{ 0xf73e3890, 0xe4ab, 0x453e, \
  { 0x8c, 0x78, 0x2d, 0x1f, 0xa4, 0x0b, 0x48, 0x00 } }

/**
 * An internal interface that abstracts some DOMNode-related parts that both
 * nsIContent and nsIDocument share.  An instance of this interface has a list
 * of nsIContent children and provides access to them.
 */
class nsINode : public nsIDOMEventTarget,
                public nsWrapperCache
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_INODE_IID)

  // Among the sub-classes that inherit (directly or indirectly) from nsINode,
  // measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - nsGenericHTMLElement:  mForm, mFieldSet
  // - nsGenericHTMLFrameElement: mFrameLoader (bug 672539), mTitleChangedListener
  // - nsHTMLBodyElement:     mContentStyleRule
  // - nsHTMLDataListElement: mOptions
  // - nsHTMLFieldSetElement: mElements, mDependentElements, mFirstLegend
  // - nsHTMLFormElement:     many!
  // - nsHTMLFrameSetElement: mRowSpecs, mColSpecs
  // - nsHTMLInputElement:    mInputData, mFiles, mFileList, mStaticDocfileList
  // - nsHTMLMapElement:      mAreas
  // - nsHTMLMediaElement:    many!
  // - nsHTMLOutputElement:   mDefaultValue, mTokenList
  // - nsHTMLRowElement:      mCells
  // - nsHTMLSelectElement:   mOptions, mRestoreState
  // - nsHTMLTableElement:    mTBodies, mRows, mTableInheritedAttributes
  // - nsHTMLTableSectionElement: mRows
  // - nsHTMLTextAreaElement: mControllers, mState
  //
  // The following members don't need to be measured:
  // - nsIContent: mPrimaryFrame, because it's non-owning and measured elsewhere
  //
  NS_DECL_SIZEOF_EXCLUDING_THIS

  // SizeOfIncludingThis doesn't need to be overridden by sub-classes because
  // sub-classes of nsINode are guaranteed to be laid out in memory in such a
  // way that |this| points to the start of the allocated object, even in
  // methods of nsINode's sub-classes, and so |aMallocSizeOf(this)| is always
  // safe to call no matter which object it was invoked on.
  virtual size_t SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  friend class nsNodeUtils;
  friend class nsNodeWeakReference;
  friend class nsNodeSupportsWeakRefTearoff;
  friend class nsAttrAndChildArray;

#ifdef MOZILLA_INTERNAL_API
#ifdef _MSC_VER
#pragma warning(push)
// Disable annoying warning about 'this' in initializers.
#pragma warning(disable:4355)
#endif
  nsINode(already_AddRefed<nsINodeInfo> aNodeInfo)
  : mNodeInfo(aNodeInfo),
    mParent(nsnull),
    mFlags(0),
    mBoolFlags(0),
    mNextSibling(nsnull),
    mPreviousSibling(nsnull),
    mFirstChild(nsnull),
    mSubtreeRoot(this),
    mSlots(nsnull)
  {
  }

#ifdef _MSC_VER
#pragma warning(pop)
#endif
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
    /** text nodes */
    eTEXT                = 1 << 3,
    /** xml processing instructions */
    ePROCESSING_INSTRUCTION = 1 << 4,
    /** comment nodes */
    eCOMMENT             = 1 << 5,
    /** form control elements */
    eHTML_FORM_CONTROL   = 1 << 6,
    /** document fragments */
    eDOCUMENT_FRAGMENT   = 1 << 7,
    /** data nodes (comments, PIs, text). Nodes of this type always
     returns a non-null value for nsIContent::GetText() */
    eDATA_NODE           = 1 << 8,
    /** nsHTMLMediaElement */
    eMEDIA               = 1 << 9,
    /** animation elements */
    eANIMATION           = 1 << 10
  };

  /**
   * API for doing a quick check if a content is of a given
   * type, such as Text, Document, Comment ...  Use this when you can instead of
   * checking the tag.
   *
   * @param aFlags what types you want to test for (see above)
   * @return whether the content matches ALL flags passed in
   */
  virtual bool IsNodeOfType(PRUint32 aFlags) const = 0;

  /**
   * Return whether the node is an Element node
   */
  bool IsElement() const {
    return GetBoolFlag(NodeIsElement);
  }

  /**
   * Return this node as an Element.  Should only be used for nodes
   * for which IsElement() is true.  This is defined inline in Element.h.
   */
  mozilla::dom::Element* AsElement();
  const mozilla::dom::Element* AsElement() const;

  /**
   * Return this node as nsIContent.  Should only be used for nodes for which
   * IsContent() is true.  This is defined inline in nsIContent.h.
   */
  nsIContent* AsContent();

  virtual nsIDOMNode* AsDOMNode() = 0;

  /**
   * Return if this node has any children.
   */
  bool HasChildren() const { return !!mFirstChild; }

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
  nsIDocument *OwnerDoc() const
  {
    return mNodeInfo->GetDocument();
  }

  /**
   * Return the "owner document" of this node as an nsINode*.  Implemented
   * in nsIDocument.h.
   */
  nsINode *OwnerDocAsNode() const;

  /**
   * Returns true if the content has an ancestor that is a document.
   *
   * @return whether this content is in a document tree
   */
  bool IsInDoc() const
  {
    return GetBoolFlag(IsInDocument);
  }

  /**
   * Get the document that this content is currently in, if any. This will be
   * null if the content has no ancestor that is a document.
   *
   * @return the current document
   */
  nsIDocument *GetCurrentDoc() const
  {
    return IsInDoc() ? OwnerDoc() : nsnull;
  }

  /**
   * The values returned by this function are the ones defined for
   * nsIDOMNode.nodeType
   */
  PRUint16 NodeType() const
  {
    return mNodeInfo->NodeType();
  }
  const nsString& NodeName() const
  {
    return mNodeInfo->NodeName();
  }
  const nsString& LocalName() const
  {
    return mNodeInfo->LocalName();
  }

  /**
   * Get the tag for this element. This will always return a non-null atom
   * pointer (as implied by the naming of the method).  For elements this is
   * the non-namespaced tag, and for other nodes it's something like "#text",
   * "#comment", "#document", etc.
   */
  nsIAtom* Tag() const
  {
    return mNodeInfo->NameAtom();
  }

  nsINode*
  InsertBefore(nsINode *aNewChild, nsINode *aRefChild, nsresult *aReturn)
  {
    return ReplaceOrInsertBefore(false, aNewChild, aRefChild, aReturn);
  }
  nsINode*
  ReplaceChild(nsINode *aNewChild, nsINode *aOldChild, nsresult *aReturn)
  {
    return ReplaceOrInsertBefore(true, aNewChild, aOldChild, aReturn);
  }
  nsINode*
  AppendChild(nsINode *aNewChild, nsresult *aReturn)
  {
    return InsertBefore(aNewChild, nsnull, aReturn);
  }
  nsresult RemoveChild(nsINode *aOldChild);

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
   * nsIDocument::GetRootElement() first if you're not sure.  Apart from this
   * one constraint, this doesn't do any checking on whether aKid is a valid
   * child of |this|.
   *
   * @throws NS_ERROR_OUT_OF_MEMORY in some cases (from BindToTree).
   */
  virtual nsresult InsertChildAt(nsIContent* aKid, PRUint32 aIndex,
                                 bool aNotify) = 0;

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
   * nsIDocument::GetRootElement() first if you're not sure.  Apart from this
   * one constraint, this doesn't do any checking on whether aKid is a valid
   * child of |this|.
   *
   * @throws NS_ERROR_OUT_OF_MEMORY in some cases (from BindToTree).
   */
  nsresult AppendChildTo(nsIContent* aKid, bool aNotify)
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
   *
   * Note: If there is no child at aIndex, this method will simply do nothing.
   */
  virtual void RemoveChildAt(PRUint32 aIndex, bool aNotify) = 0;

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
   * @param aTransfer      if true the property will not be deleted when the
   *                       ownerDocument of the node changes, if false it
   *                       will be deleted.
   *
   * @return NS_PROPTABLE_PROP_OVERWRITTEN (success value) if the property
   *                                       was already set
   * @throws NS_ERROR_OUT_OF_MEMORY if that occurs
   */
  nsresult SetProperty(nsIAtom *aPropertyName,
                       void *aValue,
                       NSPropertyDtorFunc aDtor = nsnull,
                       bool aTransfer = false)
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
   * @param aTransfer       if true the property will not be deleted when the
   *                        ownerDocument of the node changes, if false it
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
                               bool aTransfer = false,
                               void **aOldValue = nsnull);

  /**
   * Destroys a property associated with this node. The value is destroyed
   * using the destruction function given when that value was set.
   *
   * @param aPropertyName  name of property to destroy.
   */
  void DeleteProperty(nsIAtom *aPropertyName)
  {
    DeleteProperty(0, aPropertyName);
  }

  /**
   * Destroys a property associated with this node. The value is destroyed
   * using the destruction function given when that value was set.
   *
   * @param aCategory      category of property to destroy.
   * @param aPropertyName  name of property to destroy.
   */
  virtual void DeleteProperty(PRUint16 aCategory, nsIAtom *aPropertyName);

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
  
  bool HasProperties() const
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
  nsIContent* GetParent() const {
    return NS_LIKELY(GetBoolFlag(ParentIsContent)) ?
      reinterpret_cast<nsIContent*>(mParent) : nsnull;
  }

  /**
   * Get the parent nsINode for this node. This can be either an nsIContent,
   * an nsIDocument or an nsIAttribute.
   * @return the parent node
   */
  nsINode* GetNodeParent() const
  {
    return mParent;
  }
  
  /**
   * Get the parent nsINode for this node if it is an Element.
   * @return the parent node
   */
  mozilla::dom::Element* GetElementParent() const
  {
    return mParent && mParent->IsElement() ? mParent->AsElement() : nsnull;
  }

  /**
   * Get the root of the subtree this node belongs to.  This never returns
   * null.  It may return 'this' (e.g. for document nodes, and nodes that
   * are the roots of disconnected subtrees).
   */
  nsINode* SubtreeRoot() const
  {
    // There are three cases of interest here.  nsINodes that are really:
    // 1. nsIDocument nodes - Are always in the document.
    // 2. nsIContent nodes - Are either in the document, or mSubtreeRoot
    //    is updated in BindToTree/UnbindFromTree.
    // 3. nsIAttribute nodes - Are never in the document, and mSubtreeRoot
    //    is always 'this' (as set in nsINode's ctor).
    nsINode* node = IsInDoc() ? OwnerDocAsNode() : mSubtreeRoot;
    NS_ASSERTION(node, "Should always have a node here!");
#ifdef DEBUG
    {
      const nsINode* slowNode = this;
      const nsINode* iter = slowNode;
      while ((iter = iter->GetNodeParent())) {
        slowNode = iter;
      }

      NS_ASSERTION(slowNode == node, "These should always be in sync!");
    }
#endif
    return node;
  }

  /**
   * See nsIDOMEventTarget
   */
  NS_DECL_NSIDOMEVENTTARGET
  using nsIDOMEventTarget::AddEventListener;
  using nsIDOMEventTarget::AddSystemEventListener;

  /**
   * Adds a mutation observer to be notified when this node, or any of its
   * descendants, are modified. The node will hold a weak reference to the
   * observer, which means that it is the responsibility of the observer to
   * remove itself in case it dies before the node.  If an observer is added
   * while observers are being notified, it may also be notified.  In general,
   * adding observers while inside a notification is not a good idea.  An
   * observer that is already observing the node must not be added without
   * being removed first.
   */
  void AddMutationObserver(nsIMutationObserver* aMutationObserver)
  {
    nsSlots* s = GetSlots();
    if (s) {
      NS_ASSERTION(s->mMutationObservers.IndexOf(aMutationObserver) ==
                   nsTArray<int>::NoIndex,
                   "Observer already in the list");
      s->mMutationObservers.AppendElement(aMutationObserver);
    }
  }

  /**
   * Same as above, but only adds the observer if its not observing
   * the node already.
   */
  void AddMutationObserverUnlessExists(nsIMutationObserver* aMutationObserver)
  {
    nsSlots* s = GetSlots();
    if (s) {
      s->mMutationObservers.AppendElementUnlessExists(aMutationObserver);
    }
  }

  /**
   * Removes a mutation observer.
   */
  void RemoveMutationObserver(nsIMutationObserver* aMutationObserver)
  {
    nsSlots* s = GetExistingSlots();
    if (s) {
      s->mMutationObservers.RemoveElement(aMutationObserver);
    }
  }

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
   * when they don't have an ownerDocument. If this function returns true
   * it doesn't mean that the nodes actually have an ownerDocument.
   *
   * @param aOther Other node to check
   * @return Whether the owner documents of this node and of aOther are the
   *         same.
   */
  bool HasSameOwnerDoc(nsINode *aOther)
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
    nsSlots()
      : mChildNodes(nsnull),
        mWeakReference(nsnull)
    {
    }

    // If needed we could remove the vtable pointer this dtor causes by
    // putting a DestroySlots function on nsINode
    virtual ~nsSlots();

    void Traverse(nsCycleCollectionTraversalCallback &cb);
    void Unlink();

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

  bool HasFlag(PtrBits aFlag) const
  {
    return !!(GetFlags() & aFlag);
  }

  PRUint32 GetFlags() const
  {
    return mFlags;
  }

  void SetFlags(PRUint32 aFlagsToSet)
  {
    NS_ASSERTION(!(aFlagsToSet & (NODE_IS_ANONYMOUS |
                                  NODE_IS_NATIVE_ANONYMOUS_ROOT |
                                  NODE_IS_IN_ANONYMOUS_SUBTREE |
                                  NODE_ATTACH_BINDING_ON_POSTCREATE |
                                  NODE_DESCENDANTS_NEED_FRAMES |
                                  NODE_NEEDS_FRAME)) ||
                 IsNodeOfType(eCONTENT),
                 "Flag only permitted on nsIContent nodes");
    mFlags |= aFlagsToSet;
  }

  void UnsetFlags(PRUint32 aFlagsToUnset)
  {
    NS_ASSERTION(!(aFlagsToUnset &
                   (NODE_IS_ANONYMOUS |
                    NODE_IS_IN_ANONYMOUS_SUBTREE |
                    NODE_IS_NATIVE_ANONYMOUS_ROOT)),
                 "Trying to unset write-only flags");
    mFlags &= ~aFlagsToUnset;
  }

  void SetEditableFlag(bool aEditable)
  {
    if (aEditable) {
      SetFlags(NODE_IS_EDITABLE);
    }
    else {
      UnsetFlags(NODE_IS_EDITABLE);
    }
  }

  bool IsEditable() const
  {
#ifdef _IMPL_NS_LAYOUT
    return IsEditableInternal();
#else
    return IsEditableExternal();
#endif
  }

  /**
   * Returns true if |this| or any of its ancestors is native anonymous.
   */
  bool IsInNativeAnonymousSubtree() const
  {
#ifdef DEBUG
    if (HasFlag(NODE_IS_IN_ANONYMOUS_SUBTREE)) {
      return true;
    }
    CheckNotNativeAnonymous();
    return false;
#else
    return HasFlag(NODE_IS_IN_ANONYMOUS_SUBTREE);
#endif
  }

  /**
   * Returns true if |this| node is the common ancestor of the start/end
   * nodes of a Range in a Selection or a descendant of such a common ancestor.
   * This node is definitely not selected when |false| is returned, but it may
   * or may not be selected when |true| is returned.
   */
  bool IsSelectionDescendant() const
  {
    return IsDescendantOfCommonAncestorForRangeInSelection() ||
           IsCommonAncestorForRangeInSelection();
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
   * node. Be aware that if this node and the computed selection limiter are
   * not in same subtree, this returns the root content of the closeset subtree.
   */
  nsIContent* GetSelectionRootContent(nsIPresShell* aPresShell);

  virtual nsINodeList* GetChildNodesList();
  nsIContent* GetFirstChild() const { return mFirstChild; }
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

  nsresult Normalize();

  /**
   * Get the base URI for any relative URIs within this piece of
   * content. Generally, this is the document's base URI, but certain
   * content carries a local base for backward compatibility, and XML
   * supports setting a per-node base URI.
   *
   * @return the base URI
   */
  virtual already_AddRefed<nsIURI> GetBaseURI() const = 0;

  /**
   * Facility for explicitly setting a base URI on a node.
   */
  nsresult SetExplicitBaseURI(nsIURI* aURI);
  /**
   * The explicit base URI, if set, otherwise null
   */
protected:
  nsIURI* GetExplicitBaseURI() const {
    if (HasExplicitBaseURI()) {
      return static_cast<nsIURI*>(GetProperty(nsGkAtoms::baseURIProperty));
    }
    return nsnull;
  }
  
public:
  nsresult GetDOMBaseURI(nsAString &aURI) const;

  // Note! This function must never fail. It only return an nsresult so that
  // we can use it to implement nsIDOMNode
  NS_IMETHOD GetTextContent(nsAString &aTextContent)
  {
    SetDOMStringToNull(aTextContent);
    return NS_OK;
  }
  NS_IMETHOD SetTextContent(const nsAString& aTextContent)
  {
    return NS_OK;
  }

  /**
   * Associate an object aData to aKey on this node. If aData is null any
   * previously registered object and UserDataHandler associated to aKey on
   * this node will be removed.
   * Should only be used to implement the DOM Level 3 UserData API.
   *
   * @param aKey the key to associate the object to
   * @param aData the object to associate to aKey on this node (may be null)
   * @param aHandler the UserDataHandler to call when the node is
   *                 cloned/deleted/imported/renamed (may be null)
   * @param aResult [out] the previously registered object for aKey on this
   *                      node, if any
   * @return whether adding the object and UserDataHandler succeeded
   */
  nsresult SetUserData(const nsAString& aKey, nsIVariant* aData,
                       nsIDOMUserDataHandler* aHandler, nsIVariant** aResult);

  /**
   * Get the UserData object registered for a Key on this node, if any.
   * Should only be used to implement the DOM Level 3 UserData API.
   *
   * @param aKey the key to get UserData for
   * @return aResult the previously registered object for aKey on this node, if
   *                 any
   */
  nsIVariant* GetUserData(const nsAString& aKey)
  {
    nsCOMPtr<nsIAtom> key = do_GetAtom(aKey);
    if (!key) {
      return nsnull;
    }

    return static_cast<nsIVariant*>(GetProperty(DOM_USER_DATA, key));
  }

  nsresult GetUserData(const nsAString& aKey, nsIVariant** aResult)
  {
    NS_IF_ADDREF(*aResult = GetUserData(aKey));
  
    return NS_OK;
  }


  /**
   * Compares the document position of a node to this node.
   *
   * @param aOtherNode The node whose position is being compared to this node
   *
   * @return  The document position flags of the nodes. aOtherNode is compared
   *          to this node, i.e. if aOtherNode is before this node then
   *          DOCUMENT_POSITION_PRECEDING will be set.
   *
   * @see nsIDOMNode
   */
  PRUint16 CompareDocPosition(nsINode* aOtherNode);
  nsresult CompareDocPosition(nsINode* aOtherNode, PRUint16* aReturn)
  {
    NS_ENSURE_ARG(aOtherNode);
    *aReturn = CompareDocPosition(aOtherNode);
    return NS_OK;
  }
  nsresult CompareDocumentPosition(nsIDOMNode* aOther,
                                   PRUint16* aReturn);

  nsresult LookupPrefix(const nsAString& aNamespaceURI, nsAString& aPrefix);
  nsresult IsDefaultNamespace(const nsAString& aNamespaceURI, bool* aResult)
  {
    nsAutoString defaultNamespace;
    LookupNamespaceURI(EmptyString(), defaultNamespace);
    *aResult = aNamespaceURI.Equals(defaultNamespace);
    return NS_OK;
  }
  nsresult LookupNamespaceURI(const nsAString& aNamespacePrefix,
                              nsAString& aNamespaceURI);

  nsresult IsEqualNode(nsIDOMNode* aOther, bool* aReturn);
  bool IsEqualTo(nsINode* aOther);

  nsIContent* GetNextSibling() const { return mNextSibling; }
  nsIContent* GetPreviousSibling() const { return mPreviousSibling; }

  /**
   * Get the next node in the pre-order tree traversal of the DOM.  If
   * aRoot is non-null, then it must be an ancestor of |this|
   * (possibly equal to |this|) and only nodes that are descendants of
   * aRoot, not including aRoot itself, will be returned.  Returns
   * null if there are no more nodes to traverse.
   */
  nsIContent* GetNextNode(const nsINode* aRoot = nsnull) const
  {
    return GetNextNodeImpl(aRoot, false);
  }

  /**
   * Get the next node in the pre-order tree traversal of the DOM but ignoring
   * the children of this node.  If aRoot is non-null, then it must be an
   * ancestor of |this| (possibly equal to |this|) and only nodes that are
   * descendants of aRoot, not including aRoot itself, will be returned.
   * Returns null if there are no more nodes to traverse.
   */
  nsIContent* GetNextNonChildNode(const nsINode* aRoot = nsnull) const
  {
    return GetNextNodeImpl(aRoot, true);
  }

  /**
   * Returns true if 'this' is either document or element or
   * document fragment and aOther is a descendant in the same
   * anonymous tree.
   */
  bool Contains(const nsINode* aOther) const;
  nsresult Contains(nsIDOMNode* aOther, bool* aReturn);

  bool UnoptimizableCCNode() const;

private:

  nsIContent* GetNextNodeImpl(const nsINode* aRoot,
                              const bool aSkipChildren) const
  {
    // Can't use nsContentUtils::ContentIsDescendantOf here, since we
    // can't include it here.
#ifdef DEBUG
    if (aRoot) {
      const nsINode* cur = this;
      for (; cur; cur = cur->GetNodeParent())
        if (cur == aRoot) break;
      NS_ASSERTION(cur, "aRoot not an ancestor of |this|?");
    }
#endif
    if (!aSkipChildren) {
      nsIContent* kid = GetFirstChild();
      if (kid) {
        return kid;
      }
    }
    if (this == aRoot) {
      return nsnull;
    }
    const nsINode* cur = this;
    while (1) {
      nsIContent* next = cur->GetNextSibling();
      if (next) {
        return next;
      }
      nsINode* parent = cur->GetNodeParent();
      if (parent == aRoot) {
        return nsnull;
      }
      cur = parent;
    }
    NS_NOTREACHED("How did we get here?");
  }

public:

  /**
   * Get the previous nsIContent in the pre-order tree traversal of the DOM.  If
   * aRoot is non-null, then it must be an ancestor of |this|
   * (possibly equal to |this|) and only nsIContents that are descendants of
   * aRoot, including aRoot itself, will be returned.  Returns
   * null if there are no more nsIContents to traverse.
   */
  nsIContent* GetPreviousContent(const nsINode* aRoot = nsnull) const
  {
      // Can't use nsContentUtils::ContentIsDescendantOf here, since we
      // can't include it here.
#ifdef DEBUG
      if (aRoot) {
        const nsINode* cur = this;
        for (; cur; cur = cur->GetNodeParent())
          if (cur == aRoot) break;
        NS_ASSERTION(cur, "aRoot not an ancestor of |this|?");
      }
#endif

    if (this == aRoot) {
      return nsnull;
    }
    nsIContent* cur = this->GetParent();
    nsIContent* iter = this->GetPreviousSibling();
    while (iter) {
      cur = iter;
      iter = reinterpret_cast<nsINode*>(iter)->GetLastChild();
    }
    return cur;
  }

  /**
   * Boolean flags
   */
private:
  enum BooleanFlag {
    // Set if we're being used from -moz-element
    NodeHasRenderingObservers,
    // Set if our parent chain (including this node itself) terminates
    // in a document
    IsInDocument,
    // Set if mParent is an nsIContent
    ParentIsContent,
    // Set if this node is an Element
    NodeIsElement,
    // Set if the element has a non-empty id attribute. This can in rare
    // cases lie for nsXMLElement, such as when the node has been moved between
    // documents with different id mappings.
    ElementHasID,
    // Set if the element might have inline style.
    ElementMayHaveStyle,
    // Set if the element has a name attribute set.
    ElementHasName,
    // Set if the element might have a contenteditable attribute set.
    ElementMayHaveContentEditableAttr,
    // Set if the node is the common ancestor of the start/end nodes of a Range
    // that is in a Selection.
    NodeIsCommonAncestorForRangeInSelection,
    // Set if the node is a descendant of a node with the above bit set.
    NodeIsDescendantOfCommonAncestorForRangeInSelection,
    // Set if CanSkipInCC check has been done for this subtree root.
    NodeIsCCMarkedRoot,
    // Maybe set if this node is in black subtree.
    NodeIsCCBlackTree,
    // Maybe set if the node is a root of a subtree 
    // which needs to be kept in the purple buffer.
    NodeIsPurpleRoot,
    // Set if the node has an explicit base URI stored
    NodeHasExplicitBaseURI,
    // Set if the element has some style states locked
    ElementHasLockedStyleStates,
    // Set if element has pointer locked
    ElementHasPointerLock,
    // Set if the node may have DOMMutationObserver attached to it.
    NodeMayHaveDOMMutationObserver,
    // Set if node is Content
    NodeIsContent,
    // Guard value
    BooleanFlagCount
  };

  void SetBoolFlag(BooleanFlag name, bool value) {
    PR_STATIC_ASSERT(BooleanFlagCount <= 8*sizeof(mBoolFlags));
    mBoolFlags = (mBoolFlags & ~(1 << name)) | (value << name);
  }

  void SetBoolFlag(BooleanFlag name) {
    PR_STATIC_ASSERT(BooleanFlagCount <= 8*sizeof(mBoolFlags));
    mBoolFlags |= (1 << name);
  }

  void ClearBoolFlag(BooleanFlag name) {
    PR_STATIC_ASSERT(BooleanFlagCount <= 8*sizeof(mBoolFlags));
    mBoolFlags &= ~(1 << name);
  }

  bool GetBoolFlag(BooleanFlag name) const {
    PR_STATIC_ASSERT(BooleanFlagCount <= 8*sizeof(mBoolFlags));
    return mBoolFlags & (1 << name);
  }

public:
  bool HasRenderingObservers() const
    { return GetBoolFlag(NodeHasRenderingObservers); }
  void SetHasRenderingObservers(bool aValue)
    { SetBoolFlag(NodeHasRenderingObservers, aValue); }
  bool IsContent() const { return GetBoolFlag(NodeIsContent); }
  bool HasID() const { return GetBoolFlag(ElementHasID); }
  bool MayHaveStyle() const { return GetBoolFlag(ElementMayHaveStyle); }
  bool HasName() const { return GetBoolFlag(ElementHasName); }
  bool MayHaveContentEditableAttr() const
    { return GetBoolFlag(ElementMayHaveContentEditableAttr); }
  bool IsCommonAncestorForRangeInSelection() const
    { return GetBoolFlag(NodeIsCommonAncestorForRangeInSelection); }
  void SetCommonAncestorForRangeInSelection()
    { SetBoolFlag(NodeIsCommonAncestorForRangeInSelection); }
  void ClearCommonAncestorForRangeInSelection()
    { ClearBoolFlag(NodeIsCommonAncestorForRangeInSelection); }
  bool IsDescendantOfCommonAncestorForRangeInSelection() const
    { return GetBoolFlag(NodeIsDescendantOfCommonAncestorForRangeInSelection); }
  void SetDescendantOfCommonAncestorForRangeInSelection()
    { SetBoolFlag(NodeIsDescendantOfCommonAncestorForRangeInSelection); }
  void ClearDescendantOfCommonAncestorForRangeInSelection()
    { ClearBoolFlag(NodeIsDescendantOfCommonAncestorForRangeInSelection); }

  void SetCCMarkedRoot(bool aValue)
    { SetBoolFlag(NodeIsCCMarkedRoot, aValue); }
  bool CCMarkedRoot() const { return GetBoolFlag(NodeIsCCMarkedRoot); }
  void SetInCCBlackTree(bool aValue)
    { SetBoolFlag(NodeIsCCBlackTree, aValue); }
  bool InCCBlackTree() const { return GetBoolFlag(NodeIsCCBlackTree); }
  void SetIsPurpleRoot(bool aValue)
    { SetBoolFlag(NodeIsPurpleRoot, aValue); }
  bool IsPurpleRoot() const { return GetBoolFlag(NodeIsPurpleRoot); }
  bool MayHaveDOMMutationObserver()
    { return GetBoolFlag(NodeMayHaveDOMMutationObserver); }
  void SetMayHaveDOMMutationObserver()
    { SetBoolFlag(NodeMayHaveDOMMutationObserver, true); }
  bool HasListenerManager() { return HasFlag(NODE_HAS_LISTENERMANAGER); }
  bool HasPointerLock() const { return GetBoolFlag(ElementHasPointerLock); }
  void SetPointerLock() { SetBoolFlag(ElementHasPointerLock); }
  void ClearPointerLock() { ClearBoolFlag(ElementHasPointerLock); }
protected:
  void SetParentIsContent(bool aValue) { SetBoolFlag(ParentIsContent, aValue); }
  void SetInDocument() { SetBoolFlag(IsInDocument); }
  void SetNodeIsContent() { SetBoolFlag(NodeIsContent); }
  void ClearInDocument() { ClearBoolFlag(IsInDocument); }
  void SetIsElement() { SetBoolFlag(NodeIsElement); }
  void ClearIsElement() { ClearBoolFlag(NodeIsElement); }
  void SetHasID() { SetBoolFlag(ElementHasID); }
  void ClearHasID() { ClearBoolFlag(ElementHasID); }
  void SetMayHaveStyle() { SetBoolFlag(ElementMayHaveStyle); }
  void SetHasName() { SetBoolFlag(ElementHasName); }
  void ClearHasName() { ClearBoolFlag(ElementHasName); }
  void SetMayHaveContentEditableAttr()
    { SetBoolFlag(ElementMayHaveContentEditableAttr); }
  bool HasExplicitBaseURI() const { return GetBoolFlag(NodeHasExplicitBaseURI); }
  void SetHasExplicitBaseURI() { SetBoolFlag(NodeHasExplicitBaseURI); }
  void SetHasLockedStyleStates() { SetBoolFlag(ElementHasLockedStyleStates); }
  void ClearHasLockedStyleStates() { ClearBoolFlag(ElementHasLockedStyleStates); }
  bool HasLockedStyleStates() const
    { return GetBoolFlag(ElementHasLockedStyleStates); }

    void SetSubtreeRootPointer(nsINode* aSubtreeRoot)
  {
    NS_ASSERTION(aSubtreeRoot, "aSubtreeRoot can never be null!");
    NS_ASSERTION(!(IsNodeOfType(eCONTENT) && IsInDoc()), "Shouldn't be here!");
    mSubtreeRoot = aSubtreeRoot;
  }

  void ClearSubtreeRootPointer()
  {
    mSubtreeRoot = nsnull;
  }

public:
  // Optimized way to get classinfo.
  virtual nsXPCClassInfo* GetClassInfo() = 0;

  // Makes nsINode object to keep aObject alive.
  void BindObject(nsISupports* aObject);
  // After calling UnbindObject nsINode object doesn't keep
  // aObject alive anymore.
  void UnbindObject(nsISupports* aObject);

  /**
   * Returns the length of this node, as specified at
   * <http://dvcs.w3.org/hg/domcore/raw-file/tip/Overview.html#concept-node-length>
   */
  PRUint32 Length() const;

protected:

  // Override this function to create a custom slots class.
  virtual nsINode::nsSlots* CreateSlots();

  bool HasSlots() const
  {
    return mSlots != nsnull;
  }

  nsSlots* GetExistingSlots() const
  {
    return mSlots;
  }

  nsSlots* GetSlots()
  {
    if (!HasSlots()) {
      mSlots = CreateSlots();
    }
    return GetExistingSlots();
  }

  nsTObserverArray<nsIMutationObserver*> *GetMutationObservers()
  {
    return HasSlots() ? &GetExistingSlots()->mMutationObservers : nsnull;
  }

  bool IsEditableInternal() const;
  virtual bool IsEditableExternal() const
  {
    return IsEditableInternal();
  }

#ifdef DEBUG
  // Note: virtual so that IsInNativeAnonymousSubtree can be called accross
  // module boundaries.
  virtual void CheckNotNativeAnonymous() const;
#endif

  nsresult GetParentNode(nsIDOMNode** aParentNode);
  nsresult GetParentElement(nsIDOMElement** aParentElement);
  nsresult GetChildNodes(nsIDOMNodeList** aChildNodes);
  nsresult GetFirstChild(nsIDOMNode** aFirstChild);
  nsresult GetLastChild(nsIDOMNode** aLastChild);
  nsresult GetPreviousSibling(nsIDOMNode** aPrevSibling);
  nsresult GetNextSibling(nsIDOMNode** aNextSibling);
  nsresult GetOwnerDocument(nsIDOMDocument** aOwnerDocument);

  nsresult ReplaceOrInsertBefore(bool aReplace, nsIDOMNode *aNewChild,
                                 nsIDOMNode *aRefChild, nsIDOMNode **aReturn);
  nsINode* ReplaceOrInsertBefore(bool aReplace, nsINode *aNewChild,
                                 nsINode *aRefChild, nsresult *aReturn)
  {
    *aReturn = ReplaceOrInsertBefore(aReplace, aNewChild, aRefChild);
    if (NS_FAILED(*aReturn)) {
      return nsnull;
    }

    return aReplace ? aRefChild : aNewChild;
  }
  virtual nsresult ReplaceOrInsertBefore(bool aReplace, nsINode* aNewChild,
                                         nsINode* aRefChild);
  nsresult RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn);

  /**
   * Returns the Element that should be used for resolving namespaces
   * on this node (ie the ownerElement for attributes, the documentElement for
   * documents, the node itself for elements and for other nodes the parentNode
   * if it is an element).
   */
  virtual mozilla::dom::Element* GetNameSpaceElement() = 0;

  /**
   * Most of the implementation of the nsINode RemoveChildAt method.
   * Should only be called on document, element, and document fragment
   * nodes.  The aChildArray passed in should be the one for |this|.
   *
   * @param aIndex The index to remove at.
   * @param aNotify Whether to notify.
   * @param aKid The kid at aIndex.  Must not be null.
   * @param aChildArray The child array to work with.
   * @param aMutationEvent whether to fire a mutation event for this removal.
   */
  void doRemoveChildAt(PRUint32 aIndex, bool aNotify, nsIContent* aKid,
                       nsAttrAndChildArray& aChildArray);

  /**
   * Most of the implementation of the nsINode InsertChildAt method.
   * Should only be called on document, element, and document fragment
   * nodes.  The aChildArray passed in should be the one for |this|.
   *
   * @param aKid The child to insert.
   * @param aIndex The index to insert at.
   * @param aNotify Whether to notify.
   * @param aChildArray The child array to work with
   */
  nsresult doInsertChildAt(nsIContent* aKid, PRUint32 aIndex,
                           bool aNotify, nsAttrAndChildArray& aChildArray);

public:
  /* Event stuff that documents and elements share.  This needs to be
     NS_IMETHOD because some subclasses implement DOM methods with
     this exact name and signature and then the calling convention
     needs to match.

     Note that we include DOCUMENT_ONLY_EVENT events here so that we
     can forward all the document stuff to this implementation.
  */
#define EVENT(name_, id_, type_, struct_)                         \
  NS_IMETHOD GetOn##name_(JSContext *cx, JS::Value *vp);          \
  NS_IMETHOD SetOn##name_(JSContext *cx, const JS::Value &v);
#define TOUCH_EVENT EVENT
#define DOCUMENT_ONLY_EVENT EVENT
#include "nsEventNameList.h"
#undef DOCUMENT_ONLY_EVENT
#undef TOUCH_EVENT
#undef EVENT  

protected:
  static void Trace(nsINode *tmp, TraceCallback cb, void *closure);
  static bool Traverse(nsINode *tmp, nsCycleCollectionTraversalCallback &cb);
  static void Unlink(nsINode *tmp);

  nsCOMPtr<nsINodeInfo> mNodeInfo;

  nsINode* mParent;

  PRUint32 mFlags;

private:
  // Boolean flags.
  PRUint32 mBoolFlags;

protected:
  nsIContent* mNextSibling;
  nsIContent* mPreviousSibling;
  nsIContent* mFirstChild;

  union {
    // Pointer to our primary frame.  Might be null.
    nsIFrame* mPrimaryFrame;

    // Pointer to the root of our subtree.  Might be null.
    nsINode* mSubtreeRoot;
  };

  // Storage for more members that are usually not needed; allocated lazily.
  nsSlots* mSlots;
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


#endif /* nsINode_h___ */
