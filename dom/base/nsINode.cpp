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
#include "mozilla/InternalMutationEvent.h"
#include "mozilla/Likely.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h"
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
#include "nsIAtom.h"
#include "nsIBaseWindow.h"
#include "nsICategoryManager.h"
#include "nsIContentIterator.h"
#include "nsIControllers.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentType.h"
#include "nsIDOMEvent.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMMutationEvent.h"
#include "nsIDOMNodeList.h"
#include "nsIEditor.h"
#include "nsIEditorIMESupport.h"
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
#include "nsRuleProcessorData.h"
#include "nsString.h"
#include "nsStyleConsts.h"
#include "nsSVGFeatures.h"
#include "nsSVGUtils.h"
#include "nsTextNode.h"
#include "nsUnicharUtils.h"
#include "nsXBLBinding.h"
#include "nsXBLPrototypeBinding.h"
#include "mozilla/Preferences.h"
#include "prprf.h"
#include "xpcpublic.h"
#include "nsCSSRuleProcessor.h"
#include "nsCSSParser.h"
#include "HTMLLegendElement.h"
#include "nsWrapperCacheInlines.h"
#include "WrapperFactory.h"
#include "DocumentType.h"
#include <algorithm>
#include "nsGlobalWindow.h"
#include "nsDOMMutationObserver.h"
#include "GeometryUtils.h"
#include "nsIAnimationObserver.h"
#include "nsChildContentList.h"

using namespace mozilla;
using namespace mozilla::dom;

nsINode::nsSlots::nsSlots()
  : mWeakReference(nullptr)
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
nsINode::GetProperty(uint16_t aCategory, nsIAtom *aPropertyName,
                     nsresult *aStatus) const
{
  return OwnerDoc()->PropertyTable(aCategory)->GetProperty(this, aPropertyName,
                                                           aStatus);
}

nsresult
nsINode::SetProperty(uint16_t aCategory, nsIAtom *aPropertyName, void *aValue,
                     NSPropertyDtorFunc aDtor, bool aTransfer,
                     void **aOldValue)
{
  nsresult rv = OwnerDoc()->PropertyTable(aCategory)->SetProperty(this,
                                                                  aPropertyName,
                                                                  aValue, aDtor,
                                                                  nullptr,
                                                                  aTransfer,
                                                                  aOldValue);
  if (NS_SUCCEEDED(rv)) {
    SetFlags(NODE_HAS_PROPERTIES);
  }

  return rv;
}

void
nsINode::DeleteProperty(uint16_t aCategory, nsIAtom *aPropertyName)
{
  OwnerDoc()->PropertyTable(aCategory)->DeleteProperty(this, aPropertyName);
}

void*
nsINode::UnsetProperty(uint16_t aCategory, nsIAtom *aPropertyName,
                       nsresult *aStatus)
{
  return OwnerDoc()->PropertyTable(aCategory)->UnsetProperty(this,
                                                             aPropertyName,
                                                             aStatus);
}

nsINode::nsSlots*
nsINode::CreateSlots()
{
  return new nsSlots();
}

bool
nsINode::IsEditableInternal() const
{
  if (HasFlag(NODE_IS_EDITABLE)) {
    // The node is in an editable contentEditable subtree.
    return true;
  }

  nsIDocument *doc = GetUncomposedDoc();

  // Check if the node is in a document and the document is in designMode.
  return doc && doc->HasFlag(NODE_IS_EDITABLE);
}

static nsIContent* GetEditorRootContent(nsIEditor* aEditor)
{
  nsCOMPtr<nsIDOMElement> rootElement;
  aEditor->GetRootElement(getter_AddRefs(rootElement));
  nsCOMPtr<nsIContent> rootContent(do_QueryInterface(rootElement));
  return rootContent;
}

nsIContent*
nsINode::GetTextEditorRootContent(nsIEditor** aEditor)
{
  if (aEditor)
    *aEditor = nullptr;
  for (nsINode* node = this; node; node = node->GetParentNode()) {
    if (!node->IsElement() ||
        !node->IsHTMLElement())
      continue;

    nsCOMPtr<nsIEditor> editor =
      static_cast<nsGenericHTMLElement*>(node)->GetEditorInternal();
    if (!editor)
      continue;

    nsIContent* rootContent = GetEditorRootContent(editor);
    if (aEditor)
      editor.swap(*aEditor);
    return rootContent;
  }
  return nullptr;
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
  if (IsInDoc()) {
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

  if (IsNodeOfType(eDOCUMENT))
    return static_cast<nsIDocument*>(this)->GetRootElement();
  if (!IsNodeOfType(eCONTENT))
    return nullptr;

  if (GetCrossShadowCurrentDoc() != aPresShell->GetDocument()) {
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
    nsIEditor* editor = nsContentUtils::GetHTMLEditor(presContext);
    if (editor) {
      // This node is in HTML editor.
      nsIDocument* doc = GetCrossShadowCurrentDoc();
      if (!doc || doc->HasFlag(NODE_IS_EDITABLE) ||
          !HasFlag(NODE_IS_EDITABLE)) {
        nsIContent* editorRoot = GetEditorRootContent(editor);
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

  nsRefPtr<nsFrameSelection> fs = aPresShell->FrameSelection();
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
    ShadowRoot* shadowRoot = ShadowRoot::FromNode(content);
    if (shadowRoot) {
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
    slots->mChildNodes = new nsChildContentList(this);
  }

  return slots->mChildNodes;
}

void
nsINode::GetTextContentInternal(nsAString& aTextContent, ErrorResult& aError)
{
  SetDOMStringToNull(aTextContent);
}

nsIDocument*
nsINode::GetComposedDocInternal() const
{
  MOZ_ASSERT(HasFlag(NODE_IS_IN_SHADOW_TREE) && IsContent(),
             "Should only be caled on nodes in the shadow tree.");

  ShadowRoot* containingShadow = AsContent()->GetContainingShadow();
  return containingShadow->IsComposedDocParticipant() ?  OwnerDoc() : nullptr;
}

#ifdef DEBUG
void
nsINode::CheckNotNativeAnonymous() const
{
  if (!IsNodeOfType(eCONTENT))
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

nsresult
nsINode::GetParentNode(nsIDOMNode** aParentNode)
{
  *aParentNode = nullptr;

  nsINode *parent = GetParentNode();

  return parent ? CallQueryInterface(parent, aParentNode) : NS_OK;
}

nsresult
nsINode::GetParentElement(nsIDOMElement** aParentElement)
{
  *aParentElement = nullptr;
  nsINode* parent = GetParentElement();
  return parent ? CallQueryInterface(parent, aParentElement) : NS_OK;
}

nsresult
nsINode::GetChildNodes(nsIDOMNodeList** aChildNodes)
{
  NS_ADDREF(*aChildNodes = ChildNodes());

  return NS_OK;
}

nsresult
nsINode::GetFirstChild(nsIDOMNode** aNode)
{
  nsIContent* child = GetFirstChild();
  if (child) {
    return CallQueryInterface(child, aNode);
  }

  *aNode = nullptr;

  return NS_OK;
}

nsresult
nsINode::GetLastChild(nsIDOMNode** aNode)
{
  nsIContent* child = GetLastChild();
  if (child) {
    return CallQueryInterface(child, aNode);
  }

  *aNode = nullptr;

  return NS_OK;
}

nsresult
nsINode::GetPreviousSibling(nsIDOMNode** aPrevSibling)
{
  *aPrevSibling = nullptr;

  nsIContent *sibling = GetPreviousSibling();

  return sibling ? CallQueryInterface(sibling, aPrevSibling) : NS_OK;
}

nsresult
nsINode::GetNextSibling(nsIDOMNode** aNextSibling)
{
  *aNextSibling = nullptr;

  nsIContent *sibling = GetNextSibling();

  return sibling ? CallQueryInterface(sibling, aNextSibling) : NS_OK;
}

nsresult
nsINode::GetOwnerDocument(nsIDOMDocument** aOwnerDocument)
{
  *aOwnerDocument = nullptr;

  nsIDocument *ownerDoc = GetOwnerDocument();

  return ownerDoc ? CallQueryInterface(ownerDoc, aOwnerDocument) : NS_OK;
}

void
nsINode::GetNodeValueInternal(nsAString& aNodeValue)
{
  SetDOMStringToNull(aNodeValue);
}

nsINode*
nsINode::RemoveChild(nsINode& aOldChild, ErrorResult& aError)
{
  if (IsNodeOfType(eDATA_NODE)) {
    // aOldChild can't be one of our children.
    aError.Throw(NS_ERROR_DOM_NOT_FOUND_ERR);
    return nullptr;
  }

  if (aOldChild.GetParentNode() == this) {
    nsContentUtils::MaybeFireNodeRemoved(&aOldChild, this, OwnerDoc());
  }

  int32_t index = IndexOf(&aOldChild);
  if (index == -1) {
    // aOldChild isn't one of our children.
    aError.Throw(NS_ERROR_DOM_NOT_FOUND_ERR);
    return nullptr;
  }

  RemoveChildAt(index, true);
  return &aOldChild;
}

nsresult
nsINode::RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
  nsCOMPtr<nsINode> oldChild = do_QueryInterface(aOldChild);
  if (!oldChild) {
    return NS_ERROR_NULL_POINTER;
  }

  ErrorResult rv;
  RemoveChild(*oldChild, rv);
  if (!rv.Failed()) {
    NS_ADDREF(*aReturn = aOldChild);
  }
  return rv.StealNSResult();
}

void
nsINode::Normalize()
{
  // First collect list of nodes to be removed
  nsAutoTArray<nsCOMPtr<nsIContent>, 50> nodes;

  bool canMerge = false;
  for (nsIContent* node = this->GetFirstChild();
       node;
       node = node->GetNextNode(this)) {
    if (node->NodeType() != nsIDOMNode::TEXT_NODE) {
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
      NS_ASSERTION((target && target->NodeType() == nsIDOMNode::TEXT_NODE) ||
                   hasRemoveListeners,
                   "Should always have a previous text sibling unless "
                   "mutation events messed us up");
      if (!hasRemoveListeners ||
          (target && target->NodeType() == nsIDOMNode::TEXT_NODE)) {
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
      parent->RemoveChildAt(parent->IndexOf(node), true);
    }
  }
}

void
nsINode::GetBaseURI(nsAString &aURI) const
{
  nsCOMPtr<nsIURI> baseURI = GetBaseURI();

  nsAutoCString spec;
  if (baseURI) {
    baseURI->GetSpec(spec);
  }

  CopyUTF8toUTF16(spec, aURI);
}

void
nsINode::GetBaseURIFromJS(nsAString& aURI) const
{
  nsCOMPtr<nsIURI> baseURI = GetBaseURI(nsContentUtils::IsCallerChrome());
  nsAutoCString spec;
  if (baseURI) {
    baseURI->GetSpec(spec);
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
      uint32_t attrCount = content->GetAttrCount();
  
      for (uint32_t i = 0; i < attrCount; ++i) {
        const nsAttrName* name = content->GetAttrNameAt(i);
  
        if (name->NamespaceEquals(kNameSpaceID_XMLNS) &&
            content->AttrValueIs(kNameSpaceID_XMLNS, name->LocalName(),
                                 aNamespaceURI, eCaseMatters)) {
          // If the localName is "xmlns", the prefix we output should be
          // null.
          nsIAtom *localName = name->LocalName();
  
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

static nsresult
SetUserDataProperty(uint16_t aCategory, nsINode *aNode, nsIAtom *aKey,
                    nsISupports* aValue, void** aOldValue)
{
  nsresult rv = aNode->SetProperty(aCategory, aKey, aValue,
                                   nsPropertyTable::SupportsDtorFunc, true,
                                   aOldValue);
  NS_ENSURE_SUCCESS(rv, rv);

  // Property table owns it now.
  NS_ADDREF(aValue);

  return NS_OK;
}

nsresult
nsINode::SetUserData(const nsAString &aKey, nsIVariant *aData, nsIVariant **aResult)
{
  OwnerDoc()->WarnOnceAbout(nsIDocument::eGetSetUserData);
  *aResult = nullptr;

  nsCOMPtr<nsIAtom> key = do_GetAtom(aKey);
  if (!key) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv;
  void *data;
  if (aData) {
    rv = SetUserDataProperty(DOM_USER_DATA, this, key, aData, &data);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    data = UnsetProperty(DOM_USER_DATA, key);
  }

  // Take over ownership of the old data from the property table.
  nsCOMPtr<nsIVariant> oldData = dont_AddRef(static_cast<nsIVariant*>(data));
  oldData.swap(*aResult);
  return NS_OK;
}

void
nsINode::SetUserData(JSContext* aCx, const nsAString& aKey,
                     JS::Handle<JS::Value> aData,
                     JS::MutableHandle<JS::Value> aRetval,
                     ErrorResult& aError)
{
  nsCOMPtr<nsIVariant> data;
  aError = nsContentUtils::XPConnect()->JSValToVariant(aCx, aData, getter_AddRefs(data));
  if (aError.Failed()) {
    return;
  }

  nsCOMPtr<nsIVariant> oldData;
  aError = SetUserData(aKey, data, getter_AddRefs(oldData));
  if (aError.Failed()) {
    return;
  }

  if (!oldData) {
    aRetval.setNull();
    return;
  }

  JSAutoCompartment ac(aCx, GetWrapper());
  aError = nsContentUtils::XPConnect()->VariantToJS(aCx, GetWrapper(), oldData,
                                                    aRetval);
}

nsIVariant*
nsINode::GetUserData(const nsAString& aKey)
{
  OwnerDoc()->WarnOnceAbout(nsIDocument::eGetSetUserData);
  nsCOMPtr<nsIAtom> key = do_GetAtom(aKey);
  if (!key) {
    return nullptr;
  }

  return static_cast<nsIVariant*>(GetProperty(DOM_USER_DATA, key));
}

void
nsINode::GetUserData(JSContext* aCx, const nsAString& aKey,
                     JS::MutableHandle<JS::Value> aRetval, ErrorResult& aError)
{
  nsIVariant* data = GetUserData(aKey);
  if (!data) {
    aRetval.setNull();
    return;
  }

  JSAutoCompartment ac(aCx, GetWrapper());
  aError = nsContentUtils::XPConnect()->VariantToJS(aCx, GetWrapper(), data,
                                                    aRetval);
}

uint16_t
nsINode::CompareDocumentPosition(nsINode& aOtherNode) const
{
  if (this == &aOtherNode) {
    return 0;
  }
  if (GetPreviousSibling() == &aOtherNode) {
    MOZ_ASSERT(GetParentNode() == aOtherNode.GetParentNode());
    return static_cast<uint16_t>(nsIDOMNode::DOCUMENT_POSITION_PRECEDING);
  }
  if (GetNextSibling() == &aOtherNode) {
    MOZ_ASSERT(GetParentNode() == aOtherNode.GetParentNode());
    return static_cast<uint16_t>(nsIDOMNode::DOCUMENT_POSITION_FOLLOWING);
  }

  nsAutoTArray<const nsINode*, 32> parents1, parents2;

  const nsINode *node1 = &aOtherNode, *node2 = this;

  // Check if either node is an attribute
  const Attr* attr1 = nullptr;
  if (node1->IsNodeOfType(nsINode::eATTRIBUTE)) {
    attr1 = static_cast<const Attr*>(node1);
    const Element* elem = attr1->GetElement();
    // If there is an owner element add the attribute
    // to the chain and walk up to the element
    if (elem) {
      node1 = elem;
      parents1.AppendElement(attr1);
    }
  }
  if (node2->IsNodeOfType(nsINode::eATTRIBUTE)) {
    const Attr* attr2 = static_cast<const Attr*>(node2);
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
          return nsIDOMNode::DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC |
            nsIDOMNode::DOCUMENT_POSITION_PRECEDING;
        }
        if (attrName->Equals(attr2->NodeInfo())) {
          return nsIDOMNode::DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC |
            nsIDOMNode::DOCUMENT_POSITION_FOLLOWING;
        }
      }
      NS_NOTREACHED("neither attribute in the element");
      return nsIDOMNode::DOCUMENT_POSITION_DISCONNECTED;
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
      (nsIDOMNode::DOCUMENT_POSITION_PRECEDING |
       nsIDOMNode::DOCUMENT_POSITION_DISCONNECTED |
       nsIDOMNode::DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC) :
      (nsIDOMNode::DOCUMENT_POSITION_FOLLOWING |
       nsIDOMNode::DOCUMENT_POSITION_DISCONNECTED |
       nsIDOMNode::DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC);
  }

  // Find where the parent chain differs and check indices in the parent.
  const nsINode* parent = top1;
  uint32_t len;
  for (len = std::min(pos1, pos2); len > 0; --len) {
    const nsINode* child1 = parents1.ElementAt(--pos1);
    const nsINode* child2 = parents2.ElementAt(--pos2);
    if (child1 != child2) {
      // child1 or child2 can be an attribute here. This will work fine since
      // IndexOf will return -1 for the attribute making the attribute be
      // considered before any child.
      return parent->IndexOf(child1) < parent->IndexOf(child2) ?
        static_cast<uint16_t>(nsIDOMNode::DOCUMENT_POSITION_PRECEDING) :
        static_cast<uint16_t>(nsIDOMNode::DOCUMENT_POSITION_FOLLOWING);
    }
    parent = child1;
  }

  // We hit the end of one of the parent chains without finding a difference
  // between the chains. That must mean that one node is an ancestor of the
  // other. The one with the shortest chain must be the ancestor.
  return pos1 < pos2 ?
    (nsIDOMNode::DOCUMENT_POSITION_PRECEDING |
     nsIDOMNode::DOCUMENT_POSITION_CONTAINS) :
    (nsIDOMNode::DOCUMENT_POSITION_FOLLOWING |
     nsIDOMNode::DOCUMENT_POSITION_CONTAINED_BY);    
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
      case nsIDOMNode::ELEMENT_NODE:
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
      case nsIDOMNode::TEXT_NODE:
      case nsIDOMNode::COMMENT_NODE:
      case nsIDOMNode::CDATA_SECTION_NODE:
      case nsIDOMNode::PROCESSING_INSTRUCTION_NODE:
      {
        string1.Truncate();
        static_cast<nsIContent*>(node1)->AppendTextTo(string1);
        string2.Truncate();
        static_cast<nsIContent*>(node2)->AppendTextTo(string2);

        if (!string1.Equals(string2)) {
          return false;
        }

        break;
      }
      case nsIDOMNode::DOCUMENT_NODE:
      case nsIDOMNode::DOCUMENT_FRAGMENT_NODE:
        break;
      case nsIDOMNode::ATTRIBUTE_NODE:
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
      case nsIDOMNode::DOCUMENT_TYPE_NODE:
      {
        nsCOMPtr<nsIDOMDocumentType> docType1 = do_QueryInterface(node1);
        nsCOMPtr<nsIDOMDocumentType> docType2 = do_QueryInterface(node2);
    
        NS_ASSERTION(docType1 && docType2, "Why don't we have a document type node?");

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
    
        // Internal subset
        docType1->GetInternalSubset(string1);
        docType2->GetInternalSubset(string2);
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

NS_IMPL_DOMTARGET_DEFAULTS(nsINode)

NS_IMETHODIMP
nsINode::AddEventListener(const nsAString& aType,
                          nsIDOMEventListener *aListener,
                          bool aUseCapture,
                          bool aWantsUntrusted,
                          uint8_t aOptionalArgc)
{
  NS_ASSERTION(!aWantsUntrusted || aOptionalArgc > 1,
               "Won't check if this is chrome, you want to set "
               "aWantsUntrusted to false or make the aWantsUntrusted "
               "explicit by making aOptionalArgc non-zero.");

  if (!aWantsUntrusted &&
      (aOptionalArgc < 2 &&
       !nsContentUtils::IsChromeDoc(OwnerDoc()))) {
    aWantsUntrusted = true;
  }

  EventListenerManager* listener_manager = GetOrCreateListenerManager();
  NS_ENSURE_STATE(listener_manager);
  listener_manager->AddEventListener(aType, aListener, aUseCapture,
                                     aWantsUntrusted);
  return NS_OK;
}

void
nsINode::AddEventListener(const nsAString& aType,
                          EventListener* aListener,
                          bool aUseCapture,
                          const Nullable<bool>& aWantsUntrusted,
                          ErrorResult& aRv)
{
  bool wantsUntrusted;
  if (aWantsUntrusted.IsNull()) {
    wantsUntrusted = !nsContentUtils::IsChromeDoc(OwnerDoc());
  } else {
    wantsUntrusted = aWantsUntrusted.Value();
  }

  EventListenerManager* listener_manager = GetOrCreateListenerManager();
  if (!listener_manager) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }
  listener_manager->AddEventListener(aType, aListener, aUseCapture,
                                     wantsUntrusted);
}

NS_IMETHODIMP
nsINode::AddSystemEventListener(const nsAString& aType,
                                nsIDOMEventListener *aListener,
                                bool aUseCapture,
                                bool aWantsUntrusted,
                                uint8_t aOptionalArgc)
{
  NS_ASSERTION(!aWantsUntrusted || aOptionalArgc > 1,
               "Won't check if this is chrome, you want to set "
               "aWantsUntrusted to false or make the aWantsUntrusted "
               "explicit by making aOptionalArgc non-zero.");

  if (!aWantsUntrusted &&
      (aOptionalArgc < 2 &&
       !nsContentUtils::IsChromeDoc(OwnerDoc()))) {
    aWantsUntrusted = true;
  }

  return NS_AddSystemEventListener(this, aType, aListener, aUseCapture,
                                   aWantsUntrusted);
}

NS_IMETHODIMP
nsINode::RemoveEventListener(const nsAString& aType,
                             nsIDOMEventListener* aListener,
                             bool aUseCapture)
{
  EventListenerManager* elm = GetExistingListenerManager();
  if (elm) {
    elm->RemoveEventListener(aType, aListener, aUseCapture);
  }
  return NS_OK;
}

NS_IMPL_REMOVE_SYSTEM_EVENT_LISTENER(nsINode)

nsresult
nsINode::PreHandleEvent(EventChainPreVisitor& aVisitor)
{
  // This is only here so that we can use the NS_DECL_NSIDOMTARGET macro
  NS_ABORT();
  return NS_ERROR_NOT_IMPLEMENTED;
}

void
nsINode::GetBoxQuads(const BoxQuadOptions& aOptions,
                     nsTArray<nsRefPtr<DOMQuad> >& aResult,
                     mozilla::ErrorResult& aRv)
{
  mozilla::GetBoxQuads(this, aOptions, aResult, aRv);
}

already_AddRefed<DOMQuad>
nsINode::ConvertQuadFromNode(DOMQuad& aQuad,
                             const GeometryNode& aFrom,
                             const ConvertCoordinateOptions& aOptions,
                             ErrorResult& aRv)
{
  return mozilla::ConvertQuadFromNode(this, aQuad, aFrom, aOptions, aRv);
}

already_AddRefed<DOMQuad>
nsINode::ConvertRectFromNode(DOMRectReadOnly& aRect,
                             const GeometryNode& aFrom,
                             const ConvertCoordinateOptions& aOptions,
                             ErrorResult& aRv)
{
  return mozilla::ConvertRectFromNode(this, aRect, aFrom, aOptions, aRv);
}

already_AddRefed<DOMPoint>
nsINode::ConvertPointFromNode(const DOMPointInit& aPoint,
                              const GeometryNode& aFrom,
                              const ConvertCoordinateOptions& aOptions,
                              ErrorResult& aRv)
{
  return mozilla::ConvertPointFromNode(this, aPoint, aFrom, aOptions, aRv);
}

nsresult
nsINode::DispatchEvent(nsIDOMEvent *aEvent, bool* aRetVal)
{
  // XXX sXBL/XBL2 issue -- do we really want the owner here?  What
  // if that's the XBL document?  Would we want its presshell?  Or what?
  nsCOMPtr<nsIDocument> document = OwnerDoc();

  // Do nothing if the element does not belong to a document
  if (!document) {
    *aRetVal = true;
    return NS_OK;
  }

  // Obtain a presentation shell
  nsIPresShell *shell = document->GetShell();
  nsRefPtr<nsPresContext> context;
  if (shell) {
    context = shell->GetPresContext();
  }

  nsEventStatus status = nsEventStatus_eIgnore;
  nsresult rv =
    EventDispatcher::DispatchDOMEvent(this, nullptr, aEvent, context, &status);
  *aRetVal = (status != nsEventStatus_eConsumeNoDefault);
  return rv;
}

nsresult
nsINode::PostHandleEvent(EventChainPostVisitor& /*aVisitor*/)
{
  return NS_OK;
}

nsresult
nsINode::DispatchDOMEvent(WidgetEvent* aEvent,
                          nsIDOMEvent* aDOMEvent,
                          nsPresContext* aPresContext,
                          nsEventStatus* aEventStatus)
{
  return EventDispatcher::DispatchDOMEvent(this, aEvent, aDOMEvent,
                                           aPresContext, aEventStatus);
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

nsIScriptContext*
nsINode::GetContextForEventHandlers(nsresult* aRv)
{
  return nsContentUtils::GetContextForEventHandlers(this, aRv);
}

nsIDOMWindow*
nsINode::GetOwnerGlobalForBindings()
{
  bool dummy;
  return nsPIDOMWindow::GetOuterFromCurrentInner(
    static_cast<nsGlobalWindow*>(OwnerDoc()->GetScriptHandlingObject(dummy)));
}

nsIGlobalObject*
nsINode::GetOwnerGlobal() const
{
  bool dummy;
  return OwnerDoc()->GetScriptHandlingObject(dummy);
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
         NodeType() == nsIDOMNode::ATTRIBUTE_NODE ||
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
      if (tmp->IsBlack() || tmp->InCCBlackTree()) {
        return false;
      }

      if (!tmp->UnoptimizableCCNode()) {
        // If we're in a black document, return early.
        if ((currentDoc && currentDoc->IsBlack())) {
          return false;
        }
        // If we're not in anonymous content and we have a black parent,
        // return early.
        nsIContent* parent = tmp->GetParent();
        if (parent && !parent->UnoptimizableCCNode() && parent->IsBlack()) {
          MOZ_ASSERT(parent->IndexOf(tmp) >= 0, "Parent doesn't own us?");
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
    nsNodeUtils::TraverseUserData(tmp, cb);
    nsCOMArray<nsISupports>* objects =
      static_cast<nsCOMArray<nsISupports>*>(tmp->GetProperty(nsGkAtoms::keepobjectsalive));
    if (objects) {
      for (int32_t i = 0; i < objects->Count(); ++i) {
         cb.NoteXPCOMChild(objects->ObjectAt(i));
      }
    }
  }

  if (tmp->NodeType() != nsIDOMNode::DOCUMENT_NODE &&
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

  if (tmp->NodeType() != nsIDOMNode::DOCUMENT_NODE &&
      tmp->HasFlag(NODE_HAS_LISTENERMANAGER)) {
    nsContentUtils::RemoveListenerManager(tmp);
    tmp->UnsetFlags(NODE_HAS_LISTENERMANAGER);
  }

  if (tmp->HasProperties()) {
    nsNodeUtils::UnlinkUserData(tmp);
    tmp->DeleteProperty(nsGkAtoms::keepobjectsalive);
  }
}

static void
ReleaseURI(void*, /* aObject*/
           nsIAtom*, /* aPropertyName */
           void* aPropertyValue,
           void* /* aData */)
{
  nsIURI* uri = static_cast<nsIURI*>(aPropertyValue);
  NS_RELEASE(uri);
}

nsresult
nsINode::SetExplicitBaseURI(nsIURI* aURI)
{
  nsresult rv = SetProperty(nsGkAtoms::baseURIProperty, aURI, ReleaseURI);
  if (NS_SUCCEEDED(rv)) {
    SetHasExplicitBaseURI();
    NS_ADDREF(aURI);
  }
  return rv;
}

static nsresult
AdoptNodeIntoOwnerDoc(nsINode *aParent, nsINode *aNode)
{
  NS_ASSERTION(!aNode->GetParentNode(),
               "Should have removed from parent already");

  nsIDocument *doc = aParent->OwnerDoc();

  nsresult rv;
  nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(doc, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMNode> node = do_QueryInterface(aNode, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMNode> adoptedNode;
  rv = domDoc->AdoptNode(node, getter_AddRefs(adoptedNode));
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ASSERTION(aParent->OwnerDoc() == doc,
               "ownerDoc chainged while adopting");
  NS_ASSERTION(adoptedNode == node, "Uh, adopt node changed nodes?");
  NS_ASSERTION(aParent->OwnerDoc() == aNode->OwnerDoc(),
               "ownerDocument changed again after adopting!");

  return NS_OK;
}

static nsresult
CheckForOutdatedParent(nsINode* aParent, nsINode* aNode)
{
  if (JSObject* existingObjUnrooted = aNode->GetWrapper()) {
    JSRuntime* runtime = JS_GetObjectRuntime(existingObjUnrooted);
    JS::Rooted<JSObject*> existingObj(runtime, existingObjUnrooted);

    AutoJSContext cx;
    nsIGlobalObject* global = aParent->OwnerDoc()->GetScopeObject();
    MOZ_ASSERT(global);

    if (js::GetGlobalForObjectCrossCompartment(existingObj) !=
        global->GetGlobalJSObject()) {
      JSAutoCompartment ac(cx, existingObj);
      nsresult rv = ReparentWrapper(cx, existingObj);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

nsresult
nsINode::doInsertChildAt(nsIContent* aKid, uint32_t aIndex,
                         bool aNotify, nsAttrAndChildArray& aChildArray)
{
  NS_PRECONDITION(!aKid->GetParentNode(),
                  "Inserting node that already has parent");
  nsresult rv;

  // The id-handling code, and in the future possibly other code, need to
  // react to unexpected attribute changes.
  nsMutationGuard::DidMutate();

  // Do this before checking the child-count since this could cause mutations
  nsIDocument* doc = GetUncomposedDoc();
  mozAutoDocUpdate updateBatch(GetCrossShadowCurrentDoc(), UPDATE_CONTENT_MODEL, aNotify);

  if (OwnerDoc() != aKid->OwnerDoc()) {
    rv = AdoptNodeIntoOwnerDoc(this, aKid);
    NS_ENSURE_SUCCESS(rv, rv);
  } else if (OwnerDoc()->DidDocumentOpen()) {
    rv = CheckForOutdatedParent(this, aKid);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  uint32_t childCount = aChildArray.ChildCount();
  NS_ENSURE_TRUE(aIndex <= childCount, NS_ERROR_ILLEGAL_VALUE);
  bool isAppend = (aIndex == childCount);

  rv = aChildArray.InsertChildAt(aKid, aIndex);
  NS_ENSURE_SUCCESS(rv, rv);
  if (aIndex == 0) {
    mFirstChild = aKid;
  }

  nsIContent* parent =
    IsNodeOfType(eDOCUMENT) ? nullptr : static_cast<nsIContent*>(this);

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

  NS_ASSERTION(aKid->GetParentNode() == this,
               "Did we run script inappropriately?");

  if (aNotify) {
    // Note that we always want to call ContentInserted when things are added
    // as kids to documents
    if (parent && isAppend) {
      nsNodeUtils::ContentAppended(parent, aKid, aIndex);
    } else {
      nsNodeUtils::ContentInserted(this, aKid, aIndex);
    }

    if (nsContentUtils::HasMutationListeners(aKid,
          NS_EVENT_BITS_MUTATION_NODEINSERTED, this)) {
      InternalMutationEvent mutation(true, NS_MUTATION_NODEINSERTED);
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

void
nsINode::Remove()
{
  nsCOMPtr<nsINode> parent = GetParentNode();
  if (!parent) {
    return;
  }
  int32_t index = parent->IndexOf(this);
  if (index < 0) {
    NS_WARNING("Ignoring call to nsINode::Remove on anonymous child.");
    return;
  }
  parent->RemoveChildAt(uint32_t(index), true);
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
nsINode::doRemoveChildAt(uint32_t aIndex, bool aNotify,
                         nsIContent* aKid, nsAttrAndChildArray& aChildArray)
{
  NS_PRECONDITION(aKid && aKid->GetParentNode() == this &&
                  aKid == GetChildAt(aIndex) &&
                  IndexOf(aKid) == (int32_t)aIndex, "Bogus aKid");

  nsMutationGuard::DidMutate();
  mozAutoDocUpdate updateBatch(GetCrossShadowCurrentDoc(), UPDATE_CONTENT_MODEL, aNotify);

  nsIContent* previousSibling = aKid->GetPreviousSibling();

  if (GetFirstChild() == aKid) {
    mFirstChild = aKid->GetNextSibling();
  }

  aChildArray.RemoveChildAt(aIndex);

  if (aNotify) {
    nsNodeUtils::ContentRemoved(this, aKid, aIndex, previousSibling);
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
  MOZ_ASSERT(aParent->IsNodeOfType(nsINode::eDOCUMENT) ||
             aParent->IsNodeOfType(nsINode::eDOCUMENT_FRAGMENT) ||
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
  case nsIDOMNode::COMMENT_NODE :
  case nsIDOMNode::PROCESSING_INSTRUCTION_NODE :
    // OK in both cases
    return true;
  case nsIDOMNode::TEXT_NODE :
  case nsIDOMNode::CDATA_SECTION_NODE :
  case nsIDOMNode::ENTITY_REFERENCE_NODE :
    // Allowed under Elements and DocumentFragments
    return aParent->NodeType() != nsIDOMNode::DOCUMENT_NODE;
  case nsIDOMNode::ELEMENT_NODE :
    {
      if (!aParent->IsNodeOfType(nsINode::eDOCUMENT)) {
        // Always ok to have elements under other elements or document fragments
        return true;
      }

      nsIDocument* parentDocument = static_cast<nsIDocument*>(aParent);
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

      int32_t doctypeIndex = aParent->IndexOf(docTypeContent);
      int32_t insertIndex = aParent->IndexOf(aRefChild);

      // Now we're OK in the following two cases only:
      // 1) We're replacing something that's not before the doctype
      // 2) We're inserting before something that comes after the doctype 
      return aIsReplace ? (insertIndex >= doctypeIndex) :
        insertIndex > doctypeIndex;
    }
  case nsIDOMNode::DOCUMENT_TYPE_NODE :
    {
      if (!aParent->IsNodeOfType(nsINode::eDOCUMENT)) {
        // doctypes only allowed under documents
        return false;
      }

      nsIDocument* parentDocument = static_cast<nsIDocument*>(aParent);
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

      int32_t rootIndex = aParent->IndexOf(rootElement);
      int32_t insertIndex = aParent->IndexOf(aRefChild);

      // Now we're OK if and only if insertIndex <= rootIndex.  Indeed, either
      // we end up replacing aRefChild or we end up before it.  Either one is
      // ok as long as aRefChild is not after rootElement.
      return insertIndex <= rootIndex;
    }
  case nsIDOMNode::DOCUMENT_FRAGMENT_NODE :
    {
      // Note that for now we only allow nodes inside document fragments if
      // they're allowed inside elements.  If we ever change this to allow
      // doctype nodes in document fragments, we'll need to update this code.
      // Also, there's a version of this code in ReplaceOrInsertBefore.  If you
      // change this code, change that too.
      if (!aParent->IsNodeOfType(nsINode::eDOCUMENT)) {
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

  if ((!IsNodeOfType(eDOCUMENT) &&
       !IsNodeOfType(eDOCUMENT_FRAGMENT) &&
       !IsElement()) ||
      !aNewChild->IsNodeOfType(eCONTENT)) {
    aError.Throw(NS_ERROR_DOM_HIERARCHY_REQUEST_ERR);
    return nullptr;
  }

  uint16_t nodeType = aNewChild->NodeType();

  // Before we do anything else, fire all DOMNodeRemoved mutation events
  // We do this up front as to avoid having to deal with script running
  // at random places further down.
  // Scope firing mutation events so that we don't carry any state that
  // might be stale
  {
    // This check happens again further down (though then using IndexOf).
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
    if (nodeType == nsIDOMNode::DOCUMENT_FRAGMENT_NODE) {
      static_cast<FragmentOrElement*>(aNewChild)->FireNodeRemovedForChildren();
    }
    // Verify that our aRefChild is still sensible
    if (aRefChild && aRefChild->GetParentNode() != this) {
      aError.Throw(NS_ERROR_DOM_NOT_FOUND_ERR);
      return nullptr;
    }
  }

  nsIDocument* doc = OwnerDoc();
  nsIContent* newContent = static_cast<nsIContent*>(aNewChild);
  if (newContent->IsRootOfAnonymousSubtree()) {
    // This is anonymous content.  Don't allow its insertion
    // anywhere, since it might have UnbindFromTree calls coming
    // its way.
    aError.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return nullptr;
  }

  // Make sure that the inserted node is allowed as a child of its new parent.
  if (!IsAllowedAsChild(newContent, this, aReplace, aRefChild)) {
    aError.Throw(NS_ERROR_DOM_HIERARCHY_REQUEST_ERR);
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

  Maybe<nsAutoTArray<nsCOMPtr<nsIContent>, 50> > fragChildren;

  // Remove the new child from the old parent if one exists
  nsCOMPtr<nsINode> oldParent = newContent->GetParentNode();
  if (oldParent) {
    int32_t removeIndex = oldParent->IndexOf(newContent);
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
      oldParent->RemoveChildAt(removeIndex, true);
      if (nsAutoMutationBatch::GetCurrentBatch() == &mb) {
        mb.RemovalDone();
        mb.SetPrevSibling(oldParent->GetChildAt(removeIndex - 1));
        mb.SetNextSibling(oldParent->GetChildAt(removeIndex));
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
  } else if (nodeType == nsIDOMNode::DOCUMENT_FRAGMENT_NODE) {
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
        newContent->RemoveChildAt(--i, true);
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
      if (IsNodeOfType(nsINode::eDOCUMENT)) {
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

  mozAutoDocUpdate batch(GetCrossShadowCurrentDoc(), UPDATE_CONTENT_MODEL, true);
  nsAutoMutationBatch mb;

  // Figure out which index we want to insert at.  Note that we use
  // nodeToInsertBefore to determine this, because it's possible that
  // aRefChild == aNewChild, in which case we just removed it from the
  // parent list.
  int32_t insPos;
  if (nodeToInsertBefore) {
    insPos = IndexOf(nodeToInsertBefore);
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
    RemoveChildAt(insPos-1, true);
    --insPos;
  }

  // Move new child over to our document if needed. Do this after removing
  // it from its parent so that AdoptNode doesn't fire DOMNodeRemoved
  // DocumentType nodes are the only nodes that can have a null
  // ownerDocument according to the DOM spec, and we need to allow
  // inserting them w/o calling AdoptNode().
  if (doc != newContent->OwnerDoc()) {
    aError = AdoptNodeIntoOwnerDoc(this, aNewChild);
    if (aError.Failed()) {
      return nullptr;
    }
  } else if (doc->DidDocumentOpen()) {
    aError = CheckForOutdatedParent(this, aNewChild);
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
  if (nodeType == nsIDOMNode::DOCUMENT_FRAGMENT_NODE) {
    if (!aReplace) {
      mb.Init(this, true, true);
    }
    nsAutoMutationBatch* mutationBatch = nsAutoMutationBatch::GetCurrentBatch();
    if (mutationBatch) {
      mutationBatch->RemovalDone();
      mutationBatch->SetPrevSibling(GetChildAt(insPos - 1));
      mutationBatch->SetNextSibling(GetChildAt(insPos));
    }

    uint32_t count = fragChildren->Length();
    if (!count) {
      return result;
    }

    bool appending =
      !IsNodeOfType(eDOCUMENT) && uint32_t(insPos) == GetChildCount();
    int32_t firstInsPos = insPos;
    nsIContent* firstInsertedContent = fragChildren->ElementAt(0);

    // Iterate through the fragment's children, and insert them in the new
    // parent
    for (uint32_t i = 0; i < count; ++i, ++insPos) {
      // XXXbz how come no reparenting here?  That seems odd...
      // Insert the child.
      aError = InsertChildAt(fragChildren->ElementAt(i), insPos,
                             !appending);
      if (aError.Failed()) {
        // Make sure to notify on any children that we did succeed to insert
        if (appending && i != 0) {
          nsNodeUtils::ContentAppended(static_cast<nsIContent*>(this),
                                       firstInsertedContent,
                                       firstInsPos);
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
                                   firstInsertedContent, firstInsPos);
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
      mb.SetPrevSibling(GetChildAt(insPos - 1));
      mb.SetNextSibling(GetChildAt(insPos));
    }
    aError = InsertChildAt(newContent, insPos, true);
    if (aError.Failed()) {
      return nullptr;
    }
  }

  return result;
}

nsresult
nsINode::ReplaceOrInsertBefore(bool aReplace, nsIDOMNode *aNewChild,
                               nsIDOMNode *aRefChild, nsIDOMNode **aReturn)
{
  nsCOMPtr<nsINode> newChild = do_QueryInterface(aNewChild);
  if (!newChild) {
    return NS_ERROR_NULL_POINTER;
  }

  if (aReplace && !aRefChild) {
    return NS_ERROR_NULL_POINTER;
  }

  nsCOMPtr<nsINode> refChild = do_QueryInterface(aRefChild);
  if (aRefChild && !refChild) {
    return NS_NOINTERFACE;
  }

  ErrorResult rv;
  nsINode* result = ReplaceOrInsertBefore(aReplace, newChild, refChild, rv);
  if (result) {
    NS_ADDREF(*aReturn = result->AsDOMNode());
  }
  return rv.StealNSResult();
}

nsresult
nsINode::CompareDocumentPosition(nsIDOMNode* aOther, uint16_t* aReturn)
{
  nsCOMPtr<nsINode> other = do_QueryInterface(aOther);
  NS_ENSURE_ARG(other);
  *aReturn = CompareDocumentPosition(*other);
  return NS_OK;
}

nsresult
nsINode::IsEqualNode(nsIDOMNode* aOther, bool* aReturn)
{
  nsCOMPtr<nsINode> other = do_QueryInterface(aOther);
  *aReturn = IsEqualNode(other);
  return NS_OK;
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

void
nsINode::GetBoundMutationObservers(nsTArray<nsRefPtr<nsDOMMutationObserver> >& aResult)
{
  nsCOMArray<nsISupports>* objects =
    static_cast<nsCOMArray<nsISupports>*>(GetProperty(nsGkAtoms::keepobjectsalive));
  if (objects) {
    for (int32_t i = 0; i < objects->Count(); ++i) {
      nsCOMPtr<nsDOMMutationObserver> mo = do_QueryInterface(objects->ObjectAt(i));
      if (mo) {
        MOZ_ASSERT(!aResult.Contains(mo));
        aResult.AppendElement(mo.forget());
      }
    }
  }
}

size_t
nsINode::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = 0;
  EventListenerManager* elm = GetExistingListenerManager();
  if (elm) {
    n += elm->SizeOfIncludingThis(aMallocSizeOf);
  }

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - mNodeInfo
  // - mSlots
  //
  // The following members are not measured:
  // - mParent, mNextSibling, mPreviousSibling, mFirstChild: because they're
  //   non-owning
  return n;
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
      IsInDoc() != aOther->IsInDoc() ||
      !(aOther->IsElement() ||
        aOther->IsNodeOfType(nsINode::eCONTENT)) ||
      !GetFirstChild()) {
    return false;
  }

  const nsIContent* other = static_cast<const nsIContent*>(aOther);
  if (this == OwnerDoc()) {
    // document.contains(aOther) returns true if aOther is in the document,
    // but is not in any anonymous subtree.
    // IsInDoc() check is done already before this.
    return !other->IsInAnonymousSubtree();
  }

  if (!IsElement() && !IsNodeOfType(nsINode::eDOCUMENT_FRAGMENT)) {
    return false;
  }

  const nsIContent* thisContent = static_cast<const nsIContent*>(this);
  if (thisContent->GetBindingParent() != other->GetBindingParent()) {
    return false;
  }

  return nsContentUtils::ContentIsDescendantOf(other, this);
}

nsresult
nsINode::Contains(nsIDOMNode* aOther, bool* aReturn)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aOther);
  *aReturn = Contains(node);
  return NS_OK;
}

uint32_t
nsINode::Length() const
{
  switch (NodeType()) {
  case nsIDOMNode::DOCUMENT_TYPE_NODE:
    return 0;

  case nsIDOMNode::TEXT_NODE:
  case nsIDOMNode::CDATA_SECTION_NODE:
  case nsIDOMNode::PROCESSING_INSTRUCTION_NODE:
  case nsIDOMNode::COMMENT_NODE:
    MOZ_ASSERT(IsNodeOfType(eCONTENT));
    return static_cast<const nsIContent*>(this)->TextLength();

  default:
    return GetChildCount();
  }
}

nsCSSSelectorList*
nsINode::ParseSelectorList(const nsAString& aSelectorString,
                           ErrorResult& aRv)
{
  nsIDocument* doc = OwnerDoc();
  nsIDocument::SelectorCache& cache = doc->GetSelectorCache();
  nsCSSSelectorList* selectorList = nullptr;
  bool haveCachedList = cache.GetList(aSelectorString, &selectorList);
  if (haveCachedList) {
    if (!selectorList) {
      // Invalid selector.
      aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    }
    return selectorList;
  }

  nsCSSParser parser(doc->CSSLoader());

  aRv = parser.ParseSelectorString(aSelectorString,
                                   doc->GetDocumentURI(),
                                   0, // XXXbz get the line number!
                                   &selectorList);
  if (aRv.Failed()) {
    // We hit this for syntax errors, which are quite common, so don't
    // use NS_ENSURE_SUCCESS.  (For example, jQuery has an extended set
    // of selectors, but it sees if we can parse them first.)
    MOZ_ASSERT(aRv.ErrorCodeIs(NS_ERROR_DOM_SYNTAX_ERR),
               "Unexpected error, so cached version won't return it");
    cache.CacheList(aSelectorString, nullptr);
    return nullptr;
  }

  // Filter out pseudo-element selectors from selectorList
  nsCSSSelectorList** slot = &selectorList;
  do {
    nsCSSSelectorList* cur = *slot;
    if (cur->mSelectors->IsPseudoElement()) {
      *slot = cur->mNext;
      cur->mNext = nullptr;
      delete cur;
    } else {
      slot = &cur->mNext;
    }
  } while (*slot);

  if (selectorList) {
    NS_ASSERTION(selectorList->mSelectors,
                 "How can we not have any selectors?");
    cache.CacheList(aSelectorString, selectorList);
  } else {
    // This is the "only pseudo-element selectors" case, which is
    // not common, so just don't worry about caching it.  That way a
    // null cached value can always indicate an invalid selector.
  }

  return selectorList;
}

static void
AddScopeElements(TreeMatchContext& aMatchContext,
                 nsINode* aMatchContextNode)
{
  if (aMatchContextNode->IsElement()) {
    aMatchContext.SetHasSpecifiedScope();
    aMatchContext.AddScopeElement(aMatchContextNode->AsElement());
  }
}

namespace {
struct SelectorMatchInfo {
  nsCSSSelectorList* const mSelectorList;
  TreeMatchContext& mMatchContext;
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
  MOZ_ASSERT(aRoot->IsInDoc(),
             "Don't call me if the root is not in the document");
  MOZ_ASSERT(aRoot->IsElement() || aRoot->IsNodeOfType(nsINode::eDOCUMENT),
             "The optimization below to check ContentIsDescendantOf only for "
             "elements depends on aRoot being either an element or a "
             "document if it's in the document.  Note that document fragments "
             "can't be IsInDoc(), so should never show up here.");

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
      if (!aMatchInfo ||
          nsCSSRuleProcessor::SelectorListMatches(element,
                                                  aMatchInfo->mMatchContext,
                                                  aMatchInfo->mSelectorList)) {
        aList.AppendElement(element);
        if (onlyFirstMatch) {
          return;
        }
      }
    }
  }
}

// Actually find elements matching aSelectorList (which must not be
// null) and which are descendants of aRoot and put them in aList.  If
// onlyFirstMatch, then stop once the first one is found.
template<bool onlyFirstMatch, class Collector, class T>
MOZ_ALWAYS_INLINE static void
FindMatchingElements(nsINode* aRoot, nsCSSSelectorList* aSelectorList, T &aList,
                     ErrorResult& aRv)
{
  nsIDocument* doc = aRoot->OwnerDoc();

  TreeMatchContext matchingContext(false, nsRuleWalker::eRelevantLinkUnvisited,
                                   doc, TreeMatchContext::eNeverMatchVisited);
  doc->FlushPendingLinkUpdates();
  AddScopeElements(matchingContext, aRoot);

  // Fast-path selectors involving IDs.  We can only do this if aRoot
  // is in the document and the document is not in quirks mode, since
  // ID selectors are case-insensitive in quirks mode.  Also, only do
  // this if aSelectorList only has one selector, because otherwise
  // ordering the elements correctly is a pain.
  NS_ASSERTION(aRoot->IsElement() || aRoot->IsNodeOfType(nsINode::eDOCUMENT) ||
               !aRoot->IsInDoc(),
               "The optimization below to check ContentIsDescendantOf only for "
               "elements depends on aRoot being either an element or a "
               "document if it's in the document.");
  if (aRoot->IsInDoc() &&
      doc->GetCompatibilityMode() != eCompatibility_NavQuirks &&
      !aSelectorList->mNext &&
      aSelectorList->mSelectors->mIDList) {
    nsIAtom* id = aSelectorList->mSelectors->mIDList->mAtom;
    SelectorMatchInfo info = { aSelectorList, matchingContext };
    FindMatchingElementsWithId<onlyFirstMatch, T>(nsDependentAtomString(id),
                                                  aRoot, &info, aList);
    return;
  }

  Collector results;
  for (nsIContent* cur = aRoot->GetFirstChild();
       cur;
       cur = cur->GetNextNode(aRoot)) {
    if (cur->IsElement() &&
        nsCSSRuleProcessor::SelectorListMatches(cur->AsElement(),
                                                matchingContext,
                                                aSelectorList)) {
      if (onlyFirstMatch) {
        aList.AppendElement(cur->AsElement());
        return;
      }
      results.AppendElement(cur->AsElement());
    }
  }

  const uint32_t len = results.Length();
  if (len) {
    aList.SetCapacity(len);
    for (uint32_t i = 0; i < len; ++i) {
      aList.AppendElement(results.ElementAt(i));
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
  nsCSSSelectorList* selectorList = ParseSelectorList(aSelector, aResult);
  if (!selectorList) {
    // Either we failed (and aResult already has the exception), or this
    // is a pseudo-element-only selector that matches nothing.
    return nullptr;
  }
  ElementHolder holder;
  FindMatchingElements<true, ElementHolder>(this, selectorList, holder, aResult);
  return holder.mElement;
}

already_AddRefed<nsINodeList>
nsINode::QuerySelectorAll(const nsAString& aSelector, ErrorResult& aResult)
{
  nsRefPtr<nsSimpleContentList> contentList = new nsSimpleContentList(this);

  nsCSSSelectorList* selectorList = ParseSelectorList(aSelector, aResult);
  if (selectorList) {
    FindMatchingElements<false, nsAutoTArray<Element*, 128>>(this,
                                                             selectorList,
                                                             *contentList,
                                                             aResult);
  } else {
    // Either we failed (and aResult already has the exception), or this
    // is a pseudo-element-only selector that matches nothing.
  }

  return contentList.forget();
}

nsresult
nsINode::QuerySelector(const nsAString& aSelector, nsIDOMElement **aReturn)
{
  ErrorResult rv;
  Element* result = nsINode::QuerySelector(aSelector, rv);
  if (rv.Failed()) {
    return rv.StealNSResult();
  }
  nsCOMPtr<nsIDOMElement> elt = do_QueryInterface(result);
  elt.forget(aReturn);
  return NS_OK;
}

nsresult
nsINode::QuerySelectorAll(const nsAString& aSelector, nsIDOMNodeList **aReturn)
{
  ErrorResult rv;
  *aReturn = nsINode::QuerySelectorAll(aSelector, rv).take();
  return rv.StealNSResult();
}

Element*
nsINode::GetElementById(const nsAString& aId)
{
  MOZ_ASSERT(IsElement() || IsNodeOfType(eDOCUMENT_FRAGMENT),
             "Bogus this object for GetElementById call");
  if (IsInDoc()) {
    ElementHolder holder;
    FindMatchingElementsWithId<true>(aId, this, nullptr, holder);
    return holder.mElement;
  }

  for (nsIContent* kid = GetFirstChild(); kid; kid = kid->GetNextNode(this)) {
    if (!kid->IsElement()) {
      continue;
    }
    nsIAtom* id = kid->AsElement()->GetID();
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
      !nsContentUtils::IsCallerChrome()) {
    Throw(aCx, NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  JS::Rooted<JSObject*> obj(aCx, WrapNode(aCx, aGivenProto));
  MOZ_ASSERT_IF(ChromeOnlyAccess(),
                xpc::IsInContentXBLScope(obj) || !xpc::UseContentXBLScope(js::GetObjectCompartment(obj)));
  return obj;
}

already_AddRefed<nsINode>
nsINode::CloneNode(bool aDeep, ErrorResult& aError)
{
  nsCOMPtr<nsINode> result;
  aError = nsNodeUtils::CloneNodeImpl(this, aDeep, getter_AddRefs(result));
  return result.forget();
}

nsDOMAttributeMap*
nsINode::GetAttributes()
{
  if (!IsElement()) {
    return nullptr;
  }
  return AsElement()->Attributes();
}

bool
EventTarget::DispatchEvent(Event& aEvent,
                           ErrorResult& aRv)
{
  bool result = false;
  aRv = DispatchEvent(&aEvent, &result);
  return result;
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

  ShadowRoot* shadowRoot = ShadowRoot::FromNode(mParent);
  if (shadowRoot) {
    nsIContent* host = shadowRoot->GetHost();
    MOZ_ASSERT(host, "ShowRoots should always have a host");
    MOZ_ASSERT(host->IsElement(), "ShadowRoot hosts should always be Elements");
    return host->AsElement();
  }

  return nullptr;
}

bool
nsINode::HasBoxQuadsSupport(JSContext* aCx, JSObject* /* unused */)
{
  return xpc::AccessCheck::isChrome(js::GetContextCompartment(aCx)) ||
         Preferences::GetBool("layout.css.getBoxQuads.enabled");
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
