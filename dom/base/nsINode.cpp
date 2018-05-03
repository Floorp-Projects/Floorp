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
#include "mozAutoDocUpdate.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/CORSMode.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/HTMLEditor.h"
#include "mozilla/InternalMutationEvent.h"
#include "mozilla/Likely.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TextEditor.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/dom/CharacterData.h"
#include "mozilla/dom/DocumentType.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/L10nUtilsBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/ShadowRoot.h"
#include "nsAttrValueOrString.h"
#include "nsBindingManager.h"
#include "nsCCUncollectableMarker.h"
#include "nsContentCreatorFunctions.h"
#include "nsContentList.h"
#include "nsContentUtils.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDocument.h"
#include "mozilla/dom/Attr.h"
#include "nsDOMAttributeMap.h"
#include "nsDOMCID.h"
#include "nsDOMCSSAttrDeclaration.h"
#include "nsError.h"
#include "nsDOMMutationObserver.h"
#include "nsDOMString.h"
#include "nsDOMTokenList.h"
#include "nsFocusManager.h"
#include "nsFrameSelection.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsIAnonymousContentCreator.h"
#include "nsAtom.h"
#include "nsIBaseWindow.h"
#include "nsICategoryManager.h"
#include "nsIContentIterator.h"
#include "nsIControllers.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMEventListener.h"
#include "nsILinkHandler.h"
#include "mozilla/dom/NodeInfo.h"
#include "mozilla/dom/NodeInfoInlines.h"
#include "nsIPresShell.h"
#include "nsIScriptError.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptSecurityManager.h"
#include "nsIScrollableFrame.h"
#include "nsIServiceManager.h"
#include "nsIURL.h"
#include "nsView.h"
#include "nsViewManager.h"
#include "nsIWebNavigation.h"
#include "nsIWidget.h"
#include "nsLayoutUtils.h"
#include "nsNameSpaceManager.h"
#include "nsNodeInfoManager.h"
#include "nsNodeUtils.h"
#include "nsPIBoxObject.h"
#include "nsPIDOMWindow.h"
#include "nsPresContext.h"
#include "nsString.h"
#include "nsStyleConsts.h"
#include "nsSVGUtils.h"
#include "nsTextNode.h"
#include "nsUnicharUtils.h"
#include "nsXBLBinding.h"
#include "nsXBLPrototypeBinding.h"
#include "mozilla/Preferences.h"
#include "xpcpublic.h"
#include "HTMLLegendElement.h"
#include "nsWrapperCacheInlines.h"
#include "WrapperFactory.h"
#include <algorithm>
#include "nsGlobalWindow.h"
#include "nsDOMMutationObserver.h"
#include "GeometryUtils.h"
#include "nsIAnimationObserver.h"
#include "nsChildContentList.h"
#include "mozilla/dom/NodeBinding.h"
#include "mozilla/dom/BindingDeclarations.h"

#include "XPathGenerator.h"

#ifdef ACCESSIBILITY
#include "mozilla/dom/AccessibleNode.h"
#endif

using namespace mozilla;
using namespace mozilla::dom;

nsINode::nsSlots::nsSlots()
  : mWeakReference(nullptr),
    mEditableDescendantCount(0)
{
}

nsINode::nsSlots::~nsSlots()
{
  if (mChildNodes) {
    mChildNodes->DropReference();
  }

  if (mWeakReference) {
    mWeakReference->NoticeNodeDestruction();
  }
}

void
nsINode::nsSlots::Traverse(nsCycleCollectionTraversalCallback &cb)
{
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mSlots->mChildNodes");
  cb.NoteXPCOMChild(mChildNodes);
}

void
nsINode::nsSlots::Unlink()
{
  if (mChildNodes) {
    mChildNodes->DropReference();
  }
}

//----------------------------------------------------------------------

nsINode::~nsINode()
{
  MOZ_ASSERT(!HasSlots(), "nsNodeUtils::LastRelease was not called?");
  MOZ_ASSERT(mSubtreeRoot == this, "Didn't restore state properly?");
}

void*
nsINode::GetProperty(nsAtom* aPropertyName, nsresult* aStatus) const
{
  if (!HasProperties()) { // a fast HasFlag() test
    if (aStatus) {
      *aStatus = NS_PROPTABLE_PROP_NOT_THERE;
    }
    return nullptr;
  }
  return OwnerDoc()->PropertyTable().GetProperty(this, aPropertyName, aStatus);
}

nsresult
nsINode::SetProperty(nsAtom* aPropertyName,
                     void* aValue,
                     NSPropertyDtorFunc aDtor,
                     bool aTransfer)
{
  nsresult rv = OwnerDoc()->PropertyTable().SetProperty(this,
                                                        aPropertyName,
                                                        aValue,
                                                        aDtor,
                                                        nullptr,
                                                        aTransfer);
  if (NS_SUCCEEDED(rv)) {
    SetFlags(NODE_HAS_PROPERTIES);
  }

  return rv;
}

void
nsINode::DeleteProperty(nsAtom* aPropertyName)
{
  OwnerDoc()->PropertyTable().DeleteProperty(this, aPropertyName);
}

void*
nsINode::UnsetProperty(nsAtom* aPropertyName, nsresult* aStatus)
{
  return OwnerDoc()->PropertyTable().UnsetProperty(this, aPropertyName, aStatus);
}

nsINode::nsSlots*
nsINode::CreateSlots()
{
  return new nsSlots();
}

bool
nsINode::IsEditable() const
{
  if (HasFlag(NODE_IS_EDITABLE)) {
    // The node is in an editable contentEditable subtree.
    return true;
  }

  nsIDocument *doc = GetUncomposedDoc();

  // Check if the node is in a document and the document is in designMode.
  return doc && doc->HasFlag(NODE_IS_EDITABLE);
}

nsIContent*
nsINode::GetTextEditorRootContent(TextEditor** aTextEditor)
{
  if (aTextEditor) {
    *aTextEditor = nullptr;
  }
  for (nsINode* node = this; node; node = node->GetParentNode()) {
    if (!node->IsElement() ||
        !node->IsHTMLElement())
      continue;

    RefPtr<TextEditor> textEditor =
      static_cast<nsGenericHTMLElement*>(node)->GetTextEditorInternal();
    if (!textEditor) {
      continue;
    }

    MOZ_ASSERT(!textEditor->AsHTMLEditor(),
               "If it were an HTML editor, needs to use GetRootElement()");
    Element* rootElement = textEditor->GetRoot();
    if (aTextEditor) {
      textEditor.forget(aTextEditor);
    }
    return rootElement;
  }
  return nullptr;
}

nsINode* nsINode::GetRootNode(const GetRootNodeOptions& aOptions)
{
  if (aOptions.mComposed) {
    if (IsInComposedDoc() && GetComposedDoc()) {
      return OwnerDoc();
    }

    nsINode* node = this;
    while(node) {
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

nsINode*
nsINode::GetParentOrHostNode() const
{
  if (mParent) {
    return mParent;
  }

  const ShadowRoot* shadowRoot = ShadowRoot::FromNode(this);
  return shadowRoot ? shadowRoot->GetHost() : nullptr;
}

nsINode*
nsINode::SubtreeRoot() const
{
  // There are four cases of interest here.  nsINodes that are really:
  // 1. nsIDocument nodes - Are always in the document.
  // 2.a nsIContent nodes not in a shadow tree - Are either in the document,
  //     or mSubtreeRoot is updated in BindToTree/UnbindFromTree.
  // 2.b nsIContent nodes in a shadow tree - Are never in the document,
  //     ignore mSubtreeRoot and return the containing shadow root.
  // 4. nsIAttribute nodes - Are never in the document, and mSubtreeRoot
  //    is always 'this' (as set in nsINode's ctor).
  nsINode* node;
  if (IsInUncomposedDoc()) {
    node = OwnerDocAsNode();
  } else if (IsContent()) {
    ShadowRoot* containingShadow = AsContent()->GetContainingShadow();
    node = containingShadow ? containingShadow : mSubtreeRoot;
  } else {
    node = mSubtreeRoot;
  }
  NS_ASSERTION(node, "Should always have a node here!");
#ifdef DEBUG
  {
    const nsINode* slowNode = this;
    const nsINode* iter = slowNode;
    while ((iter = iter->GetParentNode())) {
      slowNode = iter;
    }

    NS_ASSERTION(slowNode == node, "These should always be in sync!");
  }
#endif
  return node;
}

static nsIContent* GetRootForContentSubtree(nsIContent* aContent)
{
  NS_ENSURE_TRUE(aContent, nullptr);

  // Special case for ShadowRoot because the ShadowRoot itself is
  // the root. This is necessary to prevent selection from crossing
  // the ShadowRoot boundary.
  ShadowRoot* containingShadow = aContent->GetContainingShadow();
  if (containingShadow) {
    return containingShadow;
  }

  nsIContent* stop = aContent->GetBindingParent();
  while (aContent) {
    nsIContent* parent = aContent->GetParent();
    if (parent == stop) {
      break;
    }
    aContent = parent;
  }
  return aContent;
}

nsIContent*
nsINode::GetSelectionRootContent(nsIPresShell* aPresShell)
{
  NS_ENSURE_TRUE(aPresShell, nullptr);

  if (IsDocument())
    return AsDocument()->GetRootElement();
  if (!IsContent())
    return nullptr;

  if (GetComposedDoc() != aPresShell->GetDocument()) {
    return nullptr;
  }

  if (static_cast<nsIContent*>(this)->HasIndependentSelection()) {
    // This node should be a descendant of input/textarea editor.
    nsIContent* content = GetTextEditorRootContent();
    if (content)
      return content;
  }

  nsPresContext* presContext = aPresShell->GetPresContext();
  if (presContext) {
    HTMLEditor* htmlEditor = nsContentUtils::GetHTMLEditor(presContext);
    if (htmlEditor) {
      // This node is in HTML editor.
      nsIDocument* doc = GetComposedDoc();
      if (!doc || doc->HasFlag(NODE_IS_EDITABLE) ||
          !HasFlag(NODE_IS_EDITABLE)) {
        nsIContent* editorRoot = htmlEditor->GetRoot();
        NS_ENSURE_TRUE(editorRoot, nullptr);
        return nsContentUtils::IsInSameAnonymousTree(this, editorRoot) ?
                 editorRoot :
                 GetRootForContentSubtree(static_cast<nsIContent*>(this));
      }
      // If the document isn't editable but this is editable, this is in
      // contenteditable.  Use the editing host element for selection root.
      return static_cast<nsIContent*>(this)->GetEditingHost();
    }
  }

  RefPtr<nsFrameSelection> fs = aPresShell->FrameSelection();
  nsIContent* content = fs->GetLimiter();
  if (!content) {
    content = fs->GetAncestorLimiter();
    if (!content) {
      nsIDocument* doc = aPresShell->GetDocument();
      NS_ENSURE_TRUE(doc, nullptr);
      content = doc->GetRootElement();
      if (!content)
        return nullptr;
    }
  }

  // This node might be in another subtree, if so, we should find this subtree's
  // root.  Otherwise, we can return the content simply.
  NS_ENSURE_TRUE(content, nullptr);
  if (!nsContentUtils::IsInSameAnonymousTree(this, content)) {
    content = GetRootForContentSubtree(static_cast<nsIContent*>(this));
    // Fixup for ShadowRoot because the ShadowRoot itself does not have a frame.
    // Use the host as the root.
    if (ShadowRoot* shadowRoot = ShadowRoot::FromNode(content)) {
      content = shadowRoot->GetHost();
    }
  }

  return content;
}

nsINodeList*
nsINode::ChildNodes()
{
  nsSlots* slots = Slots();
  if (!slots->mChildNodes) {
    slots->mChildNodes = IsAttr()
      ? new nsAttrChildContentList(this)
      : new nsParentNodeChildContentList(this);
  }

  return slots->mChildNodes;
}

void
nsINode::InvalidateChildNodes()
{
  MOZ_ASSERT(!IsAttr());

  nsSlots* slots = GetExistingSlots();
  if (!slots || !slots->mChildNodes) {
    return;
  }

  auto childNodes =
    static_cast<nsParentNodeChildContentList*>(slots->mChildNodes.get());
  childNodes->InvalidateCache();
}

void
nsINode::GetTextContentInternal(nsAString& aTextContent, OOMReporter& aError)
{
  SetDOMStringToNull(aTextContent);
}

nsIDocument*
nsINode::GetComposedDocInternal() const
{
  MOZ_ASSERT(HasFlag(NODE_IS_IN_SHADOW_TREE) && IsContent(),
             "Should only be caled on nodes in the shadow tree.");

  ShadowRoot* containingShadow = AsContent()->GetContainingShadow();
  return containingShadow && containingShadow->IsComposedDocParticipant() ?
    OwnerDoc() : nullptr;
}

#ifdef DEBUG
void
nsINode::CheckNotNativeAnonymous() const
{
  if (!IsContent())
    return;
  nsIContent* content = static_cast<const nsIContent *>(this)->GetBindingParent();
  while (content) {
    if (content->IsRootOfNativeAnonymousSubtree()) {
      NS_ERROR("Element not marked to be in native anonymous subtree!");
      break;
    }
    content = content->GetBindingParent();
  }
}
#endif

bool
nsINode::IsInAnonymousSubtree() const
{
  if (!IsContent()) {
    return false;
  }

  return AsContent()->IsInAnonymousSubtree();
}

std::ostream&
operator<<(std::ostream& aStream, const nsINode& aNode)
{
  nsAutoString elemDesc;
  const nsINode* curr = &aNode;
  while (curr) {
    const nsString& localName = curr->LocalName();
    nsString id;
    if (curr->IsElement()) {
      curr->AsElement()->GetId(id);
    }

    if (!elemDesc.IsEmpty()) {
      elemDesc = elemDesc + NS_LITERAL_STRING(".");
    }

    elemDesc = elemDesc + localName;

    if (!id.IsEmpty()) {
      elemDesc = elemDesc + NS_LITERAL_STRING("['") + id +
                 NS_LITERAL_STRING("']");
    }

    curr = curr->GetParentNode();
  }

  NS_ConvertUTF16toUTF8 str(elemDesc);
  return aStream << str.get();
}

bool
nsINode::IsAnonymousContentInSVGUseSubtree() const
{
  MOZ_ASSERT(IsInAnonymousSubtree());
  nsIContent* parent = AsContent()->GetBindingParent();
  // Watch out for parentless native-anonymous subtrees.
  return parent && parent->IsSVGElement(nsGkAtoms::use);
}

void
nsINode::GetNodeValueInternal(nsAString& aNodeValue)
{
  SetDOMStringToNull(aNodeValue);
}

nsINode*
nsINode::RemoveChild(nsINode& aOldChild, ErrorResult& aError)
{
  if (IsCharacterData()) {
    // aOldChild can't be one of our children.
    aError.Throw(NS_ERROR_DOM_NOT_FOUND_ERR);
    return nullptr;
  }

  if (aOldChild.GetParentNode() == this) {
    nsContentUtils::MaybeFireNodeRemoved(&aOldChild, this, OwnerDoc());
  }

  int32_t index = ComputeIndexOf(&aOldChild);
  if (index == -1) {
    // aOldChild isn't one of our children.
    aError.Throw(NS_ERROR_DOM_NOT_FOUND_ERR);
    return nullptr;
  }

  RemoveChildAt_Deprecated(index, true);
  return &aOldChild;
}

void
nsINode::Normalize()
{
  // First collect list of nodes to be removed
  AutoTArray<nsCOMPtr<nsIContent>, 50> nodes;

  bool canMerge = false;
  for (nsIContent* node = this->GetFirstChild();
       node;
       node = node->GetNextNode(this)) {
    if (node->NodeType() != TEXT_NODE) {
      canMerge = false;
      continue;
    }

    if (canMerge || node->TextLength() == 0) {
      // No need to touch canMerge. That way we can merge across empty
      // textnodes if and only if the node before is a textnode
      nodes.AppendElement(node);
    }
    else {
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
  nsIDocument* doc = OwnerDoc();

  // Batch possible DOMSubtreeModified events.
  mozAutoSubtreeModified subtree(doc, nullptr);

  // Fire all DOMNodeRemoved events. Optimize the common case of there being
  // no listeners
  bool hasRemoveListeners = nsContentUtils::
      HasMutationListeners(doc, NS_EVENT_BITS_MUTATION_NODEREMOVED);
  if (hasRemoveListeners) {
    for (uint32_t i = 0; i < nodes.Length(); ++i) {
      nsINode* parentNode = nodes[i]->GetParentNode();
      if (parentNode) { // Node may have already been removed.
        nsContentUtils::MaybeFireNodeRemoved(nodes[i], parentNode,
                                             doc);
      }
    }
  }

  mozAutoDocUpdate batch(doc, UPDATE_CONTENT_MODEL, true);

  // Merge and remove all nodes
  nsAutoString tmpStr;
  for (uint32_t i = 0; i < nodes.Length(); ++i) {
    nsIContent* node = nodes[i];
    // Merge with previous node unless empty
    const nsTextFragment* text = node->GetText();
    if (text->GetLength()) {
      nsIContent* target = node->GetPreviousSibling();
      NS_ASSERTION((target && target->NodeType() == TEXT_NODE) ||
                   hasRemoveListeners,
                   "Should always have a previous text sibling unless "
                   "mutation events messed us up");
      if (!hasRemoveListeners ||
          (target && target->NodeType() == TEXT_NODE)) {
        nsTextNode* t = static_cast<nsTextNode*>(target);
        if (text->Is2b()) {
          t->AppendTextForNormalize(text->Get2b(), text->GetLength(), true, node);
        }
        else {
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
      parent->RemoveChildAt_Deprecated(parent->ComputeIndexOf(node), true);
    }
  }
}

nsresult
nsINode::GetBaseURI(nsAString &aURI) const
{
  nsCOMPtr<nsIURI> baseURI = GetBaseURI();

  nsAutoCString spec;
  if (baseURI) {
    nsresult rv = baseURI->GetSpec(spec);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  CopyUTF8toUTF16(spec, aURI);
  return NS_OK;
}

void
nsINode::GetBaseURIFromJS(nsAString& aURI,
                          CallerType aCallerType,
                          ErrorResult& aRv) const
{
  nsCOMPtr<nsIURI> baseURI = GetBaseURI(aCallerType == CallerType::System);
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

already_AddRefed<nsIURI>
nsINode::GetBaseURIObject() const
{
  return GetBaseURI(true);
}

void
nsINode::LookupPrefix(const nsAString& aNamespaceURI, nsAString& aPrefix)
{
  Element *element = GetNameSpaceElement();
  if (element) {
    // XXX Waiting for DOM spec to list error codes.

    // Trace up the content parent chain looking for the namespace
    // declaration that defines the aNamespaceURI namespace. Once found,
    // return the prefix (i.e. the attribute localName).
    for (nsIContent* content = element; content;
         content = content->GetParent()) {
      if (!content->IsElement()) {
        continue;
      }

      Element* element = content->AsElement();
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
          }
          else {
            SetDOMStringToNull(aPrefix);
          }
          return;
        }
      }
    }
  }

  SetDOMStringToNull(aPrefix);
}

uint16_t
nsINode::CompareDocumentPosition(nsINode& aOtherNode) const
{
  if (this == &aOtherNode) {
    return 0;
  }
  if (GetPreviousSibling() == &aOtherNode) {
    MOZ_ASSERT(GetParentNode() == aOtherNode.GetParentNode());
    return NodeBinding::DOCUMENT_POSITION_PRECEDING;
  }
  if (GetNextSibling() == &aOtherNode) {
    MOZ_ASSERT(GetParentNode() == aOtherNode.GetParentNode());
    return NodeBinding::DOCUMENT_POSITION_FOLLOWING;
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
          return NodeBinding::DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC |
            NodeBinding::DOCUMENT_POSITION_PRECEDING;
        }
        if (attrName->Equals(attr2->NodeInfo())) {
          return NodeBinding::DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC |
            NodeBinding::DOCUMENT_POSITION_FOLLOWING;
        }
      }
      NS_NOTREACHED("neither attribute in the element");
      return NodeBinding::DOCUMENT_POSITION_DISCONNECTED;
    }

    if (elem) {
      node2 = elem;
      parents2.AppendElement(attr2);
    }
  }

  // We now know that both nodes are either nsIContents or nsIDocuments.
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
    return top1 < top2 ?
      (NodeBinding::DOCUMENT_POSITION_PRECEDING |
       NodeBinding::DOCUMENT_POSITION_DISCONNECTED |
       NodeBinding::DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC) :
      (NodeBinding::DOCUMENT_POSITION_FOLLOWING |
       NodeBinding::DOCUMENT_POSITION_DISCONNECTED |
       NodeBinding::DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC);
  }

  // Find where the parent chain differs and check indices in the parent.
  const nsINode* parent = top1;
  uint32_t len;
  for (len = std::min(pos1, pos2); len > 0; --len) {
    const nsINode* child1 = parents1.ElementAt(--pos1);
    const nsINode* child2 = parents2.ElementAt(--pos2);
    if (child1 != child2) {
      // child1 or child2 can be an attribute here. This will work fine since
      // ComputeIndexOf will return -1 for the attribute making the
      // attribute be considered before any child.
      return parent->ComputeIndexOf(child1) < parent->ComputeIndexOf(child2) ?
        NodeBinding::DOCUMENT_POSITION_PRECEDING :
        NodeBinding::DOCUMENT_POSITION_FOLLOWING;
    }
    parent = child1;
  }

  // We hit the end of one of the parent chains without finding a difference
  // between the chains. That must mean that one node is an ancestor of the
  // other. The one with the shortest chain must be the ancestor.
  return pos1 < pos2 ?
    (NodeBinding::DOCUMENT_POSITION_PRECEDING |
     NodeBinding::DOCUMENT_POSITION_CONTAINS) :
    (NodeBinding::DOCUMENT_POSITION_FOLLOWING |
     NodeBinding::DOCUMENT_POSITION_CONTAINED_BY);
}

bool
nsINode::IsSameNode(nsINode *other)
{
  return other == this;
}

bool
nsINode::IsEqualNode(nsINode* aOther)
{
  if (!aOther) {
    return false;
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

    switch(nodeType) {
      case ELEMENT_NODE:
      {
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
                                     attrName->LocalName(),
                                     string1,
                                     eCaseMatters)) {
            return false;
          }
        }
        break;
      }
      case TEXT_NODE:
      case COMMENT_NODE:
      case CDATA_SECTION_NODE:
      case PROCESSING_INSTRUCTION_NODE:
      {
        MOZ_ASSERT(node1->IsCharacterData());
        MOZ_ASSERT(node2->IsCharacterData());
        string1.Truncate();
        static_cast<CharacterData*>(node1)->AppendTextTo(string1);
        string2.Truncate();
        static_cast<CharacterData*>(node2)->AppendTextTo(string2);

        if (!string1.Equals(string2)) {
          return false;
        }

        break;
      }
      case DOCUMENT_NODE:
      case DOCUMENT_FRAGMENT_NODE:
        break;
      case ATTRIBUTE_NODE:
      {
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
      case DOCUMENT_TYPE_NODE:
      {
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
    }
    else {
      if (node2->GetFirstChild()) {
        // node2 has a firstChild, but node1 doesn't
        return false;
      }

      // Find next sibling, possibly walking parent chain.
      while (1) {
        if (node1 == this) {
          NS_ASSERTION(node2 == aOther, "Should have reached the start node "
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
  } while(node2);

  return false;
}

void
nsINode::LookupNamespaceURI(const nsAString& aNamespacePrefix,
                            nsAString& aNamespaceURI)
{
  Element *element = GetNameSpaceElement();
  if (!element ||
      NS_FAILED(element->LookupNamespaceURIInternal(aNamespacePrefix,
                                                    aNamespaceURI))) {
    SetDOMStringToNull(aNamespaceURI);
  }
}

bool
nsINode::ComputeDefaultWantsUntrusted(ErrorResult& aRv)
{
  return !nsContentUtils::IsChromeDoc(OwnerDoc());
}

void
nsINode::GetBoxQuads(const BoxQuadOptions& aOptions,
                     nsTArray<RefPtr<DOMQuad> >& aResult,
                     CallerType aCallerType,
                     mozilla::ErrorResult& aRv)
{
  mozilla::GetBoxQuads(this, aOptions, aResult, aCallerType, aRv);
}

already_AddRefed<DOMQuad>
nsINode::ConvertQuadFromNode(DOMQuad& aQuad,
                             const GeometryNode& aFrom,
                             const ConvertCoordinateOptions& aOptions,
                             CallerType aCallerType,
                             ErrorResult& aRv)
{
  return mozilla::ConvertQuadFromNode(this, aQuad, aFrom, aOptions, aCallerType,
                                      aRv);
}

already_AddRefed<DOMQuad>
nsINode::ConvertRectFromNode(DOMRectReadOnly& aRect,
                             const GeometryNode& aFrom,
                             const ConvertCoordinateOptions& aOptions,
                             CallerType aCallerType,
                             ErrorResult& aRv)
{
  return mozilla::ConvertRectFromNode(this, aRect, aFrom, aOptions, aCallerType,
                                      aRv);
}

already_AddRefed<DOMPoint>
nsINode::ConvertPointFromNode(const DOMPointInit& aPoint,
                              const GeometryNode& aFrom,
                              const ConvertCoordinateOptions& aOptions,
                              CallerType aCallerType,
                              ErrorResult& aRv)
{
  return mozilla::ConvertPointFromNode(this, aPoint, aFrom, aOptions,
                                       aCallerType, aRv);
}

bool
nsINode::DispatchEvent(Event& aEvent, CallerType aCallerType, ErrorResult& aRv)
{
  // XXX sXBL/XBL2 issue -- do we really want the owner here?  What
  // if that's the XBL document?  Would we want its presshell?  Or what?
  nsCOMPtr<nsIDocument> document = OwnerDoc();

  // Do nothing if the element does not belong to a document
  if (!document) {
    return true;
  }

  // Obtain a presentation shell
  RefPtr<nsPresContext> context = document->GetPresContext();

  nsEventStatus status = nsEventStatus_eIgnore;
  nsresult rv =
    EventDispatcher::DispatchDOMEvent(this, nullptr, &aEvent, context, &status);
  bool retval = !aEvent.DefaultPrevented(aCallerType);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
  return retval;
}

nsresult
nsINode::PostHandleEvent(EventChainPostVisitor& /*aVisitor*/)
{
  return NS_OK;
}

EventListenerManager*
nsINode::GetOrCreateListenerManager()
{
  return nsContentUtils::GetListenerManagerForNode(this);
}

EventListenerManager*
nsINode::GetExistingListenerManager() const
{
  return nsContentUtils::GetExistingListenerManagerForNode(this);
}

nsPIDOMWindowOuter*
nsINode::GetOwnerGlobalForBindings()
{
  bool dummy;
  auto* window = static_cast<nsGlobalWindowInner*>(OwnerDoc()->GetScriptHandlingObject(dummy));
  return window ? nsPIDOMWindowOuter::GetFromCurrentInner(window->AsInner()) : nullptr;
}

nsIGlobalObject*
nsINode::GetOwnerGlobal() const
{
  bool dummy;
  return OwnerDoc()->GetScriptHandlingObject(dummy);
}

void
nsINode::ChangeEditableDescendantCount(int32_t aDelta)
{
  if (aDelta == 0) {
    return;
  }

  nsSlots* s = Slots();
  MOZ_ASSERT(aDelta > 0 ||
             s->mEditableDescendantCount >= (uint32_t) (-1 * aDelta));
  s->mEditableDescendantCount += aDelta;
}

void
nsINode::ResetEditableDescendantCount()
{
  nsSlots* s = GetExistingSlots();
  if (s) {
    s->mEditableDescendantCount = 0;
  }
}

uint32_t
nsINode::EditableDescendantCount()
{
  nsSlots* s = GetExistingSlots();
  if (s) {
    return s->mEditableDescendantCount;
  }
  return 0;
}

bool
nsINode::UnoptimizableCCNode() const
{
  const uintptr_t problematicFlags = (NODE_IS_ANONYMOUS_ROOT |
                                      NODE_IS_IN_NATIVE_ANONYMOUS_SUBTREE |
                                      NODE_IS_NATIVE_ANONYMOUS_ROOT |
                                      NODE_MAY_BE_IN_BINDING_MNGR |
                                      NODE_IS_IN_SHADOW_TREE);
  return HasFlag(problematicFlags) ||
         NodeType() == ATTRIBUTE_NODE ||
         // For strange cases like xbl:content/xbl:children
         (IsElement() &&
          AsElement()->IsInNamespace(kNameSpaceID_XBL));
}

/* static */
bool
nsINode::Traverse(nsINode *tmp, nsCycleCollectionTraversalCallback &cb)
{
  if (MOZ_LIKELY(!cb.WantAllTraces())) {
    nsIDocument *currentDoc = tmp->GetUncomposedDoc();
    if (currentDoc &&
        nsCCUncollectableMarker::InGeneration(currentDoc->GetMarkedCCGeneration())) {
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
          MOZ_ASSERT(parent->ComputeIndexOf(tmp) >= 0, "Parent doesn't own us?");
          return false;
        }
      }
    }
  }

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mNodeInfo)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_RAWPTR(GetParent())

  nsSlots *slots = tmp->GetExistingSlots();
  if (slots) {
    slots->Traverse(cb);
  }

  if (tmp->HasProperties()) {
    nsCOMArray<nsISupports>* objects =
      static_cast<nsCOMArray<nsISupports>*>(tmp->GetProperty(nsGkAtoms::keepobjectsalive));
    if (objects) {
      for (int32_t i = 0; i < objects->Count(); ++i) {
         cb.NoteXPCOMChild(objects->ObjectAt(i));
      }
    }
  }

  if (tmp->NodeType() != DOCUMENT_NODE &&
      tmp->HasFlag(NODE_HAS_LISTENERMANAGER)) {
    nsContentUtils::TraverseListenerManager(tmp, cb);
  }

  return true;
}

/* static */
void
nsINode::Unlink(nsINode* tmp)
{
  tmp->ReleaseWrapper(tmp);

  nsSlots *slots = tmp->GetExistingSlots();
  if (slots) {
    slots->Unlink();
  }

  if (tmp->NodeType() != DOCUMENT_NODE &&
      tmp->HasFlag(NODE_HAS_LISTENERMANAGER)) {
    nsContentUtils::RemoveListenerManager(tmp);
    tmp->UnsetFlags(NODE_HAS_LISTENERMANAGER);
  }

  if (tmp->HasProperties()) {
    tmp->DeleteProperty(nsGkAtoms::keepobjectsalive);
  }
}

static void
AdoptNodeIntoOwnerDoc(nsINode *aParent, nsINode *aNode, ErrorResult& aError)
{
  NS_ASSERTION(!aNode->GetParentNode(),
               "Should have removed from parent already");

  nsIDocument *doc = aParent->OwnerDoc();

  DebugOnly<nsINode*> adoptedNode = doc->AdoptNode(*aNode, aError);

#ifdef DEBUG
  if (!aError.Failed()) {
    MOZ_ASSERT(aParent->OwnerDoc() == doc,
               "ownerDoc chainged while adopting");
    MOZ_ASSERT(adoptedNode == aNode, "Uh, adopt node changed nodes?");
    MOZ_ASSERT(aParent->OwnerDoc() == aNode->OwnerDoc(),
               "ownerDocument changed again after adopting!");
  }
#endif // DEBUG
}

static void
CheckForOutdatedParent(nsINode* aParent, nsINode* aNode, ErrorResult& aError)
{
  if (JSObject* existingObjUnrooted = aNode->GetWrapper()) {
    JS::Rooted<JSObject*> existingObj(RootingCx(), existingObjUnrooted);

    AutoJSContext cx;
    nsIGlobalObject* global = aParent->OwnerDoc()->GetScopeObject();
    MOZ_ASSERT(global);

    if (js::GetGlobalForObjectCrossCompartment(existingObj) !=
        global->GetGlobalJSObject()) {
      JSAutoCompartment ac(cx, existingObj);
      ReparentWrapper(cx, existingObj, aError);
    }
  }
}

nsresult
nsINode::doInsertChildAt(nsIContent* aKid, uint32_t aIndex,
                         bool aNotify, nsAttrAndChildArray& aChildArray)
{
  MOZ_ASSERT(!aKid->GetParentNode(), "Inserting node that already has parent");
  MOZ_ASSERT(!IsAttr());

  // The id-handling code, and in the future possibly other code, need to
  // react to unexpected attribute changes.
  nsMutationGuard::DidMutate();

  // Do this before checking the child-count since this could cause mutations
  nsIDocument* doc = GetUncomposedDoc();
  mozAutoDocUpdate updateBatch(GetComposedDoc(), UPDATE_CONTENT_MODEL, aNotify);

  if (OwnerDoc() != aKid->OwnerDoc()) {
    ErrorResult error;
    AdoptNodeIntoOwnerDoc(this, aKid, error);

    // Need to WouldReportJSException() if our callee can throw a JS
    // exception (which it can) and we're neither propagating the
    // error out nor unconditionally suppressing it.
    error.WouldReportJSException();
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
  } else if (OwnerDoc()->DidDocumentOpen()) {
    ErrorResult error;
    CheckForOutdatedParent(this, aKid, error);

    // Need to WouldReportJSException() if our callee can throw a JS
    // exception (which it can) and we're neither propagating the
    // error out nor unconditionally suppressing it.
    error.WouldReportJSException();
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
  }

  uint32_t childCount = aChildArray.ChildCount();
  NS_ENSURE_TRUE(aIndex <= childCount, NS_ERROR_ILLEGAL_VALUE);
  bool isAppend = (aIndex == childCount);

  nsresult rv = aChildArray.InsertChildAt(aKid, aIndex);
  NS_ENSURE_SUCCESS(rv, rv);
  if (aIndex == 0) {
    mFirstChild = aKid;
  }

  nsIContent* parent = IsContent() ? AsContent() : nullptr;

  rv = aKid->BindToTree(doc, parent,
                        parent ? parent->GetBindingParent() : nullptr,
                        true);
  if (NS_FAILED(rv)) {
    if (GetFirstChild() == aKid) {
      mFirstChild = aKid->GetNextSibling();
    }
    aChildArray.RemoveChildAt(aIndex);
    aKid->UnbindFromTree();
    return rv;
  }

  // Invalidate cached array of child nodes
  InvalidateChildNodes();

  NS_ASSERTION(aKid->GetParentNode() == this,
               "Did we run script inappropriately?");

  if (aNotify) {
    // Note that we always want to call ContentInserted when things are added
    // as kids to documents
    if (parent && isAppend) {
      nsNodeUtils::ContentAppended(parent, aKid);
    } else {
      nsNodeUtils::ContentInserted(this, aKid);
    }

    if (nsContentUtils::HasMutationListeners(aKid,
          NS_EVENT_BITS_MUTATION_NODEINSERTED, this)) {
      InternalMutationEvent mutation(true, eLegacyNodeInserted);
      mutation.mRelatedNode = do_QueryInterface(this);

      mozAutoSubtreeModified subtree(OwnerDoc(), this);
      (new AsyncEventDispatcher(aKid, mutation))->RunDOMEventWhenSafe();
    }
  }

  return NS_OK;
}

Element*
nsINode::GetPreviousElementSibling() const
{
  nsIContent* previousSibling = GetPreviousSibling();
  while (previousSibling) {
    if (previousSibling->IsElement()) {
      return previousSibling->AsElement();
    }
    previousSibling = previousSibling->GetPreviousSibling();
  }

  return nullptr;
}

Element*
nsINode::GetNextElementSibling() const
{
  nsIContent* nextSibling = GetNextSibling();
  while (nextSibling) {
    if (nextSibling->IsElement()) {
      return nextSibling->AsElement();
    }
    nextSibling = nextSibling->GetNextSibling();
  }

  return nullptr;
}

static already_AddRefed<nsINode>
GetNodeFromNodeOrString(const OwningNodeOrString& aNode,
                        nsIDocument* aDocument)
{
  if (aNode.IsNode()) {
    nsCOMPtr<nsINode> node = aNode.GetAsNode();
    return node.forget();
  }

  if (aNode.IsString()){
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
static already_AddRefed<nsINode>
ConvertNodesOrStringsIntoNode(const Sequence<OwningNodeOrString>& aNodes,
                              nsIDocument* aDocument,
                              ErrorResult& aRv)
{
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

static void
InsertNodesIntoHashset(const Sequence<OwningNodeOrString>& aNodes,
                       nsTHashtable<nsPtrHashKey<nsINode>>& aHashset)
{
  for (const auto& node : aNodes) {
    if (node.IsNode()) {
      aHashset.PutEntry(node.GetAsNode());
    }
  }
}

static nsINode*
FindViablePreviousSibling(const nsINode& aNode,
                          const Sequence<OwningNodeOrString>& aNodes)
{
  nsTHashtable<nsPtrHashKey<nsINode>> nodeSet(16);
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

static nsINode*
FindViableNextSibling(const nsINode& aNode,
                      const Sequence<OwningNodeOrString>& aNodes)
{
  nsTHashtable<nsPtrHashKey<nsINode>> nodeSet(16);
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

void
nsINode::Before(const Sequence<OwningNodeOrString>& aNodes,
                ErrorResult& aRv)
{
  nsCOMPtr<nsINode> parent = GetParentNode();
  if (!parent) {
    return;
  }

  nsCOMPtr<nsINode> viablePreviousSibling =
    FindViablePreviousSibling(*this, aNodes);

  nsCOMPtr<nsINode> node =
    ConvertNodesOrStringsIntoNode(aNodes, OwnerDoc(), aRv);
  if (aRv.Failed()) {
    return;
  }

  viablePreviousSibling = viablePreviousSibling ?
    viablePreviousSibling->GetNextSibling() : parent->GetFirstChild();

  parent->InsertBefore(*node, viablePreviousSibling, aRv);
}

void
nsINode::After(const Sequence<OwningNodeOrString>& aNodes,
               ErrorResult& aRv)
{
  nsCOMPtr<nsINode> parent = GetParentNode();
  if (!parent) {
    return;
  }

  nsCOMPtr<nsINode> viableNextSibling = FindViableNextSibling(*this, aNodes);

  nsCOMPtr<nsINode> node =
    ConvertNodesOrStringsIntoNode(aNodes, OwnerDoc(), aRv);
  if (aRv.Failed()) {
    return;
  }

  parent->InsertBefore(*node, viableNextSibling, aRv);
}

void
nsINode::ReplaceWith(const Sequence<OwningNodeOrString>& aNodes,
                     ErrorResult& aRv)
{
  nsCOMPtr<nsINode> parent = GetParentNode();
  if (!parent) {
    return;
  }

  nsCOMPtr<nsINode> viableNextSibling = FindViableNextSibling(*this, aNodes);

  nsCOMPtr<nsINode> node =
    ConvertNodesOrStringsIntoNode(aNodes, OwnerDoc(), aRv);
  if (aRv.Failed()) {
    return;
  }

  if (parent == GetParentNode()) {
    parent->ReplaceChild(*node, *this, aRv);
  } else {
    parent->InsertBefore(*node, viableNextSibling, aRv);
  }
}

void
nsINode::Remove()
{
  nsCOMPtr<nsINode> parent = GetParentNode();
  if (!parent) {
    return;
  }

  parent->RemoveChild(*this, IgnoreErrors());
}

Element*
nsINode::GetFirstElementChild() const
{
  for (nsIContent* child = GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    if (child->IsElement()) {
      return child->AsElement();
    }
  }

  return nullptr;
}

Element*
nsINode::GetLastElementChild() const
{
  for (nsIContent* child = GetLastChild();
       child;
       child = child->GetPreviousSibling()) {
    if (child->IsElement()) {
      return child->AsElement();
    }
  }

  return nullptr;
}

void
nsINode::Prepend(const Sequence<OwningNodeOrString>& aNodes,
                 ErrorResult& aRv)
{
  nsCOMPtr<nsINode> node =
    ConvertNodesOrStringsIntoNode(aNodes, OwnerDoc(), aRv);
  if (aRv.Failed()) {
    return;
  }

  nsCOMPtr<nsINode> refNode = mFirstChild;
  InsertBefore(*node, refNode, aRv);
}

void
nsINode::Append(const Sequence<OwningNodeOrString>& aNodes,
                 ErrorResult& aRv)
{
  nsCOMPtr<nsINode> node =
    ConvertNodesOrStringsIntoNode(aNodes, OwnerDoc(), aRv);
  if (aRv.Failed()) {
    return;
  }

  AppendChild(*node, aRv);
}

void
nsINode::doRemoveChildAt(uint32_t aIndex, bool aNotify,
                         nsIContent* aKid, nsAttrAndChildArray& aChildArray)
{
  // NOTE: This function must not trigger any calls to
  // nsIDocument::GetRootElement() calls until *after* it has removed aKid from
  // aChildArray. Any calls before then could potentially restore a stale
  // value for our cached root element, per note in
  // nsDocument::RemoveChildNode().
  MOZ_ASSERT(aKid && aKid->GetParentNode() == this &&
             aKid == GetChildAt_Deprecated(aIndex) &&
             ComputeIndexOf(aKid) == (int32_t)aIndex, "Bogus aKid");
  MOZ_ASSERT(!IsAttr());

  nsMutationGuard::DidMutate();
  mozAutoDocUpdate updateBatch(GetComposedDoc(), UPDATE_CONTENT_MODEL, aNotify);

  nsIContent* previousSibling = aKid->GetPreviousSibling();

  if (GetFirstChild() == aKid) {
    mFirstChild = aKid->GetNextSibling();
  }

  aChildArray.RemoveChildAt(aIndex);

  // Invalidate cached array of child nodes
  InvalidateChildNodes();

  if (aNotify) {
    nsNodeUtils::ContentRemoved(this, aKid, previousSibling);
  }

  aKid->UnbindFromTree();
}

// When replacing, aRefChild is the content being replaced; when
// inserting it's the content before which we're inserting.  In the
// latter case it may be null.
static
bool IsAllowedAsChild(nsIContent* aNewChild, nsINode* aParent,
                      bool aIsReplace, nsINode* aRefChild)
{
  MOZ_ASSERT(aNewChild, "Must have new child");
  MOZ_ASSERT_IF(aIsReplace, aRefChild);
  MOZ_ASSERT(aParent);
  MOZ_ASSERT(aParent->IsDocument() ||
             aParent->IsDocumentFragment() ||
             aParent->IsElement(),
             "Nodes that are not documents, document fragments or elements "
             "can't be parents!");

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
        aNewChild->GetShadowRoot()) &&
       nsContentUtils::ContentIsHostIncludingDescendantOf(aParent,
                                                          aNewChild))) {
    return false;
  }

  // The allowed child nodes differ for documents and elements
  switch (aNewChild->NodeType()) {
  case nsINode::COMMENT_NODE :
  case nsINode::PROCESSING_INSTRUCTION_NODE :
    // OK in both cases
    return true;
  case nsINode::TEXT_NODE :
  case nsINode::CDATA_SECTION_NODE :
  case nsINode::ENTITY_REFERENCE_NODE :
    // Allowed under Elements and DocumentFragments
    return aParent->NodeType() != nsINode::DOCUMENT_NODE;
  case nsINode::ELEMENT_NODE :
    {
      if (!aParent->IsDocument()) {
        // Always ok to have elements under other elements or document fragments
        return true;
      }

      nsIDocument* parentDocument = aParent->AsDocument();
      Element* rootElement = parentDocument->GetRootElement();
      if (rootElement) {
        // Already have a documentElement, so this is only OK if we're
        // replacing it.
        return aIsReplace && rootElement == aRefChild;
      }

      // We don't have a documentElement yet.  Our one remaining constraint is
      // that the documentElement must come after the doctype.
      if (!aRefChild) {
        // Appending is just fine.
        return true;
      }

      nsIContent* docTypeContent = parentDocument->GetDoctype();
      if (!docTypeContent) {
        // It's all good.
        return true;
      }

      int32_t doctypeIndex = aParent->ComputeIndexOf(docTypeContent);
      int32_t insertIndex = aParent->ComputeIndexOf(aRefChild);

      // Now we're OK in the following two cases only:
      // 1) We're replacing something that's not before the doctype
      // 2) We're inserting before something that comes after the doctype
      return aIsReplace ? (insertIndex >= doctypeIndex) :
        insertIndex > doctypeIndex;
    }
  case nsINode::DOCUMENT_TYPE_NODE :
    {
      if (!aParent->IsDocument()) {
        // doctypes only allowed under documents
        return false;
      }

      nsIDocument* parentDocument = aParent->AsDocument();
      nsIContent* docTypeContent = parentDocument->GetDoctype();
      if (docTypeContent) {
        // Already have a doctype, so this is only OK if we're replacing it
        return aIsReplace && docTypeContent == aRefChild;
      }

      // We don't have a doctype yet.  Our one remaining constraint is
      // that the doctype must come before the documentElement.
      Element* rootElement = parentDocument->GetRootElement();
      if (!rootElement) {
        // It's all good
        return true;
      }

      if (!aRefChild) {
        // Trying to append a doctype, but have a documentElement
        return false;
      }

      int32_t rootIndex = aParent->ComputeIndexOf(rootElement);
      int32_t insertIndex = aParent->ComputeIndexOf(aRefChild);

      // Now we're OK if and only if insertIndex <= rootIndex.  Indeed, either
      // we end up replacing aRefChild or we end up before it.  Either one is
      // ok as long as aRefChild is not after rootElement.
      return insertIndex <= rootIndex;
    }
  case nsINode::DOCUMENT_FRAGMENT_NODE :
    {
      // Note that for now we only allow nodes inside document fragments if
      // they're allowed inside elements.  If we ever change this to allow
      // doctype nodes in document fragments, we'll need to update this code.
      // Also, there's a version of this code in ReplaceOrInsertBefore.  If you
      // change this code, change that too.
      if (!aParent->IsDocument()) {
        // All good here
        return true;
      }

      bool sawElement = false;
      for (nsIContent* child = aNewChild->GetFirstChild();
           child;
           child = child->GetNextSibling()) {
        if (child->IsElement()) {
          if (sawElement) {
            // Can't put two elements into a document
            return false;
          }
          sawElement = true;
        }
        // If we can put this content at the the right place, we might be ok;
        // if not, we bail out.
        if (!IsAllowedAsChild(child, aParent, aIsReplace, aRefChild)) {
          return false;
        }
      }

      // Everything in the fragment checked out ok, so we can stick it in here
      return true;
    }
  default:
    /*
     * aNewChild is of invalid type.
     */
    break;
  }

  return false;
}

void
nsINode::EnsurePreInsertionValidity(nsINode& aNewChild, nsINode* aRefChild,
                                    ErrorResult& aError)
{
  EnsurePreInsertionValidity1(aNewChild, aRefChild, aError);
  if (aError.Failed()) {
    return;
  }
  EnsurePreInsertionValidity2(false, aNewChild, aRefChild, aError);
}

void
nsINode::EnsurePreInsertionValidity1(nsINode& aNewChild, nsINode* aRefChild,
                                     ErrorResult& aError)
{
  if ((!IsDocument() && !IsDocumentFragment() && !IsElement()) ||
      !aNewChild.IsContent()) {
    aError.Throw(NS_ERROR_DOM_HIERARCHY_REQUEST_ERR);
    return;
  }
}

void
nsINode::EnsurePreInsertionValidity2(bool aReplace, nsINode& aNewChild,
                                     nsINode* aRefChild, ErrorResult& aError)
{
  nsIContent* newContent = aNewChild.AsContent();
  if (newContent->IsRootOfAnonymousSubtree()) {
    // This is anonymous content.  Don't allow its insertion
    // anywhere, since it might have UnbindFromTree calls coming
    // its way.
    aError.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return;
  }

  // Make sure that the inserted node is allowed as a child of its new parent.
  if (!IsAllowedAsChild(newContent, this, aReplace, aRefChild)) {
    aError.Throw(NS_ERROR_DOM_HIERARCHY_REQUEST_ERR);
    return;
  }
}

nsINode*
nsINode::ReplaceOrInsertBefore(bool aReplace, nsINode* aNewChild,
                               nsINode* aRefChild, ErrorResult& aError)
{
  // XXXbz I wish I could assert that nsContentUtils::IsSafeToRunScript() so we
  // could rely on scriptblockers going out of scope to actually run XBL
  // teardown, but various crud adds nodes under scriptblockers (e.g. native
  // anonymous content).  The only good news is those insertions can't trigger
  // the bad XBL cases.
  MOZ_ASSERT_IF(aReplace, aRefChild);

  EnsurePreInsertionValidity1(*aNewChild, aRefChild, aError);
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
    // This check happens again further down (though then using
    // ComputeIndexOf).
    // We're only checking this here to avoid firing mutation events when
    // none should be fired.
    // It's ok that we do the check twice in the case when firing mutation
    // events as we need to recheck after running script anyway.
    if (aRefChild && aRefChild->GetParentNode() != this) {
      aError.Throw(NS_ERROR_DOM_NOT_FOUND_ERR);
      return nullptr;
    }

    // If we're replacing, fire for node-to-be-replaced.
    // If aRefChild == aNewChild then we'll fire for it in check below
    if (aReplace && aRefChild != aNewChild) {
      nsContentUtils::MaybeFireNodeRemoved(aRefChild, this, OwnerDoc());
    }

    // If the new node already has a parent, fire for removing from old
    // parent
    nsINode* oldParent = aNewChild->GetParentNode();
    if (oldParent) {
      nsContentUtils::MaybeFireNodeRemoved(aNewChild, oldParent,
                                           aNewChild->OwnerDoc());
    }

    // If we're inserting a fragment, fire for all the children of the
    // fragment
    if (nodeType == DOCUMENT_FRAGMENT_NODE) {
      static_cast<FragmentOrElement*>(aNewChild)->FireNodeRemovedForChildren();
    }
    // Verify that our aRefChild is still sensible
    if (aRefChild && aRefChild->GetParentNode() != this) {
      aError.Throw(NS_ERROR_DOM_NOT_FOUND_ERR);
      return nullptr;
    }
  }

  EnsurePreInsertionValidity2(aReplace, *aNewChild, aRefChild, aError);
  if (aError.Failed()) {
    return nullptr;
  }

  // Record the node to insert before, if any
  nsINode* nodeToInsertBefore;
  if (aReplace) {
    nodeToInsertBefore = aRefChild->GetNextSibling();
  } else {
    nodeToInsertBefore = aRefChild;
  }
  if (nodeToInsertBefore == aNewChild) {
    // We're going to remove aNewChild from its parent, so use its next sibling
    // as the node to insert before.
    nodeToInsertBefore = nodeToInsertBefore->GetNextSibling();
  }

  Maybe<AutoTArray<nsCOMPtr<nsIContent>, 50> > fragChildren;

  // Remove the new child from the old parent if one exists
  nsIContent* newContent = aNewChild->AsContent();
  nsCOMPtr<nsINode> oldParent = newContent->GetParentNode();
  if (oldParent) {
    int32_t removeIndex = oldParent->ComputeIndexOf(newContent);
    if (removeIndex < 0) {
      // newContent is anonymous.  We can't deal with this, so just bail
      NS_ERROR("How come our flags didn't catch this?");
      aError.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
      return nullptr;
    }

    // Hold a strong ref to nodeToInsertBefore across the removal of newContent
    nsCOMPtr<nsINode> kungFuDeathGrip = nodeToInsertBefore;

    // Removing a child can run script, via XBL destructors.
    nsMutationGuard guard;

    // Scope for the mutation batch and scriptblocker, so they go away
    // while kungFuDeathGrip is still alive.
    {
      mozAutoDocUpdate batch(newContent->GetComposedDoc(),
                             UPDATE_CONTENT_MODEL, true);
      nsAutoMutationBatch mb(oldParent, true, true);
      oldParent->RemoveChildAt_Deprecated(removeIndex, true);
      if (nsAutoMutationBatch::GetCurrentBatch() == &mb) {
        mb.RemovalDone();
        mb.SetPrevSibling(oldParent->GetChildAt_Deprecated(removeIndex - 1));
        mb.SetNextSibling(oldParent->GetChildAt_Deprecated(removeIndex));
      }
    }

    // We expect one mutation (the removal) to have happened.
    if (guard.Mutated(1)) {
      // XBL destructors, yuck.

      // Verify that nodeToInsertBefore, if non-null, is still our child.  If
      // it's not, there's no way we can do this insert sanely; just bail out.
      if (nodeToInsertBefore && nodeToInsertBefore->GetParent() != this) {
        aError.Throw(NS_ERROR_DOM_HIERARCHY_REQUEST_ERR);
        return nullptr;
      }

      // Verify that newContent has no parent.
      if (newContent->GetParentNode()) {
        aError.Throw(NS_ERROR_DOM_HIERARCHY_REQUEST_ERR);
        return nullptr;
      }

      // And verify that newContent is still allowed as our child.
      if (aNewChild == aRefChild) {
        // We've already removed aRefChild.  So even if we were doing a replace,
        // now we're doing a simple insert before nodeToInsertBefore.
        if (!IsAllowedAsChild(newContent, this, false, nodeToInsertBefore)) {
          aError.Throw(NS_ERROR_DOM_HIERARCHY_REQUEST_ERR);
          return nullptr;
        }
      } else {
        if ((aRefChild && aRefChild->GetParent() != this) ||
            !IsAllowedAsChild(newContent, this, aReplace, aRefChild)) {
          aError.Throw(NS_ERROR_DOM_HIERARCHY_REQUEST_ERR);
          return nullptr;
        }
        // And recompute nodeToInsertBefore, just in case.
        if (aReplace) {
          nodeToInsertBefore = aRefChild->GetNextSibling();
        } else {
          nodeToInsertBefore = aRefChild;
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
    for (nsIContent* child = newContent->GetFirstChild();
         child;
         child = child->GetNextSibling()) {
      NS_ASSERTION(child->GetComposedDoc() == nullptr,
                   "How did we get a child with a current doc?");
      fragChildren->AppendElement(child);
    }

    // Hold a strong ref to nodeToInsertBefore across the removals
    nsCOMPtr<nsINode> kungFuDeathGrip = nodeToInsertBefore;

    nsMutationGuard guard;

    // Scope for the mutation batch and scriptblocker, so they go away
    // while kungFuDeathGrip is still alive.
    {
      mozAutoDocUpdate batch(newContent->GetComposedDoc(),
                             UPDATE_CONTENT_MODEL, true);
      nsAutoMutationBatch mb(newContent, false, true);

      for (uint32_t i = count; i > 0;) {
        newContent->RemoveChildAt_Deprecated(--i, true);
      }
    }

    // We expect |count| removals
    if (guard.Mutated(count)) {
      // XBL destructors, yuck.

      // Verify that nodeToInsertBefore, if non-null, is still our child.  If
      // it's not, there's no way we can do this insert sanely; just bail out.
      if (nodeToInsertBefore && nodeToInsertBefore->GetParent() != this) {
        aError.Throw(NS_ERROR_DOM_HIERARCHY_REQUEST_ERR);
        return nullptr;
      }

      // Verify that all the things in fragChildren have no parent.
      for (uint32_t i = 0; i < count; ++i) {
        if (fragChildren->ElementAt(i)->GetParentNode()) {
          aError.Throw(NS_ERROR_DOM_HIERARCHY_REQUEST_ERR);
          return nullptr;
        }
      }

      // Note that unlike the single-element case above, none of our kids can
      // be aRefChild, so we can always pass through aReplace in the
      // IsAllowedAsChild checks below and don't have to worry about whether
      // recomputing nodeToInsertBefore is OK.

      // Verify that our aRefChild is still sensible
      if (aRefChild && aRefChild->GetParent() != this) {
        aError.Throw(NS_ERROR_DOM_HIERARCHY_REQUEST_ERR);
        return nullptr;
      }

      // Recompute nodeToInsertBefore, just in case.
      if (aReplace) {
        nodeToInsertBefore = aRefChild->GetNextSibling();
      } else {
        nodeToInsertBefore = aRefChild;
      }

      // And verify that newContent is still allowed as our child.  Sadly, we
      // need to reimplement the relevant part of IsAllowedAsChild() because
      // now our nodes are in an array and all.  If you change this code,
      // change the code there.
      if (IsDocument()) {
        bool sawElement = false;
        for (uint32_t i = 0; i < count; ++i) {
          nsIContent* child = fragChildren->ElementAt(i);
          if (child->IsElement()) {
            if (sawElement) {
              // No good
              aError.Throw(NS_ERROR_DOM_HIERARCHY_REQUEST_ERR);
              return nullptr;
            }
            sawElement = true;
          }
          if (!IsAllowedAsChild(child, this, aReplace, aRefChild)) {
            aError.Throw(NS_ERROR_DOM_HIERARCHY_REQUEST_ERR);
            return nullptr;
          }
        }
      }
    }
  }

  mozAutoDocUpdate batch(GetComposedDoc(), UPDATE_CONTENT_MODEL, true);
  nsAutoMutationBatch mb;

  // Figure out which index we want to insert at.  Note that we use
  // nodeToInsertBefore to determine this, because it's possible that
  // aRefChild == aNewChild, in which case we just removed it from the
  // parent list.
  int32_t insPos;
  if (nodeToInsertBefore) {
    insPos = ComputeIndexOf(nodeToInsertBefore);
    if (insPos < 0) {
      // XXXbz How the heck would _that_ happen, exactly?
      aError.Throw(NS_ERROR_DOM_NOT_FOUND_ERR);
      return nullptr;
    }
  }
  else {
    insPos = GetChildCount();
  }

  // If we're replacing and we haven't removed aRefChild yet, do so now
  if (aReplace && aRefChild != aNewChild) {
    mb.Init(this, true, true);

    // Since aRefChild is never null in the aReplace case, we know that at
    // this point nodeToInsertBefore is the next sibling of aRefChild.
    NS_ASSERTION(aRefChild->GetNextSibling() == nodeToInsertBefore,
                 "Unexpected nodeToInsertBefore");

    // An since nodeToInsertBefore is at index insPos, we want to remove
    // at the previous index.
    NS_ASSERTION(insPos >= 1, "insPos too small");
    RemoveChildAt_Deprecated(insPos-1, true);
    --insPos;
  }

  // Move new child over to our document if needed. Do this after removing
  // it from its parent so that AdoptNode doesn't fire DOMNodeRemoved
  // DocumentType nodes are the only nodes that can have a null
  // ownerDocument according to the DOM spec, and we need to allow
  // inserting them w/o calling AdoptNode().
  nsIDocument* doc = OwnerDoc();
  if (doc != newContent->OwnerDoc()) {
    AdoptNodeIntoOwnerDoc(this, aNewChild, aError);
    if (aError.Failed()) {
      return nullptr;
    }
  } else if (doc->DidDocumentOpen()) {
    CheckForOutdatedParent(this, aNewChild, aError);
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
    if (!aReplace) {
      mb.Init(this, true, true);
    }
    nsAutoMutationBatch* mutationBatch = nsAutoMutationBatch::GetCurrentBatch();
    if (mutationBatch) {
      mutationBatch->RemovalDone();
      mutationBatch->SetPrevSibling(GetChildAt_Deprecated(insPos - 1));
      mutationBatch->SetNextSibling(GetChildAt_Deprecated(insPos));
    }

    uint32_t count = fragChildren->Length();
    if (!count) {
      return result;
    }

    bool appending = !IsDocument() && uint32_t(insPos) == GetChildCount();
    nsIContent* firstInsertedContent = fragChildren->ElementAt(0);

    // Iterate through the fragment's children, and insert them in the new
    // parent
    for (uint32_t i = 0; i < count; ++i, ++insPos) {
      // XXXbz how come no reparenting here?  That seems odd...
      // Insert the child.
      aError = InsertChildAt_Deprecated(fragChildren->ElementAt(i), insPos,
                                        !appending);
      if (aError.Failed()) {
        // Make sure to notify on any children that we did succeed to insert
        if (appending && i != 0) {
          nsNodeUtils::ContentAppended(static_cast<nsIContent*>(this),
                                       firstInsertedContent);
        }
        return nullptr;
      }
    }

    if (mutationBatch && !appending) {
      mutationBatch->NodesAdded();
    }

    // Notify and fire mutation events when appending
    if (appending) {
      nsNodeUtils::ContentAppended(static_cast<nsIContent*>(this),
                                   firstInsertedContent);
      if (mutationBatch) {
        mutationBatch->NodesAdded();
      }
      // Optimize for the case when there are no listeners
      if (nsContentUtils::
            HasMutationListeners(doc, NS_EVENT_BITS_MUTATION_NODEINSERTED)) {
        Element::FireNodeInserted(doc, this, *fragChildren);
      }
    }
  }
  else {
    // Not inserting a fragment but rather a single node.

    // FIXME https://bugzilla.mozilla.org/show_bug.cgi?id=544654
    //       We need to reparent here for nodes for which the parent of their
    //       wrapper is not the wrapper for their ownerDocument (XUL elements,
    //       form controls, ...). Also applies in the fragment code above.

    if (nsAutoMutationBatch::GetCurrentBatch() == &mb) {
      mb.RemovalDone();
      mb.SetPrevSibling(GetChildAt_Deprecated(insPos - 1));
      mb.SetNextSibling(GetChildAt_Deprecated(insPos));
    }
    aError = InsertChildAt_Deprecated(newContent, insPos, true);
    if (aError.Failed()) {
      return nullptr;
    }
  }

  return result;
}

void
nsINode::BindObject(nsISupports* aObject)
{
  nsCOMArray<nsISupports>* objects =
    static_cast<nsCOMArray<nsISupports>*>(GetProperty(nsGkAtoms::keepobjectsalive));
  if (!objects) {
    objects = new nsCOMArray<nsISupports>();
    SetProperty(nsGkAtoms::keepobjectsalive, objects,
                nsINode::DeleteProperty< nsCOMArray<nsISupports> >, true);
  }
  objects->AppendObject(aObject);
}

void
nsINode::UnbindObject(nsISupports* aObject)
{
  nsCOMArray<nsISupports>* objects =
    static_cast<nsCOMArray<nsISupports>*>(GetProperty(nsGkAtoms::keepobjectsalive));
  if (objects) {
    objects->RemoveObject(aObject);
  }
}

already_AddRefed<AccessibleNode>
nsINode::GetAccessibleNode()
{
#ifdef ACCESSIBILITY
  RefPtr<AccessibleNode> anode = new AccessibleNode(this);
  return anode.forget();
#else
  return nullptr;
#endif
}

void
nsINode::AddSizeOfExcludingThis(nsWindowSizes& aSizes, size_t* aNodeSize) const
{
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
  // - mParent, mNextSibling, mPreviousSibling, mFirstChild: because they're
  //   non-owning
}

void
nsINode::AddSizeOfIncludingThis(nsWindowSizes& aSizes, size_t* aNodeSize) const
{
  *aNodeSize += aSizes.mState.mMallocSizeOf(this);
  AddSizeOfExcludingThis(aSizes, aNodeSize);
}

#define EVENT(name_, id_, type_, struct_)                                    \
  EventHandlerNonNull* nsINode::GetOn##name_() {                             \
    EventListenerManager *elm = GetExistingListenerManager();                \
    return elm ? elm->GetEventHandler(nsGkAtoms::on##name_, EmptyString())   \
               : nullptr;                                                    \
  }                                                                          \
  void nsINode::SetOn##name_(EventHandlerNonNull* handler)                   \
  {                                                                          \
    EventListenerManager *elm = GetOrCreateListenerManager();                \
    if (elm) {                                                               \
      elm->SetEventHandler(nsGkAtoms::on##name_, EmptyString(), handler);    \
    }                                                                        \
  }
#define TOUCH_EVENT EVENT
#define DOCUMENT_ONLY_EVENT EVENT
#include "mozilla/EventNameList.h"
#undef DOCUMENT_ONLY_EVENT
#undef TOUCH_EVENT
#undef EVENT

bool
nsINode::Contains(const nsINode* aOther) const
{
  if (aOther == this) {
    return true;
  }
  if (!aOther ||
      OwnerDoc() != aOther->OwnerDoc() ||
      IsInUncomposedDoc() != aOther->IsInUncomposedDoc() ||
      !aOther->IsContent() ||
      !GetFirstChild()) {
    return false;
  }

  const nsIContent* other = static_cast<const nsIContent*>(aOther);
  if (this == OwnerDoc()) {
    // document.contains(aOther) returns true if aOther is in the document,
    // but is not in any anonymous subtree.
    // IsInUncomposedDoc() check is done already before this.
    return !other->IsInAnonymousSubtree();
  }

  if (!IsElement() && !IsDocumentFragment()) {
    return false;
  }

  if (AsContent()->GetBindingParent() != other->GetBindingParent()) {
    return false;
  }

  return nsContentUtils::ContentIsDescendantOf(other, this);
}

uint32_t
nsINode::Length() const
{
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

const RawServoSelectorList*
nsINode::ParseSelectorList(const nsAString& aSelectorString,
                           ErrorResult& aRv)
{
  nsIDocument* doc = OwnerDoc();

  nsIDocument::SelectorCache& cache = doc->GetSelectorCache();
  nsIDocument::SelectorCache::SelectorList* list =
    cache.GetList(aSelectorString);
  if (list) {
    if (!*list) {
      // Invalid selector.
      aRv.ThrowDOMException(NS_ERROR_DOM_SYNTAX_ERR,
        NS_LITERAL_CSTRING("'") + NS_ConvertUTF16toUTF8(aSelectorString) +
        NS_LITERAL_CSTRING("' is not a valid selector")
      );
      return nullptr;
    }

    return list->get();
  }

  NS_ConvertUTF16toUTF8 selectorString(aSelectorString);

  auto selectorList =
    UniquePtr<RawServoSelectorList>(Servo_SelectorList_Parse(&selectorString));
  if (!selectorList) {
    aRv.ThrowDOMException(NS_ERROR_DOM_SYNTAX_ERR,
      NS_LITERAL_CSTRING("'") + selectorString +
      NS_LITERAL_CSTRING("' is not a valid selector")
    );
  }

  auto* ret = selectorList.get();
  cache.CacheList(aSelectorString, Move(selectorList));
  return ret;
}

namespace {
struct SelectorMatchInfo {
};
} // namespace

// Given an id, find elements with that id under aRoot that match aMatchInfo if
// any is provided.  If no SelectorMatchInfo is provided, just find the ones
// with the given id.  aRoot must be in the document.
template<bool onlyFirstMatch, class T>
inline static void
FindMatchingElementsWithId(const nsAString& aId, nsINode* aRoot,
                           SelectorMatchInfo* aMatchInfo,
                           T& aList)
{
  MOZ_ASSERT(aRoot->IsInUncomposedDoc(),
             "Don't call me if the root is not in the document");
  // FIXME(emilio): It'd be nice to optimize this for shadow roots too.
  MOZ_ASSERT(aRoot->IsElement() || aRoot->IsDocument(),
             "The optimization below to check ContentIsDescendantOf only for "
             "elements depends on aRoot being either an element or a "
             "document if it's in the document.  Note that document fragments "
             "can't be IsInUncomposedDoc(), so should never show up here.");

  const nsTArray<Element*>* elements = aRoot->OwnerDoc()->GetAllElementsForId(aId);
  if (!elements) {
    // Nothing to do; we're done
    return;
  }

  // XXXbz: Should we fall back to the tree walk if aRoot is not the
  // document and |elements| is long, for some value of "long"?
  for (size_t i = 0; i < elements->Length(); ++i) {
    Element* element = (*elements)[i];
    if (!aRoot->IsElement() ||
        (element != aRoot &&
           nsContentUtils::ContentIsDescendantOf(element, aRoot))) {
      // We have an element with the right id and it's a strict descendant
      // of aRoot.  Make sure it really matches the selector.
      if (!aMatchInfo
      ) {
        aList.AppendElement(element);
        if (onlyFirstMatch) {
          return;
        }
      }
    }
  }
}


struct ElementHolder {
  ElementHolder() : mElement(nullptr) {}
  void AppendElement(Element* aElement) {
    MOZ_ASSERT(!mElement, "Should only get one element");
    mElement = aElement;
  }
  void SetCapacity(uint32_t aCapacity) { MOZ_CRASH("Don't call me!"); }
  uint32_t Length() { return 0; }
  Element* ElementAt(uint32_t aIndex) { return nullptr; }

  Element* mElement;
};

Element*
nsINode::QuerySelector(const nsAString& aSelector, ErrorResult& aResult)
{
  const RawServoSelectorList* list = ParseSelectorList(aSelector, aResult);
  if (!list) {
    return nullptr;
  }
  const bool useInvalidation = false;
  return const_cast<Element*>(
    Servo_SelectorList_QueryFirst(this, list, useInvalidation));
}

already_AddRefed<nsINodeList>
nsINode::QuerySelectorAll(const nsAString& aSelector, ErrorResult& aResult)
{
  RefPtr<nsSimpleContentList> contentList = new nsSimpleContentList(this);
  const RawServoSelectorList* list = ParseSelectorList(aSelector, aResult);
  if (!list) {
    return contentList.forget();
  }

  const bool useInvalidation = false;
  Servo_SelectorList_QueryAll(this, list, contentList.get(), useInvalidation);
  return contentList.forget();
}

Element*
nsINode::GetElementById(const nsAString& aId)
{
  MOZ_ASSERT(IsElement() || IsDocumentFragment(),
             "Bogus this object for GetElementById call");
  if (IsInUncomposedDoc()) {
    ElementHolder holder;
    FindMatchingElementsWithId<true>(aId, this, nullptr, holder);
    return holder.mElement;
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

JSObject*
nsINode::WrapObject(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
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
      !hasHadScriptHandlingObject &&
      !nsContentUtils::IsSystemCaller(aCx)) {
    Throw(aCx, NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  JS::Rooted<JSObject*> obj(aCx, WrapNode(aCx, aGivenProto));
  MOZ_ASSERT_IF(obj && ChromeOnlyAccess(),
                xpc::IsInContentXBLScope(obj) ||
                !xpc::UseContentXBLScope(JS::GetObjectRealmOrNull(obj)));
  return obj;
}

already_AddRefed<nsINode>
nsINode::CloneNode(bool aDeep, ErrorResult& aError)
{
  return nsNodeUtils::CloneNodeImpl(this, aDeep, aError);
}

nsDOMAttributeMap*
nsINode::GetAttributes()
{
  if (!IsElement()) {
    return nullptr;
  }
  return AsElement()->Attributes();
}

Element*
nsINode::GetParentElementCrossingShadowRoot() const
{
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

bool
nsINode::HasBoxQuadsSupport(JSContext* aCx, JSObject* /* unused */)
{
  return xpc::AccessCheck::isChrome(js::GetContextCompartment(aCx)) ||
         nsContentUtils::GetBoxQuadsEnabled();
}

nsINode*
nsINode::GetScopeChainParent() const
{
  return nullptr;
}

void
nsINode::AddAnimationObserver(nsIAnimationObserver* aAnimationObserver)
{
  AddMutationObserver(aAnimationObserver);
  OwnerDoc()->SetMayHaveAnimationObservers();
}

void
nsINode::AddAnimationObserverUnlessExists(
                               nsIAnimationObserver* aAnimationObserver)
{
  AddMutationObserverUnlessExists(aAnimationObserver);
  OwnerDoc()->SetMayHaveAnimationObservers();
}

void
nsINode::GenerateXPath(nsAString& aResult)
{
  XPathGenerator::Generate(this, aResult);
}

bool
nsINode::IsApzAware() const
{
  return IsNodeApzAware();
}

bool
nsINode::IsNodeApzAwareInternal() const
{
  return EventTarget::IsApzAware();
}

DocGroup*
nsINode::GetDocGroup() const
{
  return OwnerDoc()->GetDocGroup();
}

class LocalizationHandler : public PromiseNativeHandler
{
public:
  LocalizationHandler() = default;

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(LocalizationHandler)

  nsTArray<nsCOMPtr<Element>>& Elements() { return mElements; }

  void SetReturnValuePromise(Promise* aReturnValuePromise)
  {
    mReturnValuePromise = aReturnValuePromise;
  }

  virtual void
  ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override
  {
    nsTArray<L10nValue> l10nData;
    if (aValue.isObject()) {
      JS::ForOfIterator iter(aCx);
      if (!iter.init(aValue, JS::ForOfIterator::AllowNonIterable)) {
        mReturnValuePromise->MaybeRejectWithUndefined();
        return;
      }
      if (!iter.valueIsIterable()) {
        mReturnValuePromise->MaybeRejectWithUndefined();
        return;
      }

      JS::Rooted<JS::Value> temp(aCx);
      while (true) {
        bool done;
        if (!iter.next(&temp, &done)) {
          mReturnValuePromise->MaybeRejectWithUndefined();
          return;
        }

        if (done) {
          break;
        }

        L10nValue* slotPtr =
          l10nData.AppendElement(mozilla::fallible);
        if (!slotPtr) {
          mReturnValuePromise->MaybeRejectWithUndefined();
          return;
        }

        if (!slotPtr->Init(aCx, temp)) {
          mReturnValuePromise->MaybeRejectWithUndefined();
          return;
        }
      }
    }

    if (mElements.Length() != l10nData.Length()) {
      mReturnValuePromise->MaybeRejectWithUndefined();
      return;
    }

    JS::Rooted<JSObject*> untranslatedElements(aCx,
      JS_NewArrayObject(aCx, mElements.Length()));
    if (!untranslatedElements) {
      mReturnValuePromise->MaybeRejectWithUndefined();
      return;
    }

    ErrorResult rv;
    for (size_t i = 0; i < l10nData.Length(); ++i) {
      Element* elem = mElements[i];
      nsString& content = l10nData[i].mValue;
      if (!content.IsVoid()) {
        elem->SetTextContent(content, rv);
        if (NS_WARN_IF(rv.Failed())) {
          mReturnValuePromise->MaybeRejectWithUndefined();
          return;
        }
      }

      Nullable<Sequence<AttributeNameValue>>& attributes =
        l10nData[i].mAttributes;
      if (!attributes.IsNull()) {
        for (size_t j = 0; j < attributes.Value().Length(); ++j) {
          // Use SetAttribute here to validate the attribute name!
          elem->SetAttribute(attributes.Value()[j].mName,
                             attributes.Value()[j].mValue,
                             rv);
          if (rv.Failed()) {
            mReturnValuePromise->MaybeRejectWithUndefined();
            return;
          }
        }
      }

      if (content.IsVoid() && attributes.IsNull()) {
        JS::Rooted<JS::Value> wrappedElem(aCx);
        if (!ToJSValue(aCx, elem, &wrappedElem)) {
          mReturnValuePromise->MaybeRejectWithUndefined();
          return;
        }

        if (!JS_DefineElement(aCx, untranslatedElements, i, wrappedElem, JSPROP_ENUMERATE)) {
          mReturnValuePromise->MaybeRejectWithUndefined();
          return;
        }
      }
    }
    mReturnValuePromise->MaybeResolve(untranslatedElements);
  }

  virtual void
  RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override
  {
    mReturnValuePromise->MaybeRejectWithUndefined();
  }

private:
  ~LocalizationHandler() = default;

  nsTArray<nsCOMPtr<Element>> mElements;
  RefPtr<Promise> mReturnValuePromise;
};

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(LocalizationHandler)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_CLASS(LocalizationHandler)

NS_IMPL_CYCLE_COLLECTING_ADDREF(LocalizationHandler)
NS_IMPL_CYCLE_COLLECTING_RELEASE(LocalizationHandler)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(LocalizationHandler)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mElements)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mReturnValuePromise)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(LocalizationHandler)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mElements)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mReturnValuePromise)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END


already_AddRefed<Promise>
nsINode::Localize(JSContext* aCx,
                  mozilla::dom::L10nCallback& aCallback,
                  mozilla::ErrorResult& aRv)
{
  Sequence<L10nElement> l10nElements;
  SequenceRooter<L10nElement> rooter(aCx, &l10nElements);
  RefPtr<LocalizationHandler> nativeHandler = new LocalizationHandler();
  nsTArray<nsCOMPtr<Element>>& domElements = nativeHandler->Elements();
  nsIContent* node = IsContent() ? AsContent() : GetFirstChild();
  nsAutoString l10nId;
  nsAutoString l10nArgs;
  nsAutoString l10nAttrs;
  nsAutoString type;
  for (; node; node = node->GetNextNode(this)) {
    if (!node->IsElement()) {
      continue;
    }

    Element* domElement = node->AsElement();
    if (!domElement->GetAttr(kNameSpaceID_None, nsGkAtoms::datal10nid, l10nId)) {
      continue;
    }

    domElement->GetAttr(kNameSpaceID_None, nsGkAtoms::datal10nargs, l10nArgs);
    domElement->GetAttr(kNameSpaceID_None, nsGkAtoms::datal10nattrs, l10nAttrs);
    L10nElement* element = l10nElements.AppendElement(fallible);
    if (!element) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return nullptr;
    }
    domElements.AppendElement(domElement, fallible);

    domElement->GetNamespaceURI(element->mNamespaceURI);
    element->mLocalName = domElement->LocalName();
    domElement->GetAttr(kNameSpaceID_None, nsGkAtoms::type, type);
    if (!type.IsEmpty()) {
      element->mType = type;
    }
    element->mL10nId = l10nId;
    if (!l10nAttrs.IsEmpty()) {
      element->mL10nAttrs = l10nAttrs;
    }
    if (!l10nArgs.IsEmpty()) {
      JS::Rooted<JS::Value> json(aCx);
      if (!JS_ParseJSON(aCx, l10nArgs.get(), l10nArgs.Length(), &json) ||
          !json.isObject()) {
        aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
        return nullptr;
      }
      element->mL10nArgs = &json.toObject();
    }
  }

  RefPtr<Promise> callbackResult = aCallback.Call(l10nElements, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(OwnerDoc()->GetParentObject(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  nativeHandler->SetReturnValuePromise(promise);
  callbackResult->AppendNativeHandler(nativeHandler);

  return promise.forget();
}

NS_IMPL_ISUPPORTS(nsNodeWeakReference,
                  nsIWeakReference)

nsNodeWeakReference::nsNodeWeakReference(nsINode* aNode)
  : nsIWeakReference(aNode)
{
}

nsNodeWeakReference::~nsNodeWeakReference()
{
  nsINode* node = static_cast<nsINode*>(mObject);

  if (node) {
    NS_ASSERTION(node->Slots()->mWeakReference == this,
                 "Weak reference has wrong value");
    node->Slots()->mWeakReference = nullptr;
  }
}

NS_IMETHODIMP
nsNodeWeakReference::QueryReferentFromScript(const nsIID& aIID, void** aInstancePtr)
{
  return QueryReferent(aIID, aInstancePtr);
}

size_t
nsNodeWeakReference::SizeOfOnlyThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this);
}
