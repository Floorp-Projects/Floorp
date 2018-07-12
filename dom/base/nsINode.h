/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsINode_h___
#define nsINode_h___

#include "mozilla/Likely.h"
#include "mozilla/UniquePtr.h"
#include "nsCOMPtr.h"               // for member, local
#include "nsGkAtoms.h"              // for nsGkAtoms::baseURIProperty
#include "mozilla/dom/NodeInfo.h"            // member (in nsCOMPtr)
#include "nsIVariant.h"             // for use in GetUserData()
#include "nsIWeakReference.h"
#include "nsNodeInfoManager.h"      // for use in NodePrincipal()
#include "nsPropertyTable.h"        // for typedefs
#include "nsTObserverArray.h"       // for member
#include "mozilla/ErrorResult.h"
#include "mozilla/LinkedList.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/EventTarget.h" // for base class
#include "js/TypeDecls.h"     // for Handle, Value, JSObject, JSContext
#include "mozilla/dom/DOMString.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/NodeBinding.h"
#include "nsTHashtable.h"
#include <iosfwd>

// Including 'windows.h' will #define GetClassInfo to something else.
#ifdef XP_WIN
#ifdef GetClassInfo
#undef GetClassInfo
#endif
#endif

class nsAttrAndChildArray;
class nsAttrChildContentList;
class nsDOMAttributeMap;
class nsIAnimationObserver;
class nsIContent;
class nsIDocument;
class nsIFrame;
class nsIMutationObserver;
class nsINode;
class nsINodeList;
class nsIPresShell;
class nsIPrincipal;
class nsIURI;
class nsNodeSupportsWeakRefTearoff;
class nsDOMMutationObserver;
class nsRange;
class nsWindowSizes;
struct RawServoSelectorList;

namespace mozilla {
class EventListenerManager;
class TextEditor;
namespace dom {
/**
 * @return true if aChar is what the WHATWG defines as a 'ascii whitespace'.
 * https://infra.spec.whatwg.org/#ascii-whitespace
 */
inline bool IsSpaceCharacter(char16_t aChar) {
  return aChar == ' ' || aChar == '\t' || aChar == '\n' || aChar == '\r' ||
         aChar == '\f';
}
inline bool IsSpaceCharacter(char aChar) {
  return aChar == ' ' || aChar == '\t' || aChar == '\n' || aChar == '\r' ||
         aChar == '\f';
}
class AccessibleNode;
struct BoxQuadOptions;
struct ConvertCoordinateOptions;
class DocGroup;
class DocumentFragment;
class DocumentOrShadowRoot;
class DOMPoint;
class DOMQuad;
class DOMRectReadOnly;
class Element;
class EventHandlerNonNull;
class L10nCallback;
template<typename T> class Optional;
class OwningNodeOrString;
class Promise;
template<typename> class Sequence;
class Text;
class TextOrElementOrDocument;
struct DOMPointInit;
struct GetRootNodeOptions;
enum class CallerType : uint32_t;
} // namespace dom
} // namespace mozilla

#define NODE_FLAG_BIT(n_) \
  (nsWrapperCache::FlagsType(1U) << (WRAPPER_CACHE_FLAGS_BITS_USED + (n_)))

enum {
  // This bit will be set if the node has a listener manager.
  NODE_HAS_LISTENERMANAGER =              NODE_FLAG_BIT(0),

  // Whether this node has had any properties set on it
  NODE_HAS_PROPERTIES =                   NODE_FLAG_BIT(1),

  // Whether this node is the root of an anonymous subtree.  Note that this
  // need not be a native anonymous subtree.  Any anonymous subtree, including
  // XBL-generated ones, will do.  This flag is set-once: once a node has it,
  // it must not be removed.
  // NOTE: Should only be used on nsIContent nodes
  NODE_IS_ANONYMOUS_ROOT =                NODE_FLAG_BIT(2),

  // Whether the node has some ancestor, possibly itself, that is native
  // anonymous.  This includes ancestors crossing XBL scopes, in cases when an
  // XBL binding is attached to an element which has a native anonymous
  // ancestor.  This flag is set-once: once a node has it, it must not be
  // removed.
  // NOTE: Should only be used on nsIContent nodes
  NODE_IS_IN_NATIVE_ANONYMOUS_SUBTREE =          NODE_FLAG_BIT(3),

  // Whether this node is the root of a native anonymous (from the perspective
  // of its parent) subtree.  This flag is set-once: once a node has it, it
  // must not be removed.
  // NOTE: Should only be used on nsIContent nodes
  NODE_IS_NATIVE_ANONYMOUS_ROOT =         NODE_FLAG_BIT(4),

  // Whether a binding manager may have a pointer to this
  NODE_MAY_BE_IN_BINDING_MNGR =           NODE_FLAG_BIT(5),

  NODE_IS_EDITABLE =                      NODE_FLAG_BIT(6),

  // Free bit here.

  // Whether the node participates in a shadow tree.
  NODE_IS_IN_SHADOW_TREE =                NODE_FLAG_BIT(8),

  // Node has an :empty or :-moz-only-whitespace selector
  NODE_HAS_EMPTY_SELECTOR =               NODE_FLAG_BIT(9),

  // A child of the node has a selector such that any insertion,
  // removal, or appending of children requires restyling the parent.
  NODE_HAS_SLOW_SELECTOR =                NODE_FLAG_BIT(10),

  // A child of the node has a :first-child, :-moz-first-node,
  // :only-child, :last-child or :-moz-last-node selector.
  NODE_HAS_EDGE_CHILD_SELECTOR =          NODE_FLAG_BIT(11),

  // A child of the node has a selector such that any insertion or
  // removal of children requires restyling later siblings of that
  // element.  Additionally (in this manner it is stronger than
  // NODE_HAS_SLOW_SELECTOR), if a child's style changes due to any
  // other content tree changes (e.g., the child changes to or from
  // matching :empty due to a grandchild insertion or removal), the
  // child's later siblings must also be restyled.
  NODE_HAS_SLOW_SELECTOR_LATER_SIBLINGS = NODE_FLAG_BIT(12),

  NODE_ALL_SELECTOR_FLAGS =               NODE_HAS_EMPTY_SELECTOR |
                                          NODE_HAS_SLOW_SELECTOR |
                                          NODE_HAS_EDGE_CHILD_SELECTOR |
                                          NODE_HAS_SLOW_SELECTOR_LATER_SIBLINGS,

  // This node needs to go through frame construction to get a frame (or
  // undisplayed entry).
  NODE_NEEDS_FRAME =                      NODE_FLAG_BIT(13),

  // At least one descendant in the flattened tree has NODE_NEEDS_FRAME set.
  // This should be set on every node on the flattened tree path between the
  // node(s) with NODE_NEEDS_FRAME and the root content.
  NODE_DESCENDANTS_NEED_FRAMES =          NODE_FLAG_BIT(14),

  // Set if the node has the accesskey attribute set.
  NODE_HAS_ACCESSKEY =                    NODE_FLAG_BIT(15),

  // Set if the node has right-to-left directionality
  NODE_HAS_DIRECTION_RTL =                NODE_FLAG_BIT(16),

  // Set if the node has left-to-right directionality
  NODE_HAS_DIRECTION_LTR =                NODE_FLAG_BIT(17),

  NODE_ALL_DIRECTION_FLAGS =              NODE_HAS_DIRECTION_LTR |
                                          NODE_HAS_DIRECTION_RTL,

  NODE_CHROME_ONLY_ACCESS =               NODE_FLAG_BIT(18),

  NODE_IS_ROOT_OF_CHROME_ONLY_ACCESS =    NODE_FLAG_BIT(19),

  // Remaining bits are node type specific.
  NODE_TYPE_SPECIFIC_BITS_OFFSET =        20
};

// Make sure we have space for our bits
#define ASSERT_NODE_FLAGS_SPACE(n) \
  static_assert(WRAPPER_CACHE_FLAGS_BITS_USED + (n) <=                          \
                  sizeof(nsWrapperCache::FlagsType) * 8,                        \
                "Not enough space for our bits")
ASSERT_NODE_FLAGS_SPACE(NODE_TYPE_SPECIFIC_BITS_OFFSET);

/**
 * Class used to detect unexpected mutations. To use the class create an
 * nsMutationGuard on the stack before unexpected mutations could occur.
 * You can then at any time call Mutated to check if any unexpected mutations
 * have occurred.
 */
class nsMutationGuard {
public:
  nsMutationGuard()
  {
    mStartingGeneration = sGeneration;
  }

  /**
   * Returns true if any unexpected mutations have occurred. You can pass in
   * an 8-bit ignore count to ignore a number of expected mutations.
   *
   * We don't need to care about overflow because subtraction of uint64_t's is
   * finding the difference between two elements of the group Z < 2^64.  Once
   * we know the difference between two elements we only need to check that is
   * less than the given number of mutations to know less than that many
   * mutations occured.  Assuming constant 1ns mutations it would take 584
   * years for sGeneration to fully wrap around so we can ignore a guard living
   * through a full wrap around.
   */
  bool Mutated(uint8_t aIgnoreCount)
  {
    return (sGeneration - mStartingGeneration) > aIgnoreCount;
  }

  // This function should be called whenever a mutation that we want to keep
  // track of happen. For now this is only done when children are added or
  // removed, but we might do it for attribute changes too in the future.
  static void DidMutate()
  {
    sGeneration++;
  }

private:
  // This is the value sGeneration had when the guard was constructed.
  uint64_t mStartingGeneration;

  // This value is incremented on every mutation, for the life of the process.
  static uint64_t sGeneration;
};

/**
 * A class that implements nsIWeakReference
 */
class nsNodeWeakReference final : public nsIWeakReference
{
public:
  explicit nsNodeWeakReference(nsINode* aNode);

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIWeakReference
  NS_DECL_NSIWEAKREFERENCE
  size_t SizeOfOnlyThis(mozilla::MallocSizeOf aMallocSizeOf) const override;

  void NoticeNodeDestruction()
  {
    mObject = nullptr;
  }

private:
  ~nsNodeWeakReference();
};

// This should be used for any nsINode sub-class that has fields of its own
// that it needs to measure; any sub-class that doesn't use it will inherit
// AddSizeOfExcludingThis from its super-class. AddSizeOfIncludingThis() need
// not be defined, it is inherited from nsINode.
#define NS_DECL_ADDSIZEOFEXCLUDINGTHIS \
  virtual void AddSizeOfExcludingThis(nsWindowSizes& aSizes, \
                                      size_t* aNodeSize) const override;

// IID for the nsINode interface
// Must be kept in sync with xpcom/rust/xpcom/src/interfaces/nonidl.rs
#define NS_INODE_IID \
{ 0x70ba4547, 0x7699, 0x44fc, \
  { 0xb3, 0x20, 0x52, 0xdb, 0xe3, 0xd1, 0xf9, 0x0a } }

/**
 * An internal interface that abstracts some DOMNode-related parts that both
 * nsIContent and nsIDocument share.  An instance of this interface has a list
 * of nsIContent children and provides access to them.
 */
class nsINode : public mozilla::dom::EventTarget
{
public:
  typedef mozilla::dom::BoxQuadOptions BoxQuadOptions;
  typedef mozilla::dom::ConvertCoordinateOptions ConvertCoordinateOptions;
  typedef mozilla::dom::DocGroup DocGroup;
  typedef mozilla::dom::DOMPoint DOMPoint;
  typedef mozilla::dom::DOMPointInit DOMPointInit;
  typedef mozilla::dom::DOMQuad DOMQuad;
  typedef mozilla::dom::DOMRectReadOnly DOMRectReadOnly;
  typedef mozilla::dom::OwningNodeOrString OwningNodeOrString;
  typedef mozilla::dom::TextOrElementOrDocument TextOrElementOrDocument;
  typedef mozilla::dom::CallerType CallerType;
  typedef mozilla::ErrorResult ErrorResult;

  // XXXbz Maybe we should codegen a class holding these constants and
  // inherit from it...
  static const auto ELEMENT_NODE = mozilla::dom::Node_Binding::ELEMENT_NODE;
  static const auto ATTRIBUTE_NODE = mozilla::dom::Node_Binding::ATTRIBUTE_NODE;
  static const auto TEXT_NODE = mozilla::dom::Node_Binding::TEXT_NODE;
  static const auto CDATA_SECTION_NODE =
    mozilla::dom::Node_Binding::CDATA_SECTION_NODE;
  static const auto ENTITY_REFERENCE_NODE =
    mozilla::dom::Node_Binding::ENTITY_REFERENCE_NODE;
  static const auto ENTITY_NODE = mozilla::dom::Node_Binding::ENTITY_NODE;
  static const auto PROCESSING_INSTRUCTION_NODE =
    mozilla::dom::Node_Binding::PROCESSING_INSTRUCTION_NODE;
  static const auto COMMENT_NODE = mozilla::dom::Node_Binding::COMMENT_NODE;
  static const auto DOCUMENT_NODE = mozilla::dom::Node_Binding::DOCUMENT_NODE;
  static const auto DOCUMENT_TYPE_NODE =
    mozilla::dom::Node_Binding::DOCUMENT_TYPE_NODE;
  static const auto DOCUMENT_FRAGMENT_NODE =
    mozilla::dom::Node_Binding::DOCUMENT_FRAGMENT_NODE;
  static const auto NOTATION_NODE = mozilla::dom::Node_Binding::NOTATION_NODE;

  template<class T>
  using Sequence = mozilla::dom::Sequence<T>;

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_INODE_IID)

  // The |aNodeSize| outparam on this function is where the actual node size
  // value is put. It gets added to the appropriate value within |aSizes| by
  // AddSizeOfNodeTree().
  //
  // Among the sub-classes that inherit (directly or indirectly) from nsINode,
  // measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - nsGenericHTMLElement:  mForm, mFieldSet
  // - nsGenericHTMLFrameElement: mFrameLoader (bug 672539)
  // - HTMLBodyElement:       mContentStyleRule
  // - HTMLDataListElement:   mOptions
  // - HTMLFieldSetElement:   mElements, mDependentElements, mFirstLegend
  // - HTMLFormElement:       many!
  // - HTMLFrameSetElement:   mRowSpecs, mColSpecs
  // - HTMLInputElement:      mInputData, mFiles, mFileList, mStaticDocfileList
  // - nsHTMLMapElement:      mAreas
  // - HTMLMediaElement:      many!
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
  virtual void AddSizeOfExcludingThis(nsWindowSizes& aSizes,
                                      size_t* aNodeSize) const;

  // SizeOfIncludingThis doesn't need to be overridden by sub-classes because
  // sub-classes of nsINode are guaranteed to be laid out in memory in such a
  // way that |this| points to the start of the allocated object, even in
  // methods of nsINode's sub-classes, so aSizes.mState.mMallocSizeOf(this) is
  // always safe to call no matter which object it was invoked on.
  void AddSizeOfIncludingThis(nsWindowSizes& aSizes, size_t* aNodeSize) const;

  friend class nsNodeUtils;
  friend class nsNodeWeakReference;
  friend class nsNodeSupportsWeakRefTearoff;
  friend class nsAttrAndChildArray;

#ifdef MOZILLA_INTERNAL_API
  explicit nsINode(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : mNodeInfo(aNodeInfo)
  , mParent(nullptr)
#ifndef BOOL_FLAGS_ON_WRAPPER_CACHE
  , mBoolFlags(0)
#endif
  , mNextSibling(nullptr)
  , mPreviousSibling(nullptr)
  , mFirstChild(nullptr)
  , mSubtreeRoot(this)
  , mSlots(nullptr)
  {
  }
#endif

  virtual ~nsINode();

  /**
   * Bit-flags to pass (or'ed together) to IsNodeOfType()
   */
  enum {
    /** form control elements */
    eHTML_FORM_CONTROL   = 1 << 6,
    /** animation elements */
    eANIMATION           = 1 << 10,
    /** filter elements that implement SVGFilterPrimitiveStandardAttributes */
    eFILTER              = 1 << 11,
    /** SVGGeometryElement */
    eSHAPE               = 1 << 12
  };

  /**
   * API for doing a quick check if a content is of a given
   * type, such as Text, Document, Comment ...  Use this when you can instead of
   * checking the tag.
   *
   * @param aFlags what types you want to test for (see above)
   * @return whether the content matches ALL flags passed in
   */
  virtual bool IsNodeOfType(uint32_t aFlags) const = 0;

  bool IsContainerNode() const
  {
    return IsElement() || IsDocument() || IsDocumentFragment();
  }

  bool IsSlotable() const
  {
    return IsElement() || IsText();
  }

  /**
   * Returns true if this is a document node.
   */
  bool IsDocument() const
  {
    // One less pointer-chase than checking NodeType().
    return !GetParentNode() && IsInUncomposedDoc();
  }

  /**
   * Return this node as a document. Asserts IsDocument().
   *
   * This is defined inline in nsIDocument.h.
   */
  inline nsIDocument* AsDocument();
  inline const nsIDocument* AsDocument() const;

  /**
   * Returns true if this is a document fragment node.
   */
  bool IsDocumentFragment() const
  {
    return NodeType() == DOCUMENT_FRAGMENT_NODE;
  }

  /**
   * Return this node as a document fragment. Asserts IsDocumentFragment().
   *
   * This is defined inline in DocumentFragment.h.
   */
  inline mozilla::dom::DocumentFragment* AsDocumentFragment();
  inline const mozilla::dom::DocumentFragment* AsDocumentFragment() const;

  virtual JSObject* WrapObject(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) override;

  /**
   * returns true if we are in priviliged code or
   * layout.css.getBoxQuads.enabled == true.
   */
  static bool HasBoxQuadsSupport(JSContext* aCx, JSObject* /* unused */);

protected:
  /**
   * WrapNode is called from WrapObject to actually wrap this node, WrapObject
   * does some additional checks and fix-up that's common to all nodes. WrapNode
   * should just call the DOM binding's Wrap function.
   *
   * aGivenProto is the prototype to use (or null if the default one should be
   * used) and should just be passed directly on to the DOM binding's Wrap
   * function.
   */
  virtual JSObject* WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) = 0;

public:
  mozilla::dom::ParentObject GetParentObject() const; // Implemented in nsIDocument.h

  /**
   * Return the scope chain parent for this node, for use in things
   * like event handler compilation.  Returning null means to use the
   * global object as the scope chain parent.
   */
  virtual nsINode* GetScopeChainParent() const;

  /**
   * Return whether the node is an Element node
   */
  bool IsElement() const
  {
    return GetBoolFlag(NodeIsElement);
  }

  /**
   * Return this node as an Element.  Should only be used for nodes
   * for which IsElement() is true.  This is defined inline in Element.h.
   */
  inline mozilla::dom::Element* AsElement();
  inline const mozilla::dom::Element* AsElement() const;

  /**
   * Return this node as nsIContent.  Should only be used for nodes for which
   * IsContent() is true.  This is defined inline in nsIContent.h.
   */
  inline nsIContent* AsContent();
  inline const nsIContent* AsContent() const;

  /*
   * Return whether the node is a Text node (which might be an actual
   * textnode, or might be a CDATA section).
   */
  bool IsText() const
  {
    uint32_t nodeType = NodeType();
    return nodeType == TEXT_NODE || nodeType == CDATA_SECTION_NODE;
  }

  /**
   * Return this node as Text if it is one, otherwise null.  This is defined
   * inline in Text.h.
   */
  inline mozilla::dom::Text* GetAsText();
  inline const mozilla::dom::Text* GetAsText() const;

  /**
   * Return this node as Text.  Asserts IsText().  This is defined inline in
   * Text.h.
   */
  inline mozilla::dom::Text* AsText();
  inline const mozilla::dom::Text* AsText() const;

  /*
   * Return whether the node is a ProcessingInstruction node.
   */
  bool IsProcessingInstruction() const
  {
    return NodeType() == PROCESSING_INSTRUCTION_NODE;
  }

  /*
   * Return whether the node is a CharacterData node (text, cdata,
   * comment, processing instruction)
   */
  bool IsCharacterData() const
  {
    uint32_t nodeType = NodeType();
    return nodeType == TEXT_NODE ||
           nodeType == CDATA_SECTION_NODE ||
           nodeType == PROCESSING_INSTRUCTION_NODE ||
           nodeType == COMMENT_NODE;
  }

  /**
   * Return whether the node is a Comment node.
   */
  bool IsComment() const
  {
    return NodeType() == COMMENT_NODE;
  }

  /**
   * Return whether the node is an Attr node.
   */
  bool IsAttr() const
  {
    return NodeType() == ATTRIBUTE_NODE;
  }

  /**
   * Return if this node has any children.
   */
  bool HasChildren() const { return !!mFirstChild; }

  /**
   * Get the number of children
   * @return the number of children
   */
  virtual uint32_t GetChildCount() const = 0;

  /**
   * NOTE: this function is going to be removed soon (hopefully!) Don't use it
   * in new code.
   *
   * Get a child by index
   * @param aIndex the index of the child to get
   * @return the child, or null if index out of bounds
   */
  virtual nsIContent* GetChildAt_Deprecated(uint32_t aIndex) const = 0;

  /**
   * Get the index of a child within this content
   * @param aPossibleChild the child to get the index of.
   * @return the index of the child, or -1 if not a child
   *
   * If the return value is not -1, then calling GetChildAt_Deprecated() with
   * that value will return aPossibleChild.
   */
  virtual int32_t ComputeIndexOf(const nsINode* aPossibleChild) const = 0;

  /**
   * Returns the "node document" of this node.
   *
   * https://dom.spec.whatwg.org/#concept-node-document
   *
   * Note that in the case that this node is a document node this method
   * will return |this|.  That is different to the Node.ownerDocument DOM
   * attribute (implemented by nsINode::GetOwnerDocument) which is specified to
   * be null in that case:
   *
   * https://dom.spec.whatwg.org/#dom-node-ownerdocument
   *
   * For all other cases OwnerDoc and GetOwnerDocument behave identically.
   */
  nsIDocument* OwnerDoc() const
  {
    return mNodeInfo->GetDocument();
  }

  /**
   * Return the "owner document" of this node as an nsINode*.  Implemented
   * in nsIDocument.h.
   */
  inline nsINode* OwnerDocAsNode() const;

  /**
   * Returns true if the content has an ancestor that is a document.
   *
   * @return whether this content is in a document tree
   */
  bool IsInUncomposedDoc() const
  {
    return GetBoolFlag(IsInDocument);
  }

  /**
   * Get the document that this content is currently in, if any. This will be
   * null if the content has no ancestor that is a document.
   *
   * @return the current document
   */

  nsIDocument* GetUncomposedDoc() const
  {
    return IsInUncomposedDoc() ? OwnerDoc() : nullptr;
  }

  /**
   * This method returns the owner document if the node is connected to it
   * (as defined in the DOM spec), otherwise it returns null.
   * In other words, returns non-null even in the case the node is in
   * Shadow DOM, if there is a possibly shadow boundary crossing path from
   * the node to its owner document.
   */
  nsIDocument* GetComposedDoc() const
  {
    return IsInShadowTree() ?
      GetComposedDocInternal() : GetUncomposedDoc();
  }

  /**
   * Returns true if GetComposedDoc() would return a non-null value.
   */
  bool IsInComposedDoc() const
  {
    return IsInUncomposedDoc() || (IsInShadowTree() && GetComposedDocInternal());
  }

  /**
   * Returns OwnerDoc() if the node is in uncomposed document and ShadowRoot if
   * the node is in Shadow DOM and is in composed document.
   */
  mozilla::dom::DocumentOrShadowRoot* GetUncomposedDocOrConnectedShadowRoot() const;

  /**
   * The values returned by this function are the ones defined for
   * Node.nodeType
   */
  uint16_t NodeType() const
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
   * Get the NodeInfo for this element
   * @return the nodes node info
   */
  inline mozilla::dom::NodeInfo* NodeInfo() const
  {
    return mNodeInfo;
  }

  inline bool IsInNamespace(int32_t aNamespace) const
  {
    return mNodeInfo->NamespaceID() == aNamespace;
  }

  /**
   * Returns the DocGroup of the "node document" of this node.
   */
  DocGroup* GetDocGroup() const;

  /**
   * Print a debugger friendly descriptor of this element. This will describe
   * the position of this element in the document.
   */
  friend std::ostream& operator<<(std::ostream& aStream, const nsINode& aNode);

protected:
  // These 2 methods are useful for the recursive templates IsHTMLElement,
  // IsSVGElement, etc.
  inline bool IsNodeInternal() const
  {
    return false;
  }

  template<typename First, typename... Args>
  inline bool IsNodeInternal(First aFirst, Args... aArgs) const
  {
    return mNodeInfo->Equals(aFirst) || IsNodeInternal(aArgs...);
  }

public:
  inline bool IsHTMLElement() const
  {
    return IsElement() && IsInNamespace(kNameSpaceID_XHTML);
  }

  inline bool IsHTMLElement(nsAtom* aTag) const
  {
    return IsElement() && mNodeInfo->Equals(aTag, kNameSpaceID_XHTML);
  }

  template<typename First, typename... Args>
  inline bool IsAnyOfHTMLElements(First aFirst, Args... aArgs) const
  {
    return IsHTMLElement() && IsNodeInternal(aFirst, aArgs...);
  }

  inline bool IsSVGElement() const
  {
    return IsElement() && IsInNamespace(kNameSpaceID_SVG);
  }

  inline bool IsSVGElement(nsAtom* aTag) const
  {
    return IsElement() && mNodeInfo->Equals(aTag, kNameSpaceID_SVG);
  }

  template<typename First, typename... Args>
  inline bool IsAnyOfSVGElements(First aFirst, Args... aArgs) const
  {
    return IsSVGElement() && IsNodeInternal(aFirst, aArgs...);
  }

  inline bool IsXULElement() const
  {
    return IsElement() && IsInNamespace(kNameSpaceID_XUL);
  }

  inline bool IsXULElement(nsAtom* aTag) const
  {
    return IsElement() && mNodeInfo->Equals(aTag, kNameSpaceID_XUL);
  }

  template<typename First, typename... Args>
  inline bool IsAnyOfXULElements(First aFirst, Args... aArgs) const
  {
    return IsXULElement() && IsNodeInternal(aFirst, aArgs...);
  }

  inline bool IsMathMLElement() const
  {
    return IsElement() && IsInNamespace(kNameSpaceID_MathML);
  }

  inline bool IsMathMLElement(nsAtom* aTag) const
  {
    return IsElement() && mNodeInfo->Equals(aTag, kNameSpaceID_MathML);
  }

  template<typename First, typename... Args>
  inline bool IsAnyOfMathMLElements(First aFirst, Args... aArgs) const
  {
    return IsMathMLElement() && IsNodeInternal(aFirst, aArgs...);
  }

  bool IsShadowRoot() const
  {
    const bool isShadowRoot = IsInShadowTree() && !GetParentNode();
    MOZ_ASSERT_IF(isShadowRoot, IsDocumentFragment());
    return isShadowRoot;
  }

  /**
   * Insert a content node before another or at the end.
   * This method handles calling BindToTree on the child appropriately.
   *
   * @param aKid the content to insert
   * @param aBeforeThis an existing node. Use nullptr if you want to
   *        add aKid at the end.
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
  virtual nsresult InsertChildBefore(nsIContent* aKid, nsIContent* aBeforeThis,
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
    return InsertChildBefore(aKid, nullptr, aNotify);
  }

  /**
   * Remove a child from this node.  This method handles calling UnbindFromTree
   * on the child appropriately.
   *
   * @param aKid the content to remove
   * @param aNotify whether to notify the document (current document for
   *        nsIContent, and |this| for nsIDocument) that the remove has
   *        occurred
   */
  virtual void RemoveChildNode(nsIContent* aKid, bool aNotify) = 0;

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
  void* GetProperty(nsAtom* aPropertyName, nsresult* aStatus = nullptr) const;

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
  nsresult SetProperty(nsAtom* aPropertyName,
                       void* aValue,
                       NSPropertyDtorFunc aDtor = nullptr,
                       bool aTransfer = false);

  /**
   * A generic destructor for property values allocated with new.
   */
  template<class T>
  static void DeleteProperty(void*, nsAtom*, void* aPropertyValue, void*)
  {
    delete static_cast<T *>(aPropertyValue);
  }

  /**
   * Destroys a property associated with this node. The value is destroyed
   * using the destruction function given when that value was set.
   *
   * @param aPropertyName  name of property to destroy.
   */
  void DeleteProperty(nsAtom* aPropertyName);

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
  void* UnsetProperty(nsAtom* aPropertyName, nsresult* aStatus = nullptr);

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
    return MOZ_LIKELY(GetBoolFlag(ParentIsContent)) ?
      reinterpret_cast<nsIContent*>(mParent) : nullptr;
  }

  /**
   * Get the parent nsINode for this node. This can be either an nsIContent,
   * an nsIDocument or an Attr.
   * @return the parent node
   */
  nsINode* GetParentNode() const
  {
    return mParent;
  }

  /**
   * This is similar to above, but in case 'this' is ShadowRoot, we return its
   * host element.
   */
  nsINode* GetParentOrHostNode() const;

  enum FlattenedParentType { eNotForStyle, eForStyle };

  /**
   * Returns the node that is the parent of this node in the flattened
   * tree. This differs from the normal parent if the node is filtered
   * into an insertion point, or if the node is a direct child of a
   * shadow root.
   *
   * @return the flattened tree parent
   */
  inline nsINode* GetFlattenedTreeParentNode() const;

  /**
   * Like GetFlattenedTreeParentNode, but returns the document for any native
   * anonymous content that was generated for ancestor frames of the document
   * element's primary frame, such as scrollbar elements created by the root
   * scroll frame.
   */
  inline nsINode* GetFlattenedTreeParentNodeForStyle() const;

  inline mozilla::dom::Element* GetFlattenedTreeParentElement() const;
  inline mozilla::dom::Element* GetFlattenedTreeParentElementForStyle() const;

  /**
   * Get the parent nsINode for this node if it is an Element.
   *
   * Defined inline in Element.h
   *
   * @return the parent node
   */
  inline mozilla::dom::Element* GetParentElement() const;

  /**
   * Get the parent Element of this node, traversing over a ShadowRoot
   * to its host if necessary.
   */
  mozilla::dom::Element* GetParentElementCrossingShadowRoot() const;

  /**
   * Get the root of the subtree this node belongs to.  This never returns
   * null.  It may return 'this' (e.g. for document nodes, and nodes that
   * are the roots of disconnected subtrees).
   */
  nsINode* SubtreeRoot() const;

  /*
   * Get context object's shadow-including root if options's composed is true,
   * and context object's root otherwise.
   */
  nsINode* GetRootNode(const mozilla::dom::GetRootNodeOptions& aOptions);

  virtual mozilla::EventListenerManager*
    GetExistingListenerManager() const override;
  virtual mozilla::EventListenerManager*
    GetOrCreateListenerManager() override;

  bool ComputeDefaultWantsUntrusted(mozilla::ErrorResult& aRv) final;

  virtual bool IsApzAware() const override;

  virtual nsPIDOMWindowOuter* GetOwnerGlobalForBindings() override;
  virtual nsIGlobalObject* GetOwnerGlobal() const override;

  using mozilla::dom::EventTarget::DispatchEvent;
  bool DispatchEvent(mozilla::dom::Event& aEvent,
                     mozilla::dom::CallerType aCallerType,
                     mozilla::ErrorResult& aRv) override;

  nsresult PostHandleEvent(mozilla::EventChainPostVisitor& aVisitor) override;

  /**
   * Adds a mutation observer to be notified when this node, or any of its
   * descendants, are modified. The node will hold a weak reference to the
   * observer, which means that it is the responsibility of the observer to
   * remove itself in case it dies before the node.  If an observer is added
   * while observers are being notified, it may also be notified.  In general,
   * adding observers while inside a notification is not a good idea.  An
   * observer that is already observing the node must not be added without
   * being removed first.
   *
   * For mutation observers that implement nsIAnimationObserver, use
   * AddAnimationObserver instead.
   */
  void AddMutationObserver(nsIMutationObserver* aMutationObserver)
  {
    nsSlots* s = Slots();
    NS_ASSERTION(s->mMutationObservers.IndexOf(aMutationObserver) ==
                 nsTArray<int>::NoIndex,
                 "Observer already in the list");
    s->mMutationObservers.AppendElement(aMutationObserver);
  }

  /**
   * Same as above, but only adds the observer if its not observing
   * the node already.
   *
   * For mutation observers that implement nsIAnimationObserver, use
   * AddAnimationObserverUnlessExists instead.
   */
  void AddMutationObserverUnlessExists(nsIMutationObserver* aMutationObserver)
  {
    nsSlots* s = Slots();
    s->mMutationObservers.AppendElementUnlessExists(aMutationObserver);
  }

  /**
   * Same as AddMutationObserver, but for nsIAnimationObservers.  This
   * additionally records on the document that animation observers have
   * been registered, which is used to determine whether notifications
   * must be fired when animations are added, removed or changed.
   */
  void AddAnimationObserver(nsIAnimationObserver* aAnimationObserver);

  /**
   * Same as above, but only adds the observer if its not observing
   * the node already.
   */
  void AddAnimationObserverUnlessExists(nsIAnimationObserver* aAnimationObserver);

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
   * @param aPreallocateChildren If true, the array of children will be
   *                             preallocated in preparation for a deep copy.
   */
  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult,
                         bool aPreallocateChildren) const = 0;

  // This class can be extended by subclasses that wish to store more
  // information in the slots.
  class nsSlots
  {
  public:
    nsSlots();

    // If needed we could remove the vtable pointer this dtor causes by
    // putting a DestroySlots function on nsINode
    virtual ~nsSlots();

    virtual void Traverse(nsCycleCollectionTraversalCallback&);
    virtual void Unlink();

    /**
     * A list of mutation observers
     */
    nsAutoTObserverArray<nsIMutationObserver*, 1> mMutationObservers;

    /**
     * An object implementing NodeList for this content (childNodes)
     * @see NodeList
     * @see nsGenericHTMLElement::GetChildNodes
     */
    RefPtr<nsAttrChildContentList> mChildNodes;

    /**
     * Weak reference to this node.  This is cleared by the destructor of
     * nsNodeWeakReference.
     */
    nsNodeWeakReference* MOZ_NON_OWNING_REF mWeakReference;

    /**
     * A set of ranges which are in the selection and which have this node as
     * their endpoints' common ancestor.  This is a UniquePtr instead of just a
     * LinkedList, because that prevents us from pushing DOMSlots up to the next
     * allocation bucket size, at the cost of some complexity.
     */
    mozilla::UniquePtr<mozilla::LinkedList<nsRange>> mCommonAncestorRanges;

    /**
     * Number of descendant nodes in the uncomposed document that have been
     * explicitly set as editable.
     */
    uint32_t mEditableDescendantCount;
  };

  /**
   * Functions for managing flags and slots
   */
#ifdef DEBUG
  nsSlots* DebugGetSlots()
  {
    return Slots();
  }
#endif

  void SetFlags(FlagsType aFlagsToSet)
  {
    NS_ASSERTION(!(aFlagsToSet & (NODE_IS_ANONYMOUS_ROOT |
                                  NODE_IS_NATIVE_ANONYMOUS_ROOT |
                                  NODE_IS_IN_NATIVE_ANONYMOUS_SUBTREE |
                                  NODE_DESCENDANTS_NEED_FRAMES |
                                  NODE_NEEDS_FRAME |
                                  NODE_CHROME_ONLY_ACCESS)) ||
                 IsContent(),
                 "Flag only permitted on nsIContent nodes");
    nsWrapperCache::SetFlags(aFlagsToSet);
  }

  void UnsetFlags(FlagsType aFlagsToUnset)
  {
    NS_ASSERTION(!(aFlagsToUnset &
                   (NODE_IS_ANONYMOUS_ROOT |
                    NODE_IS_IN_NATIVE_ANONYMOUS_SUBTREE |
                    NODE_IS_NATIVE_ANONYMOUS_ROOT)),
                 "Trying to unset write-only flags");
    nsWrapperCache::UnsetFlags(aFlagsToUnset);
  }

  void ChangeEditableDescendantCount(int32_t aDelta);

  /**
   * Returns the count of descendant nodes in the uncomposed
   * document that are explicitly set as editable.
   */
  uint32_t EditableDescendantCount();

  /**
   * Sets the editable descendant count to 0. The editable
   * descendant count only counts explicitly editable nodes
   * that are in the uncomposed document so this method
   * should be called when nodes are are removed from it.
   */
  void ResetEditableDescendantCount();

  void SetEditableFlag(bool aEditable)
  {
    if (aEditable) {
      SetFlags(NODE_IS_EDITABLE);
    }
    else {
      UnsetFlags(NODE_IS_EDITABLE);
    }
  }

  inline bool IsEditable() const;

  /**
   * Returns true if |this| or any of its ancestors is native anonymous.
   */
  bool IsInNativeAnonymousSubtree() const
  {
#ifdef DEBUG
    if (HasFlag(NODE_IS_IN_NATIVE_ANONYMOUS_SUBTREE)) {
      return true;
    }
    CheckNotNativeAnonymous();
    return false;
#else
    return HasFlag(NODE_IS_IN_NATIVE_ANONYMOUS_SUBTREE);
#endif
  }

  bool IsInAnonymousSubtree() const;

  // Note: This asserts |IsInAnonymousSubtree()|.
  bool IsAnonymousContentInSVGUseSubtree() const;

  // True for native anonymous content and for XBL content if the binding
  // has chromeOnlyContent="true".
  bool ChromeOnlyAccess() const
  {
    return HasFlag(NODE_IS_IN_NATIVE_ANONYMOUS_SUBTREE | NODE_CHROME_ONLY_ACCESS);
  }

  bool IsInShadowTree() const
  {
    return HasFlag(NODE_IS_IN_SHADOW_TREE);
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
  nsIContent* GetTextEditorRootContent(
                mozilla::TextEditor** aTextEditor = nullptr);

  /**
   * Get the nearest selection root, ie. the node that will be selected if the
   * user does "Select All" while the focus is in this node. Note that if this
   * node is not in an editor, the result comes from the nsFrameSelection that
   * is related to aPresShell, so the result might not be the ancestor of this
   * node. Be aware that if this node and the computed selection limiter are
   * not in same subtree, this returns the root content of the closeset subtree.
   */
  nsIContent* GetSelectionRootContent(nsIPresShell* aPresShell);

  nsINodeList* ChildNodes();
  nsIContent* GetFirstChild() const { return mFirstChild; }
  nsIContent* GetLastChild() const
  {
    uint32_t count = GetChildCount();

    return count > 0 ? GetChildAt_Deprecated(count - 1) : nullptr;
  }

  /**
   * Implementation is in nsIDocument.h, because it needs to cast from
   * nsIDocument* to nsINode*.
   */
  nsIDocument* GetOwnerDocument() const;

  void Normalize();

  /**
   * Get the base URI for any relative URIs within this piece of
   * content. Generally, this is the document's base URI, but certain
   * content carries a local base for backward compatibility, and XML
   * supports setting a per-node base URI.
   *
   * @return the base URI
   */
  virtual already_AddRefed<nsIURI> GetBaseURI(bool aTryUseXHRDocBaseURI = false) const = 0;
  already_AddRefed<nsIURI> GetBaseURIObject() const;

  /**
   * Return true if the node may be apz aware. There are two cases. One is that
   * the node is apz aware (such as HTMLInputElement with number type). The
   * other is that the node has apz aware listeners. This is a non-virtual
   * function which calls IsNodeApzAwareInternal only when the MayBeApzAware is
   * set. We check the details in IsNodeApzAwareInternal which may be overriden
   * by child classes
   */
  bool IsNodeApzAware() const
  {
    return NodeMayBeApzAware() ? IsNodeApzAwareInternal() : false;
  }

  /**
   * Override this function and set the flag MayBeApzAware in case the node has
   * to let APZC be aware of it. It's used when the node may handle the apz
   * aware events and may do preventDefault to stop APZC to do default actions.
   *
   * For example, instead of scrolling page by APZ, we handle mouse wheel event
   * in HTMLInputElement with number type as increasing / decreasing its value.
   */
  virtual bool IsNodeApzAwareInternal() const;

  void GetTextContent(nsAString& aTextContent,
                      mozilla::OOMReporter& aError)
  {
    GetTextContentInternal(aTextContent, aError);
  }
  void SetTextContent(const nsAString& aTextContent,
                      nsIPrincipal* aSubjectPrincipal,
                      mozilla::ErrorResult& aError)
  {
    SetTextContentInternal(aTextContent, aSubjectPrincipal, aError);
  }
  void SetTextContent(const nsAString& aTextContent,
                      mozilla::ErrorResult& aError)
  {
    SetTextContentInternal(aTextContent, nullptr, aError);
  }

  mozilla::dom::Element* QuerySelector(const nsAString& aSelector,
                                       mozilla::ErrorResult& aResult);
  already_AddRefed<nsINodeList> QuerySelectorAll(const nsAString& aSelector,
                                                 mozilla::ErrorResult& aResult);

protected:
  // nsIDocument overrides this with its own (faster) version.  This
  // should really only be called for elements and document fragments.
  mozilla::dom::Element* GetElementById(const nsAString& aId);

public:
  void LookupPrefix(const nsAString& aNamespace, nsAString& aResult);
  bool IsDefaultNamespace(const nsAString& aNamespaceURI)
  {
    nsAutoString defaultNamespace;
    LookupNamespaceURI(EmptyString(), defaultNamespace);
    return aNamespaceURI.Equals(defaultNamespace);
  }
  void LookupNamespaceURI(const nsAString& aNamespacePrefix,
                          nsAString& aNamespaceURI);

  nsIContent* GetNextSibling() const { return mNextSibling; }
  nsIContent* GetPreviousSibling() const { return mPreviousSibling; }

  /**
   * Get the next node in the pre-order tree traversal of the DOM.  If
   * aRoot is non-null, then it must be an ancestor of |this|
   * (possibly equal to |this|) and only nodes that are descendants of
   * aRoot, not including aRoot itself, will be returned.  Returns
   * null if there are no more nodes to traverse.
   */
  nsIContent* GetNextNode(const nsINode* aRoot = nullptr) const
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
  nsIContent* GetNextNonChildNode(const nsINode* aRoot = nullptr) const
  {
    return GetNextNodeImpl(aRoot, true);
  }

  /**
   * Returns true if 'this' is either document or element or
   * document fragment and aOther is a descendant in the same
   * anonymous tree.
   */
  bool Contains(const nsINode* aOther) const;

  bool UnoptimizableCCNode() const;

private:

  nsIDocument* GetComposedDocInternal() const;

  nsIContent* GetNextNodeImpl(const nsINode* aRoot,
                              const bool aSkipChildren) const
  {
    // Can't use nsContentUtils::ContentIsDescendantOf here, since we
    // can't include it here.
#ifdef DEBUG
    if (aRoot) {
      const nsINode* cur = this;
      for (; cur; cur = cur->GetParentNode())
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
      return nullptr;
    }
    const nsINode* cur = this;
    while (1) {
      nsIContent* next = cur->GetNextSibling();
      if (next) {
        return next;
      }
      nsINode* parent = cur->GetParentNode();
      if (parent == aRoot) {
        return nullptr;
      }
      cur = parent;
    }
    MOZ_ASSERT_UNREACHABLE("How did we get here?");
  }

public:

  /**
   * Get the previous nsIContent in the pre-order tree traversal of the DOM.  If
   * aRoot is non-null, then it must be an ancestor of |this|
   * (possibly equal to |this|) and only nsIContents that are descendants of
   * aRoot, including aRoot itself, will be returned.  Returns
   * null if there are no more nsIContents to traverse.
   */
  nsIContent* GetPreviousContent(const nsINode* aRoot = nullptr) const
  {
      // Can't use nsContentUtils::ContentIsDescendantOf here, since we
      // can't include it here.
#ifdef DEBUG
      if (aRoot) {
        const nsINode* cur = this;
        for (; cur; cur = cur->GetParentNode())
          if (cur == aRoot) break;
        NS_ASSERTION(cur, "aRoot not an ancestor of |this|?");
      }
#endif

    if (this == aRoot) {
      return nullptr;
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
    // Set if the element might have a class.
    ElementMayHaveClass,
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
    // Set if the element has some style states locked
    ElementHasLockedStyleStates,
    // Set if element has pointer locked
    ElementHasPointerLock,
    // Set if the node may have DOMMutationObserver attached to it.
    NodeMayHaveDOMMutationObserver,
    // Set if node is Content
    NodeIsContent,
    // Set if the node has animations or transitions
    ElementHasAnimations,
    // Set if node has a dir attribute with a valid value (ltr, rtl, or auto).
    // Note that we cannot compute this from the dir attribute event state
    // flags, because we can't use those to distinguish
    // <bdi dir="some-invalid-value"> and <bdi dir="auto">.
    NodeHasValidDirAttribute,
    // Set if the node has dir=auto and has a property pointing to the text
    // node that determines its direction
    NodeHasDirAutoSet,
    // Set if the node is a text node descendant of a node with dir=auto
    // and has a TextNodeDirectionalityMap property listing the elements whose
    // direction it determines.
    NodeHasTextNodeDirectionalityMap,
    // Set if a node in the node's parent chain has dir=auto.
    NodeAncestorHasDirAuto,
    // Set if the node is handling a click.
    NodeHandlingClick,
    // Set if the node has had :hover selectors matched against it
    NodeHasRelevantHoverRules,
    // Set if the element has a parser insertion mode other than "in body",
    // per the HTML5 "Parse state" section.
    ElementHasWeirdParserInsertionMode,
    // Parser sets this flag if it has notified about the node.
    ParserHasNotified,
    // Sets if the node is apz aware or we have apz aware listeners.
    MayBeApzAware,
    // Set if the element might have any kind of anonymous content children,
    // which would not be found through the element's children list.
    ElementMayHaveAnonymousChildren,
    // Set if element has CustomElementData.
    ElementHasCustomElementData,
    // Guard value
    BooleanFlagCount
  };

  void SetBoolFlag(BooleanFlag name, bool value) {
    static_assert(BooleanFlagCount <= 8*sizeof(mBoolFlags),
                  "Too many boolean flags");
    mBoolFlags = (mBoolFlags & ~(1 << name)) | (value << name);
  }

  void SetBoolFlag(BooleanFlag name) {
    static_assert(BooleanFlagCount <= 8*sizeof(mBoolFlags),
                  "Too many boolean flags");
    mBoolFlags |= (1 << name);
  }

  void ClearBoolFlag(BooleanFlag name) {
    static_assert(BooleanFlagCount <= 8*sizeof(mBoolFlags),
                  "Too many boolean flags");
    mBoolFlags &= ~(1 << name);
  }

  bool GetBoolFlag(BooleanFlag name) const {
    static_assert(BooleanFlagCount <= 8*sizeof(mBoolFlags),
                  "Too many boolean flags");
    return mBoolFlags & (1 << name);
  }

public:
  bool HasRenderingObservers() const
    { return GetBoolFlag(NodeHasRenderingObservers); }
  void SetHasRenderingObservers(bool aValue)
    { SetBoolFlag(NodeHasRenderingObservers, aValue); }
  bool IsContent() const { return GetBoolFlag(NodeIsContent); }
  bool HasID() const { return GetBoolFlag(ElementHasID); }
  bool MayHaveClass() const { return GetBoolFlag(ElementMayHaveClass); }
  void SetMayHaveClass() { SetBoolFlag(ElementMayHaveClass); }
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
  bool MayHaveAnimations() const { return GetBoolFlag(ElementHasAnimations); }
  void SetMayHaveAnimations() { SetBoolFlag(ElementHasAnimations); }
  void SetHasValidDir() { SetBoolFlag(NodeHasValidDirAttribute); }
  void ClearHasValidDir() { ClearBoolFlag(NodeHasValidDirAttribute); }
  bool HasValidDir() const { return GetBoolFlag(NodeHasValidDirAttribute); }
  void SetHasDirAutoSet() {
    MOZ_ASSERT(NodeType() != TEXT_NODE,
               "SetHasDirAutoSet on text node");
    SetBoolFlag(NodeHasDirAutoSet);
  }
  void ClearHasDirAutoSet() {
    MOZ_ASSERT(NodeType() != TEXT_NODE,
               "ClearHasDirAutoSet on text node");
    ClearBoolFlag(NodeHasDirAutoSet);
  }
  bool HasDirAutoSet() const
    { return GetBoolFlag(NodeHasDirAutoSet); }
  void SetHasTextNodeDirectionalityMap() {
    MOZ_ASSERT(NodeType() == TEXT_NODE,
               "SetHasTextNodeDirectionalityMap on non-text node");
    SetBoolFlag(NodeHasTextNodeDirectionalityMap);
  }
  void ClearHasTextNodeDirectionalityMap() {
    MOZ_ASSERT(NodeType() == TEXT_NODE,
               "ClearHasTextNodeDirectionalityMap on non-text node");
    ClearBoolFlag(NodeHasTextNodeDirectionalityMap);
  }
  bool HasTextNodeDirectionalityMap() const {
    MOZ_ASSERT(NodeType() == TEXT_NODE,
               "HasTextNodeDirectionalityMap on non-text node");
    return GetBoolFlag(NodeHasTextNodeDirectionalityMap);
  }

  void SetAncestorHasDirAuto() { SetBoolFlag(NodeAncestorHasDirAuto); }
  void ClearAncestorHasDirAuto() { ClearBoolFlag(NodeAncestorHasDirAuto); }
  bool AncestorHasDirAuto() const { return GetBoolFlag(NodeAncestorHasDirAuto); }

  // Implemented in nsIContentInlines.h.
  inline bool NodeOrAncestorHasDirAuto() const;

  bool HasRelevantHoverRules() const { return GetBoolFlag(NodeHasRelevantHoverRules); }
  void SetHasRelevantHoverRules() { SetBoolFlag(NodeHasRelevantHoverRules); }
  void SetParserHasNotified() { SetBoolFlag(ParserHasNotified); };
  bool HasParserNotified() { return GetBoolFlag(ParserHasNotified); }

  void SetMayBeApzAware() { SetBoolFlag(MayBeApzAware); }
  bool NodeMayBeApzAware() const
  {
    return GetBoolFlag(MayBeApzAware);
  }

  void SetMayHaveAnonymousChildren() { SetBoolFlag(ElementMayHaveAnonymousChildren); }
  bool MayHaveAnonymousChildren() const { return GetBoolFlag(ElementMayHaveAnonymousChildren); }

  void SetHasCustomElementData() { SetBoolFlag(ElementHasCustomElementData); }
  bool HasCustomElementData() const { return GetBoolFlag(ElementHasCustomElementData); }

protected:
  void SetParentIsContent(bool aValue) { SetBoolFlag(ParentIsContent, aValue); }
  void SetIsInDocument() { SetBoolFlag(IsInDocument); }
  void SetNodeIsContent() { SetBoolFlag(NodeIsContent); }
  void ClearInDocument() { ClearBoolFlag(IsInDocument); }
  void SetIsElement() { SetBoolFlag(NodeIsElement); }
  void SetHasID() { SetBoolFlag(ElementHasID); }
  void ClearHasID() { ClearBoolFlag(ElementHasID); }
  void SetMayHaveStyle() { SetBoolFlag(ElementMayHaveStyle); }
  void SetHasName() { SetBoolFlag(ElementHasName); }
  void ClearHasName() { ClearBoolFlag(ElementHasName); }
  void SetMayHaveContentEditableAttr()
    { SetBoolFlag(ElementMayHaveContentEditableAttr); }
  void SetHasLockedStyleStates() { SetBoolFlag(ElementHasLockedStyleStates); }
  void ClearHasLockedStyleStates() { ClearBoolFlag(ElementHasLockedStyleStates); }
  bool HasLockedStyleStates() const
    { return GetBoolFlag(ElementHasLockedStyleStates); }
  void SetHasWeirdParserInsertionMode() { SetBoolFlag(ElementHasWeirdParserInsertionMode); }
  bool HasWeirdParserInsertionMode() const
  { return GetBoolFlag(ElementHasWeirdParserInsertionMode); }
  bool HandlingClick() const { return GetBoolFlag(NodeHandlingClick); }
  void SetHandlingClick() { SetBoolFlag(NodeHandlingClick); }
  void ClearHandlingClick() { ClearBoolFlag(NodeHandlingClick); }

  void SetSubtreeRootPointer(nsINode* aSubtreeRoot)
  {
    NS_ASSERTION(aSubtreeRoot, "aSubtreeRoot can never be null!");
    NS_ASSERTION(!(IsContent() && IsInUncomposedDoc()) &&
                 !IsInShadowTree(), "Shouldn't be here!");
    mSubtreeRoot = aSubtreeRoot;
  }

  void ClearSubtreeRootPointer()
  {
    mSubtreeRoot = nullptr;
  }

public:
  // Makes nsINode object to keep aObject alive.
  void BindObject(nsISupports* aObject);
  // After calling UnbindObject nsINode object doesn't keep
  // aObject alive anymore.
  void UnbindObject(nsISupports* aObject);

  void GenerateXPath(nsAString& aResult);

  already_AddRefed<mozilla::dom::Promise>
  Localize(JSContext* aCx, mozilla::dom::L10nCallback& aCallback, mozilla::ErrorResult& aRv);

  already_AddRefed<mozilla::dom::AccessibleNode> GetAccessibleNode();

  /**
   * Returns the length of this node, as specified at
   * <http://dvcs.w3.org/hg/domcore/raw-file/tip/Overview.html#concept-node-length>
   */
  uint32_t Length() const;

  void GetNodeName(mozilla::dom::DOMString& aNodeName)
  {
    const nsString& nodeName = NodeName();
    aNodeName.SetKnownLiveString(nodeName);
  }
  MOZ_MUST_USE nsresult GetBaseURI(nsAString& aBaseURI) const;
  // Return the base URI for the document.
  // The returned value may differ if the document is loaded via XHR, and
  // when accessed from chrome privileged script and
  // from content privileged script for compatibility.
  void GetBaseURIFromJS(nsAString& aBaseURI,
                        CallerType aCallerType,
                        ErrorResult& aRv) const;
  bool HasChildNodes() const
  {
    return HasChildren();
  }
  uint16_t CompareDocumentPosition(nsINode& aOther) const;
  void GetNodeValue(nsAString& aNodeValue)
  {
    GetNodeValueInternal(aNodeValue);
  }
  void SetNodeValue(const nsAString& aNodeValue,
                    mozilla::ErrorResult& aError)
  {
    SetNodeValueInternal(aNodeValue, aError);
  }
  virtual void GetNodeValueInternal(nsAString& aNodeValue);
  virtual void SetNodeValueInternal(const nsAString& aNodeValue,
                                    mozilla::ErrorResult& aError)
  {
    // The DOM spec says that when nodeValue is defined to be null "setting it
    // has no effect", so we don't throw an exception.
  }
  void EnsurePreInsertionValidity(nsINode& aNewChild, nsINode* aRefChild,
                                  mozilla::ErrorResult& aError);
  nsINode* InsertBefore(nsINode& aNode, nsINode* aChild,
                        mozilla::ErrorResult& aError)
  {
    return ReplaceOrInsertBefore(false, &aNode, aChild, aError);
  }
  nsINode* AppendChild(nsINode& aNode, mozilla::ErrorResult& aError)
  {
    return InsertBefore(aNode, nullptr, aError);
  }
  nsINode* ReplaceChild(nsINode& aNode, nsINode& aChild,
                        mozilla::ErrorResult& aError)
  {
    return ReplaceOrInsertBefore(true, &aNode, &aChild, aError);
  }
  nsINode* RemoveChild(nsINode& aChild, mozilla::ErrorResult& aError);
  already_AddRefed<nsINode> CloneNode(bool aDeep, mozilla::ErrorResult& aError);
  bool IsSameNode(nsINode* aNode);
  bool IsEqualNode(nsINode* aNode);
  void GetNamespaceURI(nsAString& aNamespaceURI) const
  {
    mNodeInfo->GetNamespaceURI(aNamespaceURI);
  }
#ifdef MOZILLA_INTERNAL_API
  void GetPrefix(nsAString& aPrefix)
  {
    mNodeInfo->GetPrefix(aPrefix);
  }
#endif
  void GetLocalName(mozilla::dom::DOMString& aLocalName) const
  {
    const nsString& localName = LocalName();
    aLocalName.SetKnownLiveString(localName);
  }

  nsDOMAttributeMap* GetAttributes();

  // Helper method to remove this node from its parent. This is not exposed
  // through WebIDL.
  // Only call this if the node has a parent node.
  nsresult RemoveFromParent()
  {
    nsINode* parent = GetParentNode();
    mozilla::ErrorResult rv;
    parent->RemoveChild(*this, rv);
    return rv.StealNSResult();
  }

  // ChildNode methods
  mozilla::dom::Element* GetPreviousElementSibling() const;
  mozilla::dom::Element* GetNextElementSibling() const;

  MOZ_CAN_RUN_SCRIPT void Before(const Sequence<OwningNodeOrString>& aNodes,
                                 ErrorResult& aRv);
  MOZ_CAN_RUN_SCRIPT void After(const Sequence<OwningNodeOrString>& aNodes,
                                ErrorResult& aRv);
  MOZ_CAN_RUN_SCRIPT void ReplaceWith(const Sequence<OwningNodeOrString>& aNodes,
                                      ErrorResult& aRv);
  /**
   * Remove this node from its parent, if any.
   */
  void Remove();

  // ParentNode methods
  mozilla::dom::Element* GetFirstElementChild() const;
  mozilla::dom::Element* GetLastElementChild() const;

  MOZ_CAN_RUN_SCRIPT void Prepend(const Sequence<OwningNodeOrString>& aNodes,
                                  ErrorResult& aRv);
  MOZ_CAN_RUN_SCRIPT void Append(const Sequence<OwningNodeOrString>& aNodes,
                                 ErrorResult& aRv);

  void GetBoxQuads(const BoxQuadOptions& aOptions,
                   nsTArray<RefPtr<DOMQuad> >& aResult,
                   CallerType aCallerType,
                   ErrorResult& aRv);

  already_AddRefed<DOMQuad> ConvertQuadFromNode(DOMQuad& aQuad,
                                                const TextOrElementOrDocument& aFrom,
                                                const ConvertCoordinateOptions& aOptions,
                                                CallerType aCallerType,
                                                ErrorResult& aRv);
  already_AddRefed<DOMQuad> ConvertRectFromNode(DOMRectReadOnly& aRect,
                                                const TextOrElementOrDocument& aFrom,
                                                const ConvertCoordinateOptions& aOptions,
                                                CallerType aCallerType,
                                                ErrorResult& aRv);
  already_AddRefed<DOMPoint> ConvertPointFromNode(const DOMPointInit& aPoint,
                                                  const TextOrElementOrDocument& aFrom,
                                                  const ConvertCoordinateOptions& aOptions,
                                                  CallerType aCallerType,
                                                  ErrorResult& aRv);

  const mozilla::LinkedList<nsRange>* GetExistingCommonAncestorRanges() const
  {
    if (!HasSlots()) {
      return nullptr;
    }
    return GetExistingSlots()->mCommonAncestorRanges.get();
  }

  mozilla::LinkedList<nsRange>* GetExistingCommonAncestorRanges()
  {
    if (!HasSlots()) {
      return nullptr;
    }
    return GetExistingSlots()->mCommonAncestorRanges.get();
  }

  mozilla::UniquePtr<mozilla::LinkedList<nsRange>>& GetCommonAncestorRangesPtr()
  {
    return Slots()->mCommonAncestorRanges;
  }

  nsIWeakReference* GetExistingWeakReference()
  {
    return HasSlots() ? GetExistingSlots()->mWeakReference : nullptr;
  }

protected:

  // Override this function to create a custom slots class.
  // Must not return null.
  virtual nsINode::nsSlots* CreateSlots();

  bool HasSlots() const
  {
    return mSlots != nullptr;
  }

  nsSlots* GetExistingSlots() const
  {
    return mSlots;
  }

  nsSlots* Slots()
  {
    if (!HasSlots()) {
      mSlots = CreateSlots();
      MOZ_ASSERT(mSlots);
    }
    return GetExistingSlots();
  }

  nsAutoTObserverArray<nsIMutationObserver*, 1> *GetMutationObservers()
  {
    return HasSlots() ? &GetExistingSlots()->mMutationObservers : nullptr;
  }

  /**
   * Invalidate cached child array inside mChildNodes
   * of type nsParentNodeChildContentList.
   */
  void InvalidateChildNodes();

  virtual void GetTextContentInternal(nsAString& aTextContent,
                                      mozilla::OOMReporter& aError);
  virtual void SetTextContentInternal(const nsAString& aTextContent,
                                      nsIPrincipal* aSubjectPrincipal,
                                      mozilla::ErrorResult& aError)
  {
  }

#ifdef DEBUG
  // Note: virtual so that IsInNativeAnonymousSubtree can be called accross
  // module boundaries.
  virtual void CheckNotNativeAnonymous() const;
#endif

  void EnsurePreInsertionValidity1(nsINode& aNewChild, nsINode* aRefChild,
                                   mozilla::ErrorResult& aError);
  void EnsurePreInsertionValidity2(bool aReplace, nsINode& aNewChild,
                                   nsINode* aRefChild,
                                   mozilla::ErrorResult& aError);
  nsINode* ReplaceOrInsertBefore(bool aReplace, nsINode* aNewChild,
                                 nsINode* aRefChild,
                                 mozilla::ErrorResult& aError);

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
  void doRemoveChildAt(uint32_t aIndex, bool aNotify, nsIContent* aKid,
                       nsAttrAndChildArray& aChildArray);

  /**
   * Most of the implementation of the nsINode InsertChildAt_Deprecated method.
   * Should only be called on document, element, and document fragment
   * nodes.  The aChildArray passed in should be the one for |this|.
   *
   * @param aKid The child to insert.
   * @param aIndex The index to insert at.
   * @param aNotify Whether to notify.
   * @param aChildArray The child array to work with
   */
  nsresult doInsertChildAt(nsIContent* aKid, uint32_t aIndex,
                           bool aNotify, nsAttrAndChildArray& aChildArray);

  /**
   * Parse the given selector string into a servo SelectorList.
   *
   * Never returns null if aRv is not failing.
   *
   * Note that the selector list returned here is owned by the owner doc's
   * selector cache.
   */
  const RawServoSelectorList* ParseSelectorList(const nsAString& aSelectorString,
                                                mozilla::ErrorResult&);

public:
  /* Event stuff that documents and elements share.  This needs to be
     NS_IMETHOD because some subclasses implement DOM methods with
     this exact name and signature and then the calling convention
     needs to match.

     Note that we include DOCUMENT_ONLY_EVENT events here so that we
     can forward all the document stuff to this implementation.
  */
#define EVENT(name_, id_, type_, struct_)                             \
  mozilla::dom::EventHandlerNonNull* GetOn##name_();                  \
  void SetOn##name_(mozilla::dom::EventHandlerNonNull* listener);
#define TOUCH_EVENT EVENT
#define DOCUMENT_ONLY_EVENT EVENT
#include "mozilla/EventNameList.h"
#undef DOCUMENT_ONLY_EVENT
#undef TOUCH_EVENT
#undef EVENT

protected:
  static bool Traverse(nsINode *tmp, nsCycleCollectionTraversalCallback &cb);
  static void Unlink(nsINode *tmp);

  RefPtr<mozilla::dom::NodeInfo> mNodeInfo;

  // mParent is an owning ref most of the time, except for the case of document
  // nodes, so it cannot be represented by nsCOMPtr, so mark is as
  // MOZ_OWNING_REF.
  nsINode* MOZ_OWNING_REF mParent;

private:
#ifndef BOOL_FLAGS_ON_WRAPPER_CACHE
  // Boolean flags.
  uint32_t mBoolFlags;
#endif


protected:
  // These references are non-owning and safe, as they are managed by
  // nsAttrAndChildArray.
  nsIContent* MOZ_NON_OWNING_REF mNextSibling;
  nsIContent* MOZ_NON_OWNING_REF mPreviousSibling;
  // This reference is non-owning and safe, since in the case of documents,
  // it is set to null when the document gets destroyed, and in the case of
  // other nodes, the children keep the parents alive.
  nsIContent* MOZ_NON_OWNING_REF mFirstChild;

  union {
    // Pointer to our primary frame.  Might be null.
    nsIFrame* mPrimaryFrame;

    // Pointer to the root of our subtree.  Might be null.
    // This reference is non-owning and safe, since it either points to the
    // object itself, or is reset by ClearSubtreeRootPointer.
    nsINode* MOZ_NON_OWNING_REF mSubtreeRoot;
  };

  // Storage for more members that are usually not needed; allocated lazily.
  nsSlots* mSlots;
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

NS_DEFINE_STATIC_IID_ACCESSOR(nsINode, NS_INODE_IID)

inline nsISupports*
ToSupports(nsINode* aPointer)
{
  return aPointer;
}

// Some checks are faster to do on nsIContent or Element than on
// nsINode, so spit out FromNode versions taking those types too.
#define NS_IMPL_FROMNODE_GENERIC(_class, _check, _const)                 \
  template<typename T>                                                   \
  static auto FromNode(_const T& aNode)                                  \
    -> decltype(static_cast<_const _class*>(&aNode))                     \
  {                                                                      \
    return aNode._check ? static_cast<_const _class*>(&aNode) : nullptr; \
  }                                                                      \
  template<typename T>                                                   \
  static _const _class* FromNode(_const T* aNode)                        \
  {                                                                      \
    return FromNode(*aNode);                                             \
  }                                                                      \
  template<typename T>                                                   \
  static _const _class* FromNodeOrNull(_const T* aNode)                  \
  {                                                                      \
    return aNode ? FromNode(*aNode) : nullptr;                           \
  }

#define NS_IMPL_FROMNODE_HELPER(_class, _check)                          \
  NS_IMPL_FROMNODE_GENERIC(_class, _check, )                             \
  NS_IMPL_FROMNODE_GENERIC(_class, _check, const)                        \
                                                                         \
  template<typename T>                                                   \
  static _class* FromNode(T&& aNode)                                     \
  {                                                                      \
    /* We need the double-cast in case aNode is a smartptr.  Those */    \
    /* can cast to superclasses of the type they're templated on, */     \
    /* but not directly to subclasses.  */                               \
    return aNode->_check                                                 \
      ? static_cast<_class*>(static_cast<nsINode*>(aNode))               \
      : nullptr;                                                         \
  }                                                                      \
  template<typename T>                                                   \
  static _class* FromNodeOrNull(T&& aNode)                               \
  {                                                                      \
    return aNode ? FromNode(aNode) : nullptr;                            \
  }

#define NS_IMPL_FROMNODE(_class, _nsid)                                 \
  NS_IMPL_FROMNODE_HELPER(_class, IsInNamespace(_nsid))

#define NS_IMPL_FROMNODE_WITH_TAG(_class, _nsid, _tag)                      \
  NS_IMPL_FROMNODE_HELPER(_class, NodeInfo()->Equals(nsGkAtoms::_tag, _nsid))

#define NS_IMPL_FROMNODE_HTML_WITH_TAG(_class, _tag)                        \
  NS_IMPL_FROMNODE_WITH_TAG(_class, kNameSpaceID_XHTML, _tag)

#endif /* nsINode_h___ */
