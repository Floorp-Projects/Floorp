/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=79: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Base class for all element classes; this provides an implementation
 * of DOM Core's nsIDOMElement, implements nsIContent, provides
 * utility methods for subclasses, and so forth.
 */

#include "mozilla/Util.h"

#include "nsGenericElement.h"

#include "nsDOMAttribute.h"
#include "nsDOMAttributeMap.h"
#include "nsIAtom.h"
#include "nsINodeInfo.h"
#include "nsIDocument.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMDocument.h"
#include "nsIContentIterator.h"
#include "nsEventListenerManager.h"
#include "nsFocusManager.h"
#include "nsILinkHandler.h"
#include "nsIScriptGlobalObject.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsIFrame.h"
#include "nsIAnonymousContentCreator.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsStyleConsts.h"
#include "nsString.h"
#include "nsUnicharUtils.h"
#include "nsEventStateManager.h"
#include "nsIDOMEvent.h"
#include "nsDOMCID.h"
#include "nsIServiceManager.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsDOMCSSAttrDeclaration.h"
#include "nsINameSpaceManager.h"
#include "nsContentList.h"
#include "nsDOMTokenList.h"
#include "nsXBLPrototypeBinding.h"
#include "nsDOMError.h"
#include "nsDOMString.h"
#include "nsIScriptSecurityManager.h"
#include "nsIDOMMutationEvent.h"
#include "nsMutationEvent.h"
#include "nsNodeUtils.h"
#include "nsDocument.h"
#include "nsAttrValueOrString.h"
#ifdef MOZ_XUL
#include "nsXULElement.h"
#endif /* MOZ_XUL */
#include "nsFrameManager.h"
#include "nsFrameSelection.h"
#ifdef DEBUG
#include "nsRange.h"
#endif

#include "nsBindingManager.h"
#include "nsXBLBinding.h"
#include "nsPIDOMWindow.h"
#include "nsPIBoxObject.h"
#include "nsClientRect.h"
#include "nsSVGUtils.h"
#include "nsLayoutUtils.h"
#include "nsGkAtoms.h"
#include "nsContentUtils.h"
#include "nsIJSContextStack.h"

#include "nsIDOMEventListener.h"
#include "nsIWebNavigation.h"
#include "nsIBaseWindow.h"
#include "nsIWidget.h"

#include "jsapi.h"

#include "nsNodeInfoManager.h"
#include "nsICategoryManager.h"
#include "nsIDOMDocumentType.h"
#include "nsIDOMUserDataHandler.h"
#include "nsGenericHTMLElement.h"
#include "nsIEditor.h"
#include "nsIEditorIMESupport.h"
#include "nsIEditorDocShell.h"
#include "nsEventDispatcher.h"
#include "nsContentCreatorFunctions.h"
#include "nsIControllers.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsIScrollableFrame.h"
#include "nsXBLInsertionPoint.h"
#include "mozilla/css/StyleRule.h" /* For nsCSSSelectorList */
#include "nsCSSRuleProcessor.h"
#include "nsRuleProcessorData.h"
#include "nsAsyncDOMEvent.h"
#include "nsTextNode.h"
#include "dombindings.h"

#ifdef MOZ_XUL
#include "nsIXULDocument.h"
#endif /* MOZ_XUL */

#include "nsCycleCollectionParticipant.h"
#include "nsCCUncollectableMarker.h"

#include "mozAutoDocUpdate.h"

#include "nsCSSParser.h"
#include "prprf.h"
#include "nsDOMMutationObserver.h"
#include "nsSVGFeatures.h"
#include "nsWrapperCacheInlines.h"
#include "nsCycleCollector.h"
#include "xpcpublic.h"
#include "nsIScriptError.h"
#include "nsLayoutStatics.h"
#include "mozilla/Telemetry.h"

#include "mozilla/CORSMode.h"

#include "nsStyledElement.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_DEFINE_IID(kThisPtrOffsetsSID, NS_THISPTROFFSETS_SID);

PRInt32 nsIContent::sTabFocusModel = eTabFocus_any;
bool nsIContent::sTabFocusModelAppliesToXUL = false;
PRUint32 nsMutationGuard::sMutationCount = 0;

nsEventStates
Element::IntrinsicState() const
{
  return IsEditable() ? NS_EVENT_STATE_MOZ_READWRITE :
                        NS_EVENT_STATE_MOZ_READONLY;
}

void
Element::NotifyStateChange(nsEventStates aStates)
{
  nsIDocument* doc = GetCurrentDoc();
  if (doc) {
    nsAutoScriptBlocker scriptBlocker;
    doc->ContentStateChanged(this, aStates);
  }
}

void
Element::UpdateLinkState(nsEventStates aState)
{
  NS_ABORT_IF_FALSE(!aState.HasAtLeastOneOfStates(~(NS_EVENT_STATE_VISITED |
                                                    NS_EVENT_STATE_UNVISITED)),
                    "Unexpected link state bits");
  mState =
    (mState & ~(NS_EVENT_STATE_VISITED | NS_EVENT_STATE_UNVISITED)) |
    aState;
}

void
Element::UpdateState(bool aNotify)
{
  nsEventStates oldState = mState;
  mState = IntrinsicState() | (oldState & ESM_MANAGED_STATES);
  if (aNotify) {
    nsEventStates changedStates = oldState ^ mState;
    if (!changedStates.IsEmpty()) {
      nsIDocument* doc = GetCurrentDoc();
      if (doc) {
        nsAutoScriptBlocker scriptBlocker;
        doc->ContentStateChanged(this, changedStates);
      }
    }
  }
}

void
nsIContent::UpdateEditableState(bool aNotify)
{
  // Guaranteed to be non-element content
  NS_ASSERTION(!IsElement(), "What happened here?");
  nsIContent *parent = GetParent();

  SetEditableFlag(parent && parent->HasFlag(NODE_IS_EDITABLE));
}

void
nsGenericElement::UpdateEditableState(bool aNotify)
{
  nsIContent *parent = GetParent();

  SetEditableFlag(parent && parent->HasFlag(NODE_IS_EDITABLE));
  if (aNotify) {
    UpdateState(aNotify);
  } else {
    // Avoid calling UpdateState in this very common case, because
    // this gets called for pretty much every single element on
    // insertion into the document and UpdateState can be slow for
    // some kinds of elements even when not notifying.
    if (IsEditable()) {
      RemoveStatesSilently(NS_EVENT_STATE_MOZ_READONLY);
      AddStatesSilently(NS_EVENT_STATE_MOZ_READWRITE);
    } else {
      RemoveStatesSilently(NS_EVENT_STATE_MOZ_READWRITE);
      AddStatesSilently(NS_EVENT_STATE_MOZ_READONLY);
    }
  }
}

nsEventStates
Element::StyleStateFromLocks() const
{
  nsEventStates locks = LockedStyleStates();
  nsEventStates state = mState | locks;

  if (locks.HasState(NS_EVENT_STATE_VISITED)) {
    return state & ~NS_EVENT_STATE_UNVISITED;
  }
  if (locks.HasState(NS_EVENT_STATE_UNVISITED)) {
    return state & ~NS_EVENT_STATE_VISITED;
  }
  return state;
}

nsEventStates
Element::LockedStyleStates() const
{
  nsEventStates *locks =
    static_cast<nsEventStates*> (GetProperty(nsGkAtoms::lockedStyleStates));
  if (locks) {
    return *locks;
  }
  return nsEventStates();
}

static void
nsEventStatesPropertyDtor(void *aObject, nsIAtom *aProperty,
                          void *aPropertyValue, void *aData)
{
  nsEventStates *states = static_cast<nsEventStates*>(aPropertyValue);
  delete states;
}

void
Element::NotifyStyleStateChange(nsEventStates aStates)
{
  nsIDocument* doc = GetCurrentDoc();
  if (doc) {
    nsIPresShell *presShell = doc->GetShell();
    if (presShell) {
      nsAutoScriptBlocker scriptBlocker;
      presShell->ContentStateChanged(doc, this, aStates);
    }
  }
}

void
Element::LockStyleStates(nsEventStates aStates)
{
  nsEventStates *locks = new nsEventStates(LockedStyleStates());

  *locks |= aStates;

  if (aStates.HasState(NS_EVENT_STATE_VISITED)) {
    *locks &= ~NS_EVENT_STATE_UNVISITED;
  }
  if (aStates.HasState(NS_EVENT_STATE_UNVISITED)) {
    *locks &= ~NS_EVENT_STATE_VISITED;
  }

  SetProperty(nsGkAtoms::lockedStyleStates, locks, nsEventStatesPropertyDtor);
  SetHasLockedStyleStates();

  NotifyStyleStateChange(aStates);
}

void
Element::UnlockStyleStates(nsEventStates aStates)
{
  nsEventStates *locks = new nsEventStates(LockedStyleStates());

  *locks &= ~aStates;

  if (locks->IsEmpty()) {
    DeleteProperty(nsGkAtoms::lockedStyleStates);
    ClearHasLockedStyleStates();
    delete locks;
  }
  else {
    SetProperty(nsGkAtoms::lockedStyleStates, locks, nsEventStatesPropertyDtor);
  }

  NotifyStyleStateChange(aStates);
}

void
Element::ClearStyleStateLocks()
{
  nsEventStates locks = LockedStyleStates();

  DeleteProperty(nsGkAtoms::lockedStyleStates);
  ClearHasLockedStyleStates();

  NotifyStyleStateChange(locks);
}

nsIContent*
nsIContent::FindFirstNonNativeAnonymous() const
{
  // This handles also nested native anonymous content.
  for (const nsIContent *content = this; content;
       content = content->GetBindingParent()) {
    if (!content->IsInNativeAnonymousSubtree()) {
      // Oops, this function signature allows casting const to
      // non-const.  (Then again, so does GetChildAt(0)->GetParent().)
      return const_cast<nsIContent*>(content);
    }
  }
  return nsnull;
}

nsIContent*
nsIContent::GetFlattenedTreeParent() const
{
  nsIContent *parent = GetParent();
  if (parent && parent->HasFlag(NODE_MAY_BE_IN_BINDING_MNGR)) {
    nsIDocument *doc = parent->OwnerDoc();
    nsIContent* insertionElement =
      doc->BindingManager()->GetNestedInsertionPoint(parent, this);
    if (insertionElement) {
      parent = insertionElement;
    }
  }
  return parent;
}

nsIContent::IMEState
nsIContent::GetDesiredIMEState()
{
  if (!IsEditableInternal()) {
    // Check for the special case where we're dealing with elements which don't
    // have the editable flag set, but are readwrite (such as text controls).
    if (!IsElement() ||
        !AsElement()->State().HasState(NS_EVENT_STATE_MOZ_READWRITE)) {
      return IMEState(IMEState::DISABLED);
    }
  }
  // NOTE: The content for independent editors (e.g., input[type=text],
  // textarea) must override this method, so, we don't need to worry about
  // that here.
  nsIContent *editableAncestor = GetEditingHost();

  // This is in another editable content, use the result of it.
  if (editableAncestor && editableAncestor != this) {
    return editableAncestor->GetDesiredIMEState();
  }
  nsIDocument* doc = GetCurrentDoc();
  if (!doc) {
    return IMEState(IMEState::DISABLED);
  }
  nsIPresShell* ps = doc->GetShell();
  if (!ps) {
    return IMEState(IMEState::DISABLED);
  }
  nsPresContext* pc = ps->GetPresContext();
  if (!pc) {
    return IMEState(IMEState::DISABLED);
  }
  nsIEditor* editor = nsContentUtils::GetHTMLEditor(pc);
  nsCOMPtr<nsIEditorIMESupport> imeEditor = do_QueryInterface(editor);
  if (!imeEditor) {
    return IMEState(IMEState::DISABLED);
  }
  IMEState state;
  imeEditor->GetPreferredIMEState(&state);
  return state;
}

bool
nsIContent::HasIndependentSelection()
{
  nsIFrame* frame = GetPrimaryFrame();
  return (frame && frame->GetStateBits() & NS_FRAME_INDEPENDENT_SELECTION);
}

dom::Element*
nsIContent::GetEditingHost()
{
  // If this isn't editable, return NULL.
  NS_ENSURE_TRUE(IsEditableInternal(), nsnull);

  nsIDocument* doc = GetCurrentDoc();
  NS_ENSURE_TRUE(doc, nsnull);
  // If this is in designMode, we should return <body>
  if (doc->HasFlag(NODE_IS_EDITABLE)) {
    return doc->GetBodyElement();
  }

  nsIContent* content = this;
  for (dom::Element* parent = GetElementParent();
       parent && parent->HasFlag(NODE_IS_EDITABLE);
       parent = content->GetElementParent()) {
    content = parent;
  }
  return content->AsElement();
}

nsresult
nsIContent::LookupNamespaceURIInternal(const nsAString& aNamespacePrefix,
                                       nsAString& aNamespaceURI) const
{
  if (aNamespacePrefix.EqualsLiteral("xml")) {
    // Special-case for xml prefix
    aNamespaceURI.AssignLiteral("http://www.w3.org/XML/1998/namespace");
    return NS_OK;
  }

  if (aNamespacePrefix.EqualsLiteral("xmlns")) {
    // Special-case for xmlns prefix
    aNamespaceURI.AssignLiteral("http://www.w3.org/2000/xmlns/");
    return NS_OK;
  }

  nsCOMPtr<nsIAtom> name;
  if (!aNamespacePrefix.IsEmpty()) {
    name = do_GetAtom(aNamespacePrefix);
    NS_ENSURE_TRUE(name, NS_ERROR_OUT_OF_MEMORY);
  }
  else {
    name = nsGkAtoms::xmlns;
  }
  // Trace up the content parent chain looking for the namespace
  // declaration that declares aNamespacePrefix.
  const nsIContent* content = this;
  do {
    if (content->GetAttr(kNameSpaceID_XMLNS, name, aNamespaceURI))
      return NS_OK;
  } while ((content = content->GetParent()));
  return NS_ERROR_FAILURE;
}

already_AddRefed<nsIURI>
nsIContent::GetBaseURI() const
{
  nsIDocument* doc = OwnerDoc();
  // Start with document base
  nsCOMPtr<nsIURI> base = doc->GetDocBaseURI();

  // Collect array of xml:base attribute values up the parent chain. This
  // is slightly slower for the case when there are xml:base attributes, but
  // faster for the far more common case of there not being any such
  // attributes.
  // Also check for SVG elements which require special handling
  nsAutoTArray<nsString, 5> baseAttrs;
  nsString attr;
  const nsIContent *elem = this;
  do {
    // First check for SVG specialness (why is this SVG specific?)
    if (elem->IsSVG()) {
      nsIContent* bindingParent = elem->GetBindingParent();
      if (bindingParent) {
        nsXBLBinding* binding =
          bindingParent->OwnerDoc()->BindingManager()->GetBinding(bindingParent);
        if (binding) {
          // XXX sXBL/XBL2 issue
          // If this is an anonymous XBL element use the binding
          // document for the base URI. 
          // XXX Will fail with xml:base
          base = binding->PrototypeBinding()->DocURI();
          break;
        }
      }
    }

    nsIURI* explicitBaseURI = elem->GetExplicitBaseURI();
    if (explicitBaseURI) {
      base = explicitBaseURI;
      break;
    }
    
    // Otherwise check for xml:base attribute
    elem->GetAttr(kNameSpaceID_XML, nsGkAtoms::base, attr);
    if (!attr.IsEmpty()) {
      baseAttrs.AppendElement(attr);
    }
    elem = elem->GetParent();
  } while(elem);
  
  // Now resolve against all xml:base attrs
  for (PRUint32 i = baseAttrs.Length() - 1; i != PRUint32(-1); --i) {
    nsCOMPtr<nsIURI> newBase;
    nsresult rv = NS_NewURI(getter_AddRefs(newBase), baseAttrs[i],
                            doc->GetDocumentCharacterSet().get(), base);
    // Do a security check, almost the same as nsDocument::SetBaseURL()
    // Only need to do this on the final uri
    if (NS_SUCCEEDED(rv) && i == 0) {
      rv = nsContentUtils::GetSecurityManager()->
        CheckLoadURIWithPrincipal(NodePrincipal(), newBase,
                                  nsIScriptSecurityManager::STANDARD);
    }
    if (NS_SUCCEEDED(rv)) {
      base.swap(newBase);
    }
  }

  return base.forget();
}

//----------------------------------------------------------------------

static inline JSObject*
GetJSObjectChild(nsWrapperCache* aCache)
{
  return aCache->PreservingWrapper() ? aCache->GetWrapperPreserveColor() : NULL;
}

static bool
NeedsScriptTraverse(nsWrapperCache* aCache)
{
  JSObject* o = GetJSObjectChild(aCache);
  return o && xpc_IsGrayGCThing(o);
}

//----------------------------------------------------------------------

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsChildContentList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsChildContentList)

// If nsChildContentList is changed so that any additional fields are
// traversed by the cycle collector, then CAN_SKIP must be updated to
// check that the additional fields are null.
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(nsChildContentList)

// nsChildContentList only ever has a single child, its wrapper, so if
// the wrapper is black, the list can't be part of a garbage cycle.
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(nsChildContentList)
  return !NeedsScriptTraverse(tmp);
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_BEGIN(nsChildContentList)
  return !NeedsScriptTraverse(tmp);
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

// CanSkipThis returns false to avoid problems with incomplete unlinking.
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(nsChildContentList)
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END

NS_INTERFACE_TABLE_HEAD(nsChildContentList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_NODELIST_OFFSET_AND_INTERFACE_TABLE_BEGIN(nsChildContentList)
    NS_INTERFACE_TABLE_ENTRY(nsChildContentList, nsINodeList)
    NS_INTERFACE_TABLE_ENTRY(nsChildContentList, nsIDOMNodeList)
  NS_OFFSET_AND_INTERFACE_TABLE_END
  NS_OFFSET_AND_INTERFACE_TABLE_TO_MAP_SEGUE
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(nsChildContentList)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(NodeList)
NS_INTERFACE_MAP_END

JSObject*
nsChildContentList::WrapObject(JSContext *cx, JSObject *scope,
                               bool *triedToWrap)
{
  return mozilla::dom::binding::NodeList::create(cx, scope, this, triedToWrap);
}

NS_IMETHODIMP
nsChildContentList::GetLength(PRUint32* aLength)
{
  *aLength = mNode ? mNode->GetChildCount() : 0;

  return NS_OK;
}

NS_IMETHODIMP
nsChildContentList::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
  nsINode* node = GetNodeAt(aIndex);
  if (!node) {
    *aReturn = nsnull;

    return NS_OK;
  }

  return CallQueryInterface(node, aReturn);
}

nsIContent*
nsChildContentList::GetNodeAt(PRUint32 aIndex)
{
  if (mNode) {
    return mNode->GetChildAt(aIndex);
  }

  return nsnull;
}

PRInt32
nsChildContentList::IndexOf(nsIContent* aContent)
{
  if (mNode) {
    return mNode->IndexOf(aContent);
  }

  return -1;
}

//----------------------------------------------------------------------

NS_IMPL_CYCLE_COLLECTION_1(nsNode3Tearoff, mNode)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsNode3Tearoff)
  NS_INTERFACE_MAP_ENTRY(nsIDOMXPathNSResolver)
NS_INTERFACE_MAP_END_AGGREGATED(mNode)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsNode3Tearoff)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsNode3Tearoff)

NS_IMETHODIMP
nsNode3Tearoff::LookupNamespaceURI(const nsAString& aNamespacePrefix,
                                   nsAString& aNamespaceURI)
{
  return mNode->LookupNamespaceURI(aNamespacePrefix, aNamespaceURI);
}

nsIContent*
nsGenericElement::GetFirstElementChild()
{
  nsAttrAndChildArray& children = mAttrsAndChildren;
  PRUint32 i, count = children.ChildCount();
  for (i = 0; i < count; ++i) {
    nsIContent* child = children.ChildAt(i);
    if (child->IsElement()) {
      return child;
    }
  }
  
  return nsnull;
}

nsIContent*
nsGenericElement::GetLastElementChild()
{
  nsAttrAndChildArray& children = mAttrsAndChildren;
  PRUint32 i = children.ChildCount();
  while (i > 0) {
    nsIContent* child = children.ChildAt(--i);
    if (child->IsElement()) {
      return child;
    }
  }
  
  return nsnull;
}

nsIContent*
nsGenericElement::GetPreviousElementSibling()
{
  nsIContent* parent = GetParent();
  if (!parent) {
    return nsnull;
  }

  NS_ASSERTION(parent->IsElement() ||
               parent->IsNodeOfType(nsINode::eDOCUMENT_FRAGMENT),
               "Parent content must be an element or a doc fragment");

  nsAttrAndChildArray& children =
    static_cast<nsGenericElement*>(parent)->mAttrsAndChildren;
  PRInt32 index = children.IndexOfChild(this);
  if (index < 0) {
    return nsnull;
  }

  PRUint32 i = index;
  while (i > 0) {
    nsIContent* child = children.ChildAt((PRUint32)--i);
    if (child->IsElement()) {
      return child;
    }
  }
  
  return nsnull;
}

nsIContent*
nsGenericElement::GetNextElementSibling()
{
  nsIContent* parent = GetParent();
  if (!parent) {
    return nsnull;
  }

  NS_ASSERTION(parent->IsElement() ||
               parent->IsNodeOfType(nsINode::eDOCUMENT_FRAGMENT),
               "Parent content must be an element or a doc fragment");

  nsAttrAndChildArray& children =
    static_cast<nsGenericElement*>(parent)->mAttrsAndChildren;
  PRInt32 index = children.IndexOfChild(this);
  if (index < 0) {
    return nsnull;
  }

  PRUint32 i, count = children.ChildCount();
  for (i = (PRUint32)index + 1; i < count; ++i) {
    nsIContent* child = children.ChildAt(i);
    if (child->IsElement()) {
      return child;
    }
  }
  
  return nsnull;
}

NS_IMETHODIMP
nsGenericElement::GetChildElementCount(PRUint32* aResult)
{
  *aResult = GetChildrenList()->Length(true);
  return NS_OK;
}

// readonly attribute nsIDOMNodeList children
NS_IMETHODIMP
nsGenericElement::GetChildElements(nsIDOMNodeList** aResult)
{
  NS_ADDREF(*aResult = GetChildrenList());
  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::GetFirstElementChild(nsIDOMElement** aResult)
{
  *aResult = nsnull;

  nsIContent *result = GetFirstElementChild();

  return result ? CallQueryInterface(result, aResult) : NS_OK;
}

NS_IMETHODIMP
nsGenericElement::GetLastElementChild(nsIDOMElement** aResult)
{
  *aResult = nsnull;

  nsIContent *result = GetLastElementChild();

  return result ? CallQueryInterface(result, aResult) : NS_OK;
}

NS_IMETHODIMP
nsGenericElement::GetPreviousElementSibling(nsIDOMElement** aResult)
{
  *aResult = nsnull;

  nsIContent *result = GetPreviousElementSibling();

  return result ? CallQueryInterface(result, aResult) : NS_OK;
}

NS_IMETHODIMP
nsGenericElement::GetNextElementSibling(nsIDOMElement** aResult)
{
  *aResult = nsnull;

  nsIContent *result = GetNextElementSibling();

  return result ? CallQueryInterface(result, aResult) : NS_OK;
}

nsContentList*
nsGenericElement::GetChildrenList()
{
  nsGenericElement::nsDOMSlots *slots = DOMSlots();

  if (!slots->mChildrenList) {
    slots->mChildrenList = new nsContentList(this, kNameSpaceID_Wildcard, 
                                             nsGkAtoms::_asterix, nsGkAtoms::_asterix,
                                             false);
  }

  return slots->mChildrenList;
}

nsDOMTokenList*
nsGenericElement::GetClassList(nsresult *aResult)
{
  *aResult = NS_ERROR_OUT_OF_MEMORY;

  nsGenericElement::nsDOMSlots *slots = DOMSlots();

  if (!slots->mClassList) {
    nsCOMPtr<nsIAtom> classAttr = GetClassAttributeName();
    if (!classAttr) {
      *aResult = NS_OK;

      return nsnull;
    }

    slots->mClassList = new nsDOMTokenList(this, classAttr);
  }

  *aResult = NS_OK;

  return slots->mClassList;
}

NS_IMETHODIMP
nsGenericElement::GetClassList(nsIDOMDOMTokenList** aResult)
{
  *aResult = nsnull;

  nsresult rv;
  nsIDOMDOMTokenList* list = GetClassList(&rv);
  NS_ENSURE_TRUE(list, rv);

  NS_ADDREF(*aResult = list);

  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::SetCapture(bool aRetargetToElement)
{
  // If there is already an active capture, ignore this request. This would
  // occur if a splitter, frame resizer, etc had already captured and we don't
  // want to override those.
  if (nsIPresShell::GetCapturingContent())
    return NS_OK;

  nsIPresShell::SetCapturingContent(this, CAPTURE_PREVENTDRAG |
    (aRetargetToElement ? CAPTURE_RETARGETTOELEMENT : 0));

  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::ReleaseCapture()
{
  if (nsIPresShell::GetCapturingContent() == this) {
    nsIPresShell::SetCapturingContent(nsnull, 0);
  }

  return NS_OK;
}

nsIFrame*
nsGenericElement::GetStyledFrame()
{
  nsIFrame *frame = GetPrimaryFrame(Flush_Layout);
  return frame ? nsLayoutUtils::GetStyleFrame(frame) : nsnull;
}

void
nsGenericElement::GetOffsetRect(nsRect& aRect, nsIContent** aOffsetParent)
{
  *aOffsetParent = nsnull;
  aRect = nsRect();

  nsIFrame* frame = GetStyledFrame();
  if (!frame) {
    return;
  }

  nsPoint origin = frame->GetPosition();
  aRect.x = nsPresContext::AppUnitsToIntCSSPixels(origin.x);
  aRect.y = nsPresContext::AppUnitsToIntCSSPixels(origin.y);

  // Get the union of all rectangles in this and continuation frames.
  // It doesn't really matter what we use as aRelativeTo here, since
  // we only care about the size. Using 'parent' might make things
  // a bit faster by speeding up the internal GetOffsetTo operations.
  nsIFrame* parent = frame->GetParent() ? frame->GetParent() : frame;
  nsRect rcFrame = nsLayoutUtils::GetAllInFlowRectsUnion(frame, parent);
  aRect.width = nsPresContext::AppUnitsToIntCSSPixels(rcFrame.width);
  aRect.height = nsPresContext::AppUnitsToIntCSSPixels(rcFrame.height);
}

nsIntSize
nsGenericElement::GetPaddingRectSize()
{
  nsIFrame* frame = GetStyledFrame();
  if (!frame) {
    return nsIntSize(0, 0);
  }

  NS_ASSERTION(frame->GetParent(), "Styled frame has no parent");
  nsRect rcFrame = nsLayoutUtils::GetAllInFlowPaddingRectsUnion(frame, frame->GetParent());
  return nsIntSize(nsPresContext::AppUnitsToIntCSSPixels(rcFrame.width),
                   nsPresContext::AppUnitsToIntCSSPixels(rcFrame.height));
}

nsIScrollableFrame*
nsGenericElement::GetScrollFrame(nsIFrame **aStyledFrame)
{
  // it isn't clear what to return for SVG nodes, so just return nothing
  if (IsSVG()) {
    if (aStyledFrame) {
      *aStyledFrame = nsnull;
    }
    return nsnull;
  }

  nsIFrame* frame = GetStyledFrame();

  if (aStyledFrame) {
    *aStyledFrame = frame;
  }
  if (!frame) {
    return nsnull;
  }

  // menu frames implement GetScrollTargetFrame but we don't want
  // to use it here.  Similar for comboboxes.
  if (frame->GetType() != nsGkAtoms::menuFrame &&
      frame->GetType() != nsGkAtoms::comboboxControlFrame) {
    nsIScrollableFrame *scrollFrame = frame->GetScrollTargetFrame();
    if (scrollFrame)
      return scrollFrame;
  }

  nsIDocument* doc = OwnerDoc();
  bool quirksMode = doc->GetCompatibilityMode() == eCompatibility_NavQuirks;
  Element* elementWithRootScrollInfo =
    quirksMode ? doc->GetBodyElement() : doc->GetRootElement();
  if (this == elementWithRootScrollInfo) {
    // In quirks mode, the scroll info for the body element should map to the
    // root scrollable frame.
    // In strict mode, the scroll info for the root element should map to the
    // the root scrollable frame.
    return frame->PresContext()->PresShell()->GetRootScrollFrameAsScrollable();
  }

  return nsnull;
}

PRInt32
nsGenericElement::GetScrollTop()
{
  nsIScrollableFrame* sf = GetScrollFrame();

  return sf ?
         nsPresContext::AppUnitsToIntCSSPixels(sf->GetScrollPosition().y) :
         0;
}

NS_IMETHODIMP
nsGenericElement::GetScrollTop(PRInt32* aScrollTop)
{
  *aScrollTop = GetScrollTop();

  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::SetScrollTop(PRInt32 aScrollTop)
{
  nsIScrollableFrame* sf = GetScrollFrame();
  if (sf) {
    nsPoint pt = sf->GetScrollPosition();
    sf->ScrollToCSSPixels(nsIntPoint(nsPresContext::AppUnitsToIntCSSPixels(pt.x),
                                     aScrollTop));
  }
  return NS_OK;
}

PRInt32
nsGenericElement::GetScrollLeft()
{
  nsIScrollableFrame* sf = GetScrollFrame();

  return sf ?
         nsPresContext::AppUnitsToIntCSSPixels(sf->GetScrollPosition().x) :
         0;
}

NS_IMETHODIMP
nsGenericElement::GetScrollLeft(PRInt32* aScrollLeft)
{
  *aScrollLeft = GetScrollLeft();

  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::SetScrollLeft(PRInt32 aScrollLeft)
{
  nsIScrollableFrame* sf = GetScrollFrame();
  if (sf) {
    nsPoint pt = sf->GetScrollPosition();
    sf->ScrollToCSSPixels(nsIntPoint(aScrollLeft,
                                     nsPresContext::AppUnitsToIntCSSPixels(pt.y)));
  }
  return NS_OK;
}

PRInt32
nsGenericElement::GetScrollHeight()
{
  if (IsSVG())
    return 0;

  nsIScrollableFrame* sf = GetScrollFrame();
  if (!sf) {
    return GetPaddingRectSize().height;
  }

  nscoord height = sf->GetScrollRange().height + sf->GetScrollPortRect().height;
  return nsPresContext::AppUnitsToIntCSSPixels(height);
}

NS_IMETHODIMP
nsGenericElement::GetScrollHeight(PRInt32* aScrollHeight)
{
  *aScrollHeight = GetScrollHeight();

  return NS_OK;
}

PRInt32
nsGenericElement::GetScrollWidth()
{
  if (IsSVG())
    return 0;

  nsIScrollableFrame* sf = GetScrollFrame();
  if (!sf) {
    return GetPaddingRectSize().width;
  }

  nscoord width = sf->GetScrollRange().width + sf->GetScrollPortRect().width;
  return nsPresContext::AppUnitsToIntCSSPixels(width);
}

NS_IMETHODIMP
nsGenericElement::GetScrollWidth(PRInt32 *aScrollWidth)
{
  *aScrollWidth = GetScrollWidth();

  return NS_OK;
}

PRInt32
nsGenericElement::GetScrollLeftMax()
{
  nsIScrollableFrame* sf = GetScrollFrame();
  if (!sf) {
    return 0;
  }

  return nsPresContext::AppUnitsToIntCSSPixels(sf->GetScrollRange().XMost());
}

NS_IMETHODIMP
nsGenericElement::GetScrollLeftMax(PRInt32 *aScrollLeftMax)
{
  *aScrollLeftMax = GetScrollLeftMax();

  return NS_OK;
}

PRInt32
nsGenericElement::GetScrollTopMax()
{
  nsIScrollableFrame* sf = GetScrollFrame();
  if (!sf) {
    return 0;
  }

  return nsPresContext::AppUnitsToIntCSSPixels(sf->GetScrollRange().YMost());
}

NS_IMETHODIMP
nsGenericElement::GetScrollTopMax(PRInt32 *aScrollTopMax)
{
  *aScrollTopMax = GetScrollTopMax();

  return NS_OK;
}

nsRect
nsGenericElement::GetClientAreaRect()
{
  nsIFrame* styledFrame;
  nsIScrollableFrame* sf = GetScrollFrame(&styledFrame);

  if (sf) {
    return sf->GetScrollPortRect();
  }

  if (styledFrame &&
      (styledFrame->GetStyleDisplay()->mDisplay != NS_STYLE_DISPLAY_INLINE ||
       styledFrame->IsFrameOfType(nsIFrame::eReplaced))) {
    // Special case code to make client area work even when there isn't
    // a scroll view, see bug 180552, bug 227567.
    return styledFrame->GetPaddingRect() - styledFrame->GetPositionIgnoringScrolling();
  }

  // SVG nodes reach here and just return 0
  return nsRect(0, 0, 0, 0);
}

NS_IMETHODIMP
nsGenericElement::GetClientTop(PRInt32 *aClientTop)
{
  *aClientTop = GetClientTop();
  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::GetClientLeft(PRInt32 *aClientLeft)
{
  *aClientLeft = GetClientLeft();
  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::GetClientHeight(PRInt32 *aClientHeight)
{
  *aClientHeight = GetClientHeight();
  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::GetClientWidth(PRInt32 *aClientWidth)
{
  *aClientWidth = GetClientWidth();
  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::GetBoundingClientRect(nsIDOMClientRect** aResult)
{
  // Weak ref, since we addref it below
  nsClientRect* rect = new nsClientRect();
  NS_ADDREF(*aResult = rect);
  
  nsIFrame* frame = GetPrimaryFrame(Flush_Layout);
  if (!frame) {
    // display:none, perhaps? Return the empty rect
    return NS_OK;
  }

  nsRect r = nsLayoutUtils::GetAllInFlowRectsUnion(frame,
          nsLayoutUtils::GetContainingBlockForClientRect(frame),
          nsLayoutUtils::RECTS_ACCOUNT_FOR_TRANSFORMS);
  rect->SetLayoutRect(r);
  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::GetElementsByClassName(const nsAString& aClasses,
                                         nsIDOMNodeList** aReturn)
{
  return nsContentUtils::GetElementsByClassName(this, aClasses, aReturn);
}

NS_IMETHODIMP
nsGenericElement::GetClientRects(nsIDOMClientRectList** aResult)
{
  *aResult = nsnull;

  nsRefPtr<nsClientRectList> rectList = new nsClientRectList(this);

  nsIFrame* frame = GetPrimaryFrame(Flush_Layout);
  if (!frame) {
    // display:none, perhaps? Return an empty list
    *aResult = rectList.forget().get();
    return NS_OK;
  }

  nsLayoutUtils::RectListBuilder builder(rectList);
  nsLayoutUtils::GetAllInFlowRects(frame,
          nsLayoutUtils::GetContainingBlockForClientRect(frame), &builder,
          nsLayoutUtils::RECTS_ACCOUNT_FOR_TRANSFORMS);
  if (NS_FAILED(builder.mRV))
    return builder.mRV;
  *aResult = rectList.forget().get();
  return NS_OK;
}


//----------------------------------------------------------------------


NS_IMPL_ISUPPORTS1(nsNodeWeakReference,
                   nsIWeakReference)

nsNodeWeakReference::~nsNodeWeakReference()
{
  if (mNode) {
    NS_ASSERTION(mNode->GetSlots() &&
                 mNode->GetSlots()->mWeakReference == this,
                 "Weak reference has wrong value");
    mNode->GetSlots()->mWeakReference = nsnull;
  }
}

NS_IMETHODIMP
nsNodeWeakReference::QueryReferent(const nsIID& aIID, void** aInstancePtr)
{
  return mNode ? mNode->QueryInterface(aIID, aInstancePtr) :
                 NS_ERROR_NULL_POINTER;
}


NS_IMPL_CYCLE_COLLECTION_1(nsNodeSupportsWeakRefTearoff, mNode)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsNodeSupportsWeakRefTearoff)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END_AGGREGATED(mNode)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsNodeSupportsWeakRefTearoff)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsNodeSupportsWeakRefTearoff)

NS_IMETHODIMP
nsNodeSupportsWeakRefTearoff::GetWeakReference(nsIWeakReference** aInstancePtr)
{
  nsINode::nsSlots* slots = mNode->GetSlots();
  NS_ENSURE_TRUE(slots, NS_ERROR_OUT_OF_MEMORY);

  if (!slots->mWeakReference) {
    slots->mWeakReference = new nsNodeWeakReference(mNode);
    NS_ENSURE_TRUE(slots->mWeakReference, NS_ERROR_OUT_OF_MEMORY);
  }

  NS_ADDREF(*aInstancePtr = slots->mWeakReference);

  return NS_OK;
}

//----------------------------------------------------------------------

NS_IMPL_CYCLE_COLLECTION_1(nsNodeSelectorTearoff, mNode)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsNodeSelectorTearoff)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNodeSelector)
NS_INTERFACE_MAP_END_AGGREGATED(mNode)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsNodeSelectorTearoff)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsNodeSelectorTearoff)

NS_IMETHODIMP
nsNodeSelectorTearoff::QuerySelector(const nsAString& aSelector,
                                     nsIDOMElement **aReturn)
{
  nsresult rv;
  nsIContent* result = nsGenericElement::doQuerySelector(mNode, aSelector, &rv);
  return result ? CallQueryInterface(result, aReturn) : rv;
}

NS_IMETHODIMP
nsNodeSelectorTearoff::QuerySelectorAll(const nsAString& aSelector,
                                        nsIDOMNodeList **aReturn)
{
  return nsGenericElement::doQuerySelectorAll(mNode, aSelector, aReturn);
}

//----------------------------------------------------------------------

NS_IMPL_CYCLE_COLLECTION_1(nsTouchEventReceiverTearoff, mElement)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsTouchEventReceiverTearoff)
  NS_INTERFACE_MAP_ENTRY(nsITouchEventReceiver)
NS_INTERFACE_MAP_END_AGGREGATED(mElement)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsTouchEventReceiverTearoff)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsTouchEventReceiverTearoff)

//----------------------------------------------------------------------

NS_IMPL_CYCLE_COLLECTION_1(nsInlineEventHandlersTearoff, mElement)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsInlineEventHandlersTearoff)
  NS_INTERFACE_MAP_ENTRY(nsIInlineEventHandlers)
NS_INTERFACE_MAP_END_AGGREGATED(mElement)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsInlineEventHandlersTearoff)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsInlineEventHandlersTearoff)

//----------------------------------------------------------------------
nsGenericElement::nsDOMSlots::nsDOMSlots()
  : nsINode::nsSlots(),
    mDataset(nsnull),
    mBindingParent(nsnull)
{
}

nsGenericElement::nsDOMSlots::~nsDOMSlots()
{
  if (mAttributeMap) {
    mAttributeMap->DropReference();
  }

  if (mClassList) {
    mClassList->DropReference();
  }
}

void
nsGenericElement::nsDOMSlots::Traverse(nsCycleCollectionTraversalCallback &cb, bool aIsXUL)
{
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mSlots->mStyle");
  cb.NoteXPCOMChild(mStyle.get());

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mSlots->mSMILOverrideStyle");
  cb.NoteXPCOMChild(mSMILOverrideStyle.get());

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mSlots->mAttributeMap");
  cb.NoteXPCOMChild(mAttributeMap.get());

  if (aIsXUL) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mSlots->mControllers");
    cb.NoteXPCOMChild(mControllers);
  }

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mSlots->mChildrenList");
  cb.NoteXPCOMChild(NS_ISUPPORTS_CAST(nsIDOMNodeList*, mChildrenList));

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mSlots->mClassList");
  cb.NoteXPCOMChild(mClassList.get());
}

void
nsGenericElement::nsDOMSlots::Unlink(bool aIsXUL)
{
  mStyle = nsnull;
  mSMILOverrideStyle = nsnull;
  if (mAttributeMap) {
    mAttributeMap->DropReference();
    mAttributeMap = nsnull;
  }
  if (aIsXUL)
    NS_IF_RELEASE(mControllers);
  mChildrenList = nsnull;
  if (mClassList) {
    mClassList->DropReference();
    mClassList = nsnull;
  }
}

nsGenericElement::nsGenericElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : Element(aNodeInfo)
{
  NS_ABORT_IF_FALSE(mNodeInfo->NodeType() == nsIDOMNode::ELEMENT_NODE ||
                    (mNodeInfo->NodeType() ==
                       nsIDOMNode::DOCUMENT_FRAGMENT_NODE &&
                     mNodeInfo->Equals(nsGkAtoms::documentFragmentNodeName,
                                       kNameSpaceID_None)),
                    "Bad NodeType in aNodeInfo");

  SetIsElement();
}

nsGenericElement::~nsGenericElement()
{
  NS_PRECONDITION(!IsInDoc(),
                  "Please remove this from the document properly");
  if (GetParent()) {
    NS_RELEASE(mParent);
  }
}

NS_IMETHODIMP
nsGenericElement::GetNodeName(nsAString& aNodeName)
{
  aNodeName = NodeName();
  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::GetLocalName(nsAString& aLocalName)
{
  aLocalName = LocalName();
  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::GetNodeValue(nsAString& aNodeValue)
{
  SetDOMStringToNull(aNodeValue);

  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::SetNodeValue(const nsAString& aNodeValue)
{
  // The DOM spec says that when nodeValue is defined to be null "setting it
  // has no effect", so we don't throw an exception.
  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::GetNodeType(PRUint16* aNodeType)
{
  *aNodeType = NodeType();
  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::GetNamespaceURI(nsAString& aNamespaceURI)
{
  return mNodeInfo->GetNamespaceURI(aNamespaceURI);
}

NS_IMETHODIMP
nsGenericElement::GetPrefix(nsAString& aPrefix)
{
  mNodeInfo->GetPrefix(aPrefix);
  return NS_OK;
}

nsresult
nsGenericElement::InternalIsSupported(nsISupports* aObject,
                                      const nsAString& aFeature,
                                      const nsAString& aVersion,
                                      bool* aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = false;

  // Convert the incoming UTF16 strings to raw char*'s to save us some
  // code when doing all those string compares.
  NS_ConvertUTF16toUTF8 feature(aFeature);
  NS_ConvertUTF16toUTF8 version(aVersion);

  const char *f = feature.get();
  const char *v = version.get();

  if (PL_strcasecmp(f, "XML") == 0 ||
      PL_strcasecmp(f, "HTML") == 0) {
    if (aVersion.IsEmpty() ||
        PL_strcmp(v, "1.0") == 0 ||
        PL_strcmp(v, "2.0") == 0) {
      *aReturn = true;
    }
  } else if (PL_strcasecmp(f, "Views") == 0 ||
             PL_strcasecmp(f, "StyleSheets") == 0 ||
             PL_strcasecmp(f, "Core") == 0 ||
             PL_strcasecmp(f, "CSS") == 0 ||
             PL_strcasecmp(f, "CSS2") == 0 ||
             PL_strcasecmp(f, "Events") == 0 ||
             PL_strcasecmp(f, "UIEvents") == 0 ||
             PL_strcasecmp(f, "MouseEvents") == 0 ||
             // Non-standard!
             PL_strcasecmp(f, "MouseScrollEvents") == 0 ||
             PL_strcasecmp(f, "HTMLEvents") == 0 ||
             PL_strcasecmp(f, "Range") == 0 ||
             PL_strcasecmp(f, "XHTML") == 0) {
    if (aVersion.IsEmpty() ||
        PL_strcmp(v, "2.0") == 0) {
      *aReturn = true;
    }
  } else if (PL_strcasecmp(f, "XPath") == 0) {
    if (aVersion.IsEmpty() ||
        PL_strcmp(v, "3.0") == 0) {
      *aReturn = true;
    }
  } else if (PL_strcasecmp(f, "SVGEvents") == 0 ||
             PL_strcasecmp(f, "SVGZoomEvents") == 0 ||
             nsSVGFeatures::HasFeature(aObject, aFeature)) {
    if (aVersion.IsEmpty() ||
        PL_strcmp(v, "1.0") == 0 ||
        PL_strcmp(v, "1.1") == 0) {
      *aReturn = true;
    }
  }
  else if (NS_SMILEnabled() && PL_strcasecmp(f, "TimeControl") == 0) {
    if (aVersion.IsEmpty() || PL_strcmp(v, "1.0") == 0) {
      *aReturn = true;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::IsSupported(const nsAString& aFeature,
                              const nsAString& aVersion,
                              bool* aReturn)
{
  return InternalIsSupported(this, aFeature, aVersion, aReturn);
}

NS_IMETHODIMP
nsGenericElement::HasAttributes(bool* aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);

  *aReturn = GetAttrCount() > 0;

  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::GetAttributes(nsIDOMNamedNodeMap** aAttributes)
{
  if (!IsElement()) {
    *aAttributes = nsnull;
    return NS_OK;
  }

  nsDOMSlots *slots = DOMSlots();

  if (!slots->mAttributeMap) {
    slots->mAttributeMap = new nsDOMAttributeMap(this);
  }

  NS_ADDREF(*aAttributes = slots->mAttributeMap);

  return NS_OK;
}

nsresult
nsGenericElement::HasChildNodes(bool* aReturn)
{
  *aReturn = mAttrsAndChildren.ChildCount() > 0;

  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::GetTagName(nsAString& aTagName)
{
  aTagName = NodeName();
  return NS_OK;
}

nsresult
nsGenericElement::GetAttribute(const nsAString& aName,
                               nsAString& aReturn)
{
  const nsAttrName* name = InternalGetExistingAttrNameFromQName(aName);

  if (!name) {
    if (mNodeInfo->NamespaceID() == kNameSpaceID_XUL) {
      // XXX should be SetDOMStringToNull(aReturn);
      // See bug 232598
      aReturn.Truncate();
    }
    else {
      SetDOMStringToNull(aReturn);
    }

    return NS_OK;
  }

  GetAttr(name->NamespaceID(), name->LocalName(), aReturn);

  return NS_OK;
}

nsresult
nsGenericElement::SetAttribute(const nsAString& aName,
                               const nsAString& aValue)
{
  const nsAttrName* name = InternalGetExistingAttrNameFromQName(aName);

  if (!name) {
    nsresult rv = nsContentUtils::CheckQName(aName, false);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIAtom> nameAtom = do_GetAtom(aName);
    NS_ENSURE_TRUE(nameAtom, NS_ERROR_OUT_OF_MEMORY);

    return SetAttr(kNameSpaceID_None, nameAtom, aValue, true);
  }

  return SetAttr(name->NamespaceID(), name->LocalName(), name->GetPrefix(),
                 aValue, true);
}

nsresult
nsGenericElement::RemoveAttribute(const nsAString& aName)
{
  const nsAttrName* name = InternalGetExistingAttrNameFromQName(aName);

  if (!name) {
    // If there is no canonical nsAttrName for this attribute name, then the
    // attribute does not exist and we can't get its namespace ID and
    // local name below, so we return early.
    return NS_OK;
  }

  // Hold a strong reference here so that the atom or nodeinfo doesn't go
  // away during UnsetAttr. If it did UnsetAttr would be left with a
  // dangling pointer as argument without knowing it.
  nsAttrName tmp(*name);

  return UnsetAttr(name->NamespaceID(), name->LocalName(), true);
}

nsresult
nsGenericElement::GetAttributeNode(const nsAString& aName,
                                   nsIDOMAttr** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsIDocument* document = OwnerDoc();
  if (document) {
    document->WarnOnceAbout(nsIDocument::eGetAttributeNode);
  }

  nsCOMPtr<nsIDOMNamedNodeMap> map;
  nsresult rv = GetAttributes(getter_AddRefs(map));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMNode> node;
  rv = map->GetNamedItem(aName, getter_AddRefs(node));

  if (NS_SUCCEEDED(rv) && node) {
    rv = CallQueryInterface(node, aReturn);
  }

  return rv;
}

nsresult
nsGenericElement::SetAttributeNode(nsIDOMAttr* aAttribute,
                                   nsIDOMAttr** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  NS_ENSURE_ARG_POINTER(aAttribute);

  *aReturn = nsnull;

  OwnerDoc()->WarnOnceAbout(nsIDocument::eSetAttributeNode);

  nsCOMPtr<nsIDOMNamedNodeMap> map;
  nsresult rv = GetAttributes(getter_AddRefs(map));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMNode> returnNode;
  rv = map->SetNamedItem(aAttribute, getter_AddRefs(returnNode));
  NS_ENSURE_SUCCESS(rv, rv);

  if (returnNode) {
    rv = CallQueryInterface(returnNode, aReturn);
  }

  return rv;
}

nsresult
nsGenericElement::RemoveAttributeNode(nsIDOMAttr* aAttribute,
                                      nsIDOMAttr** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  NS_ENSURE_ARG_POINTER(aAttribute);

  *aReturn = nsnull;

  OwnerDoc()->WarnOnceAbout(nsIDocument::eRemoveAttributeNode);

  nsCOMPtr<nsIDOMNamedNodeMap> map;
  nsresult rv = GetAttributes(getter_AddRefs(map));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString name;

  rv = aAttribute->GetName(name);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIDOMNode> node;
    rv = map->RemoveNamedItem(name, getter_AddRefs(node));

    if (NS_SUCCEEDED(rv) && node) {
      rv = CallQueryInterface(node, aReturn);
    }
  }

  return rv;
}

nsresult
nsGenericElement::GetElementsByTagName(const nsAString& aTagname,
                                       nsIDOMNodeList** aReturn)
{
  nsContentList *list = NS_GetContentList(this, kNameSpaceID_Unknown, 
                                          aTagname).get();

  // transfer ref to aReturn
  *aReturn = list;
  return NS_OK;
}

nsresult
nsGenericElement::GetAttributeNS(const nsAString& aNamespaceURI,
                                 const nsAString& aLocalName,
                                 nsAString& aReturn)
{
  PRInt32 nsid =
    nsContentUtils::NameSpaceManager()->GetNameSpaceID(aNamespaceURI);

  if (nsid == kNameSpaceID_Unknown) {
    // Unknown namespace means no attribute.
    SetDOMStringToNull(aReturn);
    return NS_OK;
  }

  nsCOMPtr<nsIAtom> name = do_GetAtom(aLocalName);
  bool hasAttr = GetAttr(nsid, name, aReturn);
  if (!hasAttr) {
    SetDOMStringToNull(aReturn);
  }

  return NS_OK;
}

nsresult
nsGenericElement::SetAttributeNS(const nsAString& aNamespaceURI,
                                 const nsAString& aQualifiedName,
                                 const nsAString& aValue)
{
  nsCOMPtr<nsINodeInfo> ni;
  nsresult rv =
    nsContentUtils::GetNodeInfoFromQName(aNamespaceURI, aQualifiedName,
                                         mNodeInfo->NodeInfoManager(),
                                         nsIDOMNode::ATTRIBUTE_NODE,
                                         getter_AddRefs(ni));
  NS_ENSURE_SUCCESS(rv, rv);

  return SetAttr(ni->NamespaceID(), ni->NameAtom(), ni->GetPrefixAtom(),
                 aValue, true);
}

nsresult
nsGenericElement::RemoveAttributeNS(const nsAString& aNamespaceURI,
                                    const nsAString& aLocalName)
{
  nsCOMPtr<nsIAtom> name = do_GetAtom(aLocalName);
  PRInt32 nsid =
    nsContentUtils::NameSpaceManager()->GetNameSpaceID(aNamespaceURI);

  if (nsid == kNameSpaceID_Unknown) {
    // If the namespace ID is unknown, it means there can't possibly be an
    // existing attribute. We would need a known namespace ID to pass into
    // UnsetAttr, so we return early if we don't have one.
    return NS_OK;
  }

  UnsetAttr(nsid, name, true);

  return NS_OK;
}

nsresult
nsGenericElement::GetAttributeNodeNS(const nsAString& aNamespaceURI,
                                     const nsAString& aLocalName,
                                     nsIDOMAttr** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  OwnerDoc()->WarnOnceAbout(nsIDocument::eGetAttributeNodeNS);

  return GetAttributeNodeNSInternal(aNamespaceURI, aLocalName, aReturn);
}

nsresult
nsGenericElement::GetAttributeNodeNSInternal(const nsAString& aNamespaceURI,
                                             const nsAString& aLocalName,
                                             nsIDOMAttr** aReturn)
{
  nsCOMPtr<nsIDOMNamedNodeMap> map;
  nsresult rv = GetAttributes(getter_AddRefs(map));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMNode> node;
  rv = map->GetNamedItemNS(aNamespaceURI, aLocalName, getter_AddRefs(node));

  if (NS_SUCCEEDED(rv) && node) {
    rv = CallQueryInterface(node, aReturn);
  }

  return rv;
}

nsresult
nsGenericElement::SetAttributeNodeNS(nsIDOMAttr* aNewAttr,
                                     nsIDOMAttr** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  NS_ENSURE_ARG_POINTER(aNewAttr);
  *aReturn = nsnull;

  OwnerDoc()->WarnOnceAbout(nsIDocument::eSetAttributeNodeNS);

  nsCOMPtr<nsIDOMNamedNodeMap> map;
  nsresult rv = GetAttributes(getter_AddRefs(map));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMNode> returnNode;
  rv = map->SetNamedItemNS(aNewAttr, getter_AddRefs(returnNode));
  NS_ENSURE_SUCCESS(rv, rv);

  if (returnNode) {
    rv = CallQueryInterface(returnNode, aReturn);
  }

  return rv;
}

nsresult
nsGenericElement::GetElementsByTagNameNS(const nsAString& aNamespaceURI,
                                         const nsAString& aLocalName,
                                         nsIDOMNodeList** aReturn)
{
  PRInt32 nameSpaceId = kNameSpaceID_Wildcard;

  if (!aNamespaceURI.EqualsLiteral("*")) {
    nsresult rv =
      nsContentUtils::NameSpaceManager()->RegisterNameSpace(aNamespaceURI,
                                                            nameSpaceId);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  NS_ASSERTION(nameSpaceId != kNameSpaceID_Unknown, "Unexpected namespace ID!");

  nsContentList *list = NS_GetContentList(this, nameSpaceId, aLocalName).get();

  // transfer ref to aReturn
  *aReturn = list;
  return NS_OK;
}

nsresult
nsGenericElement::HasAttribute(const nsAString& aName, bool* aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);

  const nsAttrName* name = InternalGetExistingAttrNameFromQName(aName);
  *aReturn = (name != nsnull);

  return NS_OK;
}

nsresult
nsGenericElement::HasAttributeNS(const nsAString& aNamespaceURI,
                                 const nsAString& aLocalName,
                                 bool* aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);

  PRInt32 nsid =
    nsContentUtils::NameSpaceManager()->GetNameSpaceID(aNamespaceURI);

  if (nsid == kNameSpaceID_Unknown) {
    // Unknown namespace means no attr...

    *aReturn = false;

    return NS_OK;
  }

  nsCOMPtr<nsIAtom> name = do_GetAtom(aLocalName);
  *aReturn = HasAttr(nsid, name);

  return NS_OK;
}

static nsXBLBinding*
GetFirstBindingWithContent(nsBindingManager* aBmgr, nsIContent* aBoundElem)
{
  nsXBLBinding* binding = aBmgr->GetBinding(aBoundElem);
  while (binding) {
    if (binding->GetAnonymousContent()) {
      return binding;
    }
    binding = binding->GetBaseBinding();
  }
  
  return nsnull;
}

static nsresult
BindNodesInInsertPoints(nsXBLBinding* aBinding, nsIContent* aInsertParent,
                        nsIDocument* aDocument)
{
  NS_PRECONDITION(aBinding && aInsertParent, "Missing arguments");

  nsresult rv;
  // These should be refcounted or otherwise protectable.
  nsInsertionPointList* inserts =
    aBinding->GetExistingInsertionPointsFor(aInsertParent);
  if (inserts) {
    bool allowScripts = aBinding->AllowScripts();
#ifdef MOZ_XUL
    nsCOMPtr<nsIXULDocument> xulDoc = do_QueryInterface(aDocument);
#endif
    PRUint32 i;
    for (i = 0; i < inserts->Length(); ++i) {
      nsCOMPtr<nsIContent> insertRoot =
        inserts->ElementAt(i)->GetDefaultContent();
      if (insertRoot) {
        for (nsCOMPtr<nsIContent> child = insertRoot->GetFirstChild();
             child;
             child = child->GetNextSibling()) {
          rv = child->BindToTree(aDocument, aInsertParent,
                                 aBinding->GetBoundElement(), allowScripts);
          NS_ENSURE_SUCCESS(rv, rv);

#ifdef MOZ_XUL
          if (xulDoc) {
            xulDoc->AddSubtreeToDocument(child);
          }
#endif
        }
      }
    }
  }

  return NS_OK;
}

nsresult
nsGenericElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                             nsIContent* aBindingParent,
                             bool aCompileEventHandlers)
{
  NS_PRECONDITION(aParent || aDocument, "Must have document if no parent!");
  NS_PRECONDITION(HasSameOwnerDoc(NODE_FROM(aParent, aDocument)),
                  "Must have the same owner document");
  NS_PRECONDITION(!aParent || aDocument == aParent->GetCurrentDoc(),
                  "aDocument must be current doc of aParent");
  NS_PRECONDITION(!GetCurrentDoc(), "Already have a document.  Unbind first!");
  // Note that as we recurse into the kids, they'll have a non-null parent.  So
  // only assert if our parent is _changing_ while we have a parent.
  NS_PRECONDITION(!GetParent() || aParent == GetParent(),
                  "Already have a parent.  Unbind first!");
  NS_PRECONDITION(!GetBindingParent() ||
                  aBindingParent == GetBindingParent() ||
                  (!aBindingParent && aParent &&
                   aParent->GetBindingParent() == GetBindingParent()),
                  "Already have a binding parent.  Unbind first!");
  NS_PRECONDITION(!aParent || !aDocument ||
                  !aParent->HasFlag(NODE_FORCE_XBL_BINDINGS),
                  "Parent in document but flagged as forcing XBL");
  NS_PRECONDITION(aBindingParent != this,
                  "Content must not be its own binding parent");
  NS_PRECONDITION(!IsRootOfNativeAnonymousSubtree() ||
                  aBindingParent == aParent,
                  "Native anonymous content must have its parent as its "
                  "own binding parent");
  NS_PRECONDITION(aBindingParent || !aParent ||
                  aBindingParent == aParent->GetBindingParent(),
                  "We should be passed the right binding parent");

#ifdef MOZ_XUL
  // First set the binding parent
  nsXULElement* xulElem = nsXULElement::FromContent(this);
  if (xulElem) {
    xulElem->SetXULBindingParent(aBindingParent);
  }
  else 
#endif
  {
    if (aBindingParent) {
      nsDOMSlots *slots = DOMSlots();

      slots->mBindingParent = aBindingParent; // Weak, so no addref happens.
    }
  }
  NS_ASSERTION(!aBindingParent || IsRootOfNativeAnonymousSubtree() ||
               !HasFlag(NODE_IS_IN_ANONYMOUS_SUBTREE) ||
               (aParent && aParent->IsInNativeAnonymousSubtree()),
               "Trying to re-bind content from native anonymous subtree to "
               "non-native anonymous parent!");
  if (aParent && aParent->IsInNativeAnonymousSubtree()) {
    SetFlags(NODE_IS_IN_ANONYMOUS_SUBTREE);
  }

  bool hadForceXBL = HasFlag(NODE_FORCE_XBL_BINDINGS);

  // Now set the parent and set the "Force attach xbl" flag if needed.
  if (aParent) {
    if (!GetParent()) {
      NS_ADDREF(aParent);
    }
    mParent = aParent;

    if (aParent->HasFlag(NODE_FORCE_XBL_BINDINGS)) {
      SetFlags(NODE_FORCE_XBL_BINDINGS);
    }
  }
  else {
    mParent = aDocument;
  }
  SetParentIsContent(aParent);

  // XXXbz sXBL/XBL2 issue!

  // Finally, set the document
  if (aDocument) {
    // Notify XBL- & nsIAnonymousContentCreator-generated
    // anonymous content that the document is changing.
    // XXXbz ordering issues here?  Probably not, since ChangeDocumentFor is
    // just pretty broken anyway....  Need to get it working.
    // XXXbz XBL doesn't handle this (asserts), and we don't really want
    // to be doing this during parsing anyway... sort this out.    
    //    aDocument->BindingManager()->ChangeDocumentFor(this, nsnull,
    //                                                   aDocument);

    // We no longer need to track the subtree pointer (and in fact we'll assert
    // if we do this any later).
    ClearSubtreeRootPointer();

    // Being added to a document.
    SetInDocument();

    // Unset this flag since we now really are in a document.
    UnsetFlags(NODE_FORCE_XBL_BINDINGS |
               // And clear the lazy frame construction bits.
               NODE_NEEDS_FRAME | NODE_DESCENDANTS_NEED_FRAMES |
               // And the restyle bits
               ELEMENT_ALL_RESTYLE_FLAGS);
  } else {
    // If we're not in the doc, update our subtree pointer.
    SetSubtreeRootPointer(aParent->SubtreeRoot());
  }

  // If NODE_FORCE_XBL_BINDINGS was set we might have anonymous children
  // that also need to be told that they are moving.
  nsresult rv;
  if (hadForceXBL) {
    nsBindingManager* bmgr = OwnerDoc()->BindingManager();

    // First check if we have a binding...
    nsXBLBinding* contBinding =
      GetFirstBindingWithContent(bmgr, this);
    if (contBinding) {
      nsCOMPtr<nsIContent> anonRoot = contBinding->GetAnonymousContent();
      bool allowScripts = contBinding->AllowScripts();
      for (nsCOMPtr<nsIContent> child = anonRoot->GetFirstChild();
           child;
           child = child->GetNextSibling()) {
        rv = child->BindToTree(aDocument, this, this, allowScripts);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // ...then check if we have content in insertion points that are
      // direct children of the <content>
      rv = BindNodesInInsertPoints(contBinding, this, aDocument);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // ...and finally check if we're in a binding where we have content in
    // insertion points.
    if (aBindingParent) {
      nsXBLBinding* binding = bmgr->GetBinding(aBindingParent);
      if (binding) {
        rv = BindNodesInInsertPoints(binding, this, aDocument);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
  }

  UpdateEditableState(false);

  // Now recurse into our kids
  for (nsIContent* child = GetFirstChild(); child;
       child = child->GetNextSibling()) {
    rv = child->BindToTree(aDocument, this, aBindingParent,
                           aCompileEventHandlers);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsNodeUtils::ParentChainChanged(this);

  if (aDocument && HasID() && !aBindingParent) {
    aDocument->AddToIdTable(this, DoGetID());
  }

  if (MayHaveStyle() && !IsXUL()) {
    // XXXbz if we already have a style attr parsed, this won't do
    // anything... need to fix that.
    // If MayHaveStyle() is true, we must be an nsStyledElement
    static_cast<nsStyledElement*>(this)->ReparseStyleAttribute(false);
  }

  if (aDocument) {
    // If we're in a document now, let our mapped attrs know what their new
    // sheet is.  This is safe to run for non-mapped-attribute elements too;
    // it'll just do a small bit of unnecessary work.  But most elements in
    // practice are mapped-attribute elements.
    nsHTMLStyleSheet* sheet = aDocument->GetAttributeStyleSheet();
    if (sheet) {
      mAttrsAndChildren.SetMappedAttrStyleSheet(sheet);
    }
  }

  // XXXbz script execution during binding can trigger some of these
  // postcondition asserts....  But we do want that, since things will
  // generally be quite broken when that happens.
  NS_POSTCONDITION(aDocument == GetCurrentDoc(), "Bound to wrong document");
  NS_POSTCONDITION(aParent == GetParent(), "Bound to wrong parent");
  NS_POSTCONDITION(aBindingParent == GetBindingParent(),
                   "Bound to wrong binding parent");

  return NS_OK;
}

class RemoveFromBindingManagerRunnable : public nsRunnable {
public:
  RemoveFromBindingManagerRunnable(nsBindingManager* aManager,
                                   Element* aElement,
                                   nsIDocument* aDoc,
                                   nsIContent* aBindingParent):
    mManager(aManager), mElement(aElement), mDoc(aDoc),
    mBindingParent(aBindingParent)
  {}

  NS_IMETHOD Run()
  {
    mManager->RemovedFromDocumentInternal(mElement, mDoc, mBindingParent);
    return NS_OK;
  }

private:
  nsRefPtr<nsBindingManager> mManager;
  nsRefPtr<Element> mElement;
  nsCOMPtr<nsIDocument> mDoc;
  nsCOMPtr<nsIContent> mBindingParent;
};

void
nsGenericElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  NS_PRECONDITION(aDeep || (!GetCurrentDoc() && !GetBindingParent()),
                  "Shallow unbind won't clear document and binding parent on "
                  "kids!");

  RemoveFromIdTable();

  // Make sure to unbind this node before doing the kids
  nsIDocument *document =
    HasFlag(NODE_FORCE_XBL_BINDINGS) ? OwnerDoc() : GetCurrentDoc();

  if (aNullParent) {
    if (IsFullScreenAncestor()) {
      // The element being removed is an ancestor of the full-screen element,
      // exit full-screen state.
      nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                      "DOM", OwnerDoc(),
                                      nsContentUtils::eDOM_PROPERTIES,
                                      "RemovedFullScreenElement");
      // Fully exit full-screen.
      nsIDocument::ExitFullScreen(false);
    }
    if (HasPointerLock()) {
      nsIDocument::UnlockPointer();
    }
    if (GetParent()) {
      NS_RELEASE(mParent);
    } else {
      mParent = nsnull;
    }
    SetParentIsContent(false);
  }
  ClearInDocument();

  // Begin keeping track of our subtree root.
  SetSubtreeRootPointer(aNullParent ? this : mParent->SubtreeRoot());

  if (document) {
    // Notify XBL- & nsIAnonymousContentCreator-generated
    // anonymous content that the document is changing.
    if (HasFlag(NODE_MAY_BE_IN_BINDING_MNGR)) {
      nsContentUtils::AddScriptRunner(
        new RemoveFromBindingManagerRunnable(document->BindingManager(), this,
                                             document, GetBindingParent()));
    }

    document->ClearBoxObjectFor(this);
  }

  // Ensure that CSS transitions don't continue on an element at a
  // different place in the tree (even if reinserted before next
  // animation refresh).
  // FIXME (Bug 522599): Need a test for this.
  if (HasFlag(NODE_HAS_PROPERTIES)) {
    DeleteProperty(nsGkAtoms::transitionsOfBeforeProperty);
    DeleteProperty(nsGkAtoms::transitionsOfAfterProperty);
    DeleteProperty(nsGkAtoms::transitionsProperty);
    DeleteProperty(nsGkAtoms::animationsOfBeforeProperty);
    DeleteProperty(nsGkAtoms::animationsOfAfterProperty);
    DeleteProperty(nsGkAtoms::animationsProperty);
  }

  // Unset this since that's what the old code effectively did.
  UnsetFlags(NODE_FORCE_XBL_BINDINGS);
  
#ifdef MOZ_XUL
  nsXULElement* xulElem = nsXULElement::FromContent(this);
  if (xulElem) {
    xulElem->SetXULBindingParent(nsnull);
  }
  else
#endif
  {
    nsDOMSlots *slots = GetExistingDOMSlots();
    if (slots) {
      slots->mBindingParent = nsnull;
    }
  }

  if (aDeep) {
    // Do the kids. Don't call GetChildCount() here since that'll force
    // XUL to generate template children, which there is no need for since
    // all we're going to do is unbind them anyway.
    PRUint32 i, n = mAttrsAndChildren.ChildCount();

    for (i = 0; i < n; ++i) {
      // Note that we pass false for aNullParent here, since we don't want
      // the kids to forget us.  We _do_ want them to forget their binding
      // parent, though, since this only walks non-anonymous kids.
      mAttrsAndChildren.ChildAt(i)->UnbindFromTree(true, false);
    }
  }

  nsNodeUtils::ParentChainChanged(this);
}

already_AddRefed<nsINodeList>
nsGenericElement::GetChildren(PRUint32 aFilter)
{
  nsRefPtr<nsSimpleContentList> list = new nsSimpleContentList(this);
  if (!list) {
    return nsnull;
  }

  nsIFrame *frame = GetPrimaryFrame();

  // Append :before generated content.
  if (frame) {
    nsIFrame *beforeFrame = nsLayoutUtils::GetBeforeFrame(frame);
    if (beforeFrame) {
      list->AppendElement(beforeFrame->GetContent());
    }
  }

  // If XBL is bound to this node then append XBL anonymous content including
  // explict content altered by insertion point if we were requested for XBL
  // anonymous content, otherwise append explicit content with respect to
  // insertion point if any.
  nsINodeList *childList = nsnull;

  nsIDocument* document = OwnerDoc();
  if (!(aFilter & eAllButXBL)) {
    childList = document->BindingManager()->GetXBLChildNodesFor(this);
    if (!childList) {
      childList = GetChildNodesList();
    }

  } else {
    childList = document->BindingManager()->GetContentListFor(this);
  }

  if (childList) {
    PRUint32 length = 0;
    childList->GetLength(&length);
    for (PRUint32 idx = 0; idx < length; idx++) {
      nsIContent* child = childList->GetNodeAt(idx);
      list->AppendElement(child);
    }
  }

  if (frame) {
    // Append native anonymous content to the end.
    nsIAnonymousContentCreator* creator = do_QueryFrame(frame);
    if (creator) {
      creator->AppendAnonymousContentTo(*list, aFilter);
    }

    // Append :after generated content.
    nsIFrame *afterFrame = nsLayoutUtils::GetAfterFrame(frame);
    if (afterFrame) {
      list->AppendElement(afterFrame->GetContent());
    }
  }

  nsINodeList* returnList = nsnull;
  list.forget(&returnList);
  return returnList;
}

static nsIContent*
FindNativeAnonymousSubtreeOwner(nsIContent* aContent)
{
  if (aContent->IsInNativeAnonymousSubtree()) {
    bool isNativeAnon = false;
    while (aContent && !isNativeAnon) {
      isNativeAnon = aContent->IsRootOfNativeAnonymousSubtree();
      aContent = aContent->GetParent();
    }
  }
  return aContent;
}

nsresult
nsIContent::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
{
  //FIXME! Document how this event retargeting works, Bug 329124.
  aVisitor.mCanHandle = true;
  aVisitor.mMayHaveListenerManager = HasListenerManager();

  // Don't propagate mouseover and mouseout events when mouse is moving
  // inside native anonymous content.
  bool isAnonForEvents = IsRootOfNativeAnonymousSubtree();
  if ((aVisitor.mEvent->message == NS_MOUSE_ENTER_SYNTH ||
       aVisitor.mEvent->message == NS_MOUSE_EXIT_SYNTH) &&
      // Check if we should stop event propagation when event has just been
      // dispatched or when we're about to propagate from
      // native anonymous subtree.
      ((this == aVisitor.mEvent->originalTarget &&
        !IsInNativeAnonymousSubtree()) || isAnonForEvents)) {
     nsCOMPtr<nsIContent> relatedTarget =
       do_QueryInterface(static_cast<nsMouseEvent*>
                                    (aVisitor.mEvent)->relatedTarget);
    if (relatedTarget &&
        relatedTarget->OwnerDoc() == OwnerDoc()) {

      // If current target is anonymous for events or we know that related
      // target is descendant of an element which is anonymous for events,
      // we may want to stop event propagation.
      // If this is the original target, aVisitor.mRelatedTargetIsInAnon
      // must be updated.
      if (isAnonForEvents || aVisitor.mRelatedTargetIsInAnon ||
          (aVisitor.mEvent->originalTarget == this &&
           (aVisitor.mRelatedTargetIsInAnon =
            relatedTarget->IsInNativeAnonymousSubtree()))) {
        nsIContent* anonOwner = FindNativeAnonymousSubtreeOwner(this);
        if (anonOwner) {
          nsIContent* anonOwnerRelated =
            FindNativeAnonymousSubtreeOwner(relatedTarget);
          if (anonOwnerRelated) {
            // Note, anonOwnerRelated may still be inside some other
            // native anonymous subtree. The case where anonOwner is still
            // inside native anonymous subtree will be handled when event
            // propagates up in the DOM tree.
            while (anonOwner != anonOwnerRelated &&
                   anonOwnerRelated->IsInNativeAnonymousSubtree()) {
              anonOwnerRelated = FindNativeAnonymousSubtreeOwner(anonOwnerRelated);
            }
            if (anonOwner == anonOwnerRelated) {
#ifdef DEBUG_smaug
              nsCOMPtr<nsIContent> originalTarget =
                do_QueryInterface(aVisitor.mEvent->originalTarget);
              nsAutoString ot, ct, rt;
              if (originalTarget) {
                originalTarget->Tag()->ToString(ot);
              }
              Tag()->ToString(ct);
              relatedTarget->Tag()->ToString(rt);
              printf("Stopping %s propagation:"
                     "\n\toriginalTarget=%s \n\tcurrentTarget=%s %s"
                     "\n\trelatedTarget=%s %s \n%s",
                     (aVisitor.mEvent->message == NS_MOUSE_ENTER_SYNTH)
                       ? "mouseover" : "mouseout",
                     NS_ConvertUTF16toUTF8(ot).get(),
                     NS_ConvertUTF16toUTF8(ct).get(),
                     isAnonForEvents
                       ? "(is native anonymous)"
                       : (IsInNativeAnonymousSubtree()
                           ? "(is in native anonymous subtree)" : ""),
                     NS_ConvertUTF16toUTF8(rt).get(),
                     relatedTarget->IsInNativeAnonymousSubtree()
                       ? "(is in native anonymous subtree)" : "",
                     (originalTarget && relatedTarget->FindFirstNonNativeAnonymous() ==
                       originalTarget->FindFirstNonNativeAnonymous())
                       ? "" : "Wrong event propagation!?!\n");
#endif
              aVisitor.mParentTarget = nsnull;
              // Event should not propagate to non-anon content.
              aVisitor.mCanHandle = isAnonForEvents;
              return NS_OK;
            }
          }
        }
      }
    }
  }

  nsIContent* parent = GetParent();
  // Event may need to be retargeted if this is the root of a native
  // anonymous content subtree or event is dispatched somewhere inside XBL.
  if (isAnonForEvents) {
    // If a DOM event is explicitly dispatched using node.dispatchEvent(), then
    // all the events are allowed even in the native anonymous content..
    NS_ASSERTION(aVisitor.mEvent->eventStructType != NS_MUTATION_EVENT ||
                 aVisitor.mDOMEvent,
                 "Mutation event dispatched in native anonymous content!?!");
    aVisitor.mEventTargetAtParent = parent;
  } else if (parent && aVisitor.mOriginalTargetIsInAnon) {
    nsCOMPtr<nsIContent> content(do_QueryInterface(aVisitor.mEvent->target));
    if (content && content->GetBindingParent() == parent) {
      aVisitor.mEventTargetAtParent = parent;
    }
  }

  // check for an anonymous parent
  // XXX XBL2/sXBL issue
  if (HasFlag(NODE_MAY_BE_IN_BINDING_MNGR)) {
    nsIContent* insertionParent = OwnerDoc()->BindingManager()->
      GetInsertionParent(this);
    NS_ASSERTION(!(aVisitor.mEventTargetAtParent && insertionParent &&
                   aVisitor.mEventTargetAtParent != insertionParent),
                 "Retargeting and having insertion parent!");
    if (insertionParent) {
      parent = insertionParent;
    }
  }

  if (parent) {
    aVisitor.mParentTarget = parent;
  } else {
    aVisitor.mParentTarget = GetCurrentDoc();
  }
  return NS_OK;
}

const nsAttrValue*
nsGenericElement::DoGetClasses() const
{
  NS_NOTREACHED("Shouldn't ever be called");
  return nsnull;
}

NS_IMETHODIMP
nsGenericElement::WalkContentStyleRules(nsRuleWalker* aRuleWalker)
{
  return NS_OK;
}

nsICSSDeclaration*
nsGenericElement::GetSMILOverrideStyle()
{
  nsGenericElement::nsDOMSlots *slots = DOMSlots();

  if (!slots->mSMILOverrideStyle) {
    slots->mSMILOverrideStyle = new nsDOMCSSAttributeDeclaration(this, true);
  }

  return slots->mSMILOverrideStyle;
}

css::StyleRule*
nsGenericElement::GetSMILOverrideStyleRule()
{
  nsGenericElement::nsDOMSlots *slots = GetExistingDOMSlots();
  return slots ? slots->mSMILOverrideStyleRule.get() : nsnull;
}

nsresult
nsGenericElement::SetSMILOverrideStyleRule(css::StyleRule* aStyleRule,
                                           bool aNotify)
{
  nsGenericElement::nsDOMSlots *slots = DOMSlots();

  slots->mSMILOverrideStyleRule = aStyleRule;

  if (aNotify) {
    nsIDocument* doc = GetCurrentDoc();
    // Only need to request a restyle if we're in a document.  (We might not
    // be in a document, if we're clearing animation effects on a target node
    // that's been detached since the previous animation sample.)
    if (doc) {
      nsCOMPtr<nsIPresShell> shell = doc->GetShell();
      if (shell) {
        shell->RestyleForAnimation(this, eRestyle_Self);
      }
    }
  }

  return NS_OK;
}

bool
nsGenericElement::IsLabelable() const
{
  return false;
}

css::StyleRule*
nsGenericElement::GetInlineStyleRule()
{
  return nsnull;
}

nsresult
nsGenericElement::SetInlineStyleRule(css::StyleRule* aStyleRule,
                                     const nsAString* aSerialized,
                                     bool aNotify)
{
  NS_NOTYETIMPLEMENTED("nsGenericElement::SetInlineStyleRule");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP_(bool)
nsGenericElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  return false;
}

nsChangeHint
nsGenericElement::GetAttributeChangeHint(const nsIAtom* aAttribute,
                                         PRInt32 aModType) const
{
  return nsChangeHint(0);
}

nsIAtom *
nsGenericElement::GetClassAttributeName() const
{
  return nsnull;
}

bool
nsGenericElement::FindAttributeDependence(const nsIAtom* aAttribute,
                                          const MappedAttributeEntry* const aMaps[],
                                          PRUint32 aMapCount)
{
  for (PRUint32 mapindex = 0; mapindex < aMapCount; ++mapindex) {
    for (const MappedAttributeEntry* map = aMaps[mapindex];
         map->attribute; ++map) {
      if (aAttribute == *map->attribute) {
        return true;
      }
    }
  }

  return false;
}

already_AddRefed<nsINodeInfo>
nsGenericElement::GetExistingAttrNameFromQName(const nsAString& aStr) const
{
  const nsAttrName* name = InternalGetExistingAttrNameFromQName(aStr);
  if (!name) {
    return nsnull;
  }

  nsINodeInfo* nodeInfo;
  if (name->IsAtom()) {
    nodeInfo = mNodeInfo->NodeInfoManager()->
      GetNodeInfo(name->Atom(), nsnull, kNameSpaceID_None,
                  nsIDOMNode::ATTRIBUTE_NODE).get();
  }
  else {
    NS_ADDREF(nodeInfo = name->NodeInfo());
  }

  return nodeInfo;
}

bool
nsGenericElement::IsLink(nsIURI** aURI) const
{
  *aURI = nsnull;
  return false;
}

// static
bool
nsGenericElement::ShouldBlur(nsIContent *aContent)
{
  // Determine if the current element is focused, if it is not focused
  // then we should not try to blur
  nsIDocument *document = aContent->GetDocument();
  if (!document)
    return false;

  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(document->GetWindow());
  if (!window)
    return false;

  nsCOMPtr<nsPIDOMWindow> focusedFrame;
  nsIContent* contentToBlur =
    nsFocusManager::GetFocusedDescendant(window, false, getter_AddRefs(focusedFrame));
  if (contentToBlur == aContent)
    return true;

  // if focus on this element would get redirected, then check the redirected
  // content as well when blurring.
  return (contentToBlur && nsFocusManager::GetRedirectedFocus(aContent) == contentToBlur);
}

nsIContent*
nsGenericElement::GetBindingParent() const
{
  nsDOMSlots *slots = GetExistingDOMSlots();

  if (slots) {
    return slots->mBindingParent;
  }
  return nsnull;
}

bool
nsGenericElement::IsNodeOfType(PRUint32 aFlags) const
{
  return !(aFlags & ~eCONTENT);
}

nsresult
nsGenericElement::InsertChildAt(nsIContent* aKid,
                                PRUint32 aIndex,
                                bool aNotify)
{
  NS_PRECONDITION(aKid, "null ptr");

  return doInsertChildAt(aKid, aIndex, aNotify, mAttrsAndChildren);
}

void
nsGenericElement::RemoveChildAt(PRUint32 aIndex, bool aNotify)
{
  nsCOMPtr<nsIContent> oldKid = mAttrsAndChildren.GetSafeChildAt(aIndex);
  NS_ASSERTION(oldKid == GetChildAt(aIndex), "Unexpected child in RemoveChildAt");

  if (oldKid) {
    doRemoveChildAt(aIndex, aNotify, oldKid, mAttrsAndChildren);
  }
}

NS_IMETHODIMP
nsGenericElement::GetTextContent(nsAString &aTextContent)
{
  nsContentUtils::GetNodeTextContent(this, true, aTextContent);
  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::SetTextContent(const nsAString& aTextContent)
{
  return nsContentUtils::SetNodeTextContent(this, aTextContent, false);
}

/* static */
nsresult
nsGenericElement::DispatchEvent(nsPresContext* aPresContext,
                                nsEvent* aEvent,
                                nsIContent* aTarget,
                                bool aFullDispatch,
                                nsEventStatus* aStatus)
{
  NS_PRECONDITION(aTarget, "Must have target");
  NS_PRECONDITION(aEvent, "Must have source event");
  NS_PRECONDITION(aStatus, "Null out param?");

  if (!aPresContext) {
    return NS_OK;
  }

  nsCOMPtr<nsIPresShell> shell = aPresContext->GetPresShell();
  if (!shell) {
    return NS_OK;
  }

  if (aFullDispatch) {
    return shell->HandleEventWithTarget(aEvent, nsnull, aTarget, aStatus);
  }

  return shell->HandleDOMEventWithTarget(aTarget, aEvent, aStatus);
}

/* static */
nsresult
nsGenericElement::DispatchClickEvent(nsPresContext* aPresContext,
                                     nsInputEvent* aSourceEvent,
                                     nsIContent* aTarget,
                                     bool aFullDispatch,
                                     PRUint32 aFlags,
                                     nsEventStatus* aStatus)
{
  NS_PRECONDITION(aTarget, "Must have target");
  NS_PRECONDITION(aSourceEvent, "Must have source event");
  NS_PRECONDITION(aStatus, "Null out param?");

  nsMouseEvent event(NS_IS_TRUSTED_EVENT(aSourceEvent), NS_MOUSE_CLICK,
                     aSourceEvent->widget, nsMouseEvent::eReal);
  event.refPoint = aSourceEvent->refPoint;
  PRUint32 clickCount = 1;
  float pressure = 0;
  PRUint16 inputSource = 0;
  if (aSourceEvent->eventStructType == NS_MOUSE_EVENT) {
    clickCount = static_cast<nsMouseEvent*>(aSourceEvent)->clickCount;
    pressure = static_cast<nsMouseEvent*>(aSourceEvent)->pressure;
    inputSource = static_cast<nsMouseEvent*>(aSourceEvent)->inputSource;
  } else if (aSourceEvent->eventStructType == NS_KEY_EVENT) {
    inputSource = nsIDOMMouseEvent::MOZ_SOURCE_KEYBOARD;
  }
  event.pressure = pressure;
  event.clickCount = clickCount;
  event.inputSource = inputSource;
  event.modifiers = aSourceEvent->modifiers;
  event.flags |= aFlags; // Be careful not to overwrite existing flags!

  return DispatchEvent(aPresContext, &event, aTarget, aFullDispatch, aStatus);
}

nsIFrame*
nsGenericElement::GetPrimaryFrame(mozFlushType aType)
{
  nsIDocument* doc = GetCurrentDoc();
  if (!doc) {
    return nsnull;
  }

  // Cause a flush, so we get up-to-date frame
  // information
  doc->FlushPendingNotifications(aType);

  return GetPrimaryFrame();
}

void
nsGenericElement::DestroyContent()
{
  nsIDocument *document = OwnerDoc();
  document->BindingManager()->RemovedFromDocument(this, document);
  document->ClearBoxObjectFor(this);

  // XXX We really should let cycle collection do this, but that currently still
  //     leaks (see https://bugzilla.mozilla.org/show_bug.cgi?id=406684).
  nsContentUtils::ReleaseWrapper(this, this);

  PRUint32 i, count = mAttrsAndChildren.ChildCount();
  for (i = 0; i < count; ++i) {
    // The child can remove itself from the parent in BindToTree.
    mAttrsAndChildren.ChildAt(i)->DestroyContent();
  }
}

void
nsGenericElement::SaveSubtreeState()
{
  PRUint32 i, count = mAttrsAndChildren.ChildCount();
  for (i = 0; i < count; ++i) {
    mAttrsAndChildren.ChildAt(i)->SaveSubtreeState();
  }
}

//----------------------------------------------------------------------

// Generic DOMNode implementations

void
nsGenericElement::FireNodeInserted(nsIDocument* aDoc,
                                   nsINode* aParent,
                                   nsTArray<nsCOMPtr<nsIContent> >& aNodes)
{
  PRUint32 count = aNodes.Length();
  for (PRUint32 i = 0; i < count; ++i) {
    nsIContent* childContent = aNodes[i];

    if (nsContentUtils::HasMutationListeners(childContent,
          NS_EVENT_BITS_MUTATION_NODEINSERTED, aParent)) {
      nsMutationEvent mutation(true, NS_MUTATION_NODEINSERTED);
      mutation.mRelatedNode = do_QueryInterface(aParent);

      mozAutoSubtreeModified subtree(aDoc, aParent);
      (new nsAsyncDOMEvent(childContent, mutation))->RunDOMEventWhenSafe();
    }
  }
}

//----------------------------------------------------------------------

// nsISupports implementation

NS_IMPL_CYCLE_COLLECTION_CLASS(nsGenericElement)

#define SUBTREE_UNBINDINGS_PER_RUNNABLE 500

class ContentUnbinder : public nsRunnable
{
public:
  ContentUnbinder()
  {
    nsLayoutStatics::AddRef();
    mLast = this;
  }

  ~ContentUnbinder()
  {
    Run();
    nsLayoutStatics::Release();
  }

  void UnbindSubtree(nsIContent* aNode)
  {
    if (aNode->NodeType() != nsIDOMNode::ELEMENT_NODE &&
        aNode->NodeType() != nsIDOMNode::DOCUMENT_FRAGMENT_NODE) {
      return;  
    }
    nsGenericElement* container = static_cast<nsGenericElement*>(aNode);
    PRUint32 childCount = container->mAttrsAndChildren.ChildCount();
    if (childCount) {
      while (childCount-- > 0) {
        // Hold a strong ref to the node when we remove it, because we may be
        // the last reference to it.  We need to call TakeChildAt() and
        // update mFirstChild before calling UnbindFromTree, since this last
        // can notify various observers and they should really see consistent
        // tree state.
        nsCOMPtr<nsIContent> child =
          container->mAttrsAndChildren.TakeChildAt(childCount);
        if (childCount == 0) {
          container->mFirstChild = nsnull;
        }
        UnbindSubtree(child);
        child->UnbindFromTree();
      }
    }
  }

  NS_IMETHOD Run()
  {
    nsAutoScriptBlocker scriptBlocker;
    PRUint32 len = mSubtreeRoots.Length();
    if (len) {
      PRTime start = PR_Now();
      for (PRUint32 i = 0; i < len; ++i) {
        UnbindSubtree(mSubtreeRoots[i]);
      }
      mSubtreeRoots.Clear();
      Telemetry::Accumulate(Telemetry::CYCLE_COLLECTOR_CONTENT_UNBIND,
                            PRUint32(PR_Now() - start) / PR_USEC_PER_MSEC);
    }
    if (this == sContentUnbinder) {
      sContentUnbinder = nsnull;
      if (mNext) {
        nsRefPtr<ContentUnbinder> next;
        next.swap(mNext);
        sContentUnbinder = next;
        next->mLast = mLast;
        mLast = nsnull;
        NS_DispatchToMainThread(next);
      }
    }
    return NS_OK;
  }

  static void UnbindAll()
  {
    nsRefPtr<ContentUnbinder> ub = sContentUnbinder;
    sContentUnbinder = nsnull;
    while (ub) {
      ub->Run();
      ub = ub->mNext;
    }
  }

  static void Append(nsIContent* aSubtreeRoot)
  {
    if (!sContentUnbinder) {
      sContentUnbinder = new ContentUnbinder();
      nsCOMPtr<nsIRunnable> e = sContentUnbinder;
      NS_DispatchToMainThread(e);
    }

    if (sContentUnbinder->mLast->mSubtreeRoots.Length() >=
        SUBTREE_UNBINDINGS_PER_RUNNABLE) {
      sContentUnbinder->mLast->mNext = new ContentUnbinder();
      sContentUnbinder->mLast = sContentUnbinder->mLast->mNext;
    }
    sContentUnbinder->mLast->mSubtreeRoots.AppendElement(aSubtreeRoot);
  }

private:
  nsAutoTArray<nsCOMPtr<nsIContent>,
               SUBTREE_UNBINDINGS_PER_RUNNABLE> mSubtreeRoots;
  nsRefPtr<ContentUnbinder>                     mNext;
  ContentUnbinder*                              mLast;
  static ContentUnbinder*                       sContentUnbinder;
};

ContentUnbinder* ContentUnbinder::sContentUnbinder = nsnull;

void
nsGenericElement::ClearContentUnbinder()
{
  ContentUnbinder::UnbindAll();
}

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsGenericElement)
  nsINode::Unlink(tmp);

  if (tmp->HasProperties()) {
    if (tmp->IsHTML()) {
      tmp->DeleteProperty(nsGkAtoms::microdataProperties);
      tmp->DeleteProperty(nsGkAtoms::itemtype);
      tmp->DeleteProperty(nsGkAtoms::itemref);
      tmp->DeleteProperty(nsGkAtoms::itemprop);
    } else if (tmp->IsXUL()) {
      tmp->DeleteProperty(nsGkAtoms::contextmenulistener);
      tmp->DeleteProperty(nsGkAtoms::popuplistener);
    }
  }

  // Unlink child content (and unbind our subtree).
  if (tmp->UnoptimizableCCNode() || !nsCCUncollectableMarker::sGeneration) {
    PRUint32 childCount = tmp->mAttrsAndChildren.ChildCount();
    if (childCount) {
      // Don't allow script to run while we're unbinding everything.
      nsAutoScriptBlocker scriptBlocker;
      while (childCount-- > 0) {
        // Hold a strong ref to the node when we remove it, because we may be
        // the last reference to it.  We need to call TakeChildAt() and
        // update mFirstChild before calling UnbindFromTree, since this last
        // can notify various observers and they should really see consistent
        // tree state.
        nsCOMPtr<nsIContent> child = tmp->mAttrsAndChildren.TakeChildAt(childCount);
        if (childCount == 0) {
          tmp->mFirstChild = nsnull;
        }
        child->UnbindFromTree();
      }
    }
  } else if (!tmp->GetParent() && tmp->mAttrsAndChildren.ChildCount()) {
    ContentUnbinder::Append(tmp);
  } /* else {
    The subtree root will end up to a ContentUnbinder, and that will
    unbind the child nodes.
  } */

  // Unlink any DOM slots of interest.
  {
    nsDOMSlots *slots = tmp->GetExistingDOMSlots();
    if (slots) {
      slots->Unlink(tmp->IsXUL());
    }
  }

  {
    nsIDocument *doc;
    if (!tmp->GetNodeParent() && (doc = tmp->OwnerDoc())) {
      doc->BindingManager()->RemovedFromDocument(tmp, doc);
    }
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsGenericElement)
  nsINode::Trace(tmp, aCallback, aClosure);
NS_IMPL_CYCLE_COLLECTION_TRACE_END

void
nsGenericElement::MarkUserData(void* aObject, nsIAtom* aKey, void* aChild,
                               void* aData)
{
  PRUint32* gen = static_cast<PRUint32*>(aData);
  xpc_MarkInCCGeneration(static_cast<nsISupports*>(aChild), *gen);
}

void
nsGenericElement::MarkUserDataHandler(void* aObject, nsIAtom* aKey,
                                      void* aChild, void* aData)
{
  xpc_TryUnmarkWrappedGrayObject(static_cast<nsISupports*>(aChild));
}

void
nsGenericElement::MarkNodeChildren(nsINode* aNode)
{
  JSObject* o = GetJSObjectChild(aNode);
  xpc_UnmarkGrayObject(o);

  nsEventListenerManager* elm = aNode->GetListenerManager(false);
  if (elm) {
    elm->UnmarkGrayJSListeners();
  }

  if (aNode->HasProperties()) {
    nsIDocument* ownerDoc = aNode->OwnerDoc();
    ownerDoc->PropertyTable(DOM_USER_DATA)->
      Enumerate(aNode, nsGenericElement::MarkUserData,
                &nsCCUncollectableMarker::sGeneration);
    ownerDoc->PropertyTable(DOM_USER_DATA_HANDLER)->
      Enumerate(aNode, nsGenericElement::MarkUserDataHandler,
                &nsCCUncollectableMarker::sGeneration);
  }
}

nsINode*
FindOptimizableSubtreeRoot(nsINode* aNode)
{
  nsINode* p;
  while ((p = aNode->GetNodeParent())) {
    if (aNode->UnoptimizableCCNode()) {
      return nsnull;
    }
    aNode = p;
  }
  
  if (aNode->UnoptimizableCCNode()) {
    return nsnull;
  }
  return aNode;
}

nsAutoTArray<nsINode*, 1020>* gCCBlackMarkedNodes = nsnull;

void
ClearBlackMarkedNodes()
{
  if (!gCCBlackMarkedNodes) {
    return;
  }
  PRUint32 len = gCCBlackMarkedNodes->Length();
  for (PRUint32 i = 0; i < len; ++i) {
    nsINode* n = gCCBlackMarkedNodes->ElementAt(i);
    n->SetCCMarkedRoot(false);
    n->SetInCCBlackTree(false);
  }
  delete gCCBlackMarkedNodes;
  gCCBlackMarkedNodes = nsnull;
}

// static
bool
nsGenericElement::CanSkipInCC(nsINode* aNode)
{
  // Don't try to optimize anything during shutdown.
  if (nsCCUncollectableMarker::sGeneration == 0) {
    return false;
  }

  nsIDocument* currentDoc = aNode->GetCurrentDoc();
  if (currentDoc &&
      nsCCUncollectableMarker::InGeneration(currentDoc->GetMarkedCCGeneration())) {
    return !NeedsScriptTraverse(aNode);
  }

  // Bail out early if aNode is somewhere in anonymous content,
  // or otherwise unusual.
  if (aNode->UnoptimizableCCNode()) {
    return false;
  }

  nsINode* root =
    currentDoc ? static_cast<nsINode*>(currentDoc) :
                 FindOptimizableSubtreeRoot(aNode);
  if (!root) {
    return false;
  }
  
  // Subtree has been traversed already.
  if (root->CCMarkedRoot()) {
    return root->InCCBlackTree() && !NeedsScriptTraverse(aNode);
  }

  if (!gCCBlackMarkedNodes) {
    gCCBlackMarkedNodes = new nsAutoTArray<nsINode*, 1020>;
  }

  // nodesToUnpurple contains nodes which will be removed
  // from the purple buffer if the DOM tree is black.
  nsAutoTArray<nsIContent*, 1020> nodesToUnpurple;
  // grayNodes need script traverse, so they aren't removed from
  // the purple buffer, but are marked to be in black subtree so that
  // traverse is faster.
  nsAutoTArray<nsINode*, 1020> grayNodes;

  bool foundBlack = root->IsBlack();
  if (root != currentDoc) {
    currentDoc = nsnull;
    if (NeedsScriptTraverse(root)) {
      grayNodes.AppendElement(root);
    } else if (static_cast<nsIContent*>(root)->IsPurple()) {
      nodesToUnpurple.AppendElement(static_cast<nsIContent*>(root));
    }
  }

  // Traverse the subtree and check if we could know without CC
  // that it is black.
  // Note, this traverse is non-virtual and inline, so it should be a lot faster
  // than CC's generic traverse.
  for (nsIContent* node = root->GetFirstChild(); node;
       node = node->GetNextNode(root)) {
    foundBlack = foundBlack || node->IsBlack();
    if (foundBlack && currentDoc) {
      // If we can mark the whole document black, no need to optimize
      // so much, since when the next purple node in the document will be
      // handled, it is fast to check that currentDoc is in CCGeneration.
      break;
    }
    if (NeedsScriptTraverse(node)) {
      // Gray nodes need real CC traverse.
      grayNodes.AppendElement(node);
    } else if (node->IsPurple()) {
      nodesToUnpurple.AppendElement(node);
    }
  }

  root->SetCCMarkedRoot(true);
  root->SetInCCBlackTree(foundBlack);
  gCCBlackMarkedNodes->AppendElement(root);

  if (!foundBlack) {
    return false;
  }

  if (currentDoc) {
    // Special case documents. If we know the document is black,
    // we can mark the document to be in CCGeneration.
    currentDoc->
      MarkUncollectableForCCGeneration(nsCCUncollectableMarker::sGeneration);
  } else {
    for (PRUint32 i = 0; i < grayNodes.Length(); ++i) {
      nsINode* node = grayNodes[i];
      node->SetInCCBlackTree(true);
    }
    gCCBlackMarkedNodes->AppendElements(grayNodes);
  }

  // Subtree is black, we can remove non-gray purple nodes from
  // purple buffer.
  for (PRUint32 i = 0; i < nodesToUnpurple.Length(); ++i) {
    nsIContent* purple = nodesToUnpurple[i];
    // Can't remove currently handled purple node.
    if (purple != aNode) {
      purple->RemovePurple();
    }
  }
  return !NeedsScriptTraverse(aNode);
}

nsAutoTArray<nsINode*, 1020>* gPurpleRoots = nsnull;
nsAutoTArray<nsIContent*, 1020>* gNodesToUnbind = nsnull;

void ClearCycleCollectorCleanupData()
{
  if (gPurpleRoots) {
    PRUint32 len = gPurpleRoots->Length();
    for (PRUint32 i = 0; i < len; ++i) {
      nsINode* n = gPurpleRoots->ElementAt(i);
      n->SetIsPurpleRoot(false);
    }
    delete gPurpleRoots;
    gPurpleRoots = nsnull;
  }
  if (gNodesToUnbind) {
    PRUint32 len = gNodesToUnbind->Length();
    for (PRUint32 i = 0; i < len; ++i) {
      nsIContent* c = gNodesToUnbind->ElementAt(i);
      c->SetIsPurpleRoot(false);
      ContentUnbinder::Append(c);
    }
    delete gNodesToUnbind;
    gNodesToUnbind = nsnull;
  }
}

static bool
ShouldClearPurple(nsIContent* aContent)
{
  if (aContent && aContent->IsPurple()) {
    return true;
  }

  JSObject* o = GetJSObjectChild(aContent);
  if (o && xpc_IsGrayGCThing(o)) {
    return true;
  }

  if (aContent->HasListenerManager()) {
    return true;
  }

  return aContent->HasProperties();
}

// If aNode is not optimizable, but is an element
// with a frame in a document which has currently active presshell,
// we can act as if it was optimizable. When the primary frame dies, aNode
// will end up to the purple buffer because of the refcount change.
bool
NodeHasActiveFrame(nsIDocument* aCurrentDoc, nsINode* aNode)
{
  return aCurrentDoc->GetShell() && aNode->IsElement() &&
         aNode->AsElement()->GetPrimaryFrame();
}

bool
OwnedByBindingManager(nsIDocument* aCurrentDoc, nsINode* aNode)
{
  return aNode->IsElement() &&
    aCurrentDoc->BindingManager()->GetBinding(aNode->AsElement());
}

// CanSkip checks if aNode is black, and if it is, returns
// true. If aNode is in a black DOM tree, CanSkip may also remove other objects
// from purple buffer and unmark event listeners and user data.
// If the root of the DOM tree is a document, less optimizations are done
// since checking the blackness of the current document is usually fast and we
// don't want slow down such common cases.
bool
nsGenericElement::CanSkip(nsINode* aNode, bool aRemovingAllowed)
{
  // Don't try to optimize anything during shutdown.
  if (nsCCUncollectableMarker::sGeneration == 0) {
    return false;
  }

  bool unoptimizable = aNode->UnoptimizableCCNode();
  nsIDocument* currentDoc = aNode->GetCurrentDoc();
  if (currentDoc &&
      nsCCUncollectableMarker::InGeneration(currentDoc->GetMarkedCCGeneration()) &&
      (!unoptimizable || NodeHasActiveFrame(currentDoc, aNode) ||
       OwnedByBindingManager(currentDoc, aNode))) {
    MarkNodeChildren(aNode);
    return true;
  }

  if (unoptimizable) {
    return false;
  }

  nsINode* root = currentDoc ? static_cast<nsINode*>(currentDoc) :
                               FindOptimizableSubtreeRoot(aNode);
  if (!root) {
    return false;
  }
 
  // Subtree has been traversed already, and aNode has
  // been handled in a way that doesn't require revisiting it.
  if (root->IsPurpleRoot()) {
    return false;
  }

  // nodesToClear contains nodes which are either purple or
  // gray.
  nsAutoTArray<nsIContent*, 1020> nodesToClear;

  bool foundBlack = root->IsBlack();
  bool domOnlyCycle = false;
  if (root != currentDoc) {
    currentDoc = nsnull;
    if (!foundBlack) {
      domOnlyCycle = static_cast<nsIContent*>(root)->OwnedOnlyByTheDOMTree();
    }
    if (ShouldClearPurple(static_cast<nsIContent*>(root))) {
      nodesToClear.AppendElement(static_cast<nsIContent*>(root));
    }
  }

  // Traverse the subtree and check if we could know without CC
  // that it is black.
  // Note, this traverse is non-virtual and inline, so it should be a lot faster
  // than CC's generic traverse.
  for (nsIContent* node = root->GetFirstChild(); node;
       node = node->GetNextNode(root)) {
    foundBlack = foundBlack || node->IsBlack();
    if (foundBlack) {
      domOnlyCycle = false;
      if (currentDoc) {
        // If we can mark the whole document black, no need to optimize
        // so much, since when the next purple node in the document will be
        // handled, it is fast to check that the currentDoc is in CCGeneration.
        break;
      }
      // No need to put stuff to the nodesToClear array, if we can clear it
      // already here.
      if (node->IsPurple() && (node != aNode || aRemovingAllowed)) {
        node->RemovePurple();
      }
      MarkNodeChildren(node);
    } else {
      domOnlyCycle = domOnlyCycle && node->OwnedOnlyByTheDOMTree();
      if (ShouldClearPurple(node)) {
        // Collect interesting nodes which we can clear if we find that
        // they are kept alive in a black tree or are in a DOM-only cycle.
        nodesToClear.AppendElement(node);
      }
    }
  }

  if (!currentDoc || !foundBlack) { 
    root->SetIsPurpleRoot(true);
    if (domOnlyCycle) {
      if (!gNodesToUnbind) {
        gNodesToUnbind = new nsAutoTArray<nsIContent*, 1020>();
      }
      gNodesToUnbind->AppendElement(static_cast<nsIContent*>(root));
      for (PRUint32 i = 0; i < nodesToClear.Length(); ++i) {
        nsIContent* n = nodesToClear[i];
        if ((n != aNode || aRemovingAllowed) && n->IsPurple()) {
          n->RemovePurple();
        }
      }
      return true;
    } else {
      if (!gPurpleRoots) {
        gPurpleRoots = new nsAutoTArray<nsINode*, 1020>();
      }
      gPurpleRoots->AppendElement(root);
    }
  }

  if (!foundBlack) {
    return false;
  }

  if (currentDoc) {
    // Special case documents. If we know the document is black,
    // we can mark the document to be in CCGeneration.
    currentDoc->
      MarkUncollectableForCCGeneration(nsCCUncollectableMarker::sGeneration);
    MarkNodeChildren(currentDoc);
  }

  // Subtree is black, so we can remove purple nodes from
  // purple buffer and mark stuff that to be certainly alive.
  for (PRUint32 i = 0; i < nodesToClear.Length(); ++i) {
    nsIContent* n = nodesToClear[i];
    MarkNodeChildren(n);
    // Can't remove currently handled purple node,
    // unless aRemovingAllowed is true. 
    if ((n != aNode || aRemovingAllowed) && n->IsPurple()) {
      n->RemovePurple();
    }
  }
  return true;
}

bool
nsGenericElement::CanSkipThis(nsINode* aNode)
{
  if (nsCCUncollectableMarker::sGeneration == 0) {
    return false;
  }
  if (aNode->IsBlack()) {
    return true;
  }
  nsIDocument* c = aNode->GetCurrentDoc();
  return 
    ((c && nsCCUncollectableMarker::InGeneration(c->GetMarkedCCGeneration())) ||
     aNode->InCCBlackTree()) && !NeedsScriptTraverse(aNode);
}

void
nsGenericElement::InitCCCallbacks()
{
  nsCycleCollector_setForgetSkippableCallback(ClearCycleCollectorCleanupData);
  nsCycleCollector_setBeforeUnlinkCallback(ClearBlackMarkedNodes);
}

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(nsGenericElement)
  return nsGenericElement::CanSkip(tmp, aRemovingAllowed);
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_BEGIN(nsGenericElement)
  return nsGenericElement::CanSkipInCC(tmp);
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(nsGenericElement)
  return nsGenericElement::CanSkipThis(tmp);
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END

static const char* kNSURIs[] = {
  " ([none])",
  " (xmlns)",
  " (xml)",
  " (xhtml)",
  " (XLink)",
  " (XSLT)",
  " (XBL)",
  " (MathML)",
  " (RDF)",
  " (XUL)",
  " (SVG)",
  " (XML Events)"
};

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INTERNAL(nsGenericElement)
  if (NS_UNLIKELY(cb.WantDebugInfo())) {
    char name[512];
    PRUint32 nsid = tmp->GetNameSpaceID();
    nsAtomCString localName(tmp->NodeInfo()->NameAtom());
    nsCAutoString uri;
    if (tmp->OwnerDoc()->GetDocumentURI()) {
      tmp->OwnerDoc()->GetDocumentURI()->GetSpec(uri);
    }

    nsAutoString id;
    nsIAtom* idAtom = tmp->GetID();
    if (idAtom) {
      id.AppendLiteral(" id='");
      id.Append(nsDependentAtomString(idAtom));
      id.AppendLiteral("'");
    }

    nsAutoString classes;
    const nsAttrValue* classAttrValue = tmp->GetClasses();
    if (classAttrValue) {
      classes.AppendLiteral(" class='");
      nsAutoString classString;
      classAttrValue->ToString(classString);
      classString.ReplaceChar(PRUnichar('\n'), PRUnichar(' '));
      classes.Append(classString);
      classes.AppendLiteral("'");
    }

    const char* nsuri = nsid < ArrayLength(kNSURIs) ? kNSURIs[nsid] : "";
    PR_snprintf(name, sizeof(name), "nsGenericElement%s %s%s%s %s",
                nsuri,
                localName.get(),
                NS_ConvertUTF16toUTF8(id).get(),
                NS_ConvertUTF16toUTF8(classes).get(),
                uri.get());
    cb.DescribeRefCountedNode(tmp->mRefCnt.get(), sizeof(nsGenericElement),
                              name);
  }
  else {
    NS_IMPL_CYCLE_COLLECTION_DESCRIBE(nsGenericElement, tmp->mRefCnt.get())
  }

  // Always need to traverse script objects, so do that before we check
  // if we're uncollectable.
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS

  if (!nsINode::Traverse(tmp, cb)) {
    return NS_SUCCESS_INTERRUPTED_TRAVERSE;
  }

  tmp->OwnerDoc()->BindingManager()->Traverse(tmp, cb);

  if (tmp->HasProperties()) {
    if (tmp->IsHTML()) {
      nsISupports* property = static_cast<nsISupports*>
                                         (tmp->GetProperty(nsGkAtoms::microdataProperties));
      cb.NoteXPCOMChild(property);
      property = static_cast<nsISupports*>(tmp->GetProperty(nsGkAtoms::itemref));
      cb.NoteXPCOMChild(property);
      property = static_cast<nsISupports*>(tmp->GetProperty(nsGkAtoms::itemprop));
      cb.NoteXPCOMChild(property);
      property = static_cast<nsISupports*>(tmp->GetProperty(nsGkAtoms::itemtype));
      cb.NoteXPCOMChild(property);
    } else if (tmp->IsXUL()) {
      nsISupports* property = static_cast<nsISupports*>
                                         (tmp->GetProperty(nsGkAtoms::contextmenulistener));
      cb.NoteXPCOMChild(property);
      property = static_cast<nsISupports*>
                            (tmp->GetProperty(nsGkAtoms::popuplistener));
      cb.NoteXPCOMChild(property);
    }
  }

  // Traverse attribute names and child content.
  {
    PRUint32 i;
    PRUint32 attrs = tmp->mAttrsAndChildren.AttrCount();
    for (i = 0; i < attrs; i++) {
      const nsAttrName* name = tmp->mAttrsAndChildren.AttrNameAt(i);
      if (!name->IsAtom()) {
        NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb,
                                           "mAttrsAndChildren[i]->NodeInfo()");
        cb.NoteXPCOMChild(name->NodeInfo());
      }
    }

    PRUint32 kids = tmp->mAttrsAndChildren.ChildCount();
    for (i = 0; i < kids; i++) {
      NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mAttrsAndChildren[i]");
      cb.NoteXPCOMChild(tmp->mAttrsAndChildren.GetSafeChildAt(i));
    }
  }

  // Traverse any DOM slots of interest.
  {
    nsDOMSlots *slots = tmp->GetExistingDOMSlots();
    if (slots) {
      slots->Traverse(cb, tmp->IsXUL());
    }
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END


NS_INTERFACE_MAP_BEGIN(nsGenericElement)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(nsGenericElement)
  NS_INTERFACE_MAP_ENTRY(Element)
  NS_INTERFACE_MAP_ENTRY(nsIContent)
  NS_INTERFACE_MAP_ENTRY(nsINode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventTarget)
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsISupportsWeakReference,
                                 new nsNodeSupportsWeakRefTearoff(this))
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIDOMNodeSelector,
                                 new nsNodeSelectorTearoff(this))
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIDOMXPathNSResolver,
                                 new nsNode3Tearoff(this))
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsITouchEventReceiver,
                                 new nsTouchEventReceiverTearoff(this))
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIInlineEventHandlers,
                                 new nsInlineEventHandlersTearoff(this))
  // nsNodeSH::PreCreate() depends on the identity pointer being the
  // same as nsINode (which nsIContent inherits), so if you change the
  // below line, make sure nsNodeSH::PreCreate() still does the right
  // thing!
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIContent)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsGenericElement)
NS_IMPL_CYCLE_COLLECTING_RELEASE_WITH_DESTROY(nsGenericElement,
                                              nsNodeUtils::LastRelease(this))

nsresult
nsGenericElement::PostQueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  return OwnerDoc()->BindingManager()->GetBindingImplementation(this, aIID,
                                                                aInstancePtr);
}

//----------------------------------------------------------------------
nsresult
nsGenericElement::LeaveLink(nsPresContext* aPresContext)
{
  nsILinkHandler *handler = aPresContext->GetLinkHandler();
  if (!handler) {
    return NS_OK;
  }

  return handler->OnLeaveLink();
}

nsresult
nsGenericElement::AddScriptEventListener(nsIAtom* aEventName,
                                         const nsAString& aValue,
                                         bool aDefer)
{
  nsIDocument *ownerDoc = OwnerDoc();
  if (ownerDoc->IsLoadedAsData()) {
    // Make this a no-op rather than throwing an error to avoid
    // the error causing problems setting the attribute.
    return NS_OK;
  }

  NS_PRECONDITION(aEventName, "Must have event name!");
  bool defer = true;
  nsEventListenerManager* manager = GetEventListenerManagerForAttr(aEventName,
                                                                   &defer);
  if (!manager) {
    return NS_OK;
  }

  defer = defer && aDefer; // only defer if everyone agrees...
  manager->AddScriptEventListener(aEventName, aValue, nsIProgrammingLanguage::JAVASCRIPT,
                                  defer, !nsContentUtils::IsChromeDoc(ownerDoc));
  return NS_OK;
}


//----------------------------------------------------------------------

const nsAttrName*
nsGenericElement::InternalGetExistingAttrNameFromQName(const nsAString& aStr) const
{
  return mAttrsAndChildren.GetExistingAttrNameFromQName(aStr);
}

nsresult
nsGenericElement::CopyInnerTo(nsGenericElement* aDst)
{
  PRUint32 i, count = mAttrsAndChildren.AttrCount();
  for (i = 0; i < count; ++i) {
    const nsAttrName* name = mAttrsAndChildren.AttrNameAt(i);
    const nsAttrValue* value = mAttrsAndChildren.AttrAt(i);
    nsAutoString valStr;
    value->ToString(valStr);
    nsresult rv = aDst->SetAttr(name->NamespaceID(), name->LocalName(),
                                name->GetPrefix(), valStr, false);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

bool
nsGenericElement::MaybeCheckSameAttrVal(PRInt32 aNamespaceID,
                                        nsIAtom* aName,
                                        nsIAtom* aPrefix,
                                        const nsAttrValueOrString& aValue,
                                        bool aNotify,
                                        nsAttrValue& aOldValue,
                                        PRUint8* aModType,
                                        bool* aHasListeners)
{
  bool modification = false;
  *aHasListeners = aNotify &&
    nsContentUtils::HasMutationListeners(this,
                                         NS_EVENT_BITS_MUTATION_ATTRMODIFIED,
                                         this);

  // If we have no listeners and aNotify is false, we are almost certainly
  // coming from the content sink and will almost certainly have no previous
  // value.  Even if we do, setting the value is cheap when we have no
  // listeners and don't plan to notify.  The check for aNotify here is an
  // optimization, the check for *aHasListeners is a correctness issue.
  if (*aHasListeners || aNotify) {
    nsAttrInfo info(GetAttrInfo(aNamespaceID, aName));
    if (info.mValue) {
      // Check whether the old value is the same as the new one.  Note that we
      // only need to actually _get_ the old value if we have listeners.
      if (*aHasListeners) {
        // Need to store the old value.
        //
        // If the current attribute value contains a pointer to some other data
        // structure that gets updated in the process of setting the attribute
        // we'll no longer have the old value of the attribute. Therefore, we
        // should serialize the attribute value now to keep a snapshot.
        //
        // We have to serialize the value anyway in order to create the
        // mutation event so there's no cost in doing it now.
        aOldValue.SetToSerialized(*info.mValue);
      }
      bool valueMatches = aValue.EqualsAsStrings(*info.mValue);
      if (valueMatches && aPrefix == info.mName->GetPrefix()) {
        if (OwnerDoc()->MayHaveDOMMutationObservers()) {
          // For backward compatibility, don't fire mutation events
          // when setting an attribute to its old value.
          *aHasListeners = false;
        } else {
          return true;
        }
      }
      modification = true;
    }
  }
  *aModType = modification ?
    static_cast<PRUint8>(nsIDOMMutationEvent::MODIFICATION) :
    static_cast<PRUint8>(nsIDOMMutationEvent::ADDITION);
  return false;
}

nsresult
nsGenericElement::SetAttr(PRInt32 aNamespaceID, nsIAtom* aName,
                          nsIAtom* aPrefix, const nsAString& aValue,
                          bool aNotify)
{
  // Keep this in sync with SetParsedAttr below

  NS_ENSURE_ARG_POINTER(aName);
  NS_ASSERTION(aNamespaceID != kNameSpaceID_Unknown,
               "Don't call SetAttr with unknown namespace");

  if (!mAttrsAndChildren.CanFitMoreAttrs()) {
    return NS_ERROR_FAILURE;
  }

  PRUint8 modType;
  bool hasListeners;
  nsAttrValueOrString value(aValue);
  nsAttrValue oldValue;

  if (MaybeCheckSameAttrVal(aNamespaceID, aName, aPrefix, value, aNotify,
                            oldValue, &modType, &hasListeners)) {
    return NS_OK;
  }

  nsresult rv = BeforeSetAttr(aNamespaceID, aName, &value, aNotify);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aNotify) {
    nsNodeUtils::AttributeWillChange(this, aNamespaceID, aName, modType);
  }

  // Hold a script blocker while calling ParseAttribute since that can call
  // out to id-observers
  nsAutoScriptBlocker scriptBlocker;

  nsAttrValue attrValue;
  if (!ParseAttribute(aNamespaceID, aName, aValue, attrValue)) {
    attrValue.SetTo(aValue);
  }

  return SetAttrAndNotify(aNamespaceID, aName, aPrefix, oldValue,
                          attrValue, modType, hasListeners, aNotify,
                          kCallAfterSetAttr);
}

nsresult
nsGenericElement::SetParsedAttr(PRInt32 aNamespaceID, nsIAtom* aName,
                                nsIAtom* aPrefix, nsAttrValue& aParsedValue,
                                bool aNotify)
{
  // Keep this in sync with SetAttr above

  NS_ENSURE_ARG_POINTER(aName);
  NS_ASSERTION(aNamespaceID != kNameSpaceID_Unknown,
               "Don't call SetAttr with unknown namespace");

  if (!mAttrsAndChildren.CanFitMoreAttrs()) {
    return NS_ERROR_FAILURE;
  }


  PRUint8 modType;
  bool hasListeners;
  nsAttrValueOrString value(aParsedValue);
  nsAttrValue oldValue;

  if (MaybeCheckSameAttrVal(aNamespaceID, aName, aPrefix, value, aNotify,
                            oldValue, &modType, &hasListeners)) {
    return NS_OK;
  }

  nsresult rv = BeforeSetAttr(aNamespaceID, aName, &value, aNotify);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aNotify) {
    nsNodeUtils::AttributeWillChange(this, aNamespaceID, aName, modType);
  }

  return SetAttrAndNotify(aNamespaceID, aName, aPrefix, oldValue,
                          aParsedValue, modType, hasListeners, aNotify,
                          kCallAfterSetAttr);
}

nsresult
nsGenericElement::SetAttrAndNotify(PRInt32 aNamespaceID,
                                   nsIAtom* aName,
                                   nsIAtom* aPrefix,
                                   const nsAttrValue& aOldValue,
                                   nsAttrValue& aParsedValue,
                                   PRUint8 aModType,
                                   bool aFireMutation,
                                   bool aNotify,
                                   bool aCallAfterSetAttr)
{
  nsresult rv;

  nsIDocument* document = GetCurrentDoc();
  mozAutoDocUpdate updateBatch(document, UPDATE_CONTENT_MODEL, aNotify);

  nsMutationGuard::DidMutate();

  // Copy aParsedValue for later use since it will be lost when we call
  // SetAndTakeMappedAttr below
  nsAttrValue aValueForAfterSetAttr;
  if (aCallAfterSetAttr) {
    aValueForAfterSetAttr.SetTo(aParsedValue);
  }

  if (aNamespaceID == kNameSpaceID_None) {
    // XXXbz Perhaps we should push up the attribute mapping function
    // stuff to nsGenericElement?
    if (!IsAttributeMapped(aName) ||
        !SetMappedAttribute(document, aName, aParsedValue, &rv)) {
      rv = mAttrsAndChildren.SetAndTakeAttr(aName, aParsedValue);
    }
  }
  else {
    nsCOMPtr<nsINodeInfo> ni;
    ni = mNodeInfo->NodeInfoManager()->GetNodeInfo(aName, aPrefix,
                                                   aNamespaceID,
                                                   nsIDOMNode::ATTRIBUTE_NODE);
    NS_ENSURE_TRUE(ni, NS_ERROR_OUT_OF_MEMORY);

    rv = mAttrsAndChildren.SetAndTakeAttr(ni, aParsedValue);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  if (document || HasFlag(NODE_FORCE_XBL_BINDINGS)) {
    nsRefPtr<nsXBLBinding> binding =
      OwnerDoc()->BindingManager()->GetBinding(this);
    if (binding) {
      binding->AttributeChanged(aName, aNamespaceID, false, aNotify);
    }
  }

  UpdateState(aNotify);

  if (aNotify) {
    nsNodeUtils::AttributeChanged(this, aNamespaceID, aName, aModType);
  }

  if (aNamespaceID == kNameSpaceID_XMLEvents && 
      aName == nsGkAtoms::event && mNodeInfo->GetDocument()) {
    mNodeInfo->GetDocument()->AddXMLEventsContent(this);
  }
  if (aCallAfterSetAttr) {
    rv = AfterSetAttr(aNamespaceID, aName, &aValueForAfterSetAttr, aNotify);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (aFireMutation) {
    nsMutationEvent mutation(true, NS_MUTATION_ATTRMODIFIED);

    nsCOMPtr<nsIDOMAttr> attrNode;
    nsAutoString ns;
    nsContentUtils::NameSpaceManager()->GetNameSpaceURI(aNamespaceID, ns);
    GetAttributeNodeNSInternal(ns, nsDependentAtomString(aName),
                               getter_AddRefs(attrNode));
    mutation.mRelatedNode = attrNode;

    mutation.mAttrName = aName;
    nsAutoString newValue;
    GetAttr(aNamespaceID, aName, newValue);
    if (!newValue.IsEmpty()) {
      mutation.mNewAttrValue = do_GetAtom(newValue);
    }
    if (!aOldValue.IsEmptyString()) {
      mutation.mPrevAttrValue = aOldValue.GetAsAtom();
    }
    mutation.mAttrChange = aModType;

    mozAutoSubtreeModified subtree(OwnerDoc(), this);
    (new nsAsyncDOMEvent(this, mutation))->RunDOMEventWhenSafe();
  }

  return NS_OK;
}

bool
nsGenericElement::ParseAttribute(PRInt32 aNamespaceID,
                                 nsIAtom* aAttribute,
                                 const nsAString& aValue,
                                 nsAttrValue& aResult)
{
  return false;
}

bool
nsGenericElement::SetMappedAttribute(nsIDocument* aDocument,
                                     nsIAtom* aName,
                                     nsAttrValue& aValue,
                                     nsresult* aRetval)
{
  *aRetval = NS_OK;
  return false;
}

nsEventListenerManager*
nsGenericElement::GetEventListenerManagerForAttr(nsIAtom* aAttrName,
                                                 bool* aDefer)
{
  *aDefer = true;
  return GetListenerManager(true);
}

nsGenericElement::nsAttrInfo
nsGenericElement::GetAttrInfo(PRInt32 aNamespaceID, nsIAtom* aName) const
{
  NS_ASSERTION(nsnull != aName, "must have attribute name");
  NS_ASSERTION(aNamespaceID != kNameSpaceID_Unknown,
               "must have a real namespace ID!");

  PRInt32 index = mAttrsAndChildren.IndexOfAttr(aName, aNamespaceID);
  if (index >= 0) {
    return nsAttrInfo(mAttrsAndChildren.AttrNameAt(index),
                      mAttrsAndChildren.AttrAt(index));
  }

  return nsAttrInfo(nsnull, nsnull);
}
  

bool
nsGenericElement::GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                          nsAString& aResult) const
{
  NS_ASSERTION(nsnull != aName, "must have attribute name");
  NS_ASSERTION(aNameSpaceID != kNameSpaceID_Unknown,
               "must have a real namespace ID!");

  const nsAttrValue* val = mAttrsAndChildren.GetAttr(aName, aNameSpaceID);
  if (!val) {
    // Since we are returning a success code we'd better do
    // something about the out parameters (someone may have
    // given us a non-empty string).
    aResult.Truncate();
    
    return false;
  }

  val->ToString(aResult);

  return true;
}

bool
nsGenericElement::HasAttr(PRInt32 aNameSpaceID, nsIAtom* aName) const
{
  NS_ASSERTION(nsnull != aName, "must have attribute name");
  NS_ASSERTION(aNameSpaceID != kNameSpaceID_Unknown,
               "must have a real namespace ID!");

  return mAttrsAndChildren.IndexOfAttr(aName, aNameSpaceID) >= 0;
}

bool
nsGenericElement::AttrValueIs(PRInt32 aNameSpaceID,
                              nsIAtom* aName,
                              const nsAString& aValue,
                              nsCaseTreatment aCaseSensitive) const
{
  NS_ASSERTION(aName, "Must have attr name");
  NS_ASSERTION(aNameSpaceID != kNameSpaceID_Unknown, "Must have namespace");

  const nsAttrValue* val = mAttrsAndChildren.GetAttr(aName, aNameSpaceID);
  return val && val->Equals(aValue, aCaseSensitive);
}

bool
nsGenericElement::AttrValueIs(PRInt32 aNameSpaceID,
                              nsIAtom* aName,
                              nsIAtom* aValue,
                              nsCaseTreatment aCaseSensitive) const
{
  NS_ASSERTION(aName, "Must have attr name");
  NS_ASSERTION(aNameSpaceID != kNameSpaceID_Unknown, "Must have namespace");
  NS_ASSERTION(aValue, "Null value atom");

  const nsAttrValue* val = mAttrsAndChildren.GetAttr(aName, aNameSpaceID);
  return val && val->Equals(aValue, aCaseSensitive);
}

PRInt32
nsGenericElement::FindAttrValueIn(PRInt32 aNameSpaceID,
                                  nsIAtom* aName,
                                  AttrValuesArray* aValues,
                                  nsCaseTreatment aCaseSensitive) const
{
  NS_ASSERTION(aName, "Must have attr name");
  NS_ASSERTION(aNameSpaceID != kNameSpaceID_Unknown, "Must have namespace");
  NS_ASSERTION(aValues, "Null value array");
  
  const nsAttrValue* val = mAttrsAndChildren.GetAttr(aName, aNameSpaceID);
  if (val) {
    for (PRInt32 i = 0; aValues[i]; ++i) {
      if (val->Equals(*aValues[i], aCaseSensitive)) {
        return i;
      }
    }
    return ATTR_VALUE_NO_MATCH;
  }
  return ATTR_MISSING;
}

nsresult
nsGenericElement::UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                            bool aNotify)
{
  NS_ASSERTION(nsnull != aName, "must have attribute name");

  PRInt32 index = mAttrsAndChildren.IndexOfAttr(aName, aNameSpaceID);
  if (index < 0) {
    return NS_OK;
  }

  nsresult rv = BeforeSetAttr(aNameSpaceID, aName, nsnull, aNotify);
  NS_ENSURE_SUCCESS(rv, rv);

  nsIDocument *document = GetCurrentDoc();
  mozAutoDocUpdate updateBatch(document, UPDATE_CONTENT_MODEL, aNotify);

  if (aNotify) {
    nsNodeUtils::AttributeWillChange(this, aNameSpaceID, aName,
                                     nsIDOMMutationEvent::REMOVAL);
  }

  bool hasMutationListeners = aNotify &&
    nsContentUtils::HasMutationListeners(this,
                                         NS_EVENT_BITS_MUTATION_ATTRMODIFIED,
                                         this);

  // Grab the attr node if needed before we remove it from the attr map
  nsCOMPtr<nsIDOMAttr> attrNode;
  if (hasMutationListeners) {
    nsAutoString ns;
    nsContentUtils::NameSpaceManager()->GetNameSpaceURI(aNameSpaceID, ns);
    GetAttributeNodeNSInternal(ns, nsDependentAtomString(aName),
                               getter_AddRefs(attrNode));
  }

  // Clear binding to nsIDOMNamedNodeMap
  nsDOMSlots *slots = GetExistingDOMSlots();
  if (slots && slots->mAttributeMap) {
    slots->mAttributeMap->DropAttribute(aNameSpaceID, aName);
  }

  // The id-handling code, and in the future possibly other code, need to
  // react to unexpected attribute changes.
  nsMutationGuard::DidMutate();

  nsAttrValue oldValue;
  rv = mAttrsAndChildren.RemoveAttrAt(index, oldValue);
  NS_ENSURE_SUCCESS(rv, rv);

  if (document || HasFlag(NODE_FORCE_XBL_BINDINGS)) {
    nsRefPtr<nsXBLBinding> binding =
      OwnerDoc()->BindingManager()->GetBinding(this);
    if (binding) {
      binding->AttributeChanged(aName, aNameSpaceID, true, aNotify);
    }
  }

  UpdateState(aNotify);

  if (aNotify) {
    nsNodeUtils::AttributeChanged(this, aNameSpaceID, aName,
                                  nsIDOMMutationEvent::REMOVAL);
  }

  rv = AfterSetAttr(aNameSpaceID, aName, nsnull, aNotify);
  NS_ENSURE_SUCCESS(rv, rv);

  if (hasMutationListeners) {
    nsCOMPtr<nsIDOMEventTarget> node = do_QueryObject(this);
    nsMutationEvent mutation(true, NS_MUTATION_ATTRMODIFIED);

    mutation.mRelatedNode = attrNode;
    mutation.mAttrName = aName;

    nsAutoString value;
    oldValue.ToString(value);
    if (!value.IsEmpty())
      mutation.mPrevAttrValue = do_GetAtom(value);
    mutation.mAttrChange = nsIDOMMutationEvent::REMOVAL;

    mozAutoSubtreeModified subtree(OwnerDoc(), this);
    (new nsAsyncDOMEvent(this, mutation))->RunDOMEventWhenSafe();
  }

  return NS_OK;
}

const nsAttrName*
nsGenericElement::GetAttrNameAt(PRUint32 aIndex) const
{
  return mAttrsAndChildren.GetSafeAttrNameAt(aIndex);
}

PRUint32
nsGenericElement::GetAttrCount() const
{
  return mAttrsAndChildren.AttrCount();
}

const nsTextFragment*
nsGenericElement::GetText()
{
  return nsnull;
}

PRUint32
nsGenericElement::TextLength() const
{
  // We can remove this assertion if it turns out to be useful to be able
  // to depend on this returning 0
  NS_NOTREACHED("called nsGenericElement::TextLength");

  return 0;
}

nsresult
nsGenericElement::SetText(const PRUnichar* aBuffer, PRUint32 aLength,
                          bool aNotify)
{
  NS_ERROR("called nsGenericElement::SetText");

  return NS_ERROR_FAILURE;
}

nsresult
nsGenericElement::AppendText(const PRUnichar* aBuffer, PRUint32 aLength,
                             bool aNotify)
{
  NS_ERROR("called nsGenericElement::AppendText");

  return NS_ERROR_FAILURE;
}

bool
nsGenericElement::TextIsOnlyWhitespace()
{
  return false;
}

void
nsGenericElement::AppendTextTo(nsAString& aResult)
{
  // We can remove this assertion if it turns out to be useful to be able
  // to depend on this appending nothing.
  NS_NOTREACHED("called nsGenericElement::TextLength");
}

#ifdef DEBUG
void
nsGenericElement::ListAttributes(FILE* out) const
{
  PRUint32 index, count = mAttrsAndChildren.AttrCount();
  for (index = 0; index < count; index++) {
    nsAutoString buffer;

    // name
    mAttrsAndChildren.AttrNameAt(index)->GetQualifiedName(buffer);

    // value
    buffer.AppendLiteral("=\"");
    nsAutoString value;
    mAttrsAndChildren.AttrAt(index)->ToString(value);
    for (int i = value.Length(); i >= 0; --i) {
      if (value[i] == PRUnichar('"'))
        value.Insert(PRUnichar('\\'), PRUint32(i));
    }
    buffer.Append(value);
    buffer.AppendLiteral("\"");

    fputs(" ", out);
    fputs(NS_LossyConvertUTF16toASCII(buffer).get(), out);
  }
}

void
nsGenericElement::List(FILE* out, PRInt32 aIndent,
                       const nsCString& aPrefix) const
{
  PRInt32 indent;
  for (indent = aIndent; --indent >= 0; ) fputs("  ", out);

  fputs(aPrefix.get(), out);

  fputs(NS_LossyConvertUTF16toASCII(mNodeInfo->QualifiedName()).get(), out);

  fprintf(out, "@%p", (void *)this);

  ListAttributes(out);

  fprintf(out, " state=[%llx]", State().GetInternalValue());
  fprintf(out, " flags=[%08x]", static_cast<unsigned int>(GetFlags()));
  if (IsCommonAncestorForRangeInSelection()) {
    nsRange::RangeHashTable* ranges =
      static_cast<nsRange::RangeHashTable*>(GetProperty(nsGkAtoms::range));
    fprintf(out, " ranges:%d", ranges ? ranges->Count() : 0);
  }
  fprintf(out, " primaryframe=%p", static_cast<void*>(GetPrimaryFrame()));
  fprintf(out, " refcount=%d<", mRefCnt.get());

  nsIContent* child = GetFirstChild();
  if (child) {
    fputs("\n", out);
    
    for (; child; child = child->GetNextSibling()) {
      child->List(out, aIndent + 1);
    }

    for (indent = aIndent; --indent >= 0; ) fputs("  ", out);
  }

  fputs(">\n", out);
  
  nsGenericElement* nonConstThis = const_cast<nsGenericElement*>(this);

  // XXX sXBL/XBL2 issue! Owner or current document?
  nsIDocument *document = OwnerDoc();

  // Note: not listing nsIAnonymousContentCreator-created content...

  nsBindingManager* bindingManager = document->BindingManager();
  nsCOMPtr<nsIDOMNodeList> anonymousChildren;
  bindingManager->GetAnonymousNodesFor(nonConstThis,
                                       getter_AddRefs(anonymousChildren));

  if (anonymousChildren) {
    PRUint32 length;
    anonymousChildren->GetLength(&length);
    if (length > 0) {
      for (indent = aIndent; --indent >= 0; ) fputs("  ", out);
      fputs("anonymous-children<\n", out);

      for (PRUint32 i = 0; i < length; ++i) {
        nsCOMPtr<nsIDOMNode> node;
        anonymousChildren->Item(i, getter_AddRefs(node));
        nsCOMPtr<nsIContent> child = do_QueryInterface(node);
        child->List(out, aIndent + 1);
      }

      for (indent = aIndent; --indent >= 0; ) fputs("  ", out);
      fputs(">\n", out);
    }
  }

  if (bindingManager->HasContentListFor(nonConstThis)) {
    nsCOMPtr<nsIDOMNodeList> contentList;
    bindingManager->GetContentListFor(nonConstThis,
                                      getter_AddRefs(contentList));

    NS_ASSERTION(contentList != nsnull, "oops, binding manager lied");
    
    PRUint32 length;
    contentList->GetLength(&length);
    if (length > 0) {
      for (indent = aIndent; --indent >= 0; ) fputs("  ", out);
      fputs("content-list<\n", out);

      for (PRUint32 i = 0; i < length; ++i) {
        nsCOMPtr<nsIDOMNode> node;
        contentList->Item(i, getter_AddRefs(node));
        nsCOMPtr<nsIContent> child = do_QueryInterface(node);
        child->List(out, aIndent + 1);
      }

      for (indent = aIndent; --indent >= 0; ) fputs("  ", out);
      fputs(">\n", out);
    }
  }
}

void
nsGenericElement::DumpContent(FILE* out, PRInt32 aIndent,
                              bool aDumpAll) const
{
  PRInt32 indent;
  for (indent = aIndent; --indent >= 0; ) fputs("  ", out);

  const nsString& buf = mNodeInfo->QualifiedName();
  fputs("<", out);
  fputs(NS_LossyConvertUTF16toASCII(buf).get(), out);

  if(aDumpAll) ListAttributes(out);

  fputs(">", out);

  if(aIndent) fputs("\n", out);

  for (nsIContent* child = GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    PRInt32 indent = aIndent ? aIndent + 1 : 0;
    child->DumpContent(out, indent, aDumpAll);
  }
  for (indent = aIndent; --indent >= 0; ) fputs("  ", out);
  fputs("</", out);
  fputs(NS_LossyConvertUTF16toASCII(buf).get(), out);
  fputs(">", out);

  if(aIndent) fputs("\n", out);
}
#endif

PRUint32
nsGenericElement::GetChildCount() const
{
  return mAttrsAndChildren.ChildCount();
}

nsIContent *
nsGenericElement::GetChildAt(PRUint32 aIndex) const
{
  return mAttrsAndChildren.GetSafeChildAt(aIndex);
}

nsIContent * const *
nsGenericElement::GetChildArray(PRUint32* aChildCount) const
{
  return mAttrsAndChildren.GetChildArray(aChildCount);
}

PRInt32
nsGenericElement::IndexOf(nsINode* aPossibleChild) const
{
  return mAttrsAndChildren.IndexOfChild(aPossibleChild);
}

nsINode::nsSlots*
nsGenericElement::CreateSlots()
{
  return new nsDOMSlots();
}

bool
nsGenericElement::CheckHandleEventForLinksPrecondition(nsEventChainVisitor& aVisitor,
                                                       nsIURI** aURI) const
{
  if (aVisitor.mEventStatus == nsEventStatus_eConsumeNoDefault ||
      (!NS_IS_TRUSTED_EVENT(aVisitor.mEvent) &&
       (aVisitor.mEvent->message != NS_MOUSE_CLICK) &&
       (aVisitor.mEvent->message != NS_KEY_PRESS) &&
       (aVisitor.mEvent->message != NS_UI_ACTIVATE)) ||
      !aVisitor.mPresContext ||
      (aVisitor.mEvent->flags & NS_EVENT_FLAG_PREVENT_MULTIPLE_ACTIONS)) {
    return false;
  }

  // Make sure we actually are a link
  return IsLink(aURI);
}

nsresult
nsGenericElement::PreHandleEventForLinks(nsEventChainPreVisitor& aVisitor)
{
  // Optimisation: return early if this event doesn't interest us.
  // IMPORTANT: this switch and the switch below it must be kept in sync!
  switch (aVisitor.mEvent->message) {
  case NS_MOUSE_ENTER_SYNTH:
  case NS_FOCUS_CONTENT:
  case NS_MOUSE_EXIT_SYNTH:
  case NS_BLUR_CONTENT:
    break;
  default:
    return NS_OK;
  }

  // Make sure we meet the preconditions before continuing
  nsCOMPtr<nsIURI> absURI;
  if (!CheckHandleEventForLinksPrecondition(aVisitor, getter_AddRefs(absURI))) {
    return NS_OK;
  }

  nsresult rv = NS_OK;

  // We do the status bar updates in PreHandleEvent so that the status bar gets
  // updated even if the event is consumed before we have a chance to set it.
  switch (aVisitor.mEvent->message) {
  // Set the status bar similarly for mouseover and focus
  case NS_MOUSE_ENTER_SYNTH:
    aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
    // FALL THROUGH
  case NS_FOCUS_CONTENT:
    if (aVisitor.mEvent->eventStructType != NS_FOCUS_EVENT ||
        !static_cast<nsFocusEvent*>(aVisitor.mEvent)->isRefocus) {
      nsAutoString target;
      GetLinkTarget(target);
      nsContentUtils::TriggerLink(this, aVisitor.mPresContext, absURI, target,
                                  false, true, true);
      // Make sure any ancestor links don't also TriggerLink
      aVisitor.mEvent->flags |= NS_EVENT_FLAG_PREVENT_MULTIPLE_ACTIONS;
    }
    break;

  case NS_MOUSE_EXIT_SYNTH:
    aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
    // FALL THROUGH
  case NS_BLUR_CONTENT:
    rv = LeaveLink(aVisitor.mPresContext);
    if (NS_SUCCEEDED(rv)) {
      aVisitor.mEvent->flags |= NS_EVENT_FLAG_PREVENT_MULTIPLE_ACTIONS;
    }
    break;

  default:
    // switch not in sync with the optimization switch earlier in this function
    NS_NOTREACHED("switch statements not in sync");
    return NS_ERROR_UNEXPECTED;
  }

  return rv;
}

nsresult
nsGenericElement::PostHandleEventForLinks(nsEventChainPostVisitor& aVisitor)
{
  // Optimisation: return early if this event doesn't interest us.
  // IMPORTANT: this switch and the switch below it must be kept in sync!
  switch (aVisitor.mEvent->message) {
  case NS_MOUSE_BUTTON_DOWN:
  case NS_MOUSE_CLICK:
  case NS_UI_ACTIVATE:
  case NS_KEY_PRESS:
    break;
  default:
    return NS_OK;
  }

  // Make sure we meet the preconditions before continuing
  nsCOMPtr<nsIURI> absURI;
  if (!CheckHandleEventForLinksPrecondition(aVisitor, getter_AddRefs(absURI))) {
    return NS_OK;
  }

  nsresult rv = NS_OK;

  switch (aVisitor.mEvent->message) {
  case NS_MOUSE_BUTTON_DOWN:
    {
      if (aVisitor.mEvent->eventStructType == NS_MOUSE_EVENT &&
          static_cast<nsMouseEvent*>(aVisitor.mEvent)->button ==
          nsMouseEvent::eLeftButton) {
        // don't make the link grab the focus if there is no link handler
        nsILinkHandler *handler = aVisitor.mPresContext->GetLinkHandler();
        nsIDocument *document = GetCurrentDoc();
        if (handler && document) {
          nsIFocusManager* fm = nsFocusManager::GetFocusManager();
          if (fm) {
            aVisitor.mEvent->flags |= NS_EVENT_FLAG_PREVENT_MULTIPLE_ACTIONS;
            nsCOMPtr<nsIDOMElement> elem = do_QueryInterface(this);
            fm->SetFocus(elem, nsIFocusManager::FLAG_BYMOUSE |
                               nsIFocusManager::FLAG_NOSCROLL);
          }

          nsEventStateManager::SetActiveManager(
            aVisitor.mPresContext->EventStateManager(), this);
        }
      }
    }
    break;

  case NS_MOUSE_CLICK:
    if (NS_IS_MOUSE_LEFT_CLICK(aVisitor.mEvent)) {
      nsInputEvent* inputEvent = static_cast<nsInputEvent*>(aVisitor.mEvent);
      if (inputEvent->IsControl() || inputEvent->IsMeta() ||
          inputEvent->IsAlt() ||inputEvent->IsShift()) {
        break;
      }

      // The default action is simply to dispatch DOMActivate
      nsCOMPtr<nsIPresShell> shell = aVisitor.mPresContext->GetPresShell();
      if (shell) {
        // single-click
        nsEventStatus status = nsEventStatus_eIgnore;
        nsUIEvent actEvent(NS_IS_TRUSTED_EVENT(aVisitor.mEvent),
                           NS_UI_ACTIVATE, 1);

        rv = shell->HandleDOMEventWithTarget(this, &actEvent, &status);
        if (NS_SUCCEEDED(rv)) {
          aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
        }
      }
    }
    break;

  case NS_UI_ACTIVATE:
    {
      if (aVisitor.mEvent->originalTarget == this) {
        nsAutoString target;
        GetLinkTarget(target);
        nsContentUtils::TriggerLink(this, aVisitor.mPresContext, absURI, target,
                                    true, true, NS_IS_TRUSTED_EVENT(aVisitor.mEvent));
        aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
      }
    }
    break;

  case NS_KEY_PRESS:
    {
      if (aVisitor.mEvent->eventStructType == NS_KEY_EVENT) {
        nsKeyEvent* keyEvent = static_cast<nsKeyEvent*>(aVisitor.mEvent);
        if (keyEvent->keyCode == NS_VK_RETURN) {
          nsEventStatus status = nsEventStatus_eIgnore;
          rv = DispatchClickEvent(aVisitor.mPresContext, keyEvent, this,
                                  false, 0, &status);
          if (NS_SUCCEEDED(rv)) {
            aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
          }
        }
      }
    }
    break;

  default:
    // switch not in sync with the optimization switch earlier in this function
    NS_NOTREACHED("switch statements not in sync");
    return NS_ERROR_UNEXPECTED;
  }

  return rv;
}

void
nsGenericElement::FireNodeRemovedForChildren()
{
  nsIDocument* doc = OwnerDoc();
  // Optimize the common case
  if (!nsContentUtils::
        HasMutationListeners(doc, NS_EVENT_BITS_MUTATION_NODEREMOVED)) {
    return;
  }

  nsCOMPtr<nsIDocument> owningDoc = doc;

  nsCOMPtr<nsINode> child;
  for (child = GetFirstChild();
       child && child->GetNodeParent() == this;
       child = child->GetNextSibling()) {
    nsContentUtils::MaybeFireNodeRemoved(child, this, doc);
  }
}

void
nsGenericElement::GetLinkTarget(nsAString& aTarget)
{
  aTarget.Truncate();
}

// NOTE: The aPresContext pointer is NOT addrefed.
// *aSelectorList might be null even if NS_OK is returned; this
// happens when all the selectors were pseudo-element selectors.
static nsresult
ParseSelectorList(nsINode* aNode,
                  const nsAString& aSelectorString,
                  nsCSSSelectorList** aSelectorList)
{
  NS_ENSURE_ARG(aNode);

  nsIDocument* doc = aNode->OwnerDoc();
  nsCSSParser parser(doc->CSSLoader());

  nsCSSSelectorList* selectorList;
  nsresult rv = parser.ParseSelectorString(aSelectorString,
                                           doc->GetDocumentURI(),
                                           0, // XXXbz get the line number!
                                           &selectorList);
  NS_ENSURE_SUCCESS(rv, rv);

  // Filter out pseudo-element selectors from selectorList
  nsCSSSelectorList** slot = &selectorList;
  do {
    nsCSSSelectorList* cur = *slot;
    if (cur->mSelectors->IsPseudoElement()) {
      *slot = cur->mNext;
      cur->mNext = nsnull;
      delete cur;
    } else {
      slot = &cur->mNext;
    }
  } while (*slot);
  *aSelectorList = selectorList;

  return NS_OK;
}

// Actually find elements matching aSelectorList (which must not be
// null) and which are descendants of aRoot and put them in aList.  If
// onlyFirstMatch, then stop once the first one is found.
template<bool onlyFirstMatch, class T>
inline static nsresult FindMatchingElements(nsINode* aRoot,
                                            const nsAString& aSelector,
                                            T &aList)
{
  nsAutoPtr<nsCSSSelectorList> selectorList;
  nsresult rv = ParseSelectorList(aRoot, aSelector,
                                  getter_Transfers(selectorList));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(selectorList, NS_OK);

  NS_ASSERTION(selectorList->mSelectors,
               "How can we not have any selectors?");

  nsIDocument* doc = aRoot->OwnerDoc();  
  TreeMatchContext matchingContext(false, nsRuleWalker::eRelevantLinkUnvisited,
                                   doc, TreeMatchContext::eNeverMatchVisited);
  doc->FlushPendingLinkUpdates();

  // Fast-path selectors involving IDs.  We can only do this if aRoot
  // is in the document and the document is not in quirks mode, since
  // ID selectors are case-insensitive in quirks mode.  Also, only do
  // this if selectorList only has one selector, because otherwise
  // ordering the elements correctly is a pain.
  NS_ASSERTION(aRoot->IsElement() || aRoot->IsNodeOfType(nsINode::eDOCUMENT) ||
               !aRoot->IsInDoc(),
               "The optimization below to check ContentIsDescendantOf only for "
               "elements depends on aRoot being either an element or a "
               "document if it's in the document.");
  if (aRoot->IsInDoc() &&
      doc->GetCompatibilityMode() != eCompatibility_NavQuirks &&
      !selectorList->mNext &&
      selectorList->mSelectors->mIDList) {
    nsIAtom* id = selectorList->mSelectors->mIDList->mAtom;
    const nsSmallVoidArray* elements =
      doc->GetAllElementsForId(nsDependentAtomString(id));

    // XXXbz: Should we fall back to the tree walk if aRoot is not the
    // document and |elements| is long, for some value of "long"?
    if (elements) {
      for (PRInt32 i = 0; i < elements->Count(); ++i) {
        Element *element = static_cast<Element*>(elements->ElementAt(i));
        if (!aRoot->IsElement() ||
            (element != aRoot &&
             nsContentUtils::ContentIsDescendantOf(element, aRoot))) {
          // We have an element with the right id and it's a strict descendant
          // of aRoot.  Make sure it really matches the selector.
          if (nsCSSRuleProcessor::SelectorListMatches(element, matchingContext,
                                                      selectorList)) {
            aList.AppendElement(element);
            if (onlyFirstMatch) {
              return NS_OK;
            }
          }
        }
      }
    }

    // No elements with this id, or none of them are our descendants,
    // or none of them match.  We're done here.
    return NS_OK;
  }

  for (nsIContent* cur = aRoot->GetFirstChild();
       cur;
       cur = cur->GetNextNode(aRoot)) {
    if (cur->IsElement() &&
        nsCSSRuleProcessor::SelectorListMatches(cur->AsElement(),
                                                matchingContext,
                                                selectorList)) {
      aList.AppendElement(cur->AsElement());
      if (onlyFirstMatch) {
        return NS_OK;
      }
    }
  }

  return NS_OK;
}

struct ElementHolder {
  ElementHolder() : mElement(nsnull) {}
  void AppendElement(Element* aElement) {
    NS_ABORT_IF_FALSE(!mElement, "Should only get one element");
    mElement = aElement;
  }
  Element* mElement;
};

/* static */
nsIContent*
nsGenericElement::doQuerySelector(nsINode* aRoot, const nsAString& aSelector,
                                  nsresult *aResult)
{
  NS_PRECONDITION(aResult, "Null out param?");

  ElementHolder holder;
  *aResult = FindMatchingElements<true>(aRoot, aSelector, holder);

  return holder.mElement;
}

/* static */
nsresult
nsGenericElement::doQuerySelectorAll(nsINode* aRoot,
                                     const nsAString& aSelector,
                                     nsIDOMNodeList **aReturn)
{
  NS_PRECONDITION(aReturn, "Null out param?");

  nsSimpleContentList* contentList = new nsSimpleContentList(aRoot);
  NS_ENSURE_TRUE(contentList, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(*aReturn = contentList);
  
  return FindMatchingElements<false>(aRoot, aSelector, *contentList);
}


bool
nsGenericElement::MozMatchesSelector(const nsAString& aSelector, nsresult* aResult)
{
  nsAutoPtr<nsCSSSelectorList> selectorList;
  bool matches = false;

  *aResult = ParseSelectorList(this, aSelector, getter_Transfers(selectorList));

  if (NS_SUCCEEDED(*aResult)) {
    OwnerDoc()->FlushPendingLinkUpdates();
    TreeMatchContext matchingContext(false,
                                     nsRuleWalker::eRelevantLinkUnvisited,
                                     OwnerDoc(),
                                     TreeMatchContext::eNeverMatchVisited);
    matches = nsCSSRuleProcessor::SelectorListMatches(this, matchingContext,
                                                      selectorList);
  }

  return matches;
}

NS_IMETHODIMP
nsGenericElement::MozMatchesSelector(const nsAString& aSelector, bool* aReturn)
{
  NS_PRECONDITION(aReturn, "Null out param?");

  nsresult rv;
  *aReturn = MozMatchesSelector(aSelector, &rv);

  return rv;
}

size_t
nsGenericElement::SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf) const
{
  return Element::SizeOfExcludingThis(aMallocSizeOf) +
         mAttrsAndChildren.SizeOfExcludingThis(aMallocSizeOf);
}

static const nsAttrValue::EnumTable kCORSAttributeTable[] = {
  // Order matters here
  // See ParseCORSValue
  { "anonymous",       CORS_ANONYMOUS       },
  { "use-credentials", CORS_USE_CREDENTIALS },
  { 0 }
};

/* static */ void
nsGenericElement::ParseCORSValue(const nsAString& aValue,
                                 nsAttrValue& aResult)
{
  DebugOnly<bool> success =
    aResult.ParseEnumValue(aValue, kCORSAttributeTable, false,
                           // default value is anonymous if aValue is
                           // not a value we understand
                           &kCORSAttributeTable[0]);
  MOZ_ASSERT(success);
}

/* static */ CORSMode
nsGenericElement::StringToCORSMode(const nsAString& aValue)
{
  if (aValue.IsVoid()) {
    return CORS_NONE;
  }

  nsAttrValue val;
  nsGenericElement::ParseCORSValue(aValue, val);
  return CORSMode(val.GetEnumValue());
}

/* static */ CORSMode
nsGenericElement::AttrValueToCORSMode(const nsAttrValue* aValue)
{
  if (!aValue) {
    return CORS_NONE;
  }

  return CORSMode(aValue->GetEnumValue());
}

NS_IMETHODIMP
nsGenericElement::GetOnmouseenter(JSContext* cx, JS::Value* vp)
{
  return nsINode::GetOnmouseenter(cx, vp);
}

NS_IMETHODIMP
nsGenericElement::SetOnmouseenter(JSContext* cx, const JS::Value& v)
{
  return nsINode::SetOnmouseenter(cx, v);
}

NS_IMETHODIMP
nsGenericElement::GetOnmouseleave(JSContext* cx, JS::Value* vp)
{
  return nsINode::GetOnmouseleave(cx, vp);
}

NS_IMETHODIMP
nsGenericElement::SetOnmouseleave(JSContext* cx, const JS::Value& v)
{
  return nsINode::SetOnmouseleave(cx, v);
}

NS_IMETHODIMP
nsGenericElement::MozRequestPointerLock()
{
  OwnerDoc()->RequestPointerLock(this);
  return NS_OK;
}

static const char*
GetFullScreenError(nsIDocument* aDoc)
{
  nsCOMPtr<nsPIDOMWindow> win = aDoc->GetWindow();
  if (win && win->IsPartOfApp()) {
    return nsnull;
  }

  if (!nsContentUtils::IsRequestFullScreenAllowed()) {
    return "FullScreenDeniedNotInputDriven";
  }
  
  if (nsContentUtils::IsSitePermDeny(aDoc->NodePrincipal(), "fullscreen")) {
    return "FullScreenDeniedBlocked";
  }

  return nsnull;
}

nsresult nsGenericElement::MozRequestFullScreen()
{
  // Only grant full-screen requests if this is called from inside a trusted
  // event handler (i.e. inside an event handler for a user initiated event).
  // This stops the full-screen from being abused similar to the popups of old,
  // and it also makes it harder for bad guys' script to go full-screen and
  // spoof the browser chrome/window and phish logins etc.
  const char* error = GetFullScreenError(OwnerDoc());
  if (error) {
    nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                    "DOM", OwnerDoc(),
                                    nsContentUtils::eDOM_PROPERTIES,
                                    error);
    nsRefPtr<nsAsyncDOMEvent> e =
      new nsAsyncDOMEvent(OwnerDoc(),
                          NS_LITERAL_STRING("mozfullscreenerror"),
                          true,
                          false);
    e->PostDOMEvent();
    return NS_OK;
  }

  OwnerDoc()->AsyncRequestFullScreen(this);

  return NS_OK;
}
