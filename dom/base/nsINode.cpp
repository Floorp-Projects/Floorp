/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Base class for all DOM nodes.
 */

#include "nsINode.h"

#include "AccessCheck.h"
#include "jsapi.h"
#include "js/ForOfIterator.h"  // JS::ForOfIterator
#include "js/JSON.h"           // JS_ParseJSON
#include "mozAutoDocUpdate.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/CORSMode.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/HTMLEditor.h"
#include "mozilla/InternalMutationEvent.h"
#include "mozilla/Likely.h"
#include "mozilla/Maybe.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/PresShell.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TextControlElement.h"
#include "mozilla/TextEditor.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/dom/BindContext.h"
#include "mozilla/dom/CharacterData.h"
#include "mozilla/dom/ChildIterator.h"
#include "mozilla/dom/CustomElementRegistry.h"
#include "mozilla/dom/DebuggerNotificationBinding.h"
#include "mozilla/dom/DocumentType.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ElementBinding.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/Exceptions.h"
#include "mozilla/dom/Link.h"
#include "mozilla/dom/HTMLImageElement.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/dom/HTMLTemplateElement.h"
#include "mozilla/dom/MutationObservers.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/ShadowRoot.h"
#include "mozilla/dom/SVGUseElement.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/L10nOverlays.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/StaticPrefs_layout.h"
#include "nsAttrValueOrString.h"
#include "nsCCUncollectableMarker.h"
#include "nsContentCreatorFunctions.h"
#include "nsContentList.h"
#include "nsContentUtils.h"
#include "nsCOMArray.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/dom/Attr.h"
#include "nsDOMAttributeMap.h"
#include "nsDOMCID.h"
#include "nsDOMCSSAttrDeclaration.h"
#include "nsError.h"
#include "nsExpirationTracker.h"
#include "nsDOMMutationObserver.h"
#include "nsDOMString.h"
#include "nsDOMTokenList.h"
#include "nsFocusManager.h"
#include "nsFrameSelection.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsIAnonymousContentCreator.h"
#include "nsAtom.h"
#include "nsIContentInlines.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/DocumentInlines.h"
#include "nsIFrameInlines.h"
#include "mozilla/dom/NodeInfo.h"
#include "mozilla/dom/NodeInfoInlines.h"
#include "nsIScriptGlobalObject.h"
#include "nsView.h"
#include "nsViewManager.h"
#include "nsIWidget.h"
#include "nsLayoutUtils.h"
#include "nsNameSpaceManager.h"
#include "nsNodeInfoManager.h"
#include "nsObjectLoadingContent.h"
#include "nsPIDOMWindow.h"
#include "nsPresContext.h"
#include "nsPrintfCString.h"
#include "nsRange.h"
#include "nsString.h"
#include "nsStyleConsts.h"
#include "nsTextNode.h"
#include "nsUnicharUtils.h"
#include "nsWindowSizes.h"
#include "mozilla/Preferences.h"
#include "xpcpublic.h"
#include "HTMLLegendElement.h"
#include "nsWrapperCacheInlines.h"
#include "WrapperFactory.h"
#include <algorithm>
#include "nsGlobalWindowInner.h"
#include "GeometryUtils.h"
#include "nsIAnimationObserver.h"
#include "nsChildContentList.h"
#include "mozilla/dom/NodeBinding.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/AncestorIterator.h"
#include "xpcprivate.h"

#include "XPathGenerator.h"

#ifdef ACCESSIBILITY
#  include "mozilla/dom/AccessibleNode.h"
#endif

using namespace mozilla;
using namespace mozilla::dom;

static bool ShouldUseNACScope(const nsINode* aNode) {
  return aNode->IsInNativeAnonymousSubtree();
}

static bool ShouldUseUAWidgetScope(const nsINode* aNode) {
  return aNode->HasBeenInUAWidget();
}

void* nsINode::operator new(size_t aSize, nsNodeInfoManager* aManager) {
  if (StaticPrefs::dom_arena_allocator_enabled_AtStartup()) {
    MOZ_ASSERT(aManager, "nsNodeInfoManager needs to be initialized");
    return aManager->Allocate(aSize);
  }
  return ::operator new(aSize);
}
void nsINode::operator delete(void* aPtr) { free_impl(aPtr); }

bool nsINode::IsInclusiveDescendantOf(const nsINode* aNode) const {
  MOZ_ASSERT(aNode, "The node is nullptr.");

  if (aNode == this) {
    return true;
  }

  if (!aNode->HasFlag(NODE_MAY_HAVE_ELEMENT_CHILDREN)) {
    return GetParentNode() == aNode;
  }

  for (nsINode* node : Ancestors(*this)) {
    if (node == aNode) {
      return true;
    }
  }
  return false;
}

bool nsINode::IsInclusiveFlatTreeDescendantOf(const nsINode* aNode) const {
  MOZ_ASSERT(aNode, "The node is nullptr.");

  for (nsINode* node : InclusiveFlatTreeAncestors(*this)) {
    if (node == aNode) {
      return true;
    }
  }
  return false;
}

bool nsINode::IsShadowIncludingInclusiveDescendantOf(
    const nsINode* aNode) const {
  MOZ_ASSERT(aNode, "The node is nullptr.");

  if (this->GetComposedDoc() == aNode) {
    return true;
  }

  const nsINode* node = this;
  do {
    if (node == aNode) {
      return true;
    }

    node = node->GetParentOrShadowHostNode();
  } while (node);

  return false;
}

nsINode::nsSlots::nsSlots() : mWeakReference(nullptr) {}

nsINode::nsSlots::~nsSlots() {
  if (mChildNodes) {
    mChildNodes->InvalidateCacheIfAvailable();
  }

  if (mWeakReference) {
    mWeakReference->NoticeNodeDestruction();
  }
}

void nsINode::nsSlots::Traverse(nsCycleCollectionTraversalCallback& cb) {
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mSlots->mChildNodes");
  cb.NoteXPCOMChild(mChildNodes);
}

void nsINode::nsSlots::Unlink(nsINode&) {
  if (mChildNodes) {
    mChildNodes->InvalidateCacheIfAvailable();
    ImplCycleCollectionUnlink(mChildNodes);
  }
}

//----------------------------------------------------------------------

#ifdef MOZILLA_INTERNAL_API
nsINode::nsINode(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : mNodeInfo(std::move(aNodeInfo)),
      mParent(nullptr)
#  ifndef BOOL_FLAGS_ON_WRAPPER_CACHE
      ,
      mBoolFlags(0)
#  endif
      ,
      mChildCount(0),
      mPreviousOrLastSibling(nullptr),
      mSubtreeRoot(this),
      mSlots(nullptr) {
  SetIsOnMainThread();
}
#endif

nsINode::~nsINode() {
  MOZ_ASSERT(!HasSlots(), "LastRelease was not called?");
  MOZ_ASSERT(mSubtreeRoot == this, "Didn't restore state properly?");
}

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
void nsINode::AssertInvariantsOnNodeInfoChange() {
  MOZ_DIAGNOSTIC_ASSERT(!IsInComposedDoc());
  if (nsCOMPtr<Link> link = do_QueryInterface(this)) {
    MOZ_DIAGNOSTIC_ASSERT(!link->HasPendingLinkUpdate());
  }
}
#endif

void* nsINode::GetProperty(const nsAtom* aPropertyName,
                           nsresult* aStatus) const {
  if (!HasProperties()) {  // a fast HasFlag() test
    if (aStatus) {
      *aStatus = NS_PROPTABLE_PROP_NOT_THERE;
    }
    return nullptr;
  }
  return OwnerDoc()->PropertyTable().GetProperty(this, aPropertyName, aStatus);
}

nsresult nsINode::SetProperty(nsAtom* aPropertyName, void* aValue,
                              NSPropertyDtorFunc aDtor, bool aTransfer) {
  nsresult rv = OwnerDoc()->PropertyTable().SetProperty(
      this, aPropertyName, aValue, aDtor, nullptr, aTransfer);
  if (NS_SUCCEEDED(rv)) {
    SetFlags(NODE_HAS_PROPERTIES);
  }

  return rv;
}

void nsINode::RemoveProperty(const nsAtom* aPropertyName) {
  OwnerDoc()->PropertyTable().RemoveProperty(this, aPropertyName);
}

void* nsINode::TakeProperty(const nsAtom* aPropertyName, nsresult* aStatus) {
  return OwnerDoc()->PropertyTable().TakeProperty(this, aPropertyName, aStatus);
}

nsIContentSecurityPolicy* nsINode::GetCsp() const {
  return OwnerDoc()->GetCsp();
}

nsINode::nsSlots* nsINode::CreateSlots() { return new nsSlots(); }

static const nsINode* GetClosestCommonInclusiveAncestorForRangeInSelection(
    const nsINode* aNode) {
  while (aNode &&
         !aNode->IsClosestCommonInclusiveAncestorForRangeInSelection()) {
    const bool isNodeInShadowTree =
        StaticPrefs::dom_shadowdom_selection_across_boundary_enabled() &&
        aNode->IsInShadowTree();
    if (!aNode
             ->IsDescendantOfClosestCommonInclusiveAncestorForRangeInSelection() &&
        !isNodeInShadowTree) {
      return nullptr;
    }
    aNode = StaticPrefs::dom_shadowdom_selection_across_boundary_enabled()
                ? aNode->GetParentOrShadowHostNode()
                : aNode->GetParentNode();
  }
  return aNode;
}

/**
 * A Comparator suitable for mozilla::BinarySearchIf for searching a collection
 * of nsRange* for an overlap of (mNode, mStartOffset) .. (mNode, mEndOffset).
 */
class IsItemInRangeComparator {
 public:
  // @param aStartOffset has to be less or equal to aEndOffset.
  IsItemInRangeComparator(const nsINode& aNode, const uint32_t aStartOffset,
                          const uint32_t aEndOffset,
                          nsContentUtils::NodeIndexCache* aCache)
      : mNode(aNode),
        mStartOffset(aStartOffset),
        mEndOffset(aEndOffset),
        mCache(aCache) {
    MOZ_ASSERT(aStartOffset <= aEndOffset);
  }

  int operator()(const AbstractRange* const aRange) const {
    int32_t cmp = nsContentUtils::ComparePoints_Deprecated(
        &mNode, mEndOffset, aRange->GetMayCrossShadowBoundaryStartContainer(),
        aRange->MayCrossShadowBoundaryStartOffset(), nullptr, mCache);
    if (cmp == 1) {
      cmp = nsContentUtils::ComparePoints_Deprecated(
          &mNode, mStartOffset, aRange->GetMayCrossShadowBoundaryEndContainer(),
          aRange->MayCrossShadowBoundaryEndOffset(), nullptr, mCache);
      if (cmp == -1) {
        return 0;
      }
      return 1;
    }
    return -1;
  }

 private:
  const nsINode& mNode;
  const uint32_t mStartOffset;
  const uint32_t mEndOffset;
  nsContentUtils::NodeIndexCache* mCache;
};

bool nsINode::IsSelected(const uint32_t aStartOffset,
                         const uint32_t aEndOffset) const {
  MOZ_ASSERT(aStartOffset <= aEndOffset);

  const nsINode* n = GetClosestCommonInclusiveAncestorForRangeInSelection(this);
  NS_ASSERTION(n || !IsMaybeSelected(),
               "A node without a common inclusive ancestor for a range in "
               "Selection is for sure not selected.");

  // Collect the selection objects for potential ranges.
  nsTHashSet<Selection*> ancestorSelections;
  for (; n; n = GetClosestCommonInclusiveAncestorForRangeInSelection(
                n->GetParentNode())) {
    const LinkedList<AbstractRange>* ranges =
        n->GetExistingClosestCommonInclusiveAncestorRanges();
    if (!ranges) {
      continue;
    }
    for (const AbstractRange* range : *ranges) {
      MOZ_ASSERT(range->IsInAnySelection(),
                 "Why is this range registered with a node?");
      // Looks like that IsInSelection() assert fails sometimes...
      if (range->IsInAnySelection()) {
        for (const WeakPtr<Selection>& selection : range->GetSelections()) {
          if (selection) {
            ancestorSelections.Insert(selection);
          }
        }
      }
    }
  }

  nsContentUtils::NodeIndexCache cache;
  IsItemInRangeComparator comparator{*this, aStartOffset, aEndOffset, &cache};
  for (Selection* selection : ancestorSelections) {
    // Binary search the sorted ranges in this selection.
    // (Selection::GetRangeAt returns its ranges ordered).
    size_t low = 0;
    size_t high = selection->RangeCount();

    while (high != low) {
      size_t middle = low + (high - low) / 2;

      const AbstractRange* const range = selection->GetAbstractRangeAt(middle);
      int result = comparator(range);
      if (result == 0) {
        if (!range->Collapsed()) {
          return true;
        }

        if (range->MayCrossShadowBoundary()) {
          MOZ_ASSERT(range->IsDynamicRange(),
                     "range->MayCrossShadowBoundary() can only return true for "
                     "dynamic range");
          StaticRange* crossBoundaryRange =
              range->AsDynamicRange()->GetCrossShadowBoundaryRange();
          MOZ_ASSERT(crossBoundaryRange);
          if (!crossBoundaryRange->Collapsed()) {
            return true;
          }
        }

        const AbstractRange* middlePlus1;
        const AbstractRange* middleMinus1;
        // if node end > start of middle+1, result = 1
        if (middle + 1 < high &&
            (middlePlus1 = selection->GetAbstractRangeAt(middle + 1)) &&
            nsContentUtils::ComparePoints_Deprecated(
                this, aEndOffset, middlePlus1->GetStartContainer(),
                middlePlus1->StartOffset(), nullptr, &cache) > 0) {
          result = 1;
          // if node start < end of middle - 1, result = -1
        } else if (middle >= 1 &&
                   (middleMinus1 = selection->GetAbstractRangeAt(middle - 1)) &&
                   nsContentUtils::ComparePoints_Deprecated(
                       this, aStartOffset, middleMinus1->GetEndContainer(),
                       middleMinus1->EndOffset(), nullptr, &cache) < 0) {
          result = -1;
        } else {
          break;
        }
      }

      if (result < 0) {
        high = middle;
      } else {
        low = middle + 1;
      }
    }
  }

  return false;
}

Element* nsINode::GetAnonymousRootElementOfTextEditor(
    TextEditor** aTextEditor) {
  if (aTextEditor) {
    *aTextEditor = nullptr;
  }
  RefPtr<TextControlElement> textControlElement;
  if (IsInNativeAnonymousSubtree()) {
    textControlElement = TextControlElement::FromNodeOrNull(
        GetClosestNativeAnonymousSubtreeRootParentOrHost());
  } else {
    textControlElement = TextControlElement::FromNode(this);
  }
  if (!textControlElement) {
    return nullptr;
  }
  RefPtr<TextEditor> textEditor = textControlElement->GetTextEditor();
  if (!textEditor) {
    // The found `TextControlElement` may be an input element which is not a
    // text control element.  In this case, such element must not be in a
    // native anonymous tree of a `TextEditor` so this node is not in any
    // `TextEditor`.
    return nullptr;
  }

  Element* rootElement = textEditor->GetRoot();
  if (aTextEditor) {
    textEditor.forget(aTextEditor);
  }
  return rootElement;
}

void nsINode::QueueDevtoolsAnonymousEvent(bool aIsRemove) {
  MOZ_ASSERT(IsRootOfNativeAnonymousSubtree());
  MOZ_ASSERT(OwnerDoc()->DevToolsAnonymousAndShadowEventsEnabled());
  AsyncEventDispatcher* dispatcher = new AsyncEventDispatcher(
      this, aIsRemove ? u"anonymousrootremoved"_ns : u"anonymousrootcreated"_ns,
      CanBubble::eYes, ChromeOnlyDispatch::eYes, Composed::eYes);
  dispatcher->PostDOMEvent();
}

nsINode* nsINode::GetRootNode(const GetRootNodeOptions& aOptions) {
  if (aOptions.mComposed) {
    if (Document* doc = GetComposedDoc()) {
      return doc;
    }

    nsINode* node = this;
    while (node) {
      node = node->SubtreeRoot();
      ShadowRoot* shadow = ShadowRoot::FromNode(node);
      if (!shadow) {
        break;
      }
      node = shadow->GetHost();
    }

    return node;
  }

  return SubtreeRoot();
}

nsIContent* nsINode::GetFirstChildOfTemplateOrNode() {
  if (IsTemplateElement()) {
    DocumentFragment* frag = static_cast<HTMLTemplateElement*>(this)->Content();
    return frag->GetFirstChild();
  }

  return GetFirstChild();
}

nsINode* nsINode::SubtreeRoot() const {
  auto RootOfNode = [](const nsINode* aStart) -> nsINode* {
    const nsINode* node = aStart;
    const nsINode* iter = node;
    while ((iter = iter->GetParentNode())) {
      node = iter;
    }
    return const_cast<nsINode*>(node);
  };

  // There are four cases of interest here.  nsINodes that are really:
  // 1. Document nodes - Are always in the document.
  // 2.a nsIContent nodes not in a shadow tree - Are either in the document,
  //     or mSubtreeRoot is updated in BindToTree/UnbindFromTree.
  // 2.b nsIContent nodes in a shadow tree - Are never in the document,
  //     ignore mSubtreeRoot and return the containing shadow root.
  // 4. Attr nodes - Are never in the document, and mSubtreeRoot
  //    is always 'this' (as set in nsINode's ctor).
  nsINode* node;
  if (IsInUncomposedDoc()) {
    node = OwnerDocAsNode();
  } else if (IsContent()) {
    ShadowRoot* containingShadow = AsContent()->GetContainingShadow();
    node = containingShadow ? containingShadow : mSubtreeRoot;
    if (!node) {
      NS_WARNING("Using SubtreeRoot() on unlinked element?");
      node = RootOfNode(this);
    }
  } else {
    node = mSubtreeRoot;
  }
  MOZ_ASSERT(node, "Should always have a node here!");
#ifdef DEBUG
  {
    const nsINode* slowNode = RootOfNode(this);
    MOZ_ASSERT(slowNode == node, "These should always be in sync!");
  }
#endif
  return node;
}

static nsIContent* GetRootForContentSubtree(nsIContent* aContent) {
  NS_ENSURE_TRUE(aContent, nullptr);

  // Special case for ShadowRoot because the ShadowRoot itself is
  // the root. This is necessary to prevent selection from crossing
  // the ShadowRoot boundary.
  //
  // FIXME(emilio): The NAC check should probably be done before this? We can
  // have NAC inside shadow DOM.
  if (ShadowRoot* containingShadow = aContent->GetContainingShadow()) {
    return containingShadow;
  }
  if (nsIContent* nativeAnonRoot =
          aContent->GetClosestNativeAnonymousSubtreeRoot()) {
    return nativeAnonRoot;
  }
  if (Document* doc = aContent->GetUncomposedDoc()) {
    return doc->GetRootElement();
  }
  return nsIContent::FromNode(aContent->SubtreeRoot());
}

nsIContent* nsINode::GetSelectionRootContent(PresShell* aPresShell,
                                             bool aAllowCrossShadowBoundary) {
  NS_ENSURE_TRUE(aPresShell, nullptr);

  if (IsDocument()) return AsDocument()->GetRootElement();
  if (!IsContent()) return nullptr;

  if (GetComposedDoc() != aPresShell->GetDocument()) {
    return nullptr;
  }

  if (AsContent()->HasIndependentSelection() || IsInNativeAnonymousSubtree()) {
    // This node should be an inclusive descendant of input/textarea editor.
    // In that case, the anonymous <div> for TextEditor should be always the
    // selection root.
    // FIXME: If Selection for the document is collapsed in <input> or
    // <textarea>, returning anonymous <div> may make the callers confused.
    // Perhaps, we should do this only when this is in the native anonymous
    // subtree unless the callers explicitly want to retrieve the anonymous
    // <div> from a text control element.
    if (Element* anonymousDivElement = GetAnonymousRootElementOfTextEditor()) {
      return anonymousDivElement;
    }
  }

  nsPresContext* presContext = aPresShell->GetPresContext();
  if (presContext) {
    HTMLEditor* htmlEditor = nsContentUtils::GetHTMLEditor(presContext);
    if (htmlEditor) {
      // This node is in HTML editor.
      if (!IsInComposedDoc() || IsInDesignMode() ||
          !HasFlag(NODE_IS_EDITABLE)) {
        nsIContent* editorRoot = htmlEditor->GetRoot();
        NS_ENSURE_TRUE(editorRoot, nullptr);
        return nsContentUtils::IsInSameAnonymousTree(this, editorRoot)
                   ? editorRoot
                   : GetRootForContentSubtree(AsContent());
      }
      // If the document isn't editable but this is editable, this is in
      // contenteditable.  Use the editing host element for selection root.
      return static_cast<nsIContent*>(this)->GetEditingHost();
    }
  }

  RefPtr<nsFrameSelection> fs = aPresShell->FrameSelection();
  nsCOMPtr<nsIContent> content = fs->GetLimiter();
  if (!content) {
    content = fs->GetAncestorLimiter();
    if (!content) {
      Document* doc = aPresShell->GetDocument();
      NS_ENSURE_TRUE(doc, nullptr);
      content = doc->GetRootElement();
      if (!content) return nullptr;
    }
  }

  // This node might be in another subtree, if so, we should find this subtree's
  // root.  Otherwise, we can return the content simply.
  NS_ENSURE_TRUE(content, nullptr);
  if (!nsContentUtils::IsInSameAnonymousTree(this, content)) {
    content = GetRootForContentSubtree(AsContent());
    // Fixup for ShadowRoot because the ShadowRoot itself does not have a frame.
    // Use the host as the root.
    if (ShadowRoot* shadowRoot = ShadowRoot::FromNode(content)) {
      content = shadowRoot->GetHost();
      if (content && aAllowCrossShadowBoundary) {
        content = content->GetSelectionRootContent(aPresShell,
                                                   aAllowCrossShadowBoundary);
      }
    }
  }

  return content;
}

nsINodeList* nsINode::ChildNodes() {
  nsSlots* slots = Slots();
  if (!slots->mChildNodes) {
    slots->mChildNodes = IsAttr() ? new nsAttrChildContentList(this)
                                  : new nsParentNodeChildContentList(this);
  }

  return slots->mChildNodes;
}

nsIContent* nsINode::GetLastChild() const {
  return mFirstChild ? mFirstChild->mPreviousOrLastSibling : nullptr;
}

void nsINode::InvalidateChildNodes() {
  MOZ_ASSERT(!IsAttr());

  nsSlots* slots = GetExistingSlots();
  if (!slots || !slots->mChildNodes) {
    return;
  }

  auto childNodes =
      static_cast<nsParentNodeChildContentList*>(slots->mChildNodes.get());
  childNodes->InvalidateCache();
}

void nsINode::GetTextContentInternal(nsAString& aTextContent,
                                     OOMReporter& aError) {
  SetDOMStringToNull(aTextContent);
}

DocumentOrShadowRoot* nsINode::GetContainingDocumentOrShadowRoot() const {
  if (IsInUncomposedDoc()) {
    return OwnerDoc();
  }

  if (IsInShadowTree()) {
    return AsContent()->GetContainingShadow();
  }

  return nullptr;
}

DocumentOrShadowRoot* nsINode::GetUncomposedDocOrConnectedShadowRoot() const {
  if (IsInUncomposedDoc()) {
    return OwnerDoc();
  }

  if (IsInComposedDoc() && IsInShadowTree()) {
    return AsContent()->GetContainingShadow();
  }

  return nullptr;
}

mozilla::SafeDoublyLinkedList<nsIMutationObserver>*
nsINode::GetMutationObservers() {
  return HasSlots() ? &GetExistingSlots()->mMutationObservers : nullptr;
}

void nsINode::LastRelease() {
  nsINode::nsSlots* slots = GetExistingSlots();
  if (slots) {
    if (!slots->mMutationObservers.isEmpty()) {
      for (auto iter = slots->mMutationObservers.begin();
           iter != slots->mMutationObservers.end(); ++iter) {
        iter->NodeWillBeDestroyed(this);
      }
    }

    if (IsContent()) {
      nsIContent* content = AsContent();
      if (HTMLSlotElement* slot = content->GetManualSlotAssignment()) {
        content->SetManualSlotAssignment(nullptr);
        slot->RemoveManuallyAssignedNode(*content);
      }
    }

    if (Element* element = Element::FromNode(this)) {
      if (CustomElementData* data = element->GetCustomElementData()) {
        data->Unlink();
      }
    }

    delete slots;
    mSlots = nullptr;
  }

  // Kill properties first since that may run external code, so we want to
  // be in as complete state as possible at that time.
  if (IsDocument()) {
    // Delete all properties before tearing down the document. Some of the
    // properties are bound to nsINode objects and the destructor functions of
    // the properties may want to use the owner document of the nsINode.
    AsDocument()->RemoveAllProperties();

    AsDocument()->DropStyleSet();
  } else {
    if (HasProperties()) {
      // Strong reference to the document so that deleting properties can't
      // delete the document.
      nsCOMPtr<Document> document = OwnerDoc();
      document->RemoveAllPropertiesFor(this);
    }

    if (HasFlag(ADDED_TO_FORM)) {
      if (nsGenericHTMLFormControlElement* formControl =
              nsGenericHTMLFormControlElement::FromNode(this)) {
        // Tell the form (if any) this node is going away.  Don't
        // notify, since we're being destroyed in any case.
        formControl->ClearForm(true, true);
      } else if (HTMLImageElement* imageElem =
                     HTMLImageElement::FromNode(this)) {
        imageElem->ClearForm(true);
      }
    }
  }
  UnsetFlags(NODE_HAS_PROPERTIES);

  if (NodeType() != nsINode::DOCUMENT_NODE &&
      HasFlag(NODE_HAS_LISTENERMANAGER)) {
#ifdef DEBUG
    if (nsContentUtils::IsInitialized()) {
      EventListenerManager* manager =
          nsContentUtils::GetExistingListenerManagerForNode(this);
      if (!manager) {
        NS_ERROR(
            "Huh, our bit says we have a listener manager list, "
            "but there's nothing in the hash!?!!");
      }
    }
#endif

    nsContentUtils::RemoveListenerManager(this);
    UnsetFlags(NODE_HAS_LISTENERMANAGER);
  }

  ReleaseWrapper(this);

  FragmentOrElement::RemoveBlackMarkedNode(this);
}

std::ostream& operator<<(std::ostream& aStream, const nsINode& aNode) {
  nsAutoString elemDesc;
  const nsINode* curr = &aNode;
  while (curr) {
    nsString id;
    if (curr->IsElement()) {
      curr->AsElement()->GetId(id);
    }

    if (!elemDesc.IsEmpty()) {
      elemDesc = elemDesc + u"."_ns;
    }

    if (!curr->LocalName().IsEmpty()) {
      elemDesc.Append(curr->LocalName());
    } else {
      elemDesc.Append(curr->NodeName());
    }

    if (!id.IsEmpty()) {
      elemDesc = elemDesc + u"['"_ns + id + u"']"_ns;
    }

    if (curr->IsElement() &&
        curr->AsElement()->HasAttr(nsGkAtoms::contenteditable)) {
      nsAutoString val;
      curr->AsElement()->GetAttr(nsGkAtoms::contenteditable, val);
      elemDesc = elemDesc + u"[contenteditable=\""_ns + val + u"\"]"_ns;
    }
    if (curr->IsDocument() && curr->IsInDesignMode()) {
      elemDesc.Append(u"[designMode=\"on\"]"_ns);
    }

    curr = curr->GetParentNode();
  }

  NS_ConvertUTF16toUTF8 str(elemDesc);
  return aStream << str.get();
}

nsIContent* nsINode::DoGetShadowHost() const {
  MOZ_ASSERT(IsShadowRoot());
  return static_cast<const ShadowRoot*>(this)->GetHost();
}

ShadowRoot* nsINode::GetContainingShadow() const {
  if (!IsInShadowTree()) {
    return nullptr;
  }
  return AsContent()->GetContainingShadow();
}

Element* nsINode::GetContainingShadowHost() const {
  if (ShadowRoot* shadow = GetContainingShadow()) {
    return shadow->GetHost();
  }
  return nullptr;
}

SVGUseElement* nsINode::DoGetContainingSVGUseShadowHost() const {
  MOZ_ASSERT(IsInShadowTree());
  return SVGUseElement::FromNodeOrNull(GetContainingShadowHost());
}

void nsINode::GetNodeValueInternal(nsAString& aNodeValue) {
  SetDOMStringToNull(aNodeValue);
}

static const char* NodeTypeAsString(nsINode* aNode) {
  static const char* NodeTypeStrings[] = {
      "",  // No nodes of type 0
      "an Element",
      "an Attribute",
      "a Text",
      "a CDATASection",
      "an EntityReference",
      "an Entity",
      "a ProcessingInstruction",
      "a Comment",
      "a Document",
      "a DocumentType",
      "a DocumentFragment",
      "a Notation",
  };
  static_assert(ArrayLength(NodeTypeStrings) == nsINode::MAX_NODE_TYPE + 1,
                "Max node type out of range for our array");

  uint16_t nodeType = aNode->NodeType();
  MOZ_RELEASE_ASSERT(nodeType < ArrayLength(NodeTypeStrings),
                     "Uknown out-of-range node type");
  return NodeTypeStrings[nodeType];
}

nsINode* nsINode::RemoveChild(nsINode& aOldChild, ErrorResult& aError) {
  if (!aOldChild.IsContent()) {
    // aOldChild can't be one of our children.
    aError.ThrowNotFoundError(
        "The node to be removed is not a child of this node");
    return nullptr;
  }

  if (aOldChild.GetParentNode() == this) {
    nsContentUtils::MaybeFireNodeRemoved(&aOldChild, this);
  }

  // Check again, we may not be the child's parent anymore.
  // Can be triggered by dom/base/crashtests/293388-1.html
  if (aOldChild.IsRootOfNativeAnonymousSubtree() ||
      aOldChild.GetParentNode() != this) {
    // aOldChild isn't one of our children.
    aError.ThrowNotFoundError(
        "The node to be removed is not a child of this node");
    return nullptr;
  }

  RemoveChildNode(aOldChild.AsContent(), true);
  return &aOldChild;
}

void nsINode::Normalize() {
  // First collect list of nodes to be removed
  AutoTArray<nsCOMPtr<nsIContent>, 50> nodes;

  bool canMerge = false;
  for (nsIContent* node = this->GetFirstChild(); node;
       node = node->GetNextNode(this)) {
    if (node->NodeType() != TEXT_NODE) {
      canMerge = false;
      continue;
    }

    if (canMerge || node->TextLength() == 0) {
      // No need to touch canMerge. That way we can merge across empty
      // textnodes if and only if the node before is a textnode
      nodes.AppendElement(node);
    } else {
      canMerge = true;
    }

    // If there's no following sibling, then we need to ensure that we don't
    // collect following siblings of our (grand)parent as to-be-removed
    canMerge = canMerge && !!node->GetNextSibling();
  }

  if (nodes.IsEmpty()) {
    return;
  }

  // We're relying on mozAutoSubtreeModified to keep the doc alive here.
  RefPtr<Document> doc = OwnerDoc();

  // Batch possible DOMSubtreeModified events.
  mozAutoSubtreeModified subtree(doc, nullptr);

  // Fire all DOMNodeRemoved events. Optimize the common case of there being
  // no listeners
  bool hasRemoveListeners = nsContentUtils::HasMutationListeners(
      doc, NS_EVENT_BITS_MUTATION_NODEREMOVED);
  if (hasRemoveListeners) {
    for (nsCOMPtr<nsIContent>& node : nodes) {
      // Node may have already been removed.
      if (nsCOMPtr<nsINode> parentNode = node->GetParentNode()) {
        // TODO: Bug 1622253
        nsContentUtils::MaybeFireNodeRemoved(MOZ_KnownLive(node), parentNode);
      }
    }
  }

  mozAutoDocUpdate batch(doc, true);

  // Merge and remove all nodes
  nsAutoString tmpStr;
  for (uint32_t i = 0; i < nodes.Length(); ++i) {
    nsIContent* node = nodes[i];
    // Merge with previous node unless empty
    const nsTextFragment* text = node->GetText();
    if (text->GetLength()) {
      nsIContent* target = node->GetPreviousSibling();
      NS_ASSERTION(
          (target && target->NodeType() == TEXT_NODE) || hasRemoveListeners,
          "Should always have a previous text sibling unless "
          "mutation events messed us up");
      if (!hasRemoveListeners || (target && target->NodeType() == TEXT_NODE)) {
        nsTextNode* t = static_cast<nsTextNode*>(target);
        if (text->Is2b()) {
          t->AppendTextForNormalize(text->Get2b(), text->GetLength(), true,
                                    node);
        } else {
          tmpStr.Truncate();
          text->AppendTo(tmpStr);
          t->AppendTextForNormalize(tmpStr.get(), tmpStr.Length(), true, node);
        }
      }
    }

    // Remove node
    nsCOMPtr<nsINode> parent = node->GetParentNode();
    NS_ASSERTION(parent || hasRemoveListeners,
                 "Should always have a parent unless "
                 "mutation events messed us up");
    if (parent) {
      parent->RemoveChildNode(node, true);
    }
  }
}

nsresult nsINode::GetBaseURI(nsAString& aURI) const {
  nsIURI* baseURI = GetBaseURI();

  nsAutoCString spec;
  if (baseURI) {
    nsresult rv = baseURI->GetSpec(spec);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  CopyUTF8toUTF16(spec, aURI);
  return NS_OK;
}

void nsINode::GetBaseURIFromJS(nsAString& aURI, CallerType aCallerType,
                               ErrorResult& aRv) const {
  nsIURI* baseURI = GetBaseURI(aCallerType == CallerType::System);
  nsAutoCString spec;
  if (baseURI) {
    nsresult res = baseURI->GetSpec(spec);
    if (NS_FAILED(res)) {
      aRv.Throw(res);
      return;
    }
  }
  CopyUTF8toUTF16(spec, aURI);
}

nsIURI* nsINode::GetBaseURIObject() const { return GetBaseURI(true); }

void nsINode::LookupPrefix(const nsAString& aNamespaceURI, nsAString& aPrefix) {
  if (Element* nsElement = GetNameSpaceElement()) {
    // XXX Waiting for DOM spec to list error codes.

    // Trace up the content parent chain looking for the namespace
    // declaration that defines the aNamespaceURI namespace. Once found,
    // return the prefix (i.e. the attribute localName).
    for (Element* element : nsElement->InclusiveAncestorsOfType<Element>()) {
      uint32_t attrCount = element->GetAttrCount();

      for (uint32_t i = 0; i < attrCount; ++i) {
        const nsAttrName* name = element->GetAttrNameAt(i);

        if (name->NamespaceEquals(kNameSpaceID_XMLNS) &&
            element->AttrValueIs(kNameSpaceID_XMLNS, name->LocalName(),
                                 aNamespaceURI, eCaseMatters)) {
          // If the localName is "xmlns", the prefix we output should be
          // null.
          nsAtom* localName = name->LocalName();

          if (localName != nsGkAtoms::xmlns) {
            localName->ToString(aPrefix);
          } else {
            SetDOMStringToNull(aPrefix);
          }
          return;
        }
      }
    }
  }

  SetDOMStringToNull(aPrefix);
}

uint16_t nsINode::CompareDocumentPosition(nsINode& aOtherNode,
                                          Maybe<uint32_t>* aThisIndex,
                                          Maybe<uint32_t>* aOtherIndex) const {
  if (this == &aOtherNode) {
    return 0;
  }
  if (GetPreviousSibling() == &aOtherNode) {
    MOZ_ASSERT(GetParentNode() == aOtherNode.GetParentNode());
    return Node_Binding::DOCUMENT_POSITION_PRECEDING;
  }
  if (GetNextSibling() == &aOtherNode) {
    MOZ_ASSERT(GetParentNode() == aOtherNode.GetParentNode());
    return Node_Binding::DOCUMENT_POSITION_FOLLOWING;
  }

  AutoTArray<const nsINode*, 32> parents1, parents2;

  const nsINode* node1 = &aOtherNode;
  const nsINode* node2 = this;

  // Check if either node is an attribute
  const Attr* attr1 = Attr::FromNode(node1);
  if (attr1) {
    const Element* elem = attr1->GetElement();
    // If there is an owner element add the attribute
    // to the chain and walk up to the element
    if (elem) {
      node1 = elem;
      parents1.AppendElement(attr1);
    }
  }
  if (auto* attr2 = Attr::FromNode(node2)) {
    const Element* elem = attr2->GetElement();
    if (elem == node1 && attr1) {
      // Both nodes are attributes on the same element.
      // Compare position between the attributes.

      uint32_t i;
      const nsAttrName* attrName;
      for (i = 0; (attrName = elem->GetAttrNameAt(i)); ++i) {
        if (attrName->Equals(attr1->NodeInfo())) {
          NS_ASSERTION(!attrName->Equals(attr2->NodeInfo()),
                       "Different attrs at same position");
          return Node_Binding::DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC |
                 Node_Binding::DOCUMENT_POSITION_PRECEDING;
        }
        if (attrName->Equals(attr2->NodeInfo())) {
          return Node_Binding::DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC |
                 Node_Binding::DOCUMENT_POSITION_FOLLOWING;
        }
      }
      MOZ_ASSERT_UNREACHABLE("neither attribute in the element");
      return Node_Binding::DOCUMENT_POSITION_DISCONNECTED;
    }

    if (elem) {
      node2 = elem;
      parents2.AppendElement(attr2);
    }
  }

  // We now know that both nodes are either nsIContents or Documents.
  // If either node started out as an attribute, that attribute will have
  // the same relative position as its ownerElement, except if the
  // ownerElement ends up being the container for the other node

  // Build the chain of parents
  do {
    parents1.AppendElement(node1);
    node1 = node1->GetParentNode();
  } while (node1);
  do {
    parents2.AppendElement(node2);
    node2 = node2->GetParentNode();
  } while (node2);

  // Check if the nodes are disconnected.
  uint32_t pos1 = parents1.Length();
  uint32_t pos2 = parents2.Length();
  const nsINode* top1 = parents1.ElementAt(--pos1);
  const nsINode* top2 = parents2.ElementAt(--pos2);
  if (top1 != top2) {
    return top1 < top2
               ? (Node_Binding::DOCUMENT_POSITION_PRECEDING |
                  Node_Binding::DOCUMENT_POSITION_DISCONNECTED |
                  Node_Binding::DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC)
               : (Node_Binding::DOCUMENT_POSITION_FOLLOWING |
                  Node_Binding::DOCUMENT_POSITION_DISCONNECTED |
                  Node_Binding::DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC);
  }

  // Find where the parent chain differs and check indices in the parent.
  const nsINode* parent = top1;
  uint32_t len;
  for (len = std::min(pos1, pos2); len > 0; --len) {
    const nsINode* child1 = parents1.ElementAt(--pos1);
    const nsINode* child2 = parents2.ElementAt(--pos2);
    if (child1 != child2) {
      // child1 or child2 can be an attribute here. This will work fine since
      // ComputeIndexOf will return Nothing for the attribute making the
      // attribute be considered before any child.
      Maybe<uint32_t> child1Index;
      bool cachedChild1Index = false;
      if (&aOtherNode == child1 && aOtherIndex) {
        cachedChild1Index = true;
        child1Index = aOtherIndex->isSome() ? *aOtherIndex
                                            : parent->ComputeIndexOf(child1);
      } else {
        child1Index = parent->ComputeIndexOf(child1);
      }

      Maybe<uint32_t> child2Index;
      bool cachedChild2Index = false;
      if (this == child2 && aThisIndex) {
        cachedChild2Index = true;
        child2Index =
            aThisIndex->isSome() ? *aThisIndex : parent->ComputeIndexOf(child2);
      } else {
        child2Index = parent->ComputeIndexOf(child2);
      }

      uint16_t retVal = child1Index < child2Index
                            ? Node_Binding::DOCUMENT_POSITION_PRECEDING
                            : Node_Binding::DOCUMENT_POSITION_FOLLOWING;

      if (cachedChild1Index) {
        *aOtherIndex = child1Index;
      }
      if (cachedChild2Index) {
        *aThisIndex = child2Index;
      }

      return retVal;
    }
    parent = child1;
  }

  // We hit the end of one of the parent chains without finding a difference
  // between the chains. That must mean that one node is an ancestor of the
  // other. The one with the shortest chain must be the ancestor.
  return pos1 < pos2 ? (Node_Binding::DOCUMENT_POSITION_PRECEDING |
                        Node_Binding::DOCUMENT_POSITION_CONTAINS)
                     : (Node_Binding::DOCUMENT_POSITION_FOLLOWING |
                        Node_Binding::DOCUMENT_POSITION_CONTAINED_BY);
}

bool nsINode::IsSameNode(nsINode* other) { return other == this; }

bool nsINode::IsEqualNode(nsINode* aOther) {
  if (!aOther) {
    return false;
  }

  // Might as well do a quick check to avoid walking our kids if we're
  // obviously the same.
  if (aOther == this) {
    return true;
  }

  nsAutoString string1, string2;

  nsINode* node1 = this;
  nsINode* node2 = aOther;
  do {
    uint16_t nodeType = node1->NodeType();
    if (nodeType != node2->NodeType()) {
      return false;
    }

    mozilla::dom::NodeInfo* nodeInfo1 = node1->mNodeInfo;
    mozilla::dom::NodeInfo* nodeInfo2 = node2->mNodeInfo;
    if (!nodeInfo1->Equals(nodeInfo2) ||
        nodeInfo1->GetExtraName() != nodeInfo2->GetExtraName()) {
      return false;
    }

    switch (nodeType) {
      case ELEMENT_NODE: {
        // Both are elements (we checked that their nodeinfos are equal). Do the
        // check on attributes.
        Element* element1 = node1->AsElement();
        Element* element2 = node2->AsElement();
        uint32_t attrCount = element1->GetAttrCount();
        if (attrCount != element2->GetAttrCount()) {
          return false;
        }

        // Iterate over attributes.
        for (uint32_t i = 0; i < attrCount; ++i) {
          const nsAttrName* attrName = element1->GetAttrNameAt(i);
#ifdef DEBUG
          bool hasAttr =
#endif
              element1->GetAttr(attrName->NamespaceID(), attrName->LocalName(),
                                string1);
          NS_ASSERTION(hasAttr, "Why don't we have an attr?");

          if (!element2->AttrValueIs(attrName->NamespaceID(),
                                     attrName->LocalName(), string1,
                                     eCaseMatters)) {
            return false;
          }
        }
        break;
      }
      case TEXT_NODE:
      case COMMENT_NODE:
      case CDATA_SECTION_NODE:
      case PROCESSING_INSTRUCTION_NODE: {
        MOZ_ASSERT(node1->IsCharacterData());
        MOZ_ASSERT(node2->IsCharacterData());
        auto* data1 = static_cast<CharacterData*>(node1);
        auto* data2 = static_cast<CharacterData*>(node2);

        if (!data1->TextEquals(data2)) {
          return false;
        }

        break;
      }
      case DOCUMENT_NODE:
      case DOCUMENT_FRAGMENT_NODE:
        break;
      case ATTRIBUTE_NODE: {
        NS_ASSERTION(node1 == this && node2 == aOther,
                     "Did we come upon an attribute node while walking a "
                     "subtree?");
        node1->GetNodeValue(string1);
        node2->GetNodeValue(string2);

        // Returning here as to not bother walking subtree. And there is no
        // risk that we're half way through walking some other subtree since
        // attribute nodes doesn't appear in subtrees.
        return string1.Equals(string2);
      }
      case DOCUMENT_TYPE_NODE: {
        DocumentType* docType1 = static_cast<DocumentType*>(node1);
        DocumentType* docType2 = static_cast<DocumentType*>(node2);

        // Public ID
        docType1->GetPublicId(string1);
        docType2->GetPublicId(string2);
        if (!string1.Equals(string2)) {
          return false;
        }

        // System ID
        docType1->GetSystemId(string1);
        docType2->GetSystemId(string2);
        if (!string1.Equals(string2)) {
          return false;
        }

        break;
      }
      default:
        MOZ_ASSERT(false, "Unknown node type");
    }

    nsINode* nextNode = node1->GetFirstChild();
    if (nextNode) {
      node1 = nextNode;
      node2 = node2->GetFirstChild();
    } else {
      if (node2->GetFirstChild()) {
        // node2 has a firstChild, but node1 doesn't
        return false;
      }

      // Find next sibling, possibly walking parent chain.
      while (1) {
        if (node1 == this) {
          NS_ASSERTION(node2 == aOther,
                       "Should have reached the start node "
                       "for both trees at the same time");
          return true;
        }

        nextNode = node1->GetNextSibling();
        if (nextNode) {
          node1 = nextNode;
          node2 = node2->GetNextSibling();
          break;
        }

        if (node2->GetNextSibling()) {
          // node2 has a nextSibling, but node1 doesn't
          return false;
        }

        node1 = node1->GetParentNode();
        node2 = node2->GetParentNode();
        NS_ASSERTION(node1 && node2, "no parent while walking subtree");
      }
    }
  } while (node2);

  return false;
}

void nsINode::LookupNamespaceURI(const nsAString& aNamespacePrefix,
                                 nsAString& aNamespaceURI) {
  Element* element = GetNameSpaceElement();
  if (!element || NS_FAILED(element->LookupNamespaceURIInternal(
                      aNamespacePrefix, aNamespaceURI))) {
    SetDOMStringToNull(aNamespaceURI);
  }
}

mozilla::Maybe<mozilla::dom::EventCallbackDebuggerNotificationType>
nsINode::GetDebuggerNotificationType() const {
  return mozilla::Some(
      mozilla::dom::EventCallbackDebuggerNotificationType::Node);
}

bool nsINode::ComputeDefaultWantsUntrusted(ErrorResult& aRv) {
  return !nsContentUtils::IsChromeDoc(OwnerDoc());
}

void nsINode::GetBoxQuads(const BoxQuadOptions& aOptions,
                          nsTArray<RefPtr<DOMQuad>>& aResult,
                          CallerType aCallerType, mozilla::ErrorResult& aRv) {
  mozilla::GetBoxQuads(this, aOptions, aResult, aCallerType, aRv);
}

void nsINode::GetBoxQuadsFromWindowOrigin(const BoxQuadOptions& aOptions,
                                          nsTArray<RefPtr<DOMQuad>>& aResult,
                                          mozilla::ErrorResult& aRv) {
  mozilla::GetBoxQuadsFromWindowOrigin(this, aOptions, aResult, aRv);
}

already_AddRefed<DOMQuad> nsINode::ConvertQuadFromNode(
    DOMQuad& aQuad, const GeometryNode& aFrom,
    const ConvertCoordinateOptions& aOptions, CallerType aCallerType,
    ErrorResult& aRv) {
  return mozilla::ConvertQuadFromNode(this, aQuad, aFrom, aOptions, aCallerType,
                                      aRv);
}

already_AddRefed<DOMQuad> nsINode::ConvertRectFromNode(
    DOMRectReadOnly& aRect, const GeometryNode& aFrom,
    const ConvertCoordinateOptions& aOptions, CallerType aCallerType,
    ErrorResult& aRv) {
  return mozilla::ConvertRectFromNode(this, aRect, aFrom, aOptions, aCallerType,
                                      aRv);
}

already_AddRefed<DOMPoint> nsINode::ConvertPointFromNode(
    const DOMPointInit& aPoint, const GeometryNode& aFrom,
    const ConvertCoordinateOptions& aOptions, CallerType aCallerType,
    ErrorResult& aRv) {
  return mozilla::ConvertPointFromNode(this, aPoint, aFrom, aOptions,
                                       aCallerType, aRv);
}

bool nsINode::DispatchEvent(Event& aEvent, CallerType aCallerType,
                            ErrorResult& aRv) {
  // XXX sXBL/XBL2 issue -- do we really want the owner here?  What
  // if that's the XBL document?  Would we want its presshell?  Or what?
  nsCOMPtr<Document> document = OwnerDoc();

  // Do nothing if the element does not belong to a document
  if (!document) {
    return true;
  }

  // Obtain a presentation shell
  RefPtr<nsPresContext> context = document->GetPresContext();

  nsEventStatus status = nsEventStatus_eIgnore;
  nsresult rv = EventDispatcher::DispatchDOMEvent(this, nullptr, &aEvent,
                                                  context, &status);
  bool retval = !aEvent.DefaultPrevented(aCallerType);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
  return retval;
}

nsresult nsINode::PostHandleEvent(EventChainPostVisitor& /*aVisitor*/) {
  return NS_OK;
}

EventListenerManager* nsINode::GetOrCreateListenerManager() {
  return nsContentUtils::GetListenerManagerForNode(this);
}

EventListenerManager* nsINode::GetExistingListenerManager() const {
  return nsContentUtils::GetExistingListenerManagerForNode(this);
}

nsPIDOMWindowOuter* nsINode::GetOwnerGlobalForBindingsInternal() {
  bool dummy;
  // FIXME(bz): This cast is a bit bogus.  See
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1515709
  auto* window = static_cast<nsGlobalWindowInner*>(
      OwnerDoc()->GetScriptHandlingObject(dummy));
  return window ? nsPIDOMWindowOuter::GetFromCurrentInner(window) : nullptr;
}

nsIGlobalObject* nsINode::GetOwnerGlobal() const {
  bool dummy;
  return OwnerDoc()->GetScriptHandlingObject(dummy);
}

bool nsINode::UnoptimizableCCNode() const {
  return IsInNativeAnonymousSubtree() || IsAttr();
}

/* static */
bool nsINode::Traverse(nsINode* tmp, nsCycleCollectionTraversalCallback& cb) {
  if (MOZ_LIKELY(!cb.WantAllTraces())) {
    Document* currentDoc = tmp->GetComposedDoc();
    if (currentDoc && nsCCUncollectableMarker::InGeneration(
                          currentDoc->GetMarkedCCGeneration())) {
      return false;
    }

    if (nsCCUncollectableMarker::sGeneration) {
      // If we're black no need to traverse.
      if (tmp->HasKnownLiveWrapper() || tmp->InCCBlackTree()) {
        return false;
      }

      if (!tmp->UnoptimizableCCNode()) {
        // If we're in a black document, return early.
        if ((currentDoc && currentDoc->HasKnownLiveWrapper())) {
          return false;
        }
        // If we're not in anonymous content and we have a black parent,
        // return early.
        nsIContent* parent = tmp->GetParent();
        if (parent && !parent->UnoptimizableCCNode() &&
            parent->HasKnownLiveWrapper()) {
          MOZ_ASSERT(parent->ComputeIndexOf(tmp).isSome(),
                     "Parent doesn't own us?");
          return false;
        }
      }
    }
  }

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mNodeInfo)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFirstChild)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mNextSibling)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_RAWPTR(GetParent())

  nsSlots* slots = tmp->GetExistingSlots();
  if (slots) {
    slots->Traverse(cb);
  }

  if (tmp->HasProperties()) {
    nsCOMArray<nsISupports>* objects = static_cast<nsCOMArray<nsISupports>*>(
        tmp->GetProperty(nsGkAtoms::keepobjectsalive));
    if (objects) {
      for (int32_t i = 0; i < objects->Count(); ++i) {
        cb.NoteXPCOMChild(objects->ObjectAt(i));
      }
    }

#ifdef ACCESSIBILITY
    AccessibleNode* anode = static_cast<AccessibleNode*>(
        tmp->GetProperty(nsGkAtoms::accessiblenode));
    if (anode) {
      cb.NoteXPCOMChild(anode);
    }
#endif
  }

  if (tmp->NodeType() != DOCUMENT_NODE &&
      tmp->HasFlag(NODE_HAS_LISTENERMANAGER)) {
    nsContentUtils::TraverseListenerManager(tmp, cb);
  }

  return true;
}

/* static */
void nsINode::Unlink(nsINode* tmp) {
  tmp->ReleaseWrapper(tmp);

  if (nsSlots* slots = tmp->GetExistingSlots()) {
    slots->Unlink(*tmp);
  }

  if (tmp->NodeType() != DOCUMENT_NODE &&
      tmp->HasFlag(NODE_HAS_LISTENERMANAGER)) {
    nsContentUtils::RemoveListenerManager(tmp);
    tmp->UnsetFlags(NODE_HAS_LISTENERMANAGER);
  }

  if (tmp->HasProperties()) {
    tmp->RemoveProperty(nsGkAtoms::keepobjectsalive);
    tmp->RemoveProperty(nsGkAtoms::accessiblenode);
  }
}

static void AdoptNodeIntoOwnerDoc(nsINode* aParent, nsINode* aNode,
                                  ErrorResult& aError) {
  NS_ASSERTION(!aNode->GetParentNode(),
               "Should have removed from parent already");

  Document* doc = aParent->OwnerDoc();

  DebugOnly<nsINode*> adoptedNode = doc->AdoptNode(*aNode, aError, true);

#ifdef DEBUG
  if (!aError.Failed()) {
    MOZ_ASSERT(aParent->OwnerDoc() == doc, "ownerDoc chainged while adopting");
    MOZ_ASSERT(adoptedNode == aNode, "Uh, adopt node changed nodes?");
    MOZ_ASSERT(aParent->OwnerDoc() == aNode->OwnerDoc(),
               "ownerDocument changed again after adopting!");
  }
#endif  // DEBUG
}

static nsresult UpdateGlobalsInSubtree(nsIContent* aRoot) {
  MOZ_ASSERT(ShouldUseNACScope(aRoot));
  // Start off with no global so we don't fire any error events on failure.
  AutoJSAPI jsapi;
  jsapi.Init();

  JSContext* cx = jsapi.cx();

  ErrorResult rv;
  JS::Rooted<JSObject*> reflector(cx);
  for (nsIContent* cur = aRoot; cur; cur = cur->GetNextNode(aRoot)) {
    if ((reflector = cur->GetWrapper())) {
      JSAutoRealm ar(cx, reflector);
      UpdateReflectorGlobal(cx, reflector, rv);
      rv.WouldReportJSException();
      if (rv.Failed()) {
        // We _could_ consider BlastSubtreeToPieces here, but it's not really
        // needed.  Having some nodes in here accessible to content while others
        // are not is probably OK.  We just need to fail out of the actual
        // insertion, so they're not in the DOM.  Returning a failure here will
        // do that.
        return rv.StealNSResult();
      }
    }
  }

  return NS_OK;
}

void nsINode::InsertChildBefore(nsIContent* aKid, nsIContent* aBeforeThis,
                                bool aNotify, ErrorResult& aRv) {
  if (!IsContainerNode()) {
    aRv.ThrowHierarchyRequestError(
        "Parent is not a Document, DocumentFragment, or Element node.");
    return;
  }

  MOZ_ASSERT(!aKid->GetParentNode(), "Inserting node that already has parent");
  MOZ_ASSERT(!IsAttr());

  // The id-handling code, and in the future possibly other code, need to
  // react to unexpected attribute changes.
  nsMutationGuard::DidMutate();

  // Do this before checking the child-count since this could cause mutations
  mozAutoDocUpdate updateBatch(GetComposedDoc(), aNotify);

  if (OwnerDoc() != aKid->OwnerDoc()) {
    AdoptNodeIntoOwnerDoc(this, aKid, aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }
  }

  if (!aBeforeThis) {
    AppendChildToChildList(aKid);
  } else {
    InsertChildToChildList(aKid, aBeforeThis);
  }

  nsIContent* parent = IsContent() ? AsContent() : nullptr;

  // XXXbz Do we even need this code anymore?
  bool wasInNACScope = ShouldUseNACScope(aKid);
  BindContext context(*this);
  aRv = aKid->BindToTree(context, *this);
  if (!aRv.Failed() && !wasInNACScope && ShouldUseNACScope(aKid)) {
    MOZ_ASSERT(ShouldUseNACScope(this),
               "Why does the kid need to use an the anonymous content scope?");
    aRv = UpdateGlobalsInSubtree(aKid);
  }
  if (aRv.Failed()) {
    DisconnectChild(aKid);
    aKid->UnbindFromTree();
    return;
  }

  // Invalidate cached array of child nodes
  InvalidateChildNodes();

  NS_ASSERTION(aKid->GetParentNode() == this,
               "Did we run script inappropriately?");

  if (aNotify) {
    // Note that we always want to call ContentInserted when things are added
    // as kids to documents
    if (parent && !aBeforeThis) {
      MutationObservers::NotifyContentAppended(parent, aKid);
    } else {
      MutationObservers::NotifyContentInserted(this, aKid);
    }

    if (nsContentUtils::HasMutationListeners(
            aKid, NS_EVENT_BITS_MUTATION_NODEINSERTED, this)) {
      InternalMutationEvent mutation(true, eLegacyNodeInserted);
      mutation.mRelatedNode = this;

      mozAutoSubtreeModified subtree(OwnerDoc(), this);
      AsyncEventDispatcher::RunDOMEventWhenSafe(*aKid, mutation);
    }
  }
}

nsIContent* nsINode::GetPreviousSibling() const {
  // Do not expose circular linked list
  if (mPreviousOrLastSibling && !mPreviousOrLastSibling->mNextSibling) {
    return nullptr;
  }
  return mPreviousOrLastSibling;
}

// CACHE_POINTER_SHIFT indicates how many steps to downshift the |this| pointer.
// It should be small enough to not cause collisions between adjecent objects,
// and large enough to make sure that all indexes are used.
#define CACHE_POINTER_SHIFT 6
#define CACHE_NUM_SLOTS 128
#define CACHE_CHILD_LIMIT 10

#define CACHE_GET_INDEX(_parent) \
  ((NS_PTR_TO_INT32(_parent) >> CACHE_POINTER_SHIFT) & (CACHE_NUM_SLOTS - 1))

struct IndexCacheSlot {
  const nsINode* mParent;
  const nsINode* mChild;
  uint32_t mChildIndex;
};

static IndexCacheSlot sIndexCache[CACHE_NUM_SLOTS];

static inline void AddChildAndIndexToCache(const nsINode* aParent,
                                           const nsINode* aChild,
                                           uint32_t aChildIndex) {
  uint32_t index = CACHE_GET_INDEX(aParent);
  sIndexCache[index].mParent = aParent;
  sIndexCache[index].mChild = aChild;
  sIndexCache[index].mChildIndex = aChildIndex;
}

static inline void GetChildAndIndexFromCache(const nsINode* aParent,
                                             const nsINode** aChild,
                                             Maybe<uint32_t>* aChildIndex) {
  uint32_t index = CACHE_GET_INDEX(aParent);
  if (sIndexCache[index].mParent == aParent) {
    *aChild = sIndexCache[index].mChild;
    *aChildIndex = Some(sIndexCache[index].mChildIndex);
  } else {
    *aChild = nullptr;
    *aChildIndex = Nothing();
  }
}

static inline void RemoveFromCache(const nsINode* aParent) {
  uint32_t index = CACHE_GET_INDEX(aParent);
  if (sIndexCache[index].mParent == aParent) {
    sIndexCache[index] = {nullptr, nullptr, UINT32_MAX};
  }
}

void nsINode::AppendChildToChildList(nsIContent* aKid) {
  MOZ_ASSERT(aKid);
  MOZ_ASSERT(!aKid->mNextSibling);

  RemoveFromCache(this);

  if (mFirstChild) {
    nsIContent* lastChild = GetLastChild();
    lastChild->mNextSibling = aKid;
    aKid->mPreviousOrLastSibling = lastChild;
  } else {
    mFirstChild = aKid;
  }

  // Maintain link to the last child
  mFirstChild->mPreviousOrLastSibling = aKid;
  ++mChildCount;
}

void nsINode::InsertChildToChildList(nsIContent* aKid,
                                     nsIContent* aNextSibling) {
  MOZ_ASSERT(aKid);
  MOZ_ASSERT(aNextSibling);

  RemoveFromCache(this);

  nsIContent* previousSibling = aNextSibling->mPreviousOrLastSibling;
  aNextSibling->mPreviousOrLastSibling = aKid;
  aKid->mPreviousOrLastSibling = previousSibling;
  aKid->mNextSibling = aNextSibling;

  if (aNextSibling == mFirstChild) {
    MOZ_ASSERT(!previousSibling->mNextSibling);
    mFirstChild = aKid;
  } else {
    previousSibling->mNextSibling = aKid;
  }

  ++mChildCount;
}

void nsINode::DisconnectChild(nsIContent* aKid) {
  MOZ_ASSERT(aKid);
  MOZ_ASSERT(GetChildCount() > 0);

  RemoveFromCache(this);

  nsIContent* previousSibling = aKid->GetPreviousSibling();
  nsCOMPtr<nsIContent> ref = aKid;

  if (aKid->mNextSibling) {
    aKid->mNextSibling->mPreviousOrLastSibling = aKid->mPreviousOrLastSibling;
  } else {
    // aKid is the last child in the list
    mFirstChild->mPreviousOrLastSibling = aKid->mPreviousOrLastSibling;
  }
  aKid->mPreviousOrLastSibling = nullptr;

  if (previousSibling) {
    previousSibling->mNextSibling = std::move(aKid->mNextSibling);
  } else {
    // aKid is the first child in the list
    mFirstChild = std::move(aKid->mNextSibling);
  }

  --mChildCount;
}

nsIContent* nsINode::GetChildAt_Deprecated(uint32_t aIndex) const {
  if (aIndex >= GetChildCount()) {
    return nullptr;
  }

  nsIContent* child = mFirstChild;
  while (aIndex--) {
    child = child->GetNextSibling();
  }

  return child;
}

int32_t nsINode::ComputeIndexOf_Deprecated(
    const nsINode* aPossibleChild) const {
  Maybe<uint32_t> maybeIndex = ComputeIndexOf(aPossibleChild);
  if (!maybeIndex) {
    return -1;
  }
  MOZ_ASSERT(*maybeIndex <= INT32_MAX,
             "ComputeIndexOf_Deprecated() returns unsupported index value, use "
             "ComputeIndex() instead");
  return static_cast<int32_t>(*maybeIndex);
}

Maybe<uint32_t> nsINode::ComputeIndexOf(const nsINode* aPossibleChild) const {
  if (!aPossibleChild) {
    return Nothing();
  }

  if (aPossibleChild->GetParentNode() != this) {
    return Nothing();
  }

  if (aPossibleChild == GetFirstChild()) {
    return Some(0);
  }

  if (aPossibleChild == GetLastChild()) {
    MOZ_ASSERT(GetChildCount());
    return Some(GetChildCount() - 1);
  }

  if (mChildCount >= CACHE_CHILD_LIMIT) {
    const nsINode* child;
    Maybe<uint32_t> maybeChildIndex;
    GetChildAndIndexFromCache(this, &child, &maybeChildIndex);
    if (child) {
      if (child == aPossibleChild) {
        return maybeChildIndex;
      }

      uint32_t nextIndex = *maybeChildIndex;
      uint32_t prevIndex = *maybeChildIndex;
      nsINode* prev = child->GetPreviousSibling();
      nsINode* next = child->GetNextSibling();
      do {
        if (next) {
          MOZ_ASSERT(nextIndex < UINT32_MAX);
          ++nextIndex;
          if (next == aPossibleChild) {
            AddChildAndIndexToCache(this, aPossibleChild, nextIndex);
            return Some(nextIndex);
          }
          next = next->GetNextSibling();
        }
        if (prev) {
          MOZ_ASSERT(prevIndex > 0);
          --prevIndex;
          if (prev == aPossibleChild) {
            AddChildAndIndexToCache(this, aPossibleChild, prevIndex);
            return Some(prevIndex);
          }
          prev = prev->GetPreviousSibling();
        }
      } while (prev || next);
    }
  }

  uint32_t index = 0u;
  nsINode* current = mFirstChild;
  while (current) {
    MOZ_ASSERT(current->GetParentNode() == this);
    if (current == aPossibleChild) {
      if (mChildCount >= CACHE_CHILD_LIMIT) {
        AddChildAndIndexToCache(this, current, index);
      }
      return Some(index);
    }
    current = current->GetNextSibling();
    MOZ_ASSERT(index < UINT32_MAX);
    ++index;
  }

  return Nothing();
}

Maybe<uint32_t> nsINode::ComputeIndexInParentNode() const {
  nsINode* parent = GetParentNode();
  if (MOZ_UNLIKELY(!parent)) {
    return Nothing();
  }
  return parent->ComputeIndexOf(this);
}

Maybe<uint32_t> nsINode::ComputeIndexInParentContent() const {
  nsIContent* parent = GetParent();
  if (MOZ_UNLIKELY(!parent)) {
    return Nothing();
  }
  return parent->ComputeIndexOf(this);
}

static Maybe<uint32_t> DoComputeFlatTreeIndexOf(FlattenedChildIterator& aIter,
                                                const nsINode* aPossibleChild) {
  if (aPossibleChild->GetFlattenedTreeParentNode() != aIter.Parent()) {
    return Nothing();
  }

  uint32_t index = 0u;
  for (nsIContent* child = aIter.GetNextChild(); child;
       child = aIter.GetNextChild()) {
    if (child == aPossibleChild) {
      return Some(index);
    }

    ++index;
  }

  return Nothing();
}

Maybe<uint32_t> nsINode::ComputeFlatTreeIndexOf(
    const nsINode* aPossibleChild) const {
  if (!aPossibleChild) {
    return Nothing();
  }

  if (!IsContent()) {
    return ComputeIndexOf(aPossibleChild);
  }

  FlattenedChildIterator iter(AsContent());
  if (!iter.ShadowDOMInvolved()) {
    auto index = ComputeIndexOf(aPossibleChild);
    MOZ_ASSERT(DoComputeFlatTreeIndexOf(iter, aPossibleChild) == index);
    return index;
  }

  return DoComputeFlatTreeIndexOf(iter, aPossibleChild);
}

static already_AddRefed<nsINode> GetNodeFromNodeOrString(
    const OwningNodeOrString& aNode, Document* aDocument) {
  if (aNode.IsNode()) {
    nsCOMPtr<nsINode> node = aNode.GetAsNode();
    return node.forget();
  }

  if (aNode.IsString()) {
    RefPtr<nsTextNode> textNode =
        aDocument->CreateTextNode(aNode.GetAsString());
    return textNode.forget();
  }

  MOZ_CRASH("Impossible type");
}

/**
 * Implement the algorithm specified at
 * https://dom.spec.whatwg.org/#converting-nodes-into-a-node for |prepend()|,
 * |append()|, |before()|, |after()|, and |replaceWith()| APIs.
 */
MOZ_CAN_RUN_SCRIPT static already_AddRefed<nsINode>
ConvertNodesOrStringsIntoNode(const Sequence<OwningNodeOrString>& aNodes,
                              Document* aDocument, ErrorResult& aRv) {
  if (aNodes.Length() == 1) {
    return GetNodeFromNodeOrString(aNodes[0], aDocument);
  }

  nsCOMPtr<nsINode> fragment = aDocument->CreateDocumentFragment();

  for (const auto& node : aNodes) {
    nsCOMPtr<nsINode> childNode = GetNodeFromNodeOrString(node, aDocument);
    fragment->AppendChild(*childNode, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
  }

  return fragment.forget();
}

static void InsertNodesIntoHashset(const Sequence<OwningNodeOrString>& aNodes,
                                   nsTHashSet<nsINode*>& aHashset) {
  for (const auto& node : aNodes) {
    if (node.IsNode()) {
      aHashset.Insert(node.GetAsNode());
    }
  }
}

static nsINode* FindViablePreviousSibling(
    const nsINode& aNode, const Sequence<OwningNodeOrString>& aNodes) {
  nsTHashSet<nsINode*> nodeSet(16);
  InsertNodesIntoHashset(aNodes, nodeSet);

  nsINode* viablePreviousSibling = nullptr;
  for (nsINode* sibling = aNode.GetPreviousSibling(); sibling;
       sibling = sibling->GetPreviousSibling()) {
    if (!nodeSet.Contains(sibling)) {
      viablePreviousSibling = sibling;
      break;
    }
  }

  return viablePreviousSibling;
}

static nsINode* FindViableNextSibling(
    const nsINode& aNode, const Sequence<OwningNodeOrString>& aNodes) {
  nsTHashSet<nsINode*> nodeSet(16);
  InsertNodesIntoHashset(aNodes, nodeSet);

  nsINode* viableNextSibling = nullptr;
  for (nsINode* sibling = aNode.GetNextSibling(); sibling;
       sibling = sibling->GetNextSibling()) {
    if (!nodeSet.Contains(sibling)) {
      viableNextSibling = sibling;
      break;
    }
  }

  return viableNextSibling;
}

void nsINode::Before(const Sequence<OwningNodeOrString>& aNodes,
                     ErrorResult& aRv) {
  nsCOMPtr<nsINode> parent = GetParentNode();
  if (!parent) {
    return;
  }

  nsCOMPtr<nsINode> viablePreviousSibling =
      FindViablePreviousSibling(*this, aNodes);

  nsCOMPtr<Document> doc = OwnerDoc();
  nsCOMPtr<nsINode> node = ConvertNodesOrStringsIntoNode(aNodes, doc, aRv);
  if (aRv.Failed()) {
    return;
  }

  viablePreviousSibling = viablePreviousSibling
                              ? viablePreviousSibling->GetNextSibling()
                              : parent->GetFirstChild();

  parent->InsertBefore(*node, viablePreviousSibling, aRv);
}

void nsINode::After(const Sequence<OwningNodeOrString>& aNodes,
                    ErrorResult& aRv) {
  nsCOMPtr<nsINode> parent = GetParentNode();
  if (!parent) {
    return;
  }

  nsCOMPtr<nsINode> viableNextSibling = FindViableNextSibling(*this, aNodes);

  nsCOMPtr<Document> doc = OwnerDoc();
  nsCOMPtr<nsINode> node = ConvertNodesOrStringsIntoNode(aNodes, doc, aRv);
  if (aRv.Failed()) {
    return;
  }

  parent->InsertBefore(*node, viableNextSibling, aRv);
}

void nsINode::ReplaceWith(const Sequence<OwningNodeOrString>& aNodes,
                          ErrorResult& aRv) {
  nsCOMPtr<nsINode> parent = GetParentNode();
  if (!parent) {
    return;
  }

  nsCOMPtr<nsINode> viableNextSibling = FindViableNextSibling(*this, aNodes);

  nsCOMPtr<Document> doc = OwnerDoc();
  nsCOMPtr<nsINode> node = ConvertNodesOrStringsIntoNode(aNodes, doc, aRv);
  if (aRv.Failed()) {
    return;
  }

  if (parent == GetParentNode()) {
    parent->ReplaceChild(*node, *this, aRv);
  } else {
    parent->InsertBefore(*node, viableNextSibling, aRv);
  }
}

void nsINode::Remove() {
  nsCOMPtr<nsINode> parent = GetParentNode();
  if (!parent) {
    return;
  }

  parent->RemoveChild(*this, IgnoreErrors());
}

Element* nsINode::GetFirstElementChild() const {
  for (nsIContent* child = GetFirstChild(); child;
       child = child->GetNextSibling()) {
    if (child->IsElement()) {
      return child->AsElement();
    }
  }

  return nullptr;
}

Element* nsINode::GetLastElementChild() const {
  for (nsIContent* child = GetLastChild(); child;
       child = child->GetPreviousSibling()) {
    if (child->IsElement()) {
      return child->AsElement();
    }
  }

  return nullptr;
}

static bool MatchAttribute(Element* aElement, int32_t aNamespaceID,
                           nsAtom* aAttrName, void* aData) {
  MOZ_ASSERT(aElement, "Must have content node to work with!");
  nsString* attrValue = static_cast<nsString*>(aData);
  if (aNamespaceID != kNameSpaceID_Unknown &&
      aNamespaceID != kNameSpaceID_Wildcard) {
    return attrValue->EqualsLiteral("*")
               ? aElement->HasAttr(aNamespaceID, aAttrName)
               : aElement->AttrValueIs(aNamespaceID, aAttrName, *attrValue,
                                       eCaseMatters);
  }

  // Qualified name match. This takes more work.
  uint32_t count = aElement->GetAttrCount();
  for (uint32_t i = 0; i < count; ++i) {
    const nsAttrName* name = aElement->GetAttrNameAt(i);
    bool nameMatch;
    if (name->IsAtom()) {
      nameMatch = name->Atom() == aAttrName;
    } else if (aNamespaceID == kNameSpaceID_Wildcard) {
      nameMatch = name->NodeInfo()->Equals(aAttrName);
    } else {
      nameMatch = name->NodeInfo()->QualifiedNameEquals(aAttrName);
    }

    if (nameMatch) {
      return attrValue->EqualsLiteral("*") ||
             aElement->AttrValueIs(name->NamespaceID(), name->LocalName(),
                                   *attrValue, eCaseMatters);
    }
  }

  return false;
}

already_AddRefed<nsIHTMLCollection> nsINode::GetElementsByAttribute(
    const nsAString& aAttribute, const nsAString& aValue) {
  RefPtr<nsAtom> attrAtom(NS_Atomize(aAttribute));
  RefPtr<nsContentList> list = new nsContentList(
      this, MatchAttribute, nsContentUtils::DestroyMatchString,
      new nsString(aValue), true, attrAtom, kNameSpaceID_Unknown);

  return list.forget();
}

already_AddRefed<nsIHTMLCollection> nsINode::GetElementsByAttributeNS(
    const nsAString& aNamespaceURI, const nsAString& aAttribute,
    const nsAString& aValue, ErrorResult& aRv) {
  RefPtr<nsAtom> attrAtom(NS_Atomize(aAttribute));

  int32_t nameSpaceId = kNameSpaceID_Wildcard;
  if (!aNamespaceURI.EqualsLiteral("*")) {
    nsresult rv = nsNameSpaceManager::GetInstance()->RegisterNameSpace(
        aNamespaceURI, nameSpaceId);
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return nullptr;
    }
  }

  RefPtr<nsContentList> list = new nsContentList(
      this, MatchAttribute, nsContentUtils::DestroyMatchString,
      new nsString(aValue), true, attrAtom, nameSpaceId);
  return list.forget();
}

void nsINode::Prepend(const Sequence<OwningNodeOrString>& aNodes,
                      ErrorResult& aRv) {
  nsCOMPtr<Document> doc = OwnerDoc();
  nsCOMPtr<nsINode> node = ConvertNodesOrStringsIntoNode(aNodes, doc, aRv);
  if (aRv.Failed()) {
    return;
  }

  nsCOMPtr<nsIContent> refNode = mFirstChild;
  InsertBefore(*node, refNode, aRv);
}

void nsINode::Append(const Sequence<OwningNodeOrString>& aNodes,
                     ErrorResult& aRv) {
  nsCOMPtr<Document> doc = OwnerDoc();
  nsCOMPtr<nsINode> node = ConvertNodesOrStringsIntoNode(aNodes, doc, aRv);
  if (aRv.Failed()) {
    return;
  }

  AppendChild(*node, aRv);
}

// https://dom.spec.whatwg.org/#dom-parentnode-replacechildren
void nsINode::ReplaceChildren(const Sequence<OwningNodeOrString>& aNodes,
                              ErrorResult& aRv) {
  nsCOMPtr<Document> doc = OwnerDoc();
  nsCOMPtr<nsINode> node = ConvertNodesOrStringsIntoNode(aNodes, doc, aRv);
  if (aRv.Failed()) {
    return;
  }
  MOZ_ASSERT(node);
  return ReplaceChildren(node, aRv);
}

void nsINode::ReplaceChildren(nsINode* aNode, ErrorResult& aRv) {
  if (aNode) {
    EnsurePreInsertionValidity(*aNode, nullptr, aRv);
    if (aRv.Failed()) {
      return;
    }
  }
  nsCOMPtr<nsINode> node = aNode;

  // Batch possible DOMSubtreeModified events.
  mozAutoSubtreeModified subtree(OwnerDoc(), nullptr);

  if (nsContentUtils::HasMutationListeners(
          OwnerDoc(), NS_EVENT_BITS_MUTATION_NODEREMOVED)) {
    FireNodeRemovedForChildren();
    if (node) {
      if (node->NodeType() == DOCUMENT_FRAGMENT_NODE) {
        node->FireNodeRemovedForChildren();
      } else if (nsCOMPtr<nsINode> parent = node->GetParentNode()) {
        nsContentUtils::MaybeFireNodeRemoved(node, parent);
      }
    }
  }

  // Needed when used in combination with contenteditable (maybe)
  mozAutoDocUpdate updateBatch(OwnerDoc(), true);

  nsAutoMutationBatch mb(this, true, true);

  // The code above explicitly dispatched DOMNodeRemoved events if needed.
  nsAutoScriptBlockerSuppressNodeRemoved scriptBlocker;

  // Replace all with node within this.
  while (mFirstChild) {
    RemoveChildNode(mFirstChild, true);
  }
  mb.RemovalDone();

  if (aNode) {
    AppendChild(*aNode, aRv);
    mb.NodesAdded();
  }
}

void nsINode::RemoveChildNode(nsIContent* aKid, bool aNotify) {
  // NOTE: This function must not trigger any calls to
  // Document::GetRootElement() calls until *after* it has removed aKid from
  // aChildArray. Any calls before then could potentially restore a stale
  // value for our cached root element, per note in
  // Document::RemoveChildNode().
  MOZ_ASSERT(aKid && aKid->GetParentNode() == this, "Bogus aKid");
  MOZ_ASSERT(!IsAttr());

  nsMutationGuard::DidMutate();
  mozAutoDocUpdate updateBatch(GetComposedDoc(), aNotify);

  nsIContent* previousSibling = aKid->GetPreviousSibling();

  // Since aKid is use also after DisconnectChild, ensure it stays alive.
  nsCOMPtr<nsIContent> kungfuDeathGrip = aKid;
  DisconnectChild(aKid);

  // Invalidate cached array of child nodes
  InvalidateChildNodes();

  if (aNotify) {
    MutationObservers::NotifyContentRemoved(this, aKid, previousSibling);
  }

  aKid->UnbindFromTree();
}

// When replacing, aRefChild is the content being replaced; when
// inserting it's the content before which we're inserting.  In the
// latter case it may be null.
//
// If aRv is a failure after this call, the insertion should not happen.
//
// This implements the parts of
// https://dom.spec.whatwg.org/#concept-node-ensure-pre-insertion-validity and
// the checks in https://dom.spec.whatwg.org/#concept-node-replace that
// depend on the child nodes or come after steps that depend on the child nodes
// (steps 2-6 in both cases).
static void EnsureAllowedAsChild(nsINode* aNewChild, nsINode* aParent,
                                 bool aIsReplace, nsINode* aRefChild,
                                 ErrorResult& aRv) {
  MOZ_ASSERT(aNewChild, "Must have new child");
  MOZ_ASSERT_IF(aIsReplace, aRefChild);
  MOZ_ASSERT(aParent);
  MOZ_ASSERT(aParent->IsDocument() || aParent->IsDocumentFragment() ||
                 aParent->IsElement(),
             "Nodes that are not documents, document fragments or elements "
             "can't be parents!");

  // Step 2.
  // A common case is that aNewChild has no kids, in which case
  // aParent can't be a descendant of aNewChild unless they're
  // actually equal to each other.  Fast-path that case, since aParent
  // could be pretty deep in the DOM tree.
  if (aNewChild == aParent ||
      ((aNewChild->GetFirstChild() ||
        // HTML template elements and ShadowRoot hosts need
        // to be checked to ensure that they are not inserted into
        // the hosted content.
        aNewChild->NodeInfo()->NameAtom() == nsGkAtoms::_template ||
        (aNewChild->IsElement() && aNewChild->AsElement()->GetShadowRoot())) &&
       nsContentUtils::ContentIsHostIncludingDescendantOf(aParent,
                                                          aNewChild))) {
    aRv.ThrowHierarchyRequestError(
        "The new child is an ancestor of the parent");
    return;
  }

  // Step 3.
  if (aRefChild && aRefChild->GetParentNode() != aParent) {
    if (aIsReplace) {
      if (aNewChild->GetParentNode() == aParent) {
        aRv.ThrowNotFoundError(
            "New child already has this parent and old child does not. Please "
            "check the order of replaceChild's arguments.");
      } else {
        aRv.ThrowNotFoundError(
            "Child to be replaced is not a child of this node");
      }
    } else {
      aRv.ThrowNotFoundError(
          "Child to insert before is not a child of this node");
    }
    return;
  }

  // Step 4.
  if (!aNewChild->IsContent()) {
    aRv.ThrowHierarchyRequestError(nsPrintfCString(
        "May not add %s as a child", NodeTypeAsString(aNewChild)));
    return;
  }

  // Steps 5 and 6 combined.
  // The allowed child nodes differ for documents and elements
  switch (aNewChild->NodeType()) {
    case nsINode::COMMENT_NODE:
    case nsINode::PROCESSING_INSTRUCTION_NODE:
      // OK in both cases
      return;
    case nsINode::TEXT_NODE:
    case nsINode::CDATA_SECTION_NODE:
    case nsINode::ENTITY_REFERENCE_NODE:
      // Allowed under Elements and DocumentFragments
      if (aParent->NodeType() == nsINode::DOCUMENT_NODE) {
        aRv.ThrowHierarchyRequestError(
            nsPrintfCString("Cannot insert %s as a child of a Document",
                            NodeTypeAsString(aNewChild)));
      }
      return;
    case nsINode::ELEMENT_NODE: {
      if (!aParent->IsDocument()) {
        // Always ok to have elements under other elements or document fragments
        return;
      }

      Document* parentDocument = aParent->AsDocument();
      Element* rootElement = parentDocument->GetRootElement();
      if (rootElement) {
        // Already have a documentElement, so this is only OK if we're
        // replacing it.
        if (!aIsReplace || rootElement != aRefChild) {
          aRv.ThrowHierarchyRequestError(
              "Cannot have more than one Element child of a Document");
        }
        return;
      }

      // We don't have a documentElement yet.  Our one remaining constraint is
      // that the documentElement must come after the doctype.
      if (!aRefChild) {
        // Appending is just fine.
        return;
      }

      nsIContent* docTypeContent = parentDocument->GetDoctype();
      if (!docTypeContent) {
        // It's all good.
        return;
      }

      // The docTypeContent is retrived from the child list of the Document
      // node so that doctypeIndex is never Nothing.
      const Maybe<uint32_t> doctypeIndex =
          aParent->ComputeIndexOf(docTypeContent);
      MOZ_ASSERT(doctypeIndex.isSome());
      // If aRefChild is an NAC, its index can be Nothing.
      const Maybe<uint32_t> insertIndex = aParent->ComputeIndexOf(aRefChild);

      // Now we're OK in the following two cases only:
      // 1) We're replacing something that's not before the doctype
      // 2) We're inserting before something that comes after the doctype
      const bool ok = MOZ_LIKELY(insertIndex.isSome()) &&
                      (aIsReplace ? *insertIndex >= *doctypeIndex
                                  : *insertIndex > *doctypeIndex);
      if (!ok) {
        aRv.ThrowHierarchyRequestError(
            "Cannot insert a root element before the doctype");
      }
      return;
    }
    case nsINode::DOCUMENT_TYPE_NODE: {
      if (!aParent->IsDocument()) {
        // doctypes only allowed under documents
        aRv.ThrowHierarchyRequestError(
            nsPrintfCString("Cannot insert a DocumentType as a child of %s",
                            NodeTypeAsString(aParent)));
        return;
      }

      Document* parentDocument = aParent->AsDocument();
      nsIContent* docTypeContent = parentDocument->GetDoctype();
      if (docTypeContent) {
        // Already have a doctype, so this is only OK if we're replacing it
        if (!aIsReplace || docTypeContent != aRefChild) {
          aRv.ThrowHierarchyRequestError(
              "Cannot have more than one DocumentType child of a Document");
        }
        return;
      }

      // We don't have a doctype yet.  Our one remaining constraint is
      // that the doctype must come before the documentElement.
      Element* rootElement = parentDocument->GetRootElement();
      if (!rootElement) {
        // It's all good
        return;
      }

      if (!aRefChild) {
        // Trying to append a doctype, but have a documentElement
        aRv.ThrowHierarchyRequestError(
            "Cannot have a DocumentType node after the root element");
        return;
      }

      // rootElement is now in the child list of the Document node so that
      // ComputeIndexOf must success to find it.
      const Maybe<uint32_t> rootIndex = aParent->ComputeIndexOf(rootElement);
      MOZ_ASSERT(rootIndex.isSome());
      const Maybe<uint32_t> insertIndex = aParent->ComputeIndexOf(aRefChild);

      // Now we're OK if and only if insertIndex <= rootIndex.  Indeed, either
      // we end up replacing aRefChild or we end up before it.  Either one is
      // ok as long as aRefChild is not after rootElement.
      if (MOZ_LIKELY(insertIndex.isSome()) && *insertIndex > *rootIndex) {
        aRv.ThrowHierarchyRequestError(
            "Cannot have a DocumentType node after the root element");
      }
      return;
    }
    case nsINode::DOCUMENT_FRAGMENT_NODE: {
      // Note that for now we only allow nodes inside document fragments if
      // they're allowed inside elements.  If we ever change this to allow
      // doctype nodes in document fragments, we'll need to update this code.
      // Also, there's a version of this code in ReplaceOrInsertBefore.  If you
      // change this code, change that too.
      if (!aParent->IsDocument()) {
        // All good here
        return;
      }

      bool sawElement = false;
      for (nsIContent* child = aNewChild->GetFirstChild(); child;
           child = child->GetNextSibling()) {
        if (child->IsElement()) {
          if (sawElement) {
            // Can't put two elements into a document
            aRv.ThrowHierarchyRequestError(
                "Cannot have more than one Element child of a Document");
            return;
          }
          sawElement = true;
        }
        // If we can put this content at the right place, we might be ok;
        // if not, we bail out.
        EnsureAllowedAsChild(child, aParent, aIsReplace, aRefChild, aRv);
        if (aRv.Failed()) {
          return;
        }
      }

      // Everything in the fragment checked out ok, so we can stick it in here
      return;
    }
    default:
      /*
       * aNewChild is of invalid type.
       */
      break;
  }

  // XXXbz when can we reach this?
  aRv.ThrowHierarchyRequestError(nsPrintfCString("Cannot insert %s inside %s",
                                                 NodeTypeAsString(aNewChild),
                                                 NodeTypeAsString(aParent)));
}

// Implements
// https://dom.spec.whatwg.org/#concept-node-ensure-pre-insertion-validity
void nsINode::EnsurePreInsertionValidity(nsINode& aNewChild, nsINode* aRefChild,
                                         ErrorResult& aError) {
  EnsurePreInsertionValidity1(aError);
  if (aError.Failed()) {
    return;
  }
  EnsurePreInsertionValidity2(false, aNewChild, aRefChild, aError);
}

// Implements the parts of
// https://dom.spec.whatwg.org/#concept-node-ensure-pre-insertion-validity and
// the checks in https://dom.spec.whatwg.org/#concept-node-replace that can be
// evaluated before ever looking at the child nodes (step 1 in both cases).
void nsINode::EnsurePreInsertionValidity1(ErrorResult& aError) {
  if (!IsDocument() && !IsDocumentFragment() && !IsElement()) {
    aError.ThrowHierarchyRequestError(
        nsPrintfCString("Cannot add children to %s", NodeTypeAsString(this)));
    return;
  }
}

void nsINode::EnsurePreInsertionValidity2(bool aReplace, nsINode& aNewChild,
                                          nsINode* aRefChild,
                                          ErrorResult& aError) {
  if (aNewChild.IsRootOfNativeAnonymousSubtree()) {
    // This is anonymous content.  Don't allow its insertion
    // anywhere, since it might have UnbindFromTree calls coming
    // its way.
    aError.ThrowNotSupportedError(
        "Inserting anonymous content manually is not supported");
    return;
  }

  // Make sure that the inserted node is allowed as a child of its new parent.
  EnsureAllowedAsChild(&aNewChild, this, aReplace, aRefChild, aError);
}

nsINode* nsINode::ReplaceOrInsertBefore(bool aReplace, nsINode* aNewChild,
                                        nsINode* aRefChild,
                                        ErrorResult& aError) {
  // XXXbz I wish I could assert that nsContentUtils::IsSafeToRunScript() so we
  // could rely on scriptblockers going out of scope to actually run XBL
  // teardown, but various crud adds nodes under scriptblockers (e.g. native
  // anonymous content).  The only good news is those insertions can't trigger
  // the bad XBL cases.
  MOZ_ASSERT_IF(aReplace, aRefChild);

  // Before firing DOMNodeRemoved events, make sure this is actually an insert
  // we plan to do.
  EnsurePreInsertionValidity1(aError);
  if (aError.Failed()) {
    return nullptr;
  }

  EnsurePreInsertionValidity2(aReplace, *aNewChild, aRefChild, aError);
  if (aError.Failed()) {
    return nullptr;
  }

  uint16_t nodeType = aNewChild->NodeType();

  // Before we do anything else, fire all DOMNodeRemoved mutation events
  // We do this up front as to avoid having to deal with script running
  // at random places further down.
  // Scope firing mutation events so that we don't carry any state that
  // might be stale
  {
    nsMutationGuard guard;

    // If we're replacing, fire for node-to-be-replaced.
    // If aRefChild == aNewChild then we'll fire for it in check below
    if (aReplace && aRefChild != aNewChild) {
      nsContentUtils::MaybeFireNodeRemoved(aRefChild, this);
    }

    // If the new node already has a parent, fire for removing from old
    // parent
    if (nsCOMPtr<nsINode> oldParent = aNewChild->GetParentNode()) {
      nsContentUtils::MaybeFireNodeRemoved(aNewChild, oldParent);
    }

    // If we're inserting a fragment, fire for all the children of the
    // fragment
    if (nodeType == DOCUMENT_FRAGMENT_NODE) {
      static_cast<FragmentOrElement*>(aNewChild)->FireNodeRemovedForChildren();
    }

    if (guard.Mutated(0)) {
      // Re-check the parts of our pre-insertion validity that might depend on
      // the tree shape.
      EnsurePreInsertionValidity2(aReplace, *aNewChild, aRefChild, aError);
      if (aError.Failed()) {
        return nullptr;
      }
    }
  }

  // Record the node to insert before, if any
  nsIContent* nodeToInsertBefore;
  if (aReplace) {
    nodeToInsertBefore = aRefChild->GetNextSibling();
  } else {
    // Since aRefChild is our child, it must be an nsIContent object.
    nodeToInsertBefore = aRefChild ? aRefChild->AsContent() : nullptr;
  }
  if (nodeToInsertBefore == aNewChild) {
    // We're going to remove aNewChild from its parent, so use its next sibling
    // as the node to insert before.
    nodeToInsertBefore = nodeToInsertBefore->GetNextSibling();
  }

  Maybe<AutoTArray<nsCOMPtr<nsIContent>, 50>> fragChildren;

  // Remove the new child from the old parent if one exists
  nsIContent* newContent = aNewChild->AsContent();
  nsCOMPtr<nsINode> oldParent = newContent->GetParentNode();
  if (oldParent) {
    // Hold a strong ref to nodeToInsertBefore across the removal of newContent
    nsCOMPtr<nsINode> kungFuDeathGrip = nodeToInsertBefore;

    // Removing a child can run script, via XBL destructors.
    nsMutationGuard guard;

    // Scope for the mutation batch and scriptblocker, so they go away
    // while kungFuDeathGrip is still alive.
    {
      mozAutoDocUpdate batch(newContent->GetComposedDoc(), true);
      nsAutoMutationBatch mb(oldParent, true, true);
      // ScriptBlocker ensures previous and next stay alive.
      nsIContent* previous = aNewChild->GetPreviousSibling();
      nsIContent* next = aNewChild->GetNextSibling();
      oldParent->RemoveChildNode(aNewChild->AsContent(), true);
      if (nsAutoMutationBatch::GetCurrentBatch() == &mb) {
        mb.RemovalDone();
        mb.SetPrevSibling(previous);
        mb.SetNextSibling(next);
      }
    }

    // We expect one mutation (the removal) to have happened.
    if (guard.Mutated(1)) {
      // XBL destructors, yuck.

      // Verify that newContent has no parent.
      if (newContent->GetParentNode()) {
        aError.ThrowHierarchyRequestError(
            "New child was inserted somewhere else");
        return nullptr;
      }

      // And verify that newContent is still allowed as our child.
      if (aNewChild == aRefChild) {
        // We've already removed aRefChild.  So even if we were doing a replace,
        // now we're doing a simple insert before nodeToInsertBefore.
        EnsureAllowedAsChild(newContent, this, false, nodeToInsertBefore,
                             aError);
        if (aError.Failed()) {
          return nullptr;
        }
      } else {
        EnsureAllowedAsChild(newContent, this, aReplace, aRefChild, aError);
        if (aError.Failed()) {
          return nullptr;
        }

        // And recompute nodeToInsertBefore, just in case.
        if (aReplace) {
          nodeToInsertBefore = aRefChild->GetNextSibling();
        } else {
          nodeToInsertBefore = aRefChild ? aRefChild->AsContent() : nullptr;
        }
      }
    }
  } else if (nodeType == DOCUMENT_FRAGMENT_NODE) {
    // Make sure to remove all the fragment's kids.  We need to do this before
    // we start inserting anything, so we will run out XBL destructors and
    // binding teardown (GOD, I HATE THESE THINGS) before we insert anything
    // into the DOM.
    uint32_t count = newContent->GetChildCount();

    fragChildren.emplace();

    // Copy the children into a separate array to avoid having to deal with
    // mutations to the fragment later on here.
    fragChildren->SetCapacity(count);
    for (nsIContent* child = newContent->GetFirstChild(); child;
         child = child->GetNextSibling()) {
      NS_ASSERTION(child->GetUncomposedDoc() == nullptr,
                   "How did we get a child with a current doc?");
      fragChildren->AppendElement(child);
    }

    // Hold a strong ref to nodeToInsertBefore across the removals
    nsCOMPtr<nsINode> kungFuDeathGrip = nodeToInsertBefore;

    nsMutationGuard guard;

    // Scope for the mutation batch and scriptblocker, so they go away
    // while kungFuDeathGrip is still alive.
    {
      mozAutoDocUpdate batch(newContent->GetComposedDoc(), true);
      nsAutoMutationBatch mb(newContent, false, true);

      while (newContent->HasChildren()) {
        newContent->RemoveChildNode(newContent->GetLastChild(), true);
      }
    }

    // We expect |count| removals
    if (guard.Mutated(count)) {
      // XBL destructors, yuck.

      // Verify that nodeToInsertBefore, if non-null, is still our child.  If
      // it's not, there's no way we can do this insert sanely; just bail out.
      if (nodeToInsertBefore && nodeToInsertBefore->GetParent() != this) {
        aError.ThrowHierarchyRequestError("Don't know where to insert child");
        return nullptr;
      }

      // Verify that all the things in fragChildren have no parent.
      for (uint32_t i = 0; i < count; ++i) {
        if (fragChildren->ElementAt(i)->GetParentNode()) {
          aError.ThrowHierarchyRequestError(
              "New child was inserted somewhere else");
          return nullptr;
        }
      }

      // Note that unlike the single-element case above, none of our kids can
      // be aRefChild, so we can always pass through aReplace in the
      // EnsureAllowedAsChild checks below and don't have to worry about whether
      // recomputing nodeToInsertBefore is OK.

      // Verify that our aRefChild is still sensible
      if (aRefChild && aRefChild->GetParent() != this) {
        aError.ThrowHierarchyRequestError("Don't know where to insert child");
        return nullptr;
      }

      // Recompute nodeToInsertBefore, just in case.
      if (aReplace) {
        nodeToInsertBefore = aRefChild->GetNextSibling();
      } else {
        // If aRefChild has 'this' as a parent, it must be an nsIContent.
        nodeToInsertBefore = aRefChild ? aRefChild->AsContent() : nullptr;
      }

      // And verify that newContent is still allowed as our child.  Sadly, we
      // need to reimplement the relevant part of EnsureAllowedAsChild() because
      // now our nodes are in an array and all.  If you change this code,
      // change the code there.
      if (IsDocument()) {
        bool sawElement = false;
        for (uint32_t i = 0; i < count; ++i) {
          nsIContent* child = fragChildren->ElementAt(i);
          if (child->IsElement()) {
            if (sawElement) {
              // No good
              aError.ThrowHierarchyRequestError(
                  "Cannot have more than one Element child of a Document");
              return nullptr;
            }
            sawElement = true;
          }
          EnsureAllowedAsChild(child, this, aReplace, aRefChild, aError);
          if (aError.Failed()) {
            return nullptr;
          }
        }
      }
    }
  }

  mozAutoDocUpdate batch(GetComposedDoc(), true);
  nsAutoMutationBatch mb;

  // If we're replacing and we haven't removed aRefChild yet, do so now
  if (aReplace && aRefChild != aNewChild) {
    mb.Init(this, true, true);

    // Since aRefChild is never null in the aReplace case, we know that at
    // this point nodeToInsertBefore is the next sibling of aRefChild.
    NS_ASSERTION(aRefChild->GetNextSibling() == nodeToInsertBefore,
                 "Unexpected nodeToInsertBefore");

    nsIContent* toBeRemoved = nodeToInsertBefore
                                  ? nodeToInsertBefore->GetPreviousSibling()
                                  : GetLastChild();
    MOZ_ASSERT(toBeRemoved);

    RemoveChildNode(toBeRemoved, true);
  }

  // Move new child over to our document if needed. Do this after removing
  // it from its parent so that AdoptNode doesn't fire DOMNodeRemoved
  // DocumentType nodes are the only nodes that can have a null
  // ownerDocument according to the DOM spec, and we need to allow
  // inserting them w/o calling AdoptNode().
  Document* doc = OwnerDoc();
  if (doc != newContent->OwnerDoc() && nodeType != DOCUMENT_FRAGMENT_NODE) {
    AdoptNodeIntoOwnerDoc(this, aNewChild, aError);
    if (aError.Failed()) {
      return nullptr;
    }
  }

  /*
   * Check if we're inserting a document fragment. If we are, we need
   * to actually add its children individually (i.e. we don't add the
   * actual document fragment).
   */
  nsINode* result = aReplace ? aRefChild : aNewChild;
  if (nodeType == DOCUMENT_FRAGMENT_NODE) {
    nsAutoMutationBatch* mutationBatch = nsAutoMutationBatch::GetCurrentBatch();
    if (mutationBatch && mutationBatch != &mb) {
      mutationBatch = nullptr;
    } else if (!aReplace) {
      mb.Init(this, true, true);
      mutationBatch = nsAutoMutationBatch::GetCurrentBatch();
    }

    if (mutationBatch) {
      mutationBatch->RemovalDone();
      mutationBatch->SetPrevSibling(
          nodeToInsertBefore ? nodeToInsertBefore->GetPreviousSibling()
                             : GetLastChild());
      mutationBatch->SetNextSibling(nodeToInsertBefore);
    }

    uint32_t count = fragChildren->Length();
    if (!count) {
      return result;
    }

    bool appending = !IsDocument() && !nodeToInsertBefore;
    nsIContent* firstInsertedContent = fragChildren->ElementAt(0);

    // Iterate through the fragment's children, and insert them in the new
    // parent
    for (uint32_t i = 0; i < count; ++i) {
      // XXXbz how come no reparenting here?  That seems odd...
      // Insert the child.
      InsertChildBefore(fragChildren->ElementAt(i), nodeToInsertBefore,
                        !appending, aError);
      if (aError.Failed()) {
        // Make sure to notify on any children that we did succeed to insert
        if (appending && i != 0) {
          MutationObservers::NotifyContentAppended(
              static_cast<nsIContent*>(this), firstInsertedContent);
        }
        return nullptr;
      }
    }

    if (mutationBatch && !appending) {
      mutationBatch->NodesAdded();
    }

    // Notify and fire mutation events when appending
    if (appending) {
      MutationObservers::NotifyContentAppended(static_cast<nsIContent*>(this),
                                               firstInsertedContent);
      if (mutationBatch) {
        mutationBatch->NodesAdded();
      }
      // Optimize for the case when there are no listeners
      if (nsContentUtils::HasMutationListeners(
              doc, NS_EVENT_BITS_MUTATION_NODEINSERTED)) {
        Element::FireNodeInserted(doc, this, *fragChildren);
      }
    }
  } else {
    // Not inserting a fragment but rather a single node.

    // FIXME https://bugzilla.mozilla.org/show_bug.cgi?id=544654
    //       We need to reparent here for nodes for which the parent of their
    //       wrapper is not the wrapper for their ownerDocument (XUL elements,
    //       form controls, ...). Also applies in the fragment code above.
    if (nsAutoMutationBatch::GetCurrentBatch() == &mb) {
      mb.RemovalDone();
      mb.SetPrevSibling(nodeToInsertBefore
                            ? nodeToInsertBefore->GetPreviousSibling()
                            : GetLastChild());
      mb.SetNextSibling(nodeToInsertBefore);
    }
    InsertChildBefore(newContent, nodeToInsertBefore, true, aError);
    if (aError.Failed()) {
      return nullptr;
    }
  }

  return result;
}

void nsINode::BindObject(nsISupports* aObject) {
  nsCOMArray<nsISupports>* objects = static_cast<nsCOMArray<nsISupports>*>(
      GetProperty(nsGkAtoms::keepobjectsalive));
  if (!objects) {
    objects = new nsCOMArray<nsISupports>();
    SetProperty(nsGkAtoms::keepobjectsalive, objects,
                nsINode::DeleteProperty<nsCOMArray<nsISupports>>, true);
  }
  objects->AppendObject(aObject);
}

void nsINode::UnbindObject(nsISupports* aObject) {
  nsCOMArray<nsISupports>* objects = static_cast<nsCOMArray<nsISupports>*>(
      GetProperty(nsGkAtoms::keepobjectsalive));
  if (objects) {
    objects->RemoveObject(aObject);
  }
}

already_AddRefed<AccessibleNode> nsINode::GetAccessibleNode() {
#ifdef ACCESSIBILITY
  nsresult rv = NS_OK;

  RefPtr<AccessibleNode> anode =
      static_cast<AccessibleNode*>(GetProperty(nsGkAtoms::accessiblenode, &rv));
  if (NS_FAILED(rv)) {
    anode = new AccessibleNode(this);
    RefPtr<AccessibleNode> temp = anode;
    rv = SetProperty(nsGkAtoms::accessiblenode, temp.forget().take(),
                     nsPropertyTable::SupportsDtorFunc, true);
    if (NS_FAILED(rv)) {
      NS_WARNING("SetProperty failed");
      return nullptr;
    }
  }
  return anode.forget();
#else
  return nullptr;
#endif
}

void nsINode::AddSizeOfExcludingThis(nsWindowSizes& aSizes,
                                     size_t* aNodeSize) const {
  EventListenerManager* elm = GetExistingListenerManager();
  if (elm) {
    *aNodeSize += elm->SizeOfIncludingThis(aSizes.mState.mMallocSizeOf);
  }

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - mNodeInfo
  // - mSlots
  //
  // The following members are not measured:
  // - mParent, mNextSibling, mPreviousOrLastSibling, mFirstChild: because
  //   they're non-owning, from "exclusive ownership" point of view.
}

void nsINode::AddSizeOfIncludingThis(nsWindowSizes& aSizes,
                                     size_t* aNodeSize) const {
  *aNodeSize += aSizes.mState.mMallocSizeOf(this);
  AddSizeOfExcludingThis(aSizes, aNodeSize);
}

bool nsINode::Contains(const nsINode* aOther) const {
  if (aOther == this) {
    return true;
  }

  if (!aOther || OwnerDoc() != aOther->OwnerDoc() ||
      IsInUncomposedDoc() != aOther->IsInUncomposedDoc() ||
      !aOther->IsContent() || !HasChildren()) {
    return false;
  }

  if (IsDocument()) {
    // document.contains(aOther) returns true if aOther is in the document,
    // but is not in any anonymous subtree.
    // IsInUncomposedDoc() check is done already before this.
    return !aOther->IsInNativeAnonymousSubtree();
  }

  if (!IsElement() && !IsDocumentFragment()) {
    return false;
  }

  if (IsInShadowTree() != aOther->IsInShadowTree() ||
      IsInNativeAnonymousSubtree() != aOther->IsInNativeAnonymousSubtree()) {
    return false;
  }

  if (IsInNativeAnonymousSubtree()) {
    if (GetClosestNativeAnonymousSubtreeRoot() !=
        aOther->GetClosestNativeAnonymousSubtreeRoot()) {
      return false;
    }
  }

  if (IsInShadowTree()) {
    ShadowRoot* otherRoot = aOther->GetContainingShadow();
    if (IsShadowRoot()) {
      return otherRoot == this;
    }
    if (otherRoot != GetContainingShadow()) {
      return false;
    }
  }

  return aOther->IsInclusiveDescendantOf(this);
}

uint32_t nsINode::Length() const {
  switch (NodeType()) {
    case DOCUMENT_TYPE_NODE:
      return 0;

    case TEXT_NODE:
    case CDATA_SECTION_NODE:
    case PROCESSING_INSTRUCTION_NODE:
    case COMMENT_NODE:
      MOZ_ASSERT(IsContent());
      return AsContent()->TextLength();

    default:
      return GetChildCount();
  }
}

namespace {
class SelectorCacheKey {
 public:
  explicit SelectorCacheKey(const nsACString& aString) : mKey(aString) {
    MOZ_COUNT_CTOR(SelectorCacheKey);
  }

  nsCString mKey;
  nsExpirationState mState;

  nsExpirationState* GetExpirationState() { return &mState; }

  MOZ_COUNTED_DTOR(SelectorCacheKey)
};

class SelectorCache final : public nsExpirationTracker<SelectorCacheKey, 4> {
 public:
  using SelectorList = UniquePtr<StyleSelectorList>;
  using Table = nsTHashMap<nsCStringHashKey, SelectorList>;

  SelectorCache()
      : nsExpirationTracker<SelectorCacheKey, 4>(
            1000, "SelectorCache", GetMainThreadSerialEventTarget()) {}

  void NotifyExpired(SelectorCacheKey* aSelector) final {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aSelector);

    // There is no guarantee that this method won't be re-entered when selector
    // matching is ongoing because "memory-pressure" could be notified
    // immediately when OOM happens according to the design of
    // nsExpirationTracker. The perfect solution is to delete the |aSelector|
    // and its StyleSelectorList in mTable asynchronously. We remove these
    // objects synchronously for now because NotifyExpired() will never be
    // triggered by "memory-pressure" which is not implemented yet in the stage
    // 2 of mozalloc_handle_oom(). Once these objects are removed
    // asynchronously, we should update the warning added in
    // mozalloc_handle_oom() as well.
    RemoveObject(aSelector);
    mTable.Remove(aSelector->mKey);
    delete aSelector;
  }

  // We do not call MarkUsed because it would just slow down lookups and
  // because we're OK expiring things after a few seconds even if they're
  // being used.  Returns whether we actually had an entry for aSelector.
  //
  // If we have an entry and the selector list returned has a null
  // StyleSelectorList*, that indicates that aSelector has already been
  // parsed and is not a syntactically valid selector.
  template <typename F>
  StyleSelectorList* GetListOrInsertFrom(const nsACString& aSelector,
                                         F&& aFrom) {
    MOZ_ASSERT(NS_IsMainThread());
    return mTable.LookupOrInsertWith(aSelector, std::forward<F>(aFrom)).get();
  }

  ~SelectorCache() { AgeAllGenerations(); }

 private:
  Table mTable;
};

SelectorCache& GetSelectorCache(bool aChromeRulesEnabled) {
  static StaticAutoPtr<SelectorCache> sSelectorCache;
  static StaticAutoPtr<SelectorCache> sChromeSelectorCache;
  auto& cache = aChromeRulesEnabled ? sChromeSelectorCache : sSelectorCache;
  if (!cache) {
    cache = new SelectorCache();
    ClearOnShutdown(&cache);
  }
  return *cache;
}
}  // namespace

const StyleSelectorList* nsINode::ParseSelectorList(
    const nsACString& aSelectorString, ErrorResult& aRv) {
  Document* doc = OwnerDoc();
  const bool chromeRulesEnabled = doc->ChromeRulesEnabled();

  SelectorCache& cache = GetSelectorCache(chromeRulesEnabled);
  StyleSelectorList* list = cache.GetListOrInsertFrom(aSelectorString, [&] {
    // Note that we want to cache even if null was returned, because we
    // want to cache the "This is not a valid selector" result.
    return WrapUnique(
        Servo_SelectorList_Parse(&aSelectorString, chromeRulesEnabled));
  });

  if (!list) {
    // Invalid selector.
    aRv.ThrowSyntaxError("'"_ns + aSelectorString +
                         "' is not a valid selector"_ns);
  }

  return list;
}

// Given an id, find first element with that id under aRoot.
// If none found, return nullptr. aRoot must be in the document.
inline static Element* FindMatchingElementWithId(
    const nsAString& aId, const Element& aRoot,
    const DocumentOrShadowRoot& aContainingDocOrShadowRoot) {
  MOZ_ASSERT(aRoot.SubtreeRoot() == &aContainingDocOrShadowRoot.AsNode());
  MOZ_ASSERT(
      aRoot.IsInUncomposedDoc() || aRoot.IsInShadowTree(),
      "Don't call me if the root is not in the document or in a shadow tree");

  const nsTArray<Element*>* elements =
      aContainingDocOrShadowRoot.GetAllElementsForId(aId);
  if (!elements) {
    // Nothing to do; we're done
    return nullptr;
  }

  // XXXbz: Should we fall back to the tree walk if |elements| is long,
  // for some value of "long"?
  for (Element* element : *elements) {
    if (MOZ_UNLIKELY(element == &aRoot)) {
      continue;
    }

    if (!element->IsInclusiveDescendantOf(&aRoot)) {
      continue;
    }

    // We have an element with the right id and it's a strict descendant
    // of aRoot.
    return element;
  }

  return nullptr;
}

Element* nsINode::QuerySelector(const nsACString& aSelector,
                                ErrorResult& aResult) {
  AUTO_PROFILER_LABEL_DYNAMIC_NSCSTRING_RELEVANT_FOR_JS(
      "querySelector", LAYOUT_SelectorQuery, aSelector);

  const StyleSelectorList* list = ParseSelectorList(aSelector, aResult);
  if (!list) {
    return nullptr;
  }
  const bool useInvalidation = false;
  return const_cast<Element*>(
      Servo_SelectorList_QueryFirst(this, list, useInvalidation));
}

already_AddRefed<nsINodeList> nsINode::QuerySelectorAll(
    const nsACString& aSelector, ErrorResult& aResult) {
  AUTO_PROFILER_LABEL_DYNAMIC_NSCSTRING_RELEVANT_FOR_JS(
      "querySelectorAll", LAYOUT_SelectorQuery, aSelector);

  RefPtr<nsSimpleContentList> contentList = new nsSimpleContentList(this);
  const StyleSelectorList* list = ParseSelectorList(aSelector, aResult);
  if (!list) {
    return contentList.forget();
  }

  const bool useInvalidation = false;
  Servo_SelectorList_QueryAll(this, list, contentList.get(), useInvalidation);
  return contentList.forget();
}

Element* nsINode::GetElementById(const nsAString& aId) {
  MOZ_ASSERT(!IsShadowRoot(), "Should use the faster version");
  MOZ_ASSERT(IsElement() || IsDocumentFragment(),
             "Bogus this object for GetElementById call");
  if (IsInUncomposedDoc()) {
    MOZ_ASSERT(IsElement(), "Huh? A fragment in a document?");
    return FindMatchingElementWithId(aId, *AsElement(), *OwnerDoc());
  }

  if (ShadowRoot* containingShadow = AsContent()->GetContainingShadow()) {
    MOZ_ASSERT(IsElement(), "Huh? A fragment in a ShadowRoot?");
    return FindMatchingElementWithId(aId, *AsElement(), *containingShadow);
  }

  for (nsIContent* kid = GetFirstChild(); kid; kid = kid->GetNextNode(this)) {
    if (!kid->IsElement()) {
      continue;
    }
    nsAtom* id = kid->AsElement()->GetID();
    if (id && id->Equals(aId)) {
      return kid->AsElement();
    }
  }
  return nullptr;
}

JSObject* nsINode::WrapObject(JSContext* aCx,
                              JS::Handle<JSObject*> aGivenProto) {
  // Make sure one of these is true
  // (1) our owner document has a script handling object,
  // (2) Our owner document has had a script handling object, or has been marked
  //     to have had one,
  // (3) we are running a privileged script.
  // Event handling is possible only if (1). If (2) event handling is
  // prevented.
  // If the document has never had a script handling object, untrusted
  // scripts (3) shouldn't touch it!
  bool hasHadScriptHandlingObject = false;
  if (!OwnerDoc()->GetScriptHandlingObject(hasHadScriptHandlingObject) &&
      !hasHadScriptHandlingObject && !nsContentUtils::IsSystemCaller(aCx)) {
    Throw(aCx, NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  JS::Rooted<JSObject*> obj(aCx, WrapNode(aCx, aGivenProto));
  if (obj && ChromeOnlyAccess()) {
    MOZ_RELEASE_ASSERT(
        xpc::IsUnprivilegedJunkScope(JS::GetNonCCWObjectGlobal(obj)) ||
        xpc::IsInUAWidgetScope(obj) || xpc::AccessCheck::isChrome(obj));
  }
  return obj;
}

already_AddRefed<nsINode> nsINode::CloneNode(bool aDeep, ErrorResult& aError) {
  return Clone(aDeep, nullptr, aError);
}

nsDOMAttributeMap* nsINode::GetAttributes() {
  if (!IsElement()) {
    return nullptr;
  }
  return AsElement()->Attributes();
}

Element* nsINode::GetParentElementCrossingShadowRoot() const {
  if (!mParent) {
    return nullptr;
  }

  if (mParent->IsElement()) {
    return mParent->AsElement();
  }

  if (ShadowRoot* shadowRoot = ShadowRoot::FromNode(mParent)) {
    MOZ_ASSERT(shadowRoot->GetHost(), "ShowRoots should always have a host");
    return shadowRoot->GetHost();
  }

  return nullptr;
}

bool nsINode::HasBoxQuadsSupport(JSContext* aCx, JSObject* /* unused */) {
  return xpc::AccessCheck::isChrome(js::GetContextCompartment(aCx)) ||
         StaticPrefs::layout_css_getBoxQuads_enabled();
}

nsINode* nsINode::GetScopeChainParent() const { return nullptr; }

Element* nsINode::GetParentFlexElement() {
  if (!IsContent()) {
    return nullptr;
  }

  nsIFrame* primaryFrame = AsContent()->GetPrimaryFrame(FlushType::Frames);

  // Walk up the parent chain and pierce through any anonymous boxes
  // that might be between this frame and a possible flex parent.
  for (nsIFrame* f = primaryFrame; f; f = f->GetParent()) {
    if (f != primaryFrame && !f->Style()->IsAnonBox()) {
      // We hit a non-anonymous ancestor before finding a flex item.
      // Bail out.
      break;
    }
    if (f->IsFlexItem()) {
      return f->GetParent()->GetContent()->AsElement();
    }
  }

  return nullptr;
}

Element* nsINode::GetNearestInclusiveOpenPopover() const {
  for (auto* el : InclusiveFlatTreeAncestorsOfType<Element>()) {
    if (el->IsAutoPopover() && el->IsPopoverOpen()) {
      return el;
    }
  }
  return nullptr;
}

Element* nsINode::GetNearestInclusiveTargetPopoverForInvoker() const {
  for (auto* el : InclusiveFlatTreeAncestorsOfType<Element>()) {
    if (auto* popover = el->GetEffectiveInvokeTargetElement()) {
      if (popover->IsAutoPopover() && popover->IsPopoverOpen()) {
        return popover;
      }
    }
    if (auto* popover = el->GetEffectivePopoverTargetElement()) {
      if (popover->IsAutoPopover() && popover->IsPopoverOpen()) {
        return popover;
      }
    }
  }
  return nullptr;
}

nsGenericHTMLElement* nsINode::GetEffectiveInvokeTargetElement() const {
  if (!StaticPrefs::dom_element_invokers_enabled()) {
    return nullptr;
  }

  const auto* formControl =
      nsGenericHTMLFormControlElementWithState::FromNode(this);
  if (!formControl || formControl->IsDisabled() ||
      !formControl->IsButtonControl()) {
    return nullptr;
  }
  if (auto* popover = nsGenericHTMLElement::FromNodeOrNull(
          formControl->GetInvokeTargetElement())) {
    if (popover->GetPopoverAttributeState() != PopoverAttributeState::None) {
      return popover;
    }
  }
  return nullptr;
}

nsGenericHTMLElement* nsINode::GetEffectivePopoverTargetElement() const {
  if (!StaticPrefs::dom_element_popover_enabled()) {
    return nullptr;
  }

  const auto* formControl =
      nsGenericHTMLFormControlElementWithState::FromNode(this);
  if (!formControl || formControl->IsDisabled() ||
      !formControl->IsButtonControl()) {
    return nullptr;
  }
  if (auto* popover = nsGenericHTMLElement::FromNodeOrNull(
          formControl->GetPopoverTargetElement())) {
    if (popover->GetPopoverAttributeState() != PopoverAttributeState::None) {
      return popover;
    }
  }
  return nullptr;
}

Element* nsINode::GetTopmostClickedPopover() const {
  Element* clickedPopover = GetNearestInclusiveOpenPopover();
  Element* invokedPopover = GetNearestInclusiveTargetPopoverForInvoker();
  if (!clickedPopover) {
    return invokedPopover;
  }
  auto autoPopoverList = clickedPopover->OwnerDoc()->AutoPopoverList();
  for (Element* el : Reversed(autoPopoverList)) {
    if (el == clickedPopover || el == invokedPopover) {
      return el;
    }
  }
  return nullptr;
}

void nsINode::AddAnimationObserver(nsIAnimationObserver* aAnimationObserver) {
  AddMutationObserver(aAnimationObserver);
  OwnerDoc()->SetMayHaveAnimationObservers();
}

void nsINode::AddAnimationObserverUnlessExists(
    nsIAnimationObserver* aAnimationObserver) {
  AddMutationObserverUnlessExists(aAnimationObserver);
  OwnerDoc()->SetMayHaveAnimationObservers();
}

already_AddRefed<nsINode> nsINode::CloneAndAdopt(
    nsINode* aNode, bool aClone, bool aDeep,
    nsNodeInfoManager* aNewNodeInfoManager,
    JS::Handle<JSObject*> aReparentScope, nsINode* aParent,
    ErrorResult& aError) {
  MOZ_ASSERT((!aClone && aNewNodeInfoManager) || !aReparentScope,
             "If cloning or not getting a new nodeinfo we shouldn't rewrap");
  MOZ_ASSERT(!aParent || aNode->IsContent(),
             "Can't insert document or attribute nodes into a parent");

  // First deal with aNode and walk its attributes (and their children). Then,
  // if aDeep is true, deal with aNode's children (and recurse into their
  // attributes and children).

  nsAutoScriptBlocker scriptBlocker;

  nsNodeInfoManager* nodeInfoManager = aNewNodeInfoManager;

  // aNode.
  class NodeInfo* nodeInfo = aNode->mNodeInfo;
  RefPtr<class NodeInfo> newNodeInfo;
  if (nodeInfoManager) {
    // Don't allow importing/adopting nodes from non-privileged "scriptable"
    // documents to "non-scriptable" documents.
    Document* newDoc = nodeInfoManager->GetDocument();
    if (NS_WARN_IF(!newDoc)) {
      aError.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }
    bool hasHadScriptHandlingObject = false;
    if (!newDoc->GetScriptHandlingObject(hasHadScriptHandlingObject) &&
        !hasHadScriptHandlingObject) {
      Document* currentDoc = aNode->OwnerDoc();
      if (NS_WARN_IF(!nsContentUtils::IsChromeDoc(currentDoc) &&
                     (currentDoc->GetScriptHandlingObject(
                          hasHadScriptHandlingObject) ||
                      hasHadScriptHandlingObject))) {
        aError.Throw(NS_ERROR_UNEXPECTED);
        return nullptr;
      }
    }

    newNodeInfo = nodeInfoManager->GetNodeInfo(
        nodeInfo->NameAtom(), nodeInfo->GetPrefixAtom(),
        nodeInfo->NamespaceID(), nodeInfo->NodeType(),
        nodeInfo->GetExtraName());

    nodeInfo = newNodeInfo;
  }

  Element* elem = Element::FromNode(aNode);

  nsCOMPtr<nsINode> clone;
  if (aClone) {
    nsresult rv = aNode->Clone(nodeInfo, getter_AddRefs(clone));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aError.Throw(rv);
      return nullptr;
    }

    if (aParent) {
      // If we're cloning we need to insert the cloned children into the cloned
      // parent.
      aParent->AppendChildTo(static_cast<nsIContent*>(clone.get()),
                             /* aNotify = */ true, aError);
      if (NS_WARN_IF(aError.Failed())) {
        return nullptr;
      }
    } else if (aDeep && clone->IsDocument()) {
      // After cloning the document itself, we want to clone the children into
      // the cloned document (somewhat like cloning and importing them into the
      // cloned document).
      nodeInfoManager = clone->mNodeInfo->NodeInfoManager();
    }
  } else if (nodeInfoManager) {
    Document* oldDoc = aNode->OwnerDoc();

    DOMArena* domArenaToStore =
        !aNode->HasFlag(NODE_KEEPS_DOMARENA)
            ? aNode->NodeInfo()->NodeInfoManager()->GetArenaAllocator()
            : nullptr;

    Document* newDoc = nodeInfoManager->GetDocument();
    MOZ_ASSERT(newDoc);

    bool wasRegistered = false;
    if (elem) {
      wasRegistered = oldDoc->UnregisterActivityObserver(elem);
    }

    const bool hadProperties = aNode->HasProperties();
    if (hadProperties) {
      // NOTE: We want this to happen before NodeInfoChanged so that
      // NodeInfoChanged can use node properties normally.
      //
      // When this fails, it removes all properties for the node anyway, so no
      // extra error handling needed.
      Unused << oldDoc->PropertyTable().TransferOrRemoveAllPropertiesFor(
          aNode, newDoc->PropertyTable());
    }

    aNode->mNodeInfo.swap(newNodeInfo);
    aNode->NodeInfoChanged(oldDoc);

    MOZ_ASSERT(newDoc != oldDoc);
    if (elem) {
      // Adopted callback must be enqueued whenever a node’s
      // shadow-including inclusive descendants that is custom.
      CustomElementData* data = elem->GetCustomElementData();
      if (data && data->mState == CustomElementData::State::eCustom) {
        LifecycleCallbackArgs args;
        args.mOldDocument = oldDoc;
        args.mNewDocument = newDoc;

        nsContentUtils::EnqueueLifecycleCallback(ElementCallbackType::eAdopted,
                                                 elem, args);
      }
    }

    // XXX what if oldDoc is null, we don't know if this should be
    // registered or not! Can that really happen?
    if (wasRegistered) {
      newDoc->RegisterActivityObserver(aNode->AsElement());
    }

    if (nsPIDOMWindowInner* window = newDoc->GetInnerWindow()) {
      EventListenerManager* elm = aNode->GetExistingListenerManager();
      if (elm) {
        window->SetMutationListeners(elm->MutationListenerBits());
        if (elm->MayHaveDOMActivateListeners()) {
          window->SetHasDOMActivateEventListeners();
        }
        if (elm->MayHavePaintEventListener()) {
          window->SetHasPaintEventListeners();
        }
        if (elm->MayHaveTouchEventListener()) {
          window->SetHasTouchEventListeners();
        }
        if (elm->MayHaveMouseEnterLeaveEventListener()) {
          window->SetHasMouseEnterLeaveEventListeners();
        }
        if (elm->MayHavePointerEnterLeaveEventListener()) {
          window->SetHasPointerEnterLeaveEventListeners();
        }
        if (elm->MayHaveSelectionChangeEventListener()) {
          window->SetHasSelectionChangeEventListeners();
        }
        if (elm->MayHaveFormSelectEventListener()) {
          window->SetHasFormSelectEventListeners();
        }
        if (elm->MayHaveTransitionEventListener()) {
          window->SetHasTransitionEventListeners();
        }
        if (elm->MayHaveSMILTimeEventListener()) {
          window->SetHasSMILTimeEventListeners();
        }
      }
    }
    if (wasRegistered) {
      nsIContent* content = aNode->AsContent();
      if (auto* mediaElem = HTMLMediaElement::FromNodeOrNull(content)) {
        mediaElem->NotifyOwnerDocumentActivityChanged();
      }
      // HTMLImageElement::FromNode is insufficient since we need this for
      // <svg:image> as well.
      nsCOMPtr<nsIImageLoadingContent> imageLoadingContent =
          do_QueryInterface(aNode);
      if (imageLoadingContent) {
        auto* ilc =
            static_cast<nsImageLoadingContent*>(imageLoadingContent.get());
        ilc->NotifyOwnerDocumentActivityChanged();
      }
    }

    if (oldDoc->MayHaveDOMMutationObservers()) {
      newDoc->SetMayHaveDOMMutationObservers();
    }

    if (oldDoc->MayHaveAnimationObservers()) {
      newDoc->SetMayHaveAnimationObservers();
    }

    if (elem) {
      elem->RecompileScriptEventListeners();
    }

    if (aReparentScope) {
      AutoJSContext cx;
      JS::Rooted<JSObject*> wrapper(cx);
      if ((wrapper = aNode->GetWrapper())) {
        MOZ_ASSERT(IsDOMObject(wrapper));
        JSAutoRealm ar(cx, wrapper);
        UpdateReflectorGlobal(cx, wrapper, aError);
        if (aError.Failed()) {
          if (wasRegistered) {
            newDoc->UnregisterActivityObserver(aNode->AsElement());
          }
          if (hadProperties) {
            // NOTE: When it fails it removes all properties for the node
            // anyway, so no extra error handling needed.
            Unused << newDoc->PropertyTable().TransferOrRemoveAllPropertiesFor(
                aNode, oldDoc->PropertyTable());
          }
          aNode->mNodeInfo.swap(newNodeInfo);
          aNode->NodeInfoChanged(newDoc);
          if (wasRegistered) {
            oldDoc->RegisterActivityObserver(aNode->AsElement());
          }
          return nullptr;
        }
      }
    }

    // At this point, a new node is added to the document, and this
    // node isn't allocated by the NodeInfoManager of this document,
    // so we need to do this SetArenaAllocator logic to bypass
    // the !HasChildren() check in NodeInfoManager::Allocate.
    if (mozilla::StaticPrefs::dom_arena_allocator_enabled_AtStartup()) {
      if (!newDoc->NodeInfoManager()->HasAllocated()) {
        if (DocGroup* docGroup = newDoc->GetDocGroup()) {
          newDoc->NodeInfoManager()->SetArenaAllocator(
              docGroup->ArenaAllocator());
        }
      }

      if (domArenaToStore && newDoc->GetDocGroup() != oldDoc->GetDocGroup()) {
        nsContentUtils::AddEntryToDOMArenaTable(aNode, domArenaToStore);
      }
    }
  }

  if (aDeep && (!aClone || !aNode->IsAttr())) {
    // aNode's children.
    for (nsIContent* cloneChild = aNode->GetFirstChild(); cloneChild;
         cloneChild = cloneChild->GetNextSibling()) {
      nsCOMPtr<nsINode> child =
          CloneAndAdopt(cloneChild, aClone, true, nodeInfoManager,
                        aReparentScope, clone, aError);
      if (NS_WARN_IF(aError.Failed())) {
        return nullptr;
      }
    }
  }

  if (aDeep && aNode->IsElement()) {
    if (aClone) {
      if (nodeInfo->GetDocument()->IsStaticDocument()) {
        // Clone any animations to the node in the static document, including
        // the current timing. They will need to be paused later after the new
        // document's pres shell gets initialized.
        //
        // This needs to be done here rather than in Element::CopyInnerTo
        // because the animations clone code relies on the target (that is,
        // `clone`) being connected already.
        clone->AsElement()->CloneAnimationsFrom(*aNode->AsElement());

        // Clone the Shadow DOM
        ShadowRoot* originalShadowRoot = aNode->AsElement()->GetShadowRoot();
        if (originalShadowRoot) {
          RefPtr<ShadowRoot> newShadowRoot =
              clone->AsElement()->AttachShadowWithoutNameChecks(
                  originalShadowRoot->Mode());

          newShadowRoot->CloneInternalDataFrom(originalShadowRoot);
          for (nsIContent* origChild = originalShadowRoot->GetFirstChild();
               origChild; origChild = origChild->GetNextSibling()) {
            nsCOMPtr<nsINode> child =
                CloneAndAdopt(origChild, aClone, aDeep, nodeInfoManager,
                              aReparentScope, newShadowRoot, aError);
            if (NS_WARN_IF(aError.Failed())) {
              return nullptr;
            }
          }
        }
      }
    } else {
      if (ShadowRoot* shadowRoot = aNode->AsElement()->GetShadowRoot()) {
        nsCOMPtr<nsINode> child =
            CloneAndAdopt(shadowRoot, aClone, aDeep, nodeInfoManager,
                          aReparentScope, clone, aError);
        if (NS_WARN_IF(aError.Failed())) {
          return nullptr;
        }
      }
    }
  }

  if (aClone && aNode->IsElement() &&
      !nodeInfo->GetDocument()->IsStaticDocument()) {
    // Clone the Shadow DOM
    ShadowRoot* originalShadowRoot = aNode->AsElement()->GetShadowRoot();
    if (originalShadowRoot && originalShadowRoot->Clonable()) {
      ShadowRootInit init;
      init.mMode = originalShadowRoot->Mode();
      init.mDelegatesFocus = originalShadowRoot->DelegatesFocus();
      init.mSlotAssignment = originalShadowRoot->SlotAssignment();
      init.mClonable = true;

      RefPtr<ShadowRoot> newShadowRoot =
          clone->AsElement()->AttachShadow(init, aError);
      if (NS_WARN_IF(aError.Failed())) {
        return nullptr;
      }
      newShadowRoot->SetIsDeclarative(originalShadowRoot->IsDeclarative());

      for (nsIContent* origChild = originalShadowRoot->GetFirstChild();
           origChild; origChild = origChild->GetNextSibling()) {
        nsCOMPtr<nsINode> child =
            CloneAndAdopt(origChild, aClone, true, nodeInfoManager,
                          aReparentScope, newShadowRoot, aError);
        if (NS_WARN_IF(aError.Failed())) {
          return nullptr;
        }
      }
    }
  }

  // Cloning template element.
  if (aDeep && aClone && aNode->IsTemplateElement()) {
    DocumentFragment* origContent =
        static_cast<HTMLTemplateElement*>(aNode)->Content();
    DocumentFragment* cloneContent =
        static_cast<HTMLTemplateElement*>(clone.get())->Content();

    // Clone the children into the clone's template content owner
    // document's nodeinfo manager.
    nsNodeInfoManager* ownerNodeInfoManager =
        cloneContent->mNodeInfo->NodeInfoManager();

    for (nsIContent* cloneChild = origContent->GetFirstChild(); cloneChild;
         cloneChild = cloneChild->GetNextSibling()) {
      nsCOMPtr<nsINode> child =
          CloneAndAdopt(cloneChild, aClone, aDeep, ownerNodeInfoManager,
                        aReparentScope, cloneContent, aError);
      if (NS_WARN_IF(aError.Failed())) {
        return nullptr;
      }
    }
  }

  return clone.forget();
}

void nsINode::Adopt(nsNodeInfoManager* aNewNodeInfoManager,
                    JS::Handle<JSObject*> aReparentScope,
                    mozilla::ErrorResult& aError) {
  if (aNewNodeInfoManager) {
    Document* beforeAdoptDoc = OwnerDoc();
    Document* afterAdoptDoc = aNewNodeInfoManager->GetDocument();

    MOZ_ASSERT(beforeAdoptDoc);
    MOZ_ASSERT(afterAdoptDoc);
    MOZ_ASSERT(beforeAdoptDoc != afterAdoptDoc);

    if (afterAdoptDoc->GetDocGroup() != beforeAdoptDoc->GetDocGroup()) {
      // This is a temporary solution for Bug 1590526 to only limit
      // the restriction to chrome level documents because web extensions
      // rely on content to content node adoption.
      if (nsContentUtils::IsChromeDoc(afterAdoptDoc) ||
          nsContentUtils::IsChromeDoc(beforeAdoptDoc)) {
        return aError.ThrowSecurityError(
            "Adopting nodes across docgroups in chrome documents "
            "is unsupported");
      }
    }
  }

  // Just need to store the return value of CloneAndAdopt in a
  // temporary nsCOMPtr to make sure we release it.
  nsCOMPtr<nsINode> node = CloneAndAdopt(this, false, true, aNewNodeInfoManager,
                                         aReparentScope, nullptr, aError);

  nsMutationGuard::DidMutate();
}

already_AddRefed<nsINode> nsINode::Clone(bool aDeep,
                                         nsNodeInfoManager* aNewNodeInfoManager,
                                         ErrorResult& aError) {
  return CloneAndAdopt(this, true, aDeep, aNewNodeInfoManager, nullptr, nullptr,
                       aError);
}

void nsINode::GenerateXPath(nsAString& aResult) {
  XPathGenerator::Generate(this, aResult);
}

bool nsINode::IsApzAware() const { return IsNodeApzAware(); }

bool nsINode::IsNodeApzAwareInternal() const {
  return EventTarget::IsApzAware();
}

DocGroup* nsINode::GetDocGroup() const { return OwnerDoc()->GetDocGroup(); }

nsINode* nsINode::GetFlattenedTreeParentNodeNonInline() const {
  return GetFlattenedTreeParentNode();
}

ParentObject nsINode::GetParentObject() const {
  ParentObject p(OwnerDoc());
  // Note that mReflectionScope is a no-op for chrome, and other places where we
  // don't check this value.
  if (IsInNativeAnonymousSubtree()) {
    if (ShouldUseUAWidgetScope(this)) {
      p.mReflectionScope = ReflectionScope::UAWidget;
    } else {
      MOZ_ASSERT(ShouldUseNACScope(this));
      p.mReflectionScope = ReflectionScope::NAC;
    }
  } else {
    MOZ_ASSERT(!ShouldUseNACScope(this));
    MOZ_ASSERT(!ShouldUseUAWidgetScope(this));
  }
  return p;
}

void nsINode::AddMutationObserver(
    nsMultiMutationObserver* aMultiMutationObserver) {
  if (aMultiMutationObserver) {
    NS_ASSERTION(!aMultiMutationObserver->ContainsNode(this),
                 "Observer already in the list");
    aMultiMutationObserver->AddMutationObserverToNode(this);
  }
}

void nsINode::AddMutationObserverUnlessExists(
    nsMultiMutationObserver* aMultiMutationObserver) {
  if (aMultiMutationObserver && !aMultiMutationObserver->ContainsNode(this)) {
    aMultiMutationObserver->AddMutationObserverToNode(this);
  }
}

void nsINode::RemoveMutationObserver(
    nsMultiMutationObserver* aMultiMutationObserver) {
  if (aMultiMutationObserver) {
    aMultiMutationObserver->RemoveMutationObserverFromNode(this);
  }
}

void nsINode::FireNodeRemovedForChildren() {
  Document* doc = OwnerDoc();
  // Optimize the common case
  if (!nsContentUtils::HasMutationListeners(
          doc, NS_EVENT_BITS_MUTATION_NODEREMOVED)) {
    return;
  }

  nsCOMPtr<nsINode> child;
  for (child = GetFirstChild(); child && child->GetParentNode() == this;
       child = child->GetNextSibling()) {
    nsContentUtils::MaybeFireNodeRemoved(child, this);
  }
}

ShadowRoot* nsINode::GetShadowRoot() const {
  return IsContent() ? AsContent()->GetShadowRoot() : nullptr;
}

ShadowRoot* nsINode::GetShadowRootForSelection() const {
  if (!StaticPrefs::dom_shadowdom_selection_across_boundary_enabled()) {
    return nullptr;
  }

  ShadowRoot* shadowRoot = GetShadowRoot();
  if (!shadowRoot) {
    return nullptr;
  }

  // ie. <details> and <video>
  if (shadowRoot->IsUAWidget()) {
    return nullptr;
  }

  // ie. <use> element
  if (IsElement() && !AsElement()->CanAttachShadowDOM()) {
    return nullptr;
  }

  return shadowRoot;
}

NS_IMPL_ISUPPORTS(nsNodeWeakReference, nsIWeakReference)

nsNodeWeakReference::nsNodeWeakReference(nsINode* aNode)
    : nsIWeakReference(aNode) {}

nsNodeWeakReference::~nsNodeWeakReference() {
  nsINode* node = static_cast<nsINode*>(mObject);

  if (node) {
    NS_ASSERTION(node->Slots()->mWeakReference == this,
                 "Weak reference has wrong value");
    node->Slots()->mWeakReference = nullptr;
  }
}

NS_IMETHODIMP
nsNodeWeakReference::QueryReferentFromScript(const nsIID& aIID,
                                             void** aInstancePtr) {
  return QueryReferent(aIID, aInstancePtr);
}

size_t nsNodeWeakReference::SizeOfOnlyThis(
    mozilla::MallocSizeOf aMallocSizeOf) {
  return aMallocSizeOf(this);
}
