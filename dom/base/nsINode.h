/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsINode_h___
#define nsINode_h___

#include "mozilla/Likely.h"
#include "mozilla/UniquePtr.h"
#include "nsCOMPtr.h"              // for member, local
#include "nsGkAtoms.h"             // for nsGkAtoms::baseURIProperty
#include "mozilla/dom/NodeInfo.h"  // member (in nsCOMPtr)
#include "nsIWeakReference.h"
#include "nsNodeInfoManager.h"  // for use in NodePrincipal()
#include "nsPropertyTable.h"    // for typedefs
#include "nsTObserverArray.h"   // for member
#include "mozilla/ErrorResult.h"
#include "mozilla/LinkedList.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/EventTarget.h"  // for base class
#include "js/TypeDecls.h"             // for Handle, Value, JSObject, JSContext
#include "mozilla/dom/DOMString.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/NodeBinding.h"
#include "nsTHashtable.h"
#include <iosfwd>

// Including 'windows.h' will #define GetClassInfo to something else.
#ifdef XP_WIN
#  ifdef GetClassInfo
#    undef GetClassInfo
#  endif
#endif

class AttrArray;
class nsAttrChildContentList;
template <typename T>
class nsCOMArray;
class nsDOMAttributeMap;
class nsIAnimationObserver;
class nsIContent;
class nsIContentSecurityPolicy;
class nsIFrame;
class nsIHTMLCollection;
class nsIMutationObserver;
class nsINode;
class nsINodeList;
class nsIPrincipal;
class nsIURI;
class nsNodeSupportsWeakRefTearoff;
class nsDOMMutationObserver;
class nsRange;
class nsWindowSizes;
struct RawServoSelectorList;

namespace mozilla {
class EventListenerManager;
class PresShell;
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
template <typename T>
class AncestorsOfTypeIterator;
struct BoxQuadOptions;
struct ConvertCoordinateOptions;
class DocGroup;
class Document;
class DocumentFragment;
class DocumentOrShadowRoot;
class DOMPoint;
class DOMQuad;
class DOMRectReadOnly;
class Element;
class EventHandlerNonNull;
template <typename T>
class FlatTreeAncestorsOfTypeIterator;
template <typename T>
class InclusiveAncestorsOfTypeIterator;
template <typename T>
class InclusiveFlatTreeAncestorsOfTypeIterator;
class LinkStyle;
class MutationObservers;
template <typename T>
class Optional;
class OwningNodeOrString;
template <typename>
class Sequence;
class ShadowRoot;
class SVGUseElement;
class Text;
class TextOrElementOrDocument;
struct DOMPointInit;
struct GetRootNodeOptions;
enum class CallerType : uint32_t;
}  // namespace dom
}  // namespace mozilla

#define NODE_FLAG_BIT(n_) \
  (nsWrapperCache::FlagsType(1U) << (WRAPPER_CACHE_FLAGS_BITS_USED + (n_)))

enum {
  // This bit will be set if the node has a listener manager.
  NODE_HAS_LISTENERMANAGER = NODE_FLAG_BIT(0),

  // Whether this node has had any properties set on it
  NODE_HAS_PROPERTIES = NODE_FLAG_BIT(1),

  // Whether the node has some ancestor, possibly itself, that is native
  // anonymous.  This includes ancestors crossing XBL scopes, in cases when an
  // XBL binding is attached to an element which has a native anonymous
  // ancestor.  This flag is set-once: once a node has it, it must not be
  // removed.
  // NOTE: Should only be used on nsIContent nodes
  NODE_IS_IN_NATIVE_ANONYMOUS_SUBTREE = NODE_FLAG_BIT(2),

  // Whether this node is the root of a native anonymous (from the perspective
  // of its parent) subtree.  This flag is set-once: once a node has it, it
  // must not be removed.
  // NOTE: Should only be used on nsIContent nodes
  NODE_IS_NATIVE_ANONYMOUS_ROOT = NODE_FLAG_BIT(3),

  NODE_IS_EDITABLE = NODE_FLAG_BIT(4),

  // Whether the node participates in a shadow tree.
  NODE_IS_IN_SHADOW_TREE = NODE_FLAG_BIT(5),

  // Node has an :empty or :-moz-only-whitespace selector
  NODE_HAS_EMPTY_SELECTOR = NODE_FLAG_BIT(6),

  // A child of the node has a selector such that any insertion,
  // removal, or appending of children requires restyling the parent.
  NODE_HAS_SLOW_SELECTOR = NODE_FLAG_BIT(7),

  // A child of the node has a :first-child, :-moz-first-node,
  // :only-child, :last-child or :-moz-last-node selector.
  NODE_HAS_EDGE_CHILD_SELECTOR = NODE_FLAG_BIT(8),

  // A child of the node has a selector such that any insertion or
  // removal of children requires restyling later siblings of that
  // element.  Additionally (in this manner it is stronger than
  // NODE_HAS_SLOW_SELECTOR), if a child's style changes due to any
  // other content tree changes (e.g., the child changes to or from
  // matching :empty due to a grandchild insertion or removal), the
  // child's later siblings must also be restyled.
  NODE_HAS_SLOW_SELECTOR_LATER_SIBLINGS = NODE_FLAG_BIT(9),

  NODE_ALL_SELECTOR_FLAGS = NODE_HAS_EMPTY_SELECTOR | NODE_HAS_SLOW_SELECTOR |
                            NODE_HAS_EDGE_CHILD_SELECTOR |
                            NODE_HAS_SLOW_SELECTOR_LATER_SIBLINGS,

  // This node needs to go through frame construction to get a frame (or
  // undisplayed entry).
  NODE_NEEDS_FRAME = NODE_FLAG_BIT(10),

  // At least one descendant in the flattened tree has NODE_NEEDS_FRAME set.
  // This should be set on every node on the flattened tree path between the
  // node(s) with NODE_NEEDS_FRAME and the root content.
  NODE_DESCENDANTS_NEED_FRAMES = NODE_FLAG_BIT(11),

  // Set if the node has the accesskey attribute set.
  NODE_HAS_ACCESSKEY = NODE_FLAG_BIT(12),

  // Set if the node has right-to-left directionality
  NODE_HAS_DIRECTION_RTL = NODE_FLAG_BIT(13),

  // Set if the node has left-to-right directionality
  NODE_HAS_DIRECTION_LTR = NODE_FLAG_BIT(14),

  NODE_ALL_DIRECTION_FLAGS = NODE_HAS_DIRECTION_LTR | NODE_HAS_DIRECTION_RTL,

  NODE_HAS_BEEN_IN_UA_WIDGET = NODE_FLAG_BIT(15),

  // Set if the node has a nonce value and a header delivered CSP.
  NODE_HAS_NONCE_AND_HEADER_CSP = NODE_FLAG_BIT(16),

  NODE_KEEPS_DOMARENA = NODE_FLAG_BIT(17),
  // Remaining bits are node type specific.
  NODE_TYPE_SPECIFIC_BITS_OFFSET = 18
};

// Make sure we have space for our bits
#define ASSERT_NODE_FLAGS_SPACE(n)                         \
  static_assert(WRAPPER_CACHE_FLAGS_BITS_USED + (n) <=     \
                    sizeof(nsWrapperCache::FlagsType) * 8, \
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
  nsMutationGuard() { mStartingGeneration = sGeneration; }

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
  bool Mutated(uint8_t aIgnoreCount) {
    return (sGeneration - mStartingGeneration) > aIgnoreCount;
  }

  // This function should be called whenever a mutation that we want to keep
  // track of happen. For now this is only done when children are added or
  // removed, but we might do it for attribute changes too in the future.
  static void DidMutate() { sGeneration++; }

 private:
  // This is the value sGeneration had when the guard was constructed.
  uint64_t mStartingGeneration;

  // This value is incremented on every mutation, for the life of the process.
  static uint64_t sGeneration;
};

/**
 * A class that implements nsIWeakReference
 */
class nsNodeWeakReference final : public nsIWeakReference {
 public:
  explicit nsNodeWeakReference(nsINode* aNode);

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIWeakReference
  NS_DECL_NSIWEAKREFERENCE
  size_t SizeOfOnlyThis(mozilla::MallocSizeOf aMallocSizeOf) const override;

  void NoticeNodeDestruction() { mObject = nullptr; }

 private:
  ~nsNodeWeakReference();
};

// This should be used for any nsINode sub-class that has fields of its own
// that it needs to measure; any sub-class that doesn't use it will inherit
// AddSizeOfExcludingThis from its super-class. AddSizeOfIncludingThis() need
// not be defined, it is inherited from nsINode.
#define NS_DECL_ADDSIZEOFEXCLUDINGTHIS                       \
  virtual void AddSizeOfExcludingThis(nsWindowSizes& aSizes, \
                                      size_t* aNodeSize) const override;

// IID for the nsINode interface
// Must be kept in sync with xpcom/rust/xpcom/src/interfaces/nonidl.rs
#define NS_INODE_IID                                 \
  {                                                  \
    0x70ba4547, 0x7699, 0x44fc, {                    \
      0xb3, 0x20, 0x52, 0xdb, 0xe3, 0xd1, 0xf9, 0x0a \
    }                                                \
  }

/**
 * An internal interface that abstracts some DOMNode-related parts that both
 * nsIContent and Document share.  An instance of this interface has a list
 * of nsIContent children and provides access to them.
 */
class nsINode : public mozilla::dom::EventTarget {
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  void AssertInvariantsOnNodeInfoChange();
#endif
 public:
  using BoxQuadOptions = mozilla::dom::BoxQuadOptions;
  using ConvertCoordinateOptions = mozilla::dom::ConvertCoordinateOptions;
  using DocGroup = mozilla::dom::DocGroup;
  using Document = mozilla::dom::Document;
  using DOMPoint = mozilla::dom::DOMPoint;
  using DOMPointInit = mozilla::dom::DOMPointInit;
  using DOMQuad = mozilla::dom::DOMQuad;
  using DOMRectReadOnly = mozilla::dom::DOMRectReadOnly;
  using OwningNodeOrString = mozilla::dom::OwningNodeOrString;
  using TextOrElementOrDocument = mozilla::dom::TextOrElementOrDocument;
  using CallerType = mozilla::dom::CallerType;
  using ErrorResult = mozilla::ErrorResult;

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
  static const auto MAX_NODE_TYPE = NOTATION_NODE;

  void* operator new(size_t aSize, nsNodeInfoManager* aManager);
  void* operator new(size_t aSize) = delete;
  void operator delete(void* aPtr);

  template <class T>
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

  friend class nsNodeWeakReference;
  friend class nsNodeSupportsWeakRefTearoff;
  friend class AttrArray;

#ifdef MOZILLA_INTERNAL_API
  explicit nsINode(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
#endif

  virtual ~nsINode();

  /**
   * Bit-flags to pass (or'ed together) to IsNodeOfType()
   */
  enum {
    /** form control elements */
    eHTML_FORM_CONTROL = 1 << 6,
    /** SVG use targets */
    eUSE_TARGET = 1 << 9,
    /** SVG shapes such as lines and polygons, but not images */
    eSHAPE = 1 << 12
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

  bool IsContainerNode() const {
    return IsElement() || IsDocument() || IsDocumentFragment();
  }

  /**
   * Returns true if the node is a HTMLTemplate element.
   */
  bool IsTemplateElement() const { return IsHTMLElement(nsGkAtoms::_template); }

  bool IsSlotable() const { return IsElement() || IsText(); }

  /**
   * Returns true if this is a document node.
   */
  bool IsDocument() const {
    // One less pointer-chase than checking NodeType().
    return !GetParentNode() && IsInUncomposedDoc();
  }

  /**
   * Return this node as a document. Asserts IsDocument().
   *
   * This is defined inline in Document.h.
   */
  inline Document* AsDocument();
  inline const Document* AsDocument() const;

  /**
   * Returns true if this is a document fragment node.
   */
  bool IsDocumentFragment() const {
    return NodeType() == DOCUMENT_FRAGMENT_NODE;
  }

  /**
   * https://dom.spec.whatwg.org/#concept-tree-inclusive-descendant
   *
   * @param aNode must not be nullptr.
   */
  bool IsInclusiveDescendantOf(const nsINode* aNode) const;

  /**
   * https://dom.spec.whatwg.org/#concept-shadow-including-inclusive-descendant
   *
   * @param aNode must not be nullptr.
   */
  bool IsShadowIncludingInclusiveDescendantOf(const nsINode* aNode) const;

  /**
   * Return this node as a document fragment. Asserts IsDocumentFragment().
   *
   * This is defined inline in DocumentFragment.h.
   */
  inline mozilla::dom::DocumentFragment* AsDocumentFragment();
  inline const mozilla::dom::DocumentFragment* AsDocumentFragment() const;

  JSObject* WrapObject(JSContext*, JS::Handle<JSObject*> aGivenProto) final;

  /**
   * Hook for constructing JS::ubi::Concrete specializations for memory
   * reporting. Specializations are defined in NodeUbiReporting.h.
   */
  virtual void ConstructUbiNode(void* storage) = 0;

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
  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) = 0;

 public:
  mozilla::dom::ParentObject GetParentObject()
      const;  // Implemented in Document.h

  /**
   * Returns the first child of a node or the first child of
   * a template element's content if the provided node is a
   * template element.
   */
  nsIContent* GetFirstChildOfTemplateOrNode();

  /**
   * Return the scope chain parent for this node, for use in things
   * like event handler compilation.  Returning null means to use the
   * global object as the scope chain parent.
   */
  virtual nsINode* GetScopeChainParent() const;

  MOZ_CAN_RUN_SCRIPT mozilla::dom::Element* GetParentFlexElement();

  bool IsNode() const final { return true; }

  NS_IMPL_FROMEVENTTARGET_HELPER(nsINode, IsNode())

  /**
   * Return whether the node is an Element node. Faster than using `NodeType()`.
   */
  bool IsElement() const { return GetBoolFlag(NodeIsElement); }

  virtual bool IsTextControlElement() const { return false; }

  // Returns non-null if this element subclasses `LinkStyle`.
  virtual const mozilla::dom::LinkStyle* AsLinkStyle() const { return nullptr; }
  mozilla::dom::LinkStyle* AsLinkStyle() {
    return const_cast<mozilla::dom::LinkStyle*>(
        static_cast<const nsINode*>(this)->AsLinkStyle());
  }

  /**
   * Return this node as an Element.  Should only be used for nodes
   * for which IsElement() is true.  This is defined inline in Element.h.
   */
  inline mozilla::dom::Element* AsElement();
  inline const mozilla::dom::Element* AsElement() const;

  /**
   * Return whether the node is an nsStyledElement instance or not.
   */
  virtual bool IsStyledElement() const { return false; }

  /**
   * Return this node as nsIContent.  Should only be used for nodes for which
   * IsContent() is true.
   *
   * The assertion in nsIContent's constructor makes this safe.
   */
  nsIContent* AsContent() {
    MOZ_ASSERT(IsContent());
    return reinterpret_cast<nsIContent*>(this);
  }
  const nsIContent* AsContent() const {
    MOZ_ASSERT(IsContent());
    return reinterpret_cast<const nsIContent*>(this);
  }

  /*
   * Return whether the node is a Text node (which might be an actual
   * textnode, or might be a CDATA section).
   */
  bool IsText() const {
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
  bool IsProcessingInstruction() const {
    return NodeType() == PROCESSING_INSTRUCTION_NODE;
  }

  /*
   * Return whether the node is a CharacterData node (text, cdata,
   * comment, processing instruction)
   */
  bool IsCharacterData() const {
    uint32_t nodeType = NodeType();
    return nodeType == TEXT_NODE || nodeType == CDATA_SECTION_NODE ||
           nodeType == PROCESSING_INSTRUCTION_NODE || nodeType == COMMENT_NODE;
  }

  /**
   * Return whether the node is a Comment node.
   */
  bool IsComment() const { return NodeType() == COMMENT_NODE; }

  /**
   * Return whether the node is an Attr node.
   */
  bool IsAttr() const { return NodeType() == ATTRIBUTE_NODE; }

  /**
   * Return if this node has any children.
   */
  bool HasChildren() const { return !!mFirstChild; }

  /**
   * Get the number of children
   * @return the number of children
   */
  uint32_t GetChildCount() const { return mChildCount; }

  /**
   * NOTE: this function is going to be removed soon (hopefully!) Don't use it
   * in new code.
   *
   * Get a child by index
   * @param aIndex the index of the child to get
   * @return the child, or null if index out of bounds
   */
  nsIContent* GetChildAt_Deprecated(uint32_t aIndex) const;

  /**
   * Get the index of a child within this content.
   *
   * @param aPossibleChild the child to get the index of.
   * @return the index of the child, or -1 if not a child. Be aware that
   *         anonymous children (e.g. a <div> child of an <input> element) will
   *         result in -1.
   *
   * If the return value is not -1, then calling GetChildAt_Deprecated() with
   * that value will return aPossibleChild.
   */
  virtual int32_t ComputeIndexOf(const nsINode* aPossibleChild) const;

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
  Document* OwnerDoc() const MOZ_NONNULL_RETURN {
    return mNodeInfo->GetDocument();
  }

  /**
   * Return the "owner document" of this node as an nsINode*.  Implemented
   * in Document.h.
   */
  inline nsINode* OwnerDocAsNode() const MOZ_NONNULL_RETURN;

  /**
   * Returns true if the content has an ancestor that is a document.
   *
   * @return whether this content is in a document tree
   */
  bool IsInUncomposedDoc() const { return GetBoolFlag(IsInDocument); }

  /**
   * Get the document that this content is currently in, if any. This will be
   * null if the content has no ancestor that is a document.
   *
   * @return the current document
   */

  Document* GetUncomposedDoc() const {
    return IsInUncomposedDoc() ? OwnerDoc() : nullptr;
  }

  /**
   * Returns true if we're connected, and thus GetComposedDoc() would return a
   * non-null value.
   */
  bool IsInComposedDoc() const { return GetBoolFlag(IsConnected); }

  /**
   * This method returns the owner document if the node is connected to it
   * (as defined in the DOM spec), otherwise it returns null.
   * In other words, returns non-null even in the case the node is in
   * Shadow DOM, if there is a possibly shadow boundary crossing path from
   * the node to its owner document.
   */
  Document* GetComposedDoc() const {
    return IsInComposedDoc() ? OwnerDoc() : nullptr;
  }

  /**
   * Returns OwnerDoc() if the node is in uncomposed document and ShadowRoot if
   * the node is in Shadow DOM and is in composed document.
   */
  mozilla::dom::DocumentOrShadowRoot* GetUncomposedDocOrConnectedShadowRoot()
      const;

  /**
   * To be called when reference count of the node drops to zero.
   */
  void LastRelease();

  /**
   * The values returned by this function are the ones defined for
   * Node.nodeType
   */
  uint16_t NodeType() const { return mNodeInfo->NodeType(); }
  const nsString& NodeName() const { return mNodeInfo->NodeName(); }
  const nsString& LocalName() const { return mNodeInfo->LocalName(); }

  /**
   * Get the NodeInfo for this element
   * @return the nodes node info
   */
  inline mozilla::dom::NodeInfo* NodeInfo() const { return mNodeInfo; }

  /**
   * Called when we have been adopted, and the information of the
   * node has been changed.
   *
   * The new document can be reached via OwnerDoc().
   *
   * If you override this method,
   * please call up to the parent NodeInfoChanged.
   *
   * If you change this, change also the similar method in Link.
   */
  virtual void NodeInfoChanged(Document* aOldDoc) {
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
    AssertInvariantsOnNodeInfoChange();
#endif
  }

  inline bool IsInNamespace(int32_t aNamespace) const {
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
  inline bool IsNodeInternal() const { return false; }

  template <typename First, typename... Args>
  inline bool IsNodeInternal(First aFirst, Args... aArgs) const {
    return mNodeInfo->Equals(aFirst) || IsNodeInternal(aArgs...);
  }

 public:
  inline bool IsHTMLElement() const {
    return IsElement() && IsInNamespace(kNameSpaceID_XHTML);
  }

  inline bool IsHTMLElement(const nsAtom* aTag) const {
    return IsElement() && mNodeInfo->Equals(aTag, kNameSpaceID_XHTML);
  }

  template <typename First, typename... Args>
  inline bool IsAnyOfHTMLElements(First aFirst, Args... aArgs) const {
    return IsHTMLElement() && IsNodeInternal(aFirst, aArgs...);
  }

  inline bool IsSVGElement() const {
    return IsElement() && IsInNamespace(kNameSpaceID_SVG);
  }

  inline bool IsSVGElement(const nsAtom* aTag) const {
    return IsElement() && mNodeInfo->Equals(aTag, kNameSpaceID_SVG);
  }

  template <typename First, typename... Args>
  inline bool IsAnyOfSVGElements(First aFirst, Args... aArgs) const {
    return IsSVGElement() && IsNodeInternal(aFirst, aArgs...);
  }

  inline bool IsXULElement() const {
    return IsElement() && IsInNamespace(kNameSpaceID_XUL);
  }

  inline bool IsXULElement(const nsAtom* aTag) const {
    return IsElement() && mNodeInfo->Equals(aTag, kNameSpaceID_XUL);
  }

  template <typename First, typename... Args>
  inline bool IsAnyOfXULElements(First aFirst, Args... aArgs) const {
    return IsXULElement() && IsNodeInternal(aFirst, aArgs...);
  }

  inline bool IsMathMLElement() const {
    return IsElement() && IsInNamespace(kNameSpaceID_MathML);
  }

  inline bool IsMathMLElement(const nsAtom* aTag) const {
    return IsElement() && mNodeInfo->Equals(aTag, kNameSpaceID_MathML);
  }

  template <typename First, typename... Args>
  inline bool IsAnyOfMathMLElements(First aFirst, Args... aArgs) const {
    return IsMathMLElement() && IsNodeInternal(aFirst, aArgs...);
  }

  bool IsShadowRoot() const {
    const bool isShadowRoot = IsInShadowTree() && !GetParentNode();
    MOZ_ASSERT_IF(isShadowRoot, IsDocumentFragment());
    return isShadowRoot;
  }

  bool IsHTMLHeadingElement() const {
    return IsAnyOfHTMLElements(nsGkAtoms::h1, nsGkAtoms::h2, nsGkAtoms::h3,
                               nsGkAtoms::h4, nsGkAtoms::h5, nsGkAtoms::h6);
  }

  /**
   * Insert a content node before another or at the end.
   * This method handles calling BindToTree on the child appropriately.
   *
   * @param aKid the content to insert
   * @param aBeforeThis an existing node. Use nullptr if you want to
   *        add aKid at the end.
   * @param aNotify whether to notify the document (current document for
   *        nsIContent, and |this| for Document) that the insert has occurred
   * @param aRv The error, if any.
   *        Throw NS_ERROR_DOM_HIERARCHY_REQUEST_ERR if one attempts to have
   *        more than one element node as a child of a document.  Doing this
   *        will also assert -- you shouldn't be doing it!  Check with
   *        Document::GetRootElement() first if you're not sure.  Apart from
   *        this one constraint, this doesn't do any checking on whether aKid is
   *        a valid child of |this|.
   *        Throw NS_ERROR_OUT_OF_MEMORY in some cases (from BindToTree).
   */
  virtual void InsertChildBefore(nsIContent* aKid, nsIContent* aBeforeThis,
                                 bool aNotify, mozilla::ErrorResult& aRv);

  /**
   * Append a content node to the end of the child list.  This method handles
   * calling BindToTree on the child appropriately.
   *
   * @param aKid the content to append
   * @param aNotify whether to notify the document (current document for
   *        nsIContent, and |this| for Document) that the append has occurred
   * @param aRv The error, if any.
   *        Throw NS_ERROR_DOM_HIERARCHY_REQUEST_ERR if one attempts to have
   *        more than one element node as a child of a document.  Doing this
   *        will also assert -- you shouldn't be doing it!  Check with
   *        Document::GetRootElement() first if you're not sure.  Apart from
   *        this one constraint, this doesn't do any checking on whether aKid is
   *        a valid child of |this|.
   *        Throw NS_ERROR_OUT_OF_MEMORY in some cases (from BindToTree).
   */
  void AppendChildTo(nsIContent* aKid, bool aNotify,
                     mozilla::ErrorResult& aRv) {
    InsertChildBefore(aKid, nullptr, aNotify, aRv);
  }

  /**
   * Remove a child from this node.  This method handles calling UnbindFromTree
   * on the child appropriately.
   *
   * @param aKid the content to remove
   * @param aNotify whether to notify the document (current document for
   *        nsIContent, and |this| for Document) that the remove has occurred
   */
  virtual void RemoveChildNode(nsIContent* aKid, bool aNotify);

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
  void* GetProperty(const nsAtom* aPropertyName,
                    nsresult* aStatus = nullptr) const;

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
  nsresult SetProperty(nsAtom* aPropertyName, void* aValue,
                       NSPropertyDtorFunc aDtor = nullptr,
                       bool aTransfer = false);

  /**
   * A generic destructor for property values allocated with new.
   */
  template <class T>
  static void DeleteProperty(void*, nsAtom*, void* aPropertyValue, void*) {
    delete static_cast<T*>(aPropertyValue);
  }

  /**
   * Removes a property associated with this node. The value is destroyed using
   * the destruction function given when that value was set.
   *
   * @param aPropertyName  name of property to destroy.
   */
  void RemoveProperty(const nsAtom* aPropertyName);

  /**
   * Take a property associated with this node. The value will not be destroyed
   * but rather returned. It is the caller's responsibility to destroy the value
   * after that point.
   *
   * @param aPropertyName  name of property to unset.
   * @param aStatus        out parameter for storing resulting status.
   *                       Set to NS_PROPTABLE_PROP_NOT_THERE if the property
   *                       is not set.
   * @return               the property. Null if the property is not set
   *                       (though a null return value does not imply the
   *                       property was not set, i.e. it can be set to null).
   */
  void* TakeProperty(const nsAtom* aPropertyName, nsresult* aStatus = nullptr);

  bool HasProperties() const { return HasFlag(NODE_HAS_PROPERTIES); }

  /**
   * Return the principal of this node.  This is guaranteed to never be a null
   * pointer.
   */
  nsIPrincipal* NodePrincipal() const {
    return mNodeInfo->NodeInfoManager()->DocumentPrincipal();
  }

  /**
   * Return the CSP of this node's document, if any.
   */
  nsIContentSecurityPolicy* GetCsp() const;

  /**
   * Get the parent nsIContent for this node.
   * @return the parent, or null if no parent or the parent is not an nsIContent
   */
  nsIContent* GetParent() const {
    return MOZ_LIKELY(GetBoolFlag(ParentIsContent)) ? mParent->AsContent()
                                                    : nullptr;
  }

  /**
   * Get the parent nsINode for this node. This can be either an nsIContent, a
   * Document or an Attr.
   * @return the parent node
   */
  nsINode* GetParentNode() const { return mParent; }

  nsINode* GetParentOrShadowHostNode() const;

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

  nsINode* GetFlattenedTreeParentNodeNonInline() const;

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
   * Get closest element node for the node.  Meaning that if the node is an
   * element node, returns itself.  Otherwise, returns parent element or null.
   */
  inline mozilla::dom::Element* GetAsElementOrParentElement() const;

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

  virtual mozilla::EventListenerManager* GetExistingListenerManager()
      const override;
  virtual mozilla::EventListenerManager* GetOrCreateListenerManager() override;

  mozilla::Maybe<mozilla::dom::EventCallbackDebuggerNotificationType>
  GetDebuggerNotificationType() const override;

  bool ComputeDefaultWantsUntrusted(mozilla::ErrorResult& aRv) final;

  virtual bool IsApzAware() const override;

  virtual nsPIDOMWindowOuter* GetOwnerGlobalForBindingsInternal() override;
  virtual nsIGlobalObject* GetOwnerGlobal() const override;

  using mozilla::dom::EventTarget::DispatchEvent;
  bool DispatchEvent(mozilla::dom::Event& aEvent,
                     mozilla::dom::CallerType aCallerType,
                     mozilla::ErrorResult& aRv) override;

  MOZ_CAN_RUN_SCRIPT
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
  void AddMutationObserver(nsIMutationObserver* aMutationObserver) {
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
  void AddMutationObserverUnlessExists(nsIMutationObserver* aMutationObserver) {
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
  void AddAnimationObserverUnlessExists(
      nsIAnimationObserver* aAnimationObserver);

  /**
   * Removes a mutation observer.
   */
  void RemoveMutationObserver(nsIMutationObserver* aMutationObserver) {
    nsSlots* s = GetExistingSlots();
    if (s) {
      s->mMutationObservers.RemoveElement(aMutationObserver);
    }
  }

  nsAutoTObserverArray<nsIMutationObserver*, 1>* GetMutationObservers() {
    return HasSlots() ? &GetExistingSlots()->mMutationObservers : nullptr;
  }

  /**
   * Helper methods to access ancestor node(s) of type T.
   * The implementations of the methods are in mozilla/dom/AncestorIterator.h.
   */
  template <typename T>
  inline mozilla::dom::AncestorsOfTypeIterator<T> AncestorsOfType() const;

  template <typename T>
  inline mozilla::dom::InclusiveAncestorsOfTypeIterator<T>
  InclusiveAncestorsOfType() const;

  template <typename T>
  inline mozilla::dom::FlatTreeAncestorsOfTypeIterator<T>
  FlatTreeAncestorsOfType() const;

  template <typename T>
  inline mozilla::dom::InclusiveFlatTreeAncestorsOfTypeIterator<T>
  InclusiveFlatTreeAncestorsOfType() const;

  template <typename T>
  T* FirstAncestorOfType() const;

 private:
  /**
   * Walks aNode, its attributes and, if aDeep is true, its descendant nodes.
   * If aClone is true the nodes will be cloned. If aNewNodeInfoManager is
   * not null, it is used to create new nodeinfos for the nodes. Also reparents
   * the XPConnect wrappers for the nodes into aReparentScope if non-null.
   *
   * @param aNode Node to adopt/clone.
   * @param aClone If true the node will be cloned and the cloned node will
   *               be returned.
   * @param aDeep If true the function will be called recursively on
   *              descendants of the node
   * @param aNewNodeInfoManager The nodeinfo manager to use to create new
   *                            nodeinfos for aNode and its attributes and
   *                            descendants. May be null if the nodeinfos
   *                            shouldn't be changed.
   * @param aReparentScope Scope into which wrappers should be reparented, or
   *                             null if no reparenting should be done.
   * @param aParent If aClone is true the cloned node will be appended to
   *                aParent's children. May be null. If not null then aNode
   *                must be an nsIContent.
   * @param aError The error, if any.
   *
   * @return If aClone is true then the cloned node will be returned,
   *          unless an error occurred.  In error conditions, null
   *          will be returned.
   */
  static already_AddRefed<nsINode> CloneAndAdopt(
      nsINode* aNode, bool aClone, bool aDeep,
      nsNodeInfoManager* aNewNodeInfoManager,
      JS::Handle<JSObject*> aReparentScope, nsINode* aParent,
      mozilla::ErrorResult& aError);

 public:
  /**
   * Walks the node, its attributes and descendant nodes. If aNewNodeInfoManager
   * is not null, it is used to create new nodeinfos for the nodes. Also
   * reparents the XPConnect wrappers for the nodes into aReparentScope if
   * non-null.
   *
   * @param aNewNodeInfoManager The nodeinfo manager to use to create new
   *                            nodeinfos for the node and its attributes and
   *                            descendants. May be null if the nodeinfos
   *                            shouldn't be changed.
   * @param aReparentScope New scope for the wrappers, or null if no reparenting
   *                       should be done.
   * @param aError The error, if any.
   */
  void Adopt(nsNodeInfoManager* aNewNodeInfoManager,
             JS::Handle<JSObject*> aReparentScope,
             mozilla::ErrorResult& aError);

  /**
   * Clones the node, its attributes and, if aDeep is true, its descendant nodes
   * If aNewNodeInfoManager is not null, it is used to create new nodeinfos for
   * the clones.
   *
   * @param aDeep If true the function will be called recursively on
   *              descendants of the node
   * @param aNewNodeInfoManager The nodeinfo manager to use to create new
   *                            nodeinfos for the node and its attributes and
   *                            descendants. May be null if the nodeinfos
   *                            shouldn't be changed.
   * @param aError The error, if any.
   *
   * @return The newly created node.  Null in error conditions.
   */
  already_AddRefed<nsINode> Clone(bool aDeep,
                                  nsNodeInfoManager* aNewNodeInfoManager,
                                  mozilla::ErrorResult& aError);

  /**
   * Clones this node. This needs to be overriden by all node classes. aNodeInfo
   * should be identical to this node's nodeInfo, except for the document which
   * may be different. When cloning an element, all attributes of the element
   * will be cloned. The children of the node will not be cloned.
   *
   * @param aNodeInfo the nodeinfo to use for the clone
   * @param aResult the clone
   */
  virtual nsresult Clone(mozilla::dom::NodeInfo*, nsINode** aResult) const = 0;

  // This class can be extended by subclasses that wish to store more
  // information in the slots.
  class nsSlots {
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
     * their endpoints' closest common inclusive ancestor
     * (https://dom.spec.whatwg.org/#concept-tree-inclusive-ancestor).  This is
     * a UniquePtr instead of just a LinkedList, because that prevents us from
     * pushing DOMSlots up to the next allocation bucket size, at the cost of
     * some complexity.
     */
    mozilla::UniquePtr<mozilla::LinkedList<nsRange>>
        mClosestCommonInclusiveAncestorRanges;
  };

  /**
   * Functions for managing flags and slots
   */
#ifdef DEBUG
  nsSlots* DebugGetSlots() { return Slots(); }
#endif

  void SetFlags(FlagsType aFlagsToSet) {
    NS_ASSERTION(
        !(aFlagsToSet &
          (NODE_IS_NATIVE_ANONYMOUS_ROOT | NODE_IS_IN_NATIVE_ANONYMOUS_SUBTREE |
           NODE_DESCENDANTS_NEED_FRAMES | NODE_NEEDS_FRAME |
           NODE_HAS_BEEN_IN_UA_WIDGET)) ||
            IsContent(),
        "Flag only permitted on nsIContent nodes");
    nsWrapperCache::SetFlags(aFlagsToSet);
  }

  void UnsetFlags(FlagsType aFlagsToUnset) {
    NS_ASSERTION(!(aFlagsToUnset & (NODE_HAS_BEEN_IN_UA_WIDGET |
                                    NODE_IS_NATIVE_ANONYMOUS_ROOT)),
                 "Trying to unset write-only flags");
    nsWrapperCache::UnsetFlags(aFlagsToUnset);
  }

  void SetEditableFlag(bool aEditable) {
    if (aEditable) {
      SetFlags(NODE_IS_EDITABLE);
    } else {
      UnsetFlags(NODE_IS_EDITABLE);
    }
  }

  inline bool IsEditable() const;

  /**
   * Check if this node is in design mode or not.  When this returns true and:
   * - if this is a Document node, it's the design mode root.
   * - if this is a content node, it's connected, it's not in a shadow tree
   *   (except shadow tree for UI widget and native anonymous subtree) and its
   *   uncomposed document is in design mode.
   * Note that returning true does NOT mean the node or its children is
   * editable.  E.g., when this node is in a shadow tree of a UA widget and its
   * host is in design mode.
   */
  inline bool IsInDesignMode() const;

  /**
   * Returns true if |this| or any of its ancestors is native anonymous.
   */
  bool IsInNativeAnonymousSubtree() const {
    return HasFlag(NODE_IS_IN_NATIVE_ANONYMOUS_SUBTREE);
  }

  /**
   * If |this| or any ancestor is native anonymous, return the root of the
   * native anonymous subtree. Note that in case of nested native anonymous
   * content, this returns the innermost root, not the outermost.
   */
  nsIContent* GetClosestNativeAnonymousSubtreeRoot() const {
    if (!IsInNativeAnonymousSubtree()) {
      return nullptr;
    }
    MOZ_ASSERT(IsContent(), "How did non-content end up in NAC?");
    for (const nsINode* node = this; node; node = node->GetParentNode()) {
      if (node->IsRootOfNativeAnonymousSubtree()) {
        return const_cast<nsINode*>(node)->AsContent();
      }
    }
    // FIXME(emilio): This should not happen, usually, but editor removes nodes
    // in native anonymous subtrees, and we don't clean nodes from the current
    // event content stack from ContentRemoved, so it can actually happen, see
    // bug 1510208.
    NS_WARNING("GetClosestNativeAnonymousSubtreeRoot on disconnected NAC!");
    return nullptr;
  }

  /**
   * If |this| or any ancestor is native anonymous, return the parent of the
   * native anonymous subtree. Note that in case of nested native anonymous
   * content, this returns the parent of the innermost root, not the outermost.
   */
  nsIContent* GetClosestNativeAnonymousSubtreeRootParent() const {
    const nsIContent* root = GetClosestNativeAnonymousSubtreeRoot();
    if (!root) {
      return nullptr;
    }
    // We could put this in nsIContentInlines.h or such to avoid this
    // reinterpret_cast, but it doesn't seem worth it.
    return reinterpret_cast<const nsINode*>(root)->GetParent();
  }

  /**
   * Gets the root of the node tree for this content if it is in a shadow tree.
   */
  mozilla::dom::ShadowRoot* GetContainingShadow() const;
  /**
   * Gets the shadow host if this content is in a shadow tree. That is, the host
   * of |GetContainingShadow|, if its not null.
   *
   * @return The shadow host, if this is in shadow tree, or null.
   */
  nsIContent* GetContainingShadowHost() const;

  bool IsInSVGUseShadowTree() const {
    return !!GetContainingSVGUseShadowHost();
  }

  mozilla::dom::SVGUseElement* GetContainingSVGUseShadowHost() const {
    if (!IsInShadowTree()) {
      return nullptr;
    }
    return DoGetContainingSVGUseShadowHost();
  }

  // Whether this node has ever been part of a UA widget shadow tree.
  bool HasBeenInUAWidget() const { return HasFlag(NODE_HAS_BEEN_IN_UA_WIDGET); }

  // True for native anonymous content and for content in UA widgets.
  // Only nsIContent can fulfill this condition.
  bool ChromeOnlyAccess() const {
    return HasFlag(NODE_IS_IN_NATIVE_ANONYMOUS_SUBTREE |
                   NODE_HAS_BEEN_IN_UA_WIDGET);
  }

  const nsIContent* GetChromeOnlyAccessSubtreeRootParent() const {
    if (!ChromeOnlyAccess()) {
      return nullptr;
    }
    // We can have NAC in UA widgets, but not the other way around.
    if (IsInNativeAnonymousSubtree()) {
      return GetClosestNativeAnonymousSubtreeRootParent();
    }
    return GetContainingShadowHost();
  }

  bool IsInShadowTree() const { return HasFlag(NODE_IS_IN_SHADOW_TREE); }

  /**
   * Get whether this node is C++-generated anonymous content
   * @see nsIAnonymousContentCreator
   * @return whether this content is anonymous
   */
  bool IsRootOfNativeAnonymousSubtree() const {
    NS_ASSERTION(
        !HasFlag(NODE_IS_NATIVE_ANONYMOUS_ROOT) || IsInNativeAnonymousSubtree(),
        "Some flags seem to be missing!");
    return HasFlag(NODE_IS_NATIVE_ANONYMOUS_ROOT);
  }

  // Whether this node is a UA Widget ShadowRoot.
  inline bool IsUAWidget() const;
  // Whether this node is currently in a UA Widget Shadow tree.
  inline bool IsInUAWidget() const;
  // Whether this node is the root of a ChromeOnlyAccess DOM subtree.
  inline bool IsRootOfChromeAccessOnlySubtree() const;

  /**
   * Returns true if |this| node is the closest common inclusive ancestor
   * (https://dom.spec.whatwg.org/#concept-tree-inclusive-ancestor) of the
   * start/end nodes of a Range in a Selection or a descendant of such a common
   * ancestor. This node is definitely not selected when |false| is returned,
   * but it may or may not be selected when |true| is returned.
   */
  bool IsMaybeSelected() const {
    return IsDescendantOfClosestCommonInclusiveAncestorForRangeInSelection() ||
           IsClosestCommonInclusiveAncestorForRangeInSelection();
  }

  /**
   * Return true if any part of (this, aStartOffset) .. (this, aEndOffset)
   * overlaps any nsRange in
   * GetClosestCommonInclusiveAncestorForRangeInSelection ranges (i.e.
   * where this is a descendant of a range's common inclusive ancestor node).
   * If a nsRange starts in (this, aEndOffset) or if it ends in
   * (this, aStartOffset) then it is non-overlapping and the result is false
   * for that nsRange.  Collapsed ranges always counts as non-overlapping.
   *
   * @param aStartOffset has to be less or equal to aEndOffset.
   */
  bool IsSelected(uint32_t aStartOffset, uint32_t aEndOffset) const;

  /**
   * Get the root element of the text editor associated with this node or the
   * root element of the text editor of the ancestor 'TextControlElement' if
   * this is in its native anonymous subtree.  I.e., this returns anonymous
   * `<div>` element of a `TextEditor`. Note that this can be used only for
   * getting root content of `<input>` or `<textarea>`.  I.e., this method
   * doesn't support HTML editors. Note that this may create a `TextEditor`
   * instance, and it means that the `TextEditor` may modify its native
   * anonymous subtree and may run selection listeners.
   */
  MOZ_CAN_RUN_SCRIPT mozilla::dom::Element* GetAnonymousRootElementOfTextEditor(
      mozilla::TextEditor** aTextEditor = nullptr);

  /**
   * Get the nearest selection root, ie. the node that will be selected if the
   * user does "Select All" while the focus is in this node. Note that if this
   * node is not in an editor, the result comes from the nsFrameSelection that
   * is related to aPresShell, so the result might not be the ancestor of this
   * node. Be aware that if this node and the computed selection limiter are
   * not in same subtree, this returns the root content of the closeset subtree.
   */
  MOZ_CAN_RUN_SCRIPT nsIContent* GetSelectionRootContent(
      mozilla::PresShell* aPresShell);

  nsINodeList* ChildNodes();

  nsIContent* GetFirstChild() const { return mFirstChild; }

  nsIContent* GetLastChild() const;

  /**
   * Implementation is in Document.h, because it needs to cast from
   * Document* to nsINode*.
   */
  Document* GetOwnerDocument() const;

  void Normalize();

  /**
   * Get the base URI for any relative URIs within this piece of
   * content. Generally, this is the document's base URI, but certain
   * content carries a local base for backward compatibility.
   *
   * @return the base URI.  May return null.
   */
  virtual nsIURI* GetBaseURI(bool aTryUseXHRDocBaseURI = false) const = 0;
  nsIURI* GetBaseURIObject() const;

  /**
   * Return true if the node may be apz aware. There are two cases. One is that
   * the node is apz aware (such as HTMLInputElement with number type). The
   * other is that the node has apz aware listeners. This is a non-virtual
   * function which calls IsNodeApzAwareInternal only when the MayBeApzAware is
   * set. We check the details in IsNodeApzAwareInternal which may be overriden
   * by child classes
   */
  bool IsNodeApzAware() const {
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

  void GetTextContent(nsAString& aTextContent, mozilla::OOMReporter& aError) {
    GetTextContentInternal(aTextContent, aError);
  }
  void SetTextContent(const nsAString& aTextContent,
                      nsIPrincipal* aSubjectPrincipal,
                      mozilla::ErrorResult& aError) {
    SetTextContentInternal(aTextContent, aSubjectPrincipal, aError);
  }
  void SetTextContent(const nsAString& aTextContent,
                      mozilla::ErrorResult& aError) {
    SetTextContentInternal(aTextContent, nullptr, aError);
  }

  mozilla::dom::Element* QuerySelector(const nsACString& aSelector,
                                       mozilla::ErrorResult& aResult);
  already_AddRefed<nsINodeList> QuerySelectorAll(const nsACString& aSelector,
                                                 mozilla::ErrorResult& aResult);

 protected:
  // Document and ShadowRoot override this with its own (faster) version.
  // This should really only be called for elements and document fragments.
  mozilla::dom::Element* GetElementById(const nsAString& aId);

  void AppendChildToChildList(nsIContent* aKid);
  void InsertChildToChildList(nsIContent* aKid, nsIContent* aNextSibling);
  void DisconnectChild(nsIContent* aKid);

 public:
  void LookupPrefix(const nsAString& aNamespace, nsAString& aResult);
  bool IsDefaultNamespace(const nsAString& aNamespaceURI) {
    nsAutoString defaultNamespace;
    LookupNamespaceURI(u""_ns, defaultNamespace);
    return aNamespaceURI.Equals(defaultNamespace);
  }
  void LookupNamespaceURI(const nsAString& aNamespacePrefix,
                          nsAString& aNamespaceURI);

  nsIContent* GetNextSibling() const { return mNextSibling; }
  nsIContent* GetPreviousSibling() const;

  /**
   * Get the next node in the pre-order tree traversal of the DOM.  If
   * aRoot is non-null, then it must be an ancestor of |this|
   * (possibly equal to |this|) and only nodes that are descendants of
   * aRoot, not including aRoot itself, will be returned.  Returns
   * null if there are no more nodes to traverse.
   */
  nsIContent* GetNextNode(const nsINode* aRoot = nullptr) const {
    return GetNextNodeImpl(aRoot, false);
  }

  /**
   * Get the next node in the pre-order tree traversal of the DOM but ignoring
   * the children of this node.  If aRoot is non-null, then it must be an
   * ancestor of |this| (possibly equal to |this|) and only nodes that are
   * descendants of aRoot, not including aRoot itself, will be returned.
   * Returns null if there are no more nodes to traverse.
   */
  nsIContent* GetNextNonChildNode(const nsINode* aRoot = nullptr) const {
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
  mozilla::dom::SVGUseElement* DoGetContainingSVGUseShadowHost() const;

  nsIContent* GetNextNodeImpl(const nsINode* aRoot,
                              const bool aSkipChildren) const {
#ifdef DEBUG
    if (aRoot) {
      // TODO: perhaps nsINode::IsInclusiveDescendantOf could be used instead.
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
  nsIContent* GetPreviousContent(const nsINode* aRoot = nullptr) const {
#ifdef DEBUG
    if (aRoot) {
      // TODO: perhaps nsINode::IsInclusiveDescendantOf could be used instead.
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
    // Set if we're part of the composed doc.
    // https://dom.spec.whatwg.org/#connected
    IsConnected,
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
    // Set if the element has a part attribute set.
    ElementHasPart,
    // Set if the element might have a contenteditable attribute set.
    ElementMayHaveContentEditableAttr,
    // Set if the node is the closest common inclusive ancestor of the start/end
    // nodes of a Range that is in a Selection.
    NodeIsClosestCommonInclusiveAncestorForRangeInSelection,
    // Set if the node is a descendant of a node with the above bit set.
    NodeIsDescendantOfClosestCommonInclusiveAncestorForRangeInSelection,
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
    // Set if the element was created from prototype cache and
    // its l10n attributes haven't been changed.
    ElementCreatedFromPrototypeAndHasUnmodifiedL10n,
    // Guard value
    BooleanFlagCount
  };

  void SetBoolFlag(BooleanFlag name, bool value) {
    static_assert(BooleanFlagCount <= 8 * sizeof(mBoolFlags),
                  "Too many boolean flags");
    mBoolFlags = (mBoolFlags & ~(1 << name)) | (value << name);
  }

  void SetBoolFlag(BooleanFlag name) {
    static_assert(BooleanFlagCount <= 8 * sizeof(mBoolFlags),
                  "Too many boolean flags");
    mBoolFlags |= (1 << name);
  }

  void ClearBoolFlag(BooleanFlag name) {
    static_assert(BooleanFlagCount <= 8 * sizeof(mBoolFlags),
                  "Too many boolean flags");
    mBoolFlags &= ~(1 << name);
  }

  bool GetBoolFlag(BooleanFlag name) const {
    static_assert(BooleanFlagCount <= 8 * sizeof(mBoolFlags),
                  "Too many boolean flags");
    return mBoolFlags & (1 << name);
  }

 public:
  bool HasRenderingObservers() const {
    return GetBoolFlag(NodeHasRenderingObservers);
  }
  void SetHasRenderingObservers(bool aValue) {
    SetBoolFlag(NodeHasRenderingObservers, aValue);
  }
  bool IsContent() const { return GetBoolFlag(NodeIsContent); }
  bool HasID() const { return GetBoolFlag(ElementHasID); }
  bool MayHaveClass() const { return GetBoolFlag(ElementMayHaveClass); }
  void SetMayHaveClass() { SetBoolFlag(ElementMayHaveClass); }
  bool MayHaveStyle() const { return GetBoolFlag(ElementMayHaveStyle); }
  bool HasName() const { return GetBoolFlag(ElementHasName); }
  bool HasPartAttribute() const { return GetBoolFlag(ElementHasPart); }
  bool MayHaveContentEditableAttr() const {
    return GetBoolFlag(ElementMayHaveContentEditableAttr);
  }
  /**
   * https://dom.spec.whatwg.org/#concept-tree-inclusive-ancestor
   */
  bool IsClosestCommonInclusiveAncestorForRangeInSelection() const {
    return GetBoolFlag(NodeIsClosestCommonInclusiveAncestorForRangeInSelection);
  }
  /**
   * https://dom.spec.whatwg.org/#concept-tree-inclusive-ancestor
   */
  void SetClosestCommonInclusiveAncestorForRangeInSelection() {
    SetBoolFlag(NodeIsClosestCommonInclusiveAncestorForRangeInSelection);
  }
  /**
   * https://dom.spec.whatwg.org/#concept-tree-inclusive-ancestor
   */
  void ClearClosestCommonInclusiveAncestorForRangeInSelection() {
    ClearBoolFlag(NodeIsClosestCommonInclusiveAncestorForRangeInSelection);
  }
  /**
   * https://dom.spec.whatwg.org/#concept-tree-inclusive-ancestor
   */
  bool IsDescendantOfClosestCommonInclusiveAncestorForRangeInSelection() const {
    return GetBoolFlag(
        NodeIsDescendantOfClosestCommonInclusiveAncestorForRangeInSelection);
  }
  /**
   * https://dom.spec.whatwg.org/#concept-tree-inclusive-ancestor
   */
  void SetDescendantOfClosestCommonInclusiveAncestorForRangeInSelection() {
    SetBoolFlag(
        NodeIsDescendantOfClosestCommonInclusiveAncestorForRangeInSelection);
  }
  /**
   * https://dom.spec.whatwg.org/#concept-tree-inclusive-ancestor
   */
  void ClearDescendantOfClosestCommonInclusiveAncestorForRangeInSelection() {
    ClearBoolFlag(
        NodeIsDescendantOfClosestCommonInclusiveAncestorForRangeInSelection);
  }

  void SetCCMarkedRoot(bool aValue) { SetBoolFlag(NodeIsCCMarkedRoot, aValue); }
  bool CCMarkedRoot() const { return GetBoolFlag(NodeIsCCMarkedRoot); }
  void SetInCCBlackTree(bool aValue) { SetBoolFlag(NodeIsCCBlackTree, aValue); }
  bool InCCBlackTree() const { return GetBoolFlag(NodeIsCCBlackTree); }
  void SetIsPurpleRoot(bool aValue) { SetBoolFlag(NodeIsPurpleRoot, aValue); }
  bool IsPurpleRoot() const { return GetBoolFlag(NodeIsPurpleRoot); }
  bool MayHaveDOMMutationObserver() {
    return GetBoolFlag(NodeMayHaveDOMMutationObserver);
  }
  void SetMayHaveDOMMutationObserver() {
    SetBoolFlag(NodeMayHaveDOMMutationObserver, true);
  }
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
    MOZ_ASSERT(NodeType() != TEXT_NODE, "SetHasDirAutoSet on text node");
    SetBoolFlag(NodeHasDirAutoSet);
  }
  void ClearHasDirAutoSet() {
    MOZ_ASSERT(NodeType() != TEXT_NODE, "ClearHasDirAutoSet on text node");
    ClearBoolFlag(NodeHasDirAutoSet);
  }
  bool HasDirAutoSet() const { return GetBoolFlag(NodeHasDirAutoSet); }
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
  bool AncestorHasDirAuto() const {
    return GetBoolFlag(NodeAncestorHasDirAuto);
  }

  // Implemented in nsIContentInlines.h.
  inline bool NodeOrAncestorHasDirAuto() const;

  void SetParserHasNotified() { SetBoolFlag(ParserHasNotified); };
  bool HasParserNotified() { return GetBoolFlag(ParserHasNotified); }

  void SetMayBeApzAware() { SetBoolFlag(MayBeApzAware); }
  bool NodeMayBeApzAware() const { return GetBoolFlag(MayBeApzAware); }

  void SetMayHaveAnonymousChildren() {
    SetBoolFlag(ElementMayHaveAnonymousChildren);
  }
  bool MayHaveAnonymousChildren() const {
    return GetBoolFlag(ElementMayHaveAnonymousChildren);
  }

  void SetHasCustomElementData() { SetBoolFlag(ElementHasCustomElementData); }
  bool HasCustomElementData() const {
    return GetBoolFlag(ElementHasCustomElementData);
  }

  void SetElementCreatedFromPrototypeAndHasUnmodifiedL10n() {
    SetBoolFlag(ElementCreatedFromPrototypeAndHasUnmodifiedL10n);
  }
  bool HasElementCreatedFromPrototypeAndHasUnmodifiedL10n() {
    return GetBoolFlag(ElementCreatedFromPrototypeAndHasUnmodifiedL10n);
  }
  void ClearElementCreatedFromPrototypeAndHasUnmodifiedL10n() {
    ClearBoolFlag(ElementCreatedFromPrototypeAndHasUnmodifiedL10n);
  }

 protected:
  void SetParentIsContent(bool aValue) { SetBoolFlag(ParentIsContent, aValue); }
  void SetIsInDocument() { SetBoolFlag(IsInDocument); }
  void ClearInDocument() { ClearBoolFlag(IsInDocument); }
  void SetIsConnected(bool aConnected) { SetBoolFlag(IsConnected, aConnected); }
  void SetNodeIsContent() { SetBoolFlag(NodeIsContent); }
  void SetIsElement() { SetBoolFlag(NodeIsElement); }
  void SetHasID() { SetBoolFlag(ElementHasID); }
  void ClearHasID() { ClearBoolFlag(ElementHasID); }
  void SetMayHaveStyle() { SetBoolFlag(ElementMayHaveStyle); }
  void SetHasName() { SetBoolFlag(ElementHasName); }
  void ClearHasName() { ClearBoolFlag(ElementHasName); }
  void SetHasPartAttribute(bool aPart) { SetBoolFlag(ElementHasPart, aPart); }
  void SetMayHaveContentEditableAttr() {
    SetBoolFlag(ElementMayHaveContentEditableAttr);
  }
  void SetHasLockedStyleStates() { SetBoolFlag(ElementHasLockedStyleStates); }
  void ClearHasLockedStyleStates() {
    ClearBoolFlag(ElementHasLockedStyleStates);
  }
  bool HasLockedStyleStates() const {
    return GetBoolFlag(ElementHasLockedStyleStates);
  }
  void SetHasWeirdParserInsertionMode() {
    SetBoolFlag(ElementHasWeirdParserInsertionMode);
  }
  bool HasWeirdParserInsertionMode() const {
    return GetBoolFlag(ElementHasWeirdParserInsertionMode);
  }
  bool HandlingClick() const { return GetBoolFlag(NodeHandlingClick); }
  void SetHandlingClick() { SetBoolFlag(NodeHandlingClick); }
  void ClearHandlingClick() { ClearBoolFlag(NodeHandlingClick); }

  void SetSubtreeRootPointer(nsINode* aSubtreeRoot) {
    NS_ASSERTION(aSubtreeRoot, "aSubtreeRoot can never be null!");
    NS_ASSERTION(!(IsContent() && IsInUncomposedDoc()) && !IsInShadowTree(),
                 "Shouldn't be here!");
    mSubtreeRoot = aSubtreeRoot;
  }

  void ClearSubtreeRootPointer() { mSubtreeRoot = nullptr; }

 public:
  // Makes nsINode object to keep aObject alive.
  void BindObject(nsISupports* aObject);
  // After calling UnbindObject nsINode object doesn't keep
  // aObject alive anymore.
  void UnbindObject(nsISupports* aObject);

  void GenerateXPath(nsAString& aResult);

  already_AddRefed<mozilla::dom::AccessibleNode> GetAccessibleNode();

  /**
   * Returns the length of this node, as specified at
   * <http://dvcs.w3.org/hg/domcore/raw-file/tip/Overview.html#concept-node-length>
   */
  uint32_t Length() const;

  void GetNodeName(mozilla::dom::DOMString& aNodeName) {
    const nsString& nodeName = NodeName();
    aNodeName.SetKnownLiveString(nodeName);
  }
  [[nodiscard]] nsresult GetBaseURI(nsAString& aBaseURI) const;
  // Return the base URI for the document.
  // The returned value may differ if the document is loaded via XHR, and
  // when accessed from chrome privileged script and
  // from content privileged script for compatibility.
  void GetBaseURIFromJS(nsAString& aBaseURI, CallerType aCallerType,
                        ErrorResult& aRv) const;
  bool HasChildNodes() const { return HasChildren(); }

  // See nsContentUtils::PositionIsBefore for aThisIndex and aOtherIndex usage.
  uint16_t CompareDocumentPosition(nsINode& aOther,
                                   int32_t* aThisIndex = nullptr,
                                   int32_t* aOtherIndex = nullptr) const;
  void GetNodeValue(nsAString& aNodeValue) { GetNodeValueInternal(aNodeValue); }
  void SetNodeValue(const nsAString& aNodeValue, mozilla::ErrorResult& aError) {
    SetNodeValueInternal(aNodeValue, aError);
  }
  virtual void GetNodeValueInternal(nsAString& aNodeValue);
  virtual void SetNodeValueInternal(const nsAString& aNodeValue,
                                    mozilla::ErrorResult& aError) {
    // The DOM spec says that when nodeValue is defined to be null "setting it
    // has no effect", so we don't throw an exception.
  }
  void EnsurePreInsertionValidity(nsINode& aNewChild, nsINode* aRefChild,
                                  mozilla::ErrorResult& aError);
  nsINode* InsertBefore(nsINode& aNode, nsINode* aChild,
                        mozilla::ErrorResult& aError) {
    return ReplaceOrInsertBefore(false, &aNode, aChild, aError);
  }

  /**
   * See <https://dom.spec.whatwg.org/#dom-node-appendchild>.
   */
  nsINode* AppendChild(nsINode& aNode, mozilla::ErrorResult& aError) {
    return InsertBefore(aNode, nullptr, aError);
  }

  nsINode* ReplaceChild(nsINode& aNode, nsINode& aChild,
                        mozilla::ErrorResult& aError) {
    return ReplaceOrInsertBefore(true, &aNode, &aChild, aError);
  }
  nsINode* RemoveChild(nsINode& aChild, mozilla::ErrorResult& aError);
  already_AddRefed<nsINode> CloneNode(bool aDeep, mozilla::ErrorResult& aError);
  bool IsSameNode(nsINode* aNode);
  bool IsEqualNode(nsINode* aNode);
  void GetNamespaceURI(nsAString& aNamespaceURI) const {
    mNodeInfo->GetNamespaceURI(aNamespaceURI);
  }
#ifdef MOZILLA_INTERNAL_API
  void GetPrefix(nsAString& aPrefix) { mNodeInfo->GetPrefix(aPrefix); }
#endif
  void GetLocalName(mozilla::dom::DOMString& aLocalName) const {
    const nsString& localName = LocalName();
    aLocalName.SetKnownLiveString(localName);
  }

  nsDOMAttributeMap* GetAttributes();

  // Helper method to remove this node from its parent. This is not exposed
  // through WebIDL.
  // Only call this if the node has a parent node.
  nsresult RemoveFromParent() {
    nsINode* parent = GetParentNode();
    mozilla::ErrorResult rv;
    parent->RemoveChild(*this, rv);
    return rv.StealNSResult();
  }

  // ChildNode methods
  inline mozilla::dom::Element* GetPreviousElementSibling() const;
  inline mozilla::dom::Element* GetNextElementSibling() const;

  MOZ_CAN_RUN_SCRIPT void Before(const Sequence<OwningNodeOrString>& aNodes,
                                 ErrorResult& aRv);
  MOZ_CAN_RUN_SCRIPT void After(const Sequence<OwningNodeOrString>& aNodes,
                                ErrorResult& aRv);
  MOZ_CAN_RUN_SCRIPT void ReplaceWith(
      const Sequence<OwningNodeOrString>& aNodes, ErrorResult& aRv);
  /**
   * Remove this node from its parent, if any.
   */
  void Remove();

  // ParentNode methods
  mozilla::dom::Element* GetFirstElementChild() const;
  mozilla::dom::Element* GetLastElementChild() const;

  already_AddRefed<nsIHTMLCollection> GetElementsByAttribute(
      const nsAString& aAttribute, const nsAString& aValue);
  already_AddRefed<nsIHTMLCollection> GetElementsByAttributeNS(
      const nsAString& aNamespaceURI, const nsAString& aAttribute,
      const nsAString& aValue, ErrorResult& aRv);

  MOZ_CAN_RUN_SCRIPT void Prepend(const Sequence<OwningNodeOrString>& aNodes,
                                  ErrorResult& aRv);
  MOZ_CAN_RUN_SCRIPT void Append(const Sequence<OwningNodeOrString>& aNodes,
                                 ErrorResult& aRv);
  MOZ_CAN_RUN_SCRIPT void ReplaceChildren(
      const Sequence<OwningNodeOrString>& aNodes, ErrorResult& aRv);

  void GetBoxQuads(const BoxQuadOptions& aOptions,
                   nsTArray<RefPtr<DOMQuad>>& aResult, CallerType aCallerType,
                   ErrorResult& aRv);

  void GetBoxQuadsFromWindowOrigin(const BoxQuadOptions& aOptions,
                                   nsTArray<RefPtr<DOMQuad>>& aResult,
                                   ErrorResult& aRv);

  already_AddRefed<DOMQuad> ConvertQuadFromNode(
      DOMQuad& aQuad, const TextOrElementOrDocument& aFrom,
      const ConvertCoordinateOptions& aOptions, CallerType aCallerType,
      ErrorResult& aRv);
  already_AddRefed<DOMQuad> ConvertRectFromNode(
      DOMRectReadOnly& aRect, const TextOrElementOrDocument& aFrom,
      const ConvertCoordinateOptions& aOptions, CallerType aCallerType,
      ErrorResult& aRv);
  already_AddRefed<DOMPoint> ConvertPointFromNode(
      const DOMPointInit& aPoint, const TextOrElementOrDocument& aFrom,
      const ConvertCoordinateOptions& aOptions, CallerType aCallerType,
      ErrorResult& aRv);

  /**
   * See nsSlots::mClosestCommonInclusiveAncestorRanges.
   */
  const mozilla::LinkedList<nsRange>*
  GetExistingClosestCommonInclusiveAncestorRanges() const {
    if (!HasSlots()) {
      return nullptr;
    }
    return GetExistingSlots()->mClosestCommonInclusiveAncestorRanges.get();
  }

  /**
   * See nsSlots::mClosestCommonInclusiveAncestorRanges.
   */
  mozilla::LinkedList<nsRange>*
  GetExistingClosestCommonInclusiveAncestorRanges() {
    if (!HasSlots()) {
      return nullptr;
    }
    return GetExistingSlots()->mClosestCommonInclusiveAncestorRanges.get();
  }

  /**
   * See nsSlots::mClosestCommonInclusiveAncestorRanges.
   */
  mozilla::UniquePtr<mozilla::LinkedList<nsRange>>&
  GetClosestCommonInclusiveAncestorRangesPtr() {
    return Slots()->mClosestCommonInclusiveAncestorRanges;
  }

  nsIWeakReference* GetExistingWeakReference() {
    return HasSlots() ? GetExistingSlots()->mWeakReference : nullptr;
  }

 protected:
  // Override this function to create a custom slots class.
  // Must not return null.
  virtual nsINode::nsSlots* CreateSlots();

  bool HasSlots() const { return mSlots != nullptr; }

  nsSlots* GetExistingSlots() const { return mSlots; }

  nsSlots* Slots() {
    if (!HasSlots()) {
      mSlots = CreateSlots();
      MOZ_ASSERT(mSlots);
    }
    return GetExistingSlots();
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
                                      mozilla::ErrorResult& aError) {}

  void EnsurePreInsertionValidity1(mozilla::ErrorResult& aError);
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
   * Parse the given selector string into a servo SelectorList.
   *
   * Never returns null if aRv is not failing.
   *
   * Note that the selector list returned here is owned by the owner doc's
   * selector cache.
   */
  const RawServoSelectorList* ParseSelectorList(
      const nsACString& aSelectorString, mozilla::ErrorResult&);

 public:
  /* Event stuff that documents and elements share.

     Note that we include DOCUMENT_ONLY_EVENT events here so that we
     can forward all the document stuff to this implementation.
  */
#define EVENT(name_, id_, type_, struct_)                         \
  mozilla::dom::EventHandlerNonNull* GetOn##name_() {             \
    return GetEventHandler(nsGkAtoms::on##name_);                 \
  }                                                               \
  void SetOn##name_(mozilla::dom::EventHandlerNonNull* handler) { \
    SetEventHandler(nsGkAtoms::on##name_, handler);               \
  }
#define TOUCH_EVENT EVENT
#define DOCUMENT_ONLY_EVENT EVENT
#include "mozilla/EventNameList.h"
#undef DOCUMENT_ONLY_EVENT
#undef TOUCH_EVENT
#undef EVENT

 protected:
  static bool Traverse(nsINode* tmp, nsCycleCollectionTraversalCallback& cb);
  static void Unlink(nsINode* tmp);

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

  // NOTE, there are 32 bits left here, at least in 64 bit builds.

  uint32_t mChildCount;

 protected:
  // mNextSibling and mFirstChild are strong references while
  // mPreviousOrLastSibling is a weak ref. |mFirstChild->mPreviousOrLastSibling|
  // points to the last child node.
  nsCOMPtr<nsIContent> mFirstChild;
  nsCOMPtr<nsIContent> mNextSibling;
  nsIContent* MOZ_NON_OWNING_REF mPreviousOrLastSibling;

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

inline nsINode* mozilla::dom::EventTarget::GetAsNode() {
  return IsNode() ? AsNode() : nullptr;
}

inline const nsINode* mozilla::dom::EventTarget::GetAsNode() const {
  return const_cast<mozilla::dom::EventTarget*>(this)->GetAsNode();
}

inline nsINode* mozilla::dom::EventTarget::AsNode() {
  MOZ_DIAGNOSTIC_ASSERT(IsNode());
  return static_cast<nsINode*>(this);
}

inline const nsINode* mozilla::dom::EventTarget::AsNode() const {
  MOZ_DIAGNOSTIC_ASSERT(IsNode());
  return static_cast<const nsINode*>(this);
}

// Useful inline function for getting a node given an nsIContent and a Document.
// Returns the first argument cast to nsINode if it is non-null, otherwise
// returns the second (which may be null).  We use type variables instead of
// nsIContent* and Document* because the actual types must be
// known for the cast to work.
template <class C, class D>
inline nsINode* NODE_FROM(C& aContent, D& aDocument) {
  if (aContent) return static_cast<nsINode*>(aContent);
  return static_cast<nsINode*>(aDocument);
}

NS_DEFINE_STATIC_IID_ACCESSOR(nsINode, NS_INODE_IID)

inline nsISupports* ToSupports(nsINode* aPointer) { return aPointer; }

// Some checks are faster to do on nsIContent or Element than on
// nsINode, so spit out FromNode versions taking those types too.
#define NS_IMPL_FROMNODE_GENERIC(_class, _check, _const)                 \
  template <typename T>                                                  \
  static auto FromNode(_const T& aNode)                                  \
      ->decltype(static_cast<_const _class*>(&aNode)) {                  \
    return aNode._check ? static_cast<_const _class*>(&aNode) : nullptr; \
  }                                                                      \
  template <typename T>                                                  \
  static _const _class* FromNode(_const T* aNode) {                      \
    return FromNode(*aNode);                                             \
  }                                                                      \
  template <typename T>                                                  \
  static _const _class* FromNodeOrNull(_const T* aNode) {                \
    return aNode ? FromNode(*aNode) : nullptr;                           \
  }                                                                      \
  template <typename T>                                                  \
  static auto FromEventTarget(_const T& aEventTarget)                    \
      ->decltype(static_cast<_const _class*>(&aEventTarget)) {           \
    return aEventTarget.IsNode() && aEventTarget.AsNode()->_check        \
               ? static_cast<_const _class*>(&aEventTarget)              \
               : nullptr;                                                \
  }                                                                      \
  template <typename T>                                                  \
  static _const _class* FromEventTarget(_const T* aEventTarget) {        \
    return FromEventTarget(*aEventTarget);                               \
  }                                                                      \
  template <typename T>                                                  \
  static _const _class* FromEventTargetOrNull(_const T* aEventTarget) {  \
    return aEventTarget ? FromEventTarget(*aEventTarget) : nullptr;      \
  }

#define NS_IMPL_FROMNODE_HELPER(_class, _check)                                \
  NS_IMPL_FROMNODE_GENERIC(_class, _check, )                                   \
  NS_IMPL_FROMNODE_GENERIC(_class, _check, const)                              \
                                                                               \
  template <typename T>                                                        \
  static _class* FromNode(T&& aNode) {                                         \
    /* We need the double-cast in case aNode is a smartptr.  Those */          \
    /* can cast to superclasses of the type they're templated on, */           \
    /* but not directly to subclasses.  */                                     \
    return aNode->_check ? static_cast<_class*>(static_cast<nsINode*>(aNode))  \
                         : nullptr;                                            \
  }                                                                            \
  template <typename T>                                                        \
  static _class* FromNodeOrNull(T&& aNode) {                                   \
    return aNode ? FromNode(aNode) : nullptr;                                  \
  }                                                                            \
  template <typename T>                                                        \
  static _class* FromEventTarget(T&& aEventTarget) {                           \
    /* We need the double-cast in case aEventTarget is a smartptr.  Those */   \
    /* can cast to superclasses of the type they're templated on, */           \
    /* but not directly to subclasses.  */                                     \
    return aEventTarget->IsNode() && aEventTarget->AsNode()->_check            \
               ? static_cast<_class*>(static_cast<EventTarget*>(aEventTarget)) \
               : nullptr;                                                      \
  }                                                                            \
  template <typename T>                                                        \
  static _class* FromEventTargetOrNull(T&& aEventTarget) {                     \
    return aEventTarget ? FromEventTarget(aEventTarget) : nullptr;             \
  }

#define NS_IMPL_FROMNODE(_class, _nsid) \
  NS_IMPL_FROMNODE_HELPER(_class, IsInNamespace(_nsid))

#define NS_IMPL_FROMNODE_WITH_TAG(_class, _nsid, _tag) \
  NS_IMPL_FROMNODE_HELPER(_class, NodeInfo()->Equals(nsGkAtoms::_tag, _nsid))

#define NS_IMPL_FROMNODE_HTML_WITH_TAG(_class, _tag) \
  NS_IMPL_FROMNODE_WITH_TAG(_class, kNameSpaceID_XHTML, _tag)

#endif /* nsINode_h___ */
