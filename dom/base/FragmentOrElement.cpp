/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Base class for all element classes; this provides an implementation
 * of DOM Core's nsIDOMElement, implements nsIContent, provides
 * utility methods for subclasses, and so forth.
 */

#include "mozilla/ArrayUtils.h"
#include "mozilla/Likely.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/StaticPtr.h"

#include "mozilla/dom/FragmentOrElement.h"

#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/EventStates.h"
#include "mozilla/dom/Attr.h"
#include "nsDOMAttributeMap.h"
#include "nsIAtom.h"
#include "mozilla/dom/NodeInfo.h"
#include "mozilla/dom/Event.h"
#include "nsIDocumentInlines.h"
#include "nsIDocumentEncoder.h"
#include "nsIDOMNodeList.h"
#include "nsIContentIterator.h"
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
#include "nsIDOMEvent.h"
#include "nsDOMCID.h"
#include "nsIServiceManager.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsDOMCSSAttrDeclaration.h"
#include "nsNameSpaceManager.h"
#include "nsContentList.h"
#include "nsDOMTokenList.h"
#include "nsXBLPrototypeBinding.h"
#include "nsError.h"
#include "nsDOMString.h"
#include "nsIScriptSecurityManager.h"
#include "nsIDOMMutationEvent.h"
#include "mozilla/InternalMutationEvent.h"
#include "mozilla/MouseEvents.h"
#include "nsNodeUtils.h"
#include "nsDocument.h"
#include "nsAttrValueOrString.h"
#ifdef MOZ_XUL
#include "nsXULElement.h"
#endif /* MOZ_XUL */
#include "nsFrameSelection.h"
#ifdef DEBUG
#include "nsRange.h"
#endif

#include "nsBindingManager.h"
#include "nsXBLBinding.h"
#include "nsPIDOMWindow.h"
#include "nsPIBoxObject.h"
#include "nsSVGUtils.h"
#include "nsLayoutUtils.h"
#include "nsGkAtoms.h"
#include "nsContentUtils.h"
#include "nsTextFragment.h"
#include "nsContentCID.h"

#include "nsIDOMEventListener.h"
#include "nsIWebNavigation.h"
#include "nsIBaseWindow.h"
#include "nsIWidget.h"

#include "js/GCAPI.h"

#include "nsNodeInfoManager.h"
#include "nsICategoryManager.h"
#include "nsGenericHTMLElement.h"
#include "nsIEditor.h"
#include "nsIEditorIMESupport.h"
#include "nsContentCreatorFunctions.h"
#include "nsIControllers.h"
#include "nsView.h"
#include "nsViewManager.h"
#include "nsIScrollableFrame.h"
#include "ChildIterator.h"
#include "mozilla/css/StyleRule.h" /* For nsCSSSelectorList */
#include "nsRuleProcessorData.h"
#include "nsTextNode.h"
#include "mozilla/dom/NodeListBinding.h"
#include "mozilla/dom/UndoManager.h"

#ifdef MOZ_XUL
#include "nsIXULDocument.h"
#endif /* MOZ_XUL */

#include "nsCCUncollectableMarker.h"

#include "mozAutoDocUpdate.h"

#include "prprf.h"
#include "nsDOMMutationObserver.h"
#include "nsWrapperCacheInlines.h"
#include "nsCycleCollector.h"
#include "xpcpublic.h"
#include "nsIScriptError.h"
#include "mozilla/Telemetry.h"

#include "mozilla/CORSMode.h"

#include "mozilla/dom/ShadowRoot.h"
#include "mozilla/dom/HTMLTemplateElement.h"

#include "nsStyledElement.h"
#include "nsIContentInlines.h"

using namespace mozilla;
using namespace mozilla::dom;

int32_t nsIContent::sTabFocusModel = eTabFocus_any;
bool nsIContent::sTabFocusModelAppliesToXUL = false;
uint64_t nsMutationGuard::sGeneration = 0;

nsIContent*
nsIContent::FindFirstNonChromeOnlyAccessContent() const
{
  // This handles also nested native anonymous content.
  for (const nsIContent *content = this; content;
       content = content->GetBindingParent()) {
    if (!content->ChromeOnlyAccess()) {
      // Oops, this function signature allows casting const to
      // non-const.  (Then again, so does GetChildAt(0)->GetParent().)
      return const_cast<nsIContent*>(content);
    }
  }
  return nullptr;
}

nsIContent*
nsIContent::GetFlattenedTreeParent() const
{
  nsIContent* parent = GetParent();

  if (parent && nsContentUtils::HasDistributedChildren(parent) &&
      nsContentUtils::IsInSameAnonymousTree(parent, this)) {
    // This node is distributed to insertion points, thus we
    // need to consult the destination insertion points list to
    // figure out where this node was inserted in the flattened tree.
    // It may be the case that |parent| distributes its children
    // but the child does not match any insertion points, thus
    // the flattened tree parent is nullptr.
    nsTArray<nsIContent*>* destInsertionPoints = GetExistingDestInsertionPoints();
    parent = destInsertionPoints && !destInsertionPoints->IsEmpty() ?
      destInsertionPoints->LastElement()->GetParent() : nullptr;
  } else if (HasFlag(NODE_MAY_BE_IN_BINDING_MNGR)) {
    nsIContent* insertionParent = GetXBLInsertionParent();
    if (insertionParent) {
      parent = insertionParent;
    }
  }

  // Shadow roots never shows up in the flattened tree. Return the host
  // instead.
  if (parent && parent->IsInShadowTree()) {
    ShadowRoot* parentShadowRoot = ShadowRoot::FromNode(parent);
    if (parentShadowRoot) {
      return parentShadowRoot->GetHost();
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
  nsIDocument* doc = GetComposedDoc();
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
  // If this isn't editable, return nullptr.
  NS_ENSURE_TRUE(IsEditableInternal(), nullptr);

  nsIDocument* doc = GetComposedDoc();
  NS_ENSURE_TRUE(doc, nullptr);
  // If this is in designMode, we should return <body>
  if (doc->HasFlag(NODE_IS_EDITABLE) && !IsInShadowTree()) {
    return doc->GetBodyElement();
  }

  nsIContent* content = this;
  for (dom::Element* parent = GetParentElement();
       parent && parent->HasFlag(NODE_IS_EDITABLE);
       parent = content->GetParentElement()) {
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
nsIContent::GetBaseURI(bool aTryUseXHRDocBaseURI) const
{
  nsIDocument* doc = OwnerDoc();
  // Start with document base
  nsCOMPtr<nsIURI> base = doc->GetBaseURI(aTryUseXHRDocBaseURI);

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
    if (elem->IsSVGElement()) {
      nsIContent* bindingParent = elem->GetBindingParent();
      if (bindingParent) {
        nsXBLBinding* binding = bindingParent->GetXBLBinding();
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
  for (uint32_t i = baseAttrs.Length() - 1; i != uint32_t(-1); --i) {
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
  return aCache->PreservingWrapper() ? aCache->GetWrapperPreserveColor() : nullptr;
}

static bool
NeedsScriptTraverse(nsINode* aNode)
{
  return aNode->PreservingWrapper() && aNode->GetWrapperPreserveColor() &&
         !aNode->IsBlackAndDoesNotNeedTracing(aNode);
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
  return tmp->IsBlack();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_BEGIN(nsChildContentList)
  return tmp->IsBlackAndDoesNotNeedTracing(tmp);
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

// CanSkipThis returns false to avoid problems with incomplete unlinking.
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(nsChildContentList)
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END

NS_INTERFACE_TABLE_HEAD(nsChildContentList)
  NS_WRAPPERCACHE_INTERFACE_TABLE_ENTRY
  NS_INTERFACE_TABLE(nsChildContentList, nsINodeList, nsIDOMNodeList)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(nsChildContentList)
NS_INTERFACE_MAP_END

JSObject*
nsChildContentList::WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto)
{
  return NodeListBinding::Wrap(cx, this, aGivenProto);
}

NS_IMETHODIMP
nsChildContentList::GetLength(uint32_t* aLength)
{
  *aLength = mNode ? mNode->GetChildCount() : 0;

  return NS_OK;
}

NS_IMETHODIMP
nsChildContentList::Item(uint32_t aIndex, nsIDOMNode** aReturn)
{
  nsINode* node = Item(aIndex);
  if (!node) {
    *aReturn = nullptr;

    return NS_OK;
  }

  return CallQueryInterface(node, aReturn);
}

nsIContent*
nsChildContentList::Item(uint32_t aIndex)
{
  if (mNode) {
    return mNode->GetChildAt(aIndex);
  }

  return nullptr;
}

int32_t
nsChildContentList::IndexOf(nsIContent* aContent)
{
  if (mNode) {
    return mNode->IndexOf(aContent);
  }

  return -1;
}

//----------------------------------------------------------------------

nsIHTMLCollection*
FragmentOrElement::Children()
{
  FragmentOrElement::nsDOMSlots *slots = DOMSlots();

  if (!slots->mChildrenList) {
    slots->mChildrenList = new nsContentList(this, kNameSpaceID_Wildcard,
                                             nsGkAtoms::_asterisk, nsGkAtoms::_asterisk,
                                             false);
  }

  return slots->mChildrenList;
}


//----------------------------------------------------------------------


NS_IMPL_ISUPPORTS(nsNodeWeakReference,
                  nsIWeakReference)

nsNodeWeakReference::~nsNodeWeakReference()
{
  if (mNode) {
    NS_ASSERTION(mNode->Slots()->mWeakReference == this,
                 "Weak reference has wrong value");
    mNode->Slots()->mWeakReference = nullptr;
  }
}

NS_IMETHODIMP
nsNodeWeakReference::QueryReferent(const nsIID& aIID, void** aInstancePtr)
{
  return mNode ? mNode->QueryInterface(aIID, aInstancePtr) :
                 NS_ERROR_NULL_POINTER;
}

size_t
nsNodeWeakReference::SizeOfOnlyThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this);
}


NS_IMPL_CYCLE_COLLECTION(nsNodeSupportsWeakRefTearoff, mNode)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsNodeSupportsWeakRefTearoff)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END_AGGREGATED(mNode)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsNodeSupportsWeakRefTearoff)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsNodeSupportsWeakRefTearoff)

NS_IMETHODIMP
nsNodeSupportsWeakRefTearoff::GetWeakReference(nsIWeakReference** aInstancePtr)
{
  nsINode::nsSlots* slots = mNode->Slots();
  if (!slots->mWeakReference) {
    slots->mWeakReference = new nsNodeWeakReference(mNode);
    NS_ENSURE_TRUE(slots->mWeakReference, NS_ERROR_OUT_OF_MEMORY);
  }

  NS_ADDREF(*aInstancePtr = slots->mWeakReference);

  return NS_OK;
}

//----------------------------------------------------------------------
FragmentOrElement::nsDOMSlots::nsDOMSlots()
  : nsINode::nsSlots(),
    mDataset(nullptr),
    mUndoManager(nullptr),
    mBindingParent(nullptr)
{
}

FragmentOrElement::nsDOMSlots::~nsDOMSlots()
{
  if (mAttributeMap) {
    mAttributeMap->DropReference();
  }
}

void
FragmentOrElement::nsDOMSlots::Traverse(nsCycleCollectionTraversalCallback &cb, bool aIsXUL)
{
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mSlots->mStyle");
  cb.NoteXPCOMChild(mStyle.get());

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mSlots->mSMILOverrideStyle");
  cb.NoteXPCOMChild(mSMILOverrideStyle.get());

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mSlots->mAttributeMap");
  cb.NoteXPCOMChild(mAttributeMap.get());

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mSlots->mUndoManager");
  cb.NoteXPCOMChild(mUndoManager.get());

  if (aIsXUL) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mSlots->mControllers");
    cb.NoteXPCOMChild(mControllers);
  }

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mSlots->mXBLBinding");
  cb.NoteNativeChild(mXBLBinding, NS_CYCLE_COLLECTION_PARTICIPANT(nsXBLBinding));

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mSlots->mXBLInsertionParent");
  cb.NoteXPCOMChild(mXBLInsertionParent.get());

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mSlots->mShadowRoot");
  cb.NoteXPCOMChild(NS_ISUPPORTS_CAST(nsIContent*, mShadowRoot));

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mSlots->mContainingShadow");
  cb.NoteXPCOMChild(NS_ISUPPORTS_CAST(nsIContent*, mContainingShadow));

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mSlots->mChildrenList");
  cb.NoteXPCOMChild(NS_ISUPPORTS_CAST(nsIDOMNodeList*, mChildrenList));

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mSlots->mClassList");
  cb.NoteXPCOMChild(mClassList.get());

  if (mCustomElementData) {
    for (uint32_t i = 0; i < mCustomElementData->mCallbackQueue.Length(); i++) {
      mCustomElementData->mCallbackQueue[i]->Traverse(cb);
    }
  }
}

void
FragmentOrElement::nsDOMSlots::Unlink(bool aIsXUL)
{
  mStyle = nullptr;
  mSMILOverrideStyle = nullptr;
  if (mAttributeMap) {
    mAttributeMap->DropReference();
    mAttributeMap = nullptr;
  }
  if (aIsXUL)
    NS_IF_RELEASE(mControllers);
  mXBLBinding = nullptr;
  mXBLInsertionParent = nullptr;
  mShadowRoot = nullptr;
  mContainingShadow = nullptr;
  mChildrenList = nullptr;
  mUndoManager = nullptr;
  mCustomElementData = nullptr;
  mClassList = nullptr;
}

size_t
FragmentOrElement::nsDOMSlots::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);

  if (mAttributeMap) {
    n += mAttributeMap->SizeOfIncludingThis(aMallocSizeOf);
  }

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - Superclass members (nsINode::nsSlots)
  // - mStyle
  // - mDataSet
  // - mSMILOverrideStyle
  // - mSMILOverrideStyleRule
  // - mChildrenList
  // - mClassList

  // The following members are not measured:
  // - mBindingParent / mControllers: because they're   non-owning
  return n;
}

FragmentOrElement::FragmentOrElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : nsIContent(aNodeInfo)
{
}

FragmentOrElement::FragmentOrElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
  : nsIContent(aNodeInfo)
{
}

FragmentOrElement::~FragmentOrElement()
{
  NS_PRECONDITION(!IsInDoc(),
                  "Please remove this from the document properly");
  if (GetParent()) {
    NS_RELEASE(mParent);
  }
}

already_AddRefed<nsINodeList>
FragmentOrElement::GetChildren(uint32_t aFilter)
{
  nsRefPtr<nsSimpleContentList> list = new nsSimpleContentList(this);
  AllChildrenIterator iter(this, aFilter);
  while (nsIContent* kid = iter.GetNextChild()) {
    list->AppendElement(kid);
  }

  return list.forget();
}

static nsIContent*
FindChromeAccessOnlySubtreeOwner(nsIContent* aContent)
{
  if (aContent->ChromeOnlyAccess()) {
    bool chromeAccessOnly = false;
    while (aContent && !chromeAccessOnly) {
      chromeAccessOnly = aContent->IsRootOfChromeAccessOnlySubtree();
      aContent = aContent->GetParent();
    }
  }
  return aContent;
}

nsresult
nsIContent::PreHandleEvent(EventChainPreVisitor& aVisitor)
{
  //FIXME! Document how this event retargeting works, Bug 329124.
  aVisitor.mCanHandle = true;
  aVisitor.mMayHaveListenerManager = HasListenerManager();

  // Don't propagate mouseover and mouseout events when mouse is moving
  // inside chrome access only content.
  bool isAnonForEvents = IsRootOfChromeAccessOnlySubtree();
  if ((aVisitor.mEvent->message == NS_MOUSE_OVER ||
       aVisitor.mEvent->message == NS_MOUSE_OUT ||
       aVisitor.mEvent->message == NS_POINTER_OVER ||
       aVisitor.mEvent->message == NS_POINTER_OUT) &&
      // Check if we should stop event propagation when event has just been
      // dispatched or when we're about to propagate from
      // chrome access only subtree or if we are about to propagate out of
      // a shadow root to a shadow root host.
      ((this == aVisitor.mEvent->originalTarget &&
        !ChromeOnlyAccess()) || isAnonForEvents || GetShadowRoot())) {
     nsCOMPtr<nsIContent> relatedTarget =
       do_QueryInterface(aVisitor.mEvent->AsMouseEvent()->relatedTarget);
    if (relatedTarget &&
        relatedTarget->OwnerDoc() == OwnerDoc()) {

      // In the web components case, we may need to stop propagation of events
      // at shadow root host.
      if (GetShadowRoot()) {
        nsIContent* adjustedTarget =
          Event::GetShadowRelatedTarget(this, relatedTarget);
        if (this == adjustedTarget) {
          aVisitor.mParentTarget = nullptr;
          aVisitor.mCanHandle = false;
          return NS_OK;
        }
      }

      // If current target is anonymous for events or we know that related
      // target is descendant of an element which is anonymous for events,
      // we may want to stop event propagation.
      // If this is the original target, aVisitor.mRelatedTargetIsInAnon
      // must be updated.
      if (isAnonForEvents || aVisitor.mRelatedTargetIsInAnon ||
          (aVisitor.mEvent->originalTarget == this &&
           (aVisitor.mRelatedTargetIsInAnon =
            relatedTarget->ChromeOnlyAccess()))) {
        nsIContent* anonOwner = FindChromeAccessOnlySubtreeOwner(this);
        if (anonOwner) {
          nsIContent* anonOwnerRelated =
            FindChromeAccessOnlySubtreeOwner(relatedTarget);
          if (anonOwnerRelated) {
            // Note, anonOwnerRelated may still be inside some other
            // native anonymous subtree. The case where anonOwner is still
            // inside native anonymous subtree will be handled when event
            // propagates up in the DOM tree.
            while (anonOwner != anonOwnerRelated &&
                   anonOwnerRelated->ChromeOnlyAccess()) {
              anonOwnerRelated = FindChromeAccessOnlySubtreeOwner(anonOwnerRelated);
            }
            if (anonOwner == anonOwnerRelated) {
#ifdef DEBUG_smaug
              nsCOMPtr<nsIContent> originalTarget =
                do_QueryInterface(aVisitor.mEvent->originalTarget);
              nsAutoString ot, ct, rt;
              if (originalTarget) {
                originalTarget->NodeInfo()->NameAtom()->ToString(ot);
              }
              NodeInfo()->NameAtom()->ToString(ct);
              relatedTarget->NodeInfo()->NameAtom()->ToString(rt);
              printf("Stopping %s propagation:"
                     "\n\toriginalTarget=%s \n\tcurrentTarget=%s %s"
                     "\n\trelatedTarget=%s %s \n%s",
                     (aVisitor.mEvent->message == NS_MOUSE_OVER)
                       ? "mouseover" : "mouseout",
                     NS_ConvertUTF16toUTF8(ot).get(),
                     NS_ConvertUTF16toUTF8(ct).get(),
                     isAnonForEvents
                       ? "(is native anonymous)"
                       : (ChromeOnlyAccess()
                           ? "(is in native anonymous subtree)" : ""),
                     NS_ConvertUTF16toUTF8(rt).get(),
                     relatedTarget->ChromeOnlyAccess()
                       ? "(is in native anonymous subtree)" : "",
                     (originalTarget &&
                      relatedTarget->FindFirstNonChromeOnlyAccessContent() ==
                        originalTarget->FindFirstNonChromeOnlyAccessContent())
                       ? "" : "Wrong event propagation!?!\n");
#endif
              aVisitor.mParentTarget = nullptr;
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

  // Web components have a special event chain that need to account
  // for destination insertion points where nodes have been distributed.
  nsTArray<nsIContent*>* destPoints = GetExistingDestInsertionPoints();
  if (destPoints && !destPoints->IsEmpty()) {
    // Push destination insertion points to aVisitor.mDestInsertionPoints
    // excluding shadow insertion points.
    bool didPushNonShadowInsertionPoint = false;
    for (uint32_t i = 0; i < destPoints->Length(); i++) {
      nsIContent* point = destPoints->ElementAt(i);
      if (!ShadowRoot::IsShadowInsertionPoint(point)) {
        aVisitor.mDestInsertionPoints.AppendElement(point);
        didPushNonShadowInsertionPoint = true;
      }
    }

    // Next node in the event path is the final destination
    // (non-shadow) insertion point that was pushed.
    if (didPushNonShadowInsertionPoint) {
      parent = aVisitor.mDestInsertionPoints.LastElement();
      aVisitor.mDestInsertionPoints.SetLength(
        aVisitor.mDestInsertionPoints.Length() - 1);
    }
  }

  ShadowRoot* thisShadowRoot = ShadowRoot::FromNode(this);
  if (thisShadowRoot) {
    // The following events must always be stopped at the root node of the node tree:
    //   abort
    //   error
    //   select
    //   change
    //   load
    //   reset
    //   resize
    //   scroll
    //   selectstart
    bool stopEvent = false;
    switch (aVisitor.mEvent->message) {
      case NS_IMAGE_ABORT:
      case NS_LOAD_ERROR:
      case NS_FORM_SELECTED:
      case NS_FORM_CHANGE:
      case NS_LOAD:
      case NS_FORM_RESET:
      case NS_RESIZE_EVENT:
      case NS_SCROLL_EVENT:
        stopEvent = true;
        break;
      case NS_USER_DEFINED_EVENT:
        if (aVisitor.mDOMEvent) {
          nsAutoString eventType;
          aVisitor.mDOMEvent->GetType(eventType);
          if (eventType.EqualsLiteral("abort") ||
              eventType.EqualsLiteral("error") ||
              eventType.EqualsLiteral("select") ||
              eventType.EqualsLiteral("change") ||
              eventType.EqualsLiteral("load") ||
              eventType.EqualsLiteral("reset") ||
              eventType.EqualsLiteral("resize") ||
              eventType.EqualsLiteral("scroll") ||
              eventType.EqualsLiteral("selectstart")) {
            stopEvent = true;
          }
        }
        break;
    }

    if (stopEvent) {
      // If we do stop propagation, we still want to propagate
      // the event to chrome (nsPIDOMWindow::GetParentTarget()).
      // The load event is special in that we don't ever propagate it
      // to chrome.
      nsCOMPtr<nsPIDOMWindow> win = OwnerDoc()->GetWindow();
      EventTarget* parentTarget = win && aVisitor.mEvent->message != NS_LOAD
        ? win->GetParentTarget() : nullptr;

      aVisitor.mParentTarget = parentTarget;
      return NS_OK;
    }

    if (!aVisitor.mDestInsertionPoints.IsEmpty()) {
      parent = aVisitor.mDestInsertionPoints.LastElement();
      aVisitor.mDestInsertionPoints.SetLength(
        aVisitor.mDestInsertionPoints.Length() - 1);
    } else {
      // The pool host for the youngest shadow root is shadow DOM host,
      // for older shadow roots, it is the shadow insertion point
      // where the shadow root is projected, nullptr if none exists.
      parent = thisShadowRoot->GetPoolHost();
    }
  }

  // Event may need to be retargeted if this is the root of a native
  // anonymous content subtree or event is dispatched somewhere inside XBL.
  if (isAnonForEvents) {
#ifdef DEBUG
    // If a DOM event is explicitly dispatched using node.dispatchEvent(), then
    // all the events are allowed even in the native anonymous content..
    nsCOMPtr<nsIContent> t = do_QueryInterface(aVisitor.mEvent->originalTarget);
    NS_ASSERTION(!t || !t->ChromeOnlyAccess() ||
                 aVisitor.mEvent->mClass != eMutationEventClass ||
                 aVisitor.mDOMEvent,
                 "Mutation event dispatched in native anonymous content!?!");
#endif
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
    nsIContent* insertionParent = GetXBLInsertionParent();
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
    aVisitor.mParentTarget = GetComposedDoc();
  }
  return NS_OK;
}

bool
nsIContent::GetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                    nsAString& aResult) const
{
  if (IsElement()) {
    return AsElement()->GetAttr(aNameSpaceID, aName, aResult);
  }
  aResult.Truncate();
  return false;
}

bool
nsIContent::HasAttr(int32_t aNameSpaceID, nsIAtom* aName) const
{
  return IsElement() && AsElement()->HasAttr(aNameSpaceID, aName);
}

bool
nsIContent::AttrValueIs(int32_t aNameSpaceID,
                        nsIAtom* aName,
                        const nsAString& aValue,
                        nsCaseTreatment aCaseSensitive) const
{
  return IsElement() &&
    AsElement()->AttrValueIs(aNameSpaceID, aName, aValue, aCaseSensitive);
}

bool
nsIContent::AttrValueIs(int32_t aNameSpaceID,
                        nsIAtom* aName,
                        nsIAtom* aValue,
                        nsCaseTreatment aCaseSensitive) const
{
  return IsElement() &&
    AsElement()->AttrValueIs(aNameSpaceID, aName, aValue, aCaseSensitive);
}

bool
nsIContent::IsFocusable(int32_t* aTabIndex, bool aWithMouse)
{
  bool focusable = IsFocusableInternal(aTabIndex, aWithMouse);
  // Ensure that the return value and aTabIndex are consistent in the case
  // we're in userfocusignored context.
  if (focusable || (aTabIndex && *aTabIndex != -1)) {
    if (nsContentUtils::IsUserFocusIgnored(this)) {
      if (aTabIndex) {
        *aTabIndex = -1;
      }
      return false;
    }
    return focusable;
  }
  return false;
}

bool
nsIContent::IsFocusableInternal(int32_t* aTabIndex, bool aWithMouse)
{
  if (aTabIndex) {
    *aTabIndex = -1; // Default, not tabbable
  }
  return false;
}

NS_IMETHODIMP
FragmentOrElement::WalkContentStyleRules(nsRuleWalker* aRuleWalker)
{
  return NS_OK;
}

bool
FragmentOrElement::IsLink(nsIURI** aURI) const
{
  *aURI = nullptr;
  return false;
}

nsIContent*
FragmentOrElement::GetBindingParent() const
{
  nsDOMSlots *slots = GetExistingDOMSlots();

  if (slots) {
    return slots->mBindingParent;
  }
  return nullptr;
}

nsXBLBinding*
FragmentOrElement::GetXBLBinding() const
{
  if (HasFlag(NODE_MAY_BE_IN_BINDING_MNGR)) {
    nsDOMSlots *slots = GetExistingDOMSlots();
    if (slots) {
      return slots->mXBLBinding;
    }
  }

  return nullptr;
}

void
FragmentOrElement::SetXBLBinding(nsXBLBinding* aBinding,
                                 nsBindingManager* aOldBindingManager)
{
  nsBindingManager* bindingManager;
  if (aOldBindingManager) {
    MOZ_ASSERT(!aBinding, "aOldBindingManager should only be provided "
                          "when removing a binding.");
    bindingManager = aOldBindingManager;
  } else {
    bindingManager = OwnerDoc()->BindingManager();
  }

  // After this point, aBinding will be the most-derived binding for aContent.
  // If we already have a binding for aContent, make sure to
  // remove it from the attached stack.  Otherwise we might end up firing its
  // constructor twice (if aBinding inherits from it) or firing its constructor
  // after aContent has been deleted (if aBinding is null and the content node
  // dies before we process mAttachedStack).
  nsRefPtr<nsXBLBinding> oldBinding = GetXBLBinding();
  if (oldBinding) {
    bindingManager->RemoveFromAttachedQueue(oldBinding);
  }

  if (aBinding) {
    SetFlags(NODE_MAY_BE_IN_BINDING_MNGR);
    nsDOMSlots *slots = DOMSlots();
    slots->mXBLBinding = aBinding;
    bindingManager->AddBoundContent(this);
  } else {
    nsDOMSlots *slots = GetExistingDOMSlots();
    if (slots) {
      slots->mXBLBinding = nullptr;
    }
    bindingManager->RemoveBoundContent(this);
    if (oldBinding) {
      oldBinding->SetBoundElement(nullptr);
    }
  }
}

nsIContent*
FragmentOrElement::GetXBLInsertionParent() const
{
  if (HasFlag(NODE_MAY_BE_IN_BINDING_MNGR)) {
    nsDOMSlots *slots = GetExistingDOMSlots();
    if (slots) {
      return slots->mXBLInsertionParent;
    }
  }

  return nullptr;
}

ShadowRoot*
FragmentOrElement::GetShadowRoot() const
{
  nsDOMSlots *slots = GetExistingDOMSlots();
  if (slots) {
    return slots->mShadowRoot;
  }
  return nullptr;
}

ShadowRoot*
FragmentOrElement::GetContainingShadow() const
{
  nsDOMSlots *slots = GetExistingDOMSlots();
  if (slots) {
    return slots->mContainingShadow;
  }
  return nullptr;
}

void
FragmentOrElement::SetShadowRoot(ShadowRoot* aShadowRoot)
{
  nsDOMSlots *slots = DOMSlots();
  slots->mShadowRoot = aShadowRoot;
}

nsTArray<nsIContent*>&
FragmentOrElement::DestInsertionPoints()
{
  nsDOMSlots *slots = DOMSlots();
  return slots->mDestInsertionPoints;
}

nsTArray<nsIContent*>*
FragmentOrElement::GetExistingDestInsertionPoints() const
{
  nsDOMSlots *slots = GetExistingDOMSlots();
  if (slots) {
    return &slots->mDestInsertionPoints;
  }
  return nullptr;
}

void
FragmentOrElement::SetXBLInsertionParent(nsIContent* aContent)
{
  if (aContent) {
    nsDOMSlots *slots = DOMSlots();
    SetFlags(NODE_MAY_BE_IN_BINDING_MNGR);
    slots->mXBLInsertionParent = aContent;
  } else {
    nsDOMSlots *slots = GetExistingDOMSlots();
    if (slots) {
      slots->mXBLInsertionParent = nullptr;
    }
  }
}

CustomElementData*
FragmentOrElement::GetCustomElementData() const
{
  nsDOMSlots *slots = GetExistingDOMSlots();
  if (slots) {
    return slots->mCustomElementData;
  }
  return nullptr;
}

void
FragmentOrElement::SetCustomElementData(CustomElementData* aData)
{
  nsDOMSlots *slots = DOMSlots();
  MOZ_ASSERT(!slots->mCustomElementData, "Custom element data may not be changed once set.");
  slots->mCustomElementData = aData;
}

nsresult
FragmentOrElement::InsertChildAt(nsIContent* aKid,
                                uint32_t aIndex,
                                bool aNotify)
{
  NS_PRECONDITION(aKid, "null ptr");

  return doInsertChildAt(aKid, aIndex, aNotify, mAttrsAndChildren);
}

void
FragmentOrElement::RemoveChildAt(uint32_t aIndex, bool aNotify)
{
  nsCOMPtr<nsIContent> oldKid = mAttrsAndChildren.GetSafeChildAt(aIndex);
  NS_ASSERTION(oldKid == GetChildAt(aIndex), "Unexpected child in RemoveChildAt");

  if (oldKid) {
    doRemoveChildAt(aIndex, aNotify, oldKid, mAttrsAndChildren);
  }
}

void
FragmentOrElement::GetTextContentInternal(nsAString& aTextContent,
                                          ErrorResult& aError)
{
  if(!nsContentUtils::GetNodeTextContent(this, true, aTextContent)) {
    aError.Throw(NS_ERROR_OUT_OF_MEMORY);
  }
}

void
FragmentOrElement::SetTextContentInternal(const nsAString& aTextContent,
                                          ErrorResult& aError)
{
  aError = nsContentUtils::SetNodeTextContent(this, aTextContent, false);
}

void
FragmentOrElement::DestroyContent()
{
  nsIDocument *document = OwnerDoc();
  document->BindingManager()->RemovedFromDocument(this, document);
  document->ClearBoxObjectFor(this);

  // XXX We really should let cycle collection do this, but that currently still
  //     leaks (see https://bugzilla.mozilla.org/show_bug.cgi?id=406684).
  ReleaseWrapper(this);

  uint32_t i, count = mAttrsAndChildren.ChildCount();
  for (i = 0; i < count; ++i) {
    // The child can remove itself from the parent in BindToTree.
    mAttrsAndChildren.ChildAt(i)->DestroyContent();
  }
  ShadowRoot* shadowRoot = GetShadowRoot();
  if (shadowRoot) {
    shadowRoot->DestroyContent();
  }
}

void
FragmentOrElement::SaveSubtreeState()
{
  uint32_t i, count = mAttrsAndChildren.ChildCount();
  for (i = 0; i < count; ++i) {
    mAttrsAndChildren.ChildAt(i)->SaveSubtreeState();
  }
}

//----------------------------------------------------------------------

// Generic DOMNode implementations

void
FragmentOrElement::FireNodeInserted(nsIDocument* aDoc,
                                   nsINode* aParent,
                                   nsTArray<nsCOMPtr<nsIContent> >& aNodes)
{
  uint32_t count = aNodes.Length();
  for (uint32_t i = 0; i < count; ++i) {
    nsIContent* childContent = aNodes[i];

    if (nsContentUtils::HasMutationListeners(childContent,
          NS_EVENT_BITS_MUTATION_NODEINSERTED, aParent)) {
      InternalMutationEvent mutation(true, NS_MUTATION_NODEINSERTED);
      mutation.mRelatedNode = do_QueryInterface(aParent);

      mozAutoSubtreeModified subtree(aDoc, aParent);
      (new AsyncEventDispatcher(childContent, mutation))->RunDOMEventWhenSafe();
    }
  }
}

//----------------------------------------------------------------------

// nsISupports implementation

#define SUBTREE_UNBINDINGS_PER_RUNNABLE 500

class ContentUnbinder : public nsRunnable
{
public:
  ContentUnbinder()
  {
    mLast = this;
  }

  ~ContentUnbinder()
  {
    Run();
  }

  void UnbindSubtree(nsIContent* aNode)
  {
    if (aNode->NodeType() != nsIDOMNode::ELEMENT_NODE &&
        aNode->NodeType() != nsIDOMNode::DOCUMENT_FRAGMENT_NODE) {
      return;
    }
    FragmentOrElement* container = static_cast<FragmentOrElement*>(aNode);
    uint32_t childCount = container->mAttrsAndChildren.ChildCount();
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
          container->mFirstChild = nullptr;
        }
        UnbindSubtree(child);
        child->UnbindFromTree();
      }
    }
  }

  NS_IMETHOD Run()
  {
    nsAutoScriptBlocker scriptBlocker;
    uint32_t len = mSubtreeRoots.Length();
    if (len) {
      PRTime start = PR_Now();
      for (uint32_t i = 0; i < len; ++i) {
        UnbindSubtree(mSubtreeRoots[i]);
      }
      mSubtreeRoots.Clear();
      Telemetry::Accumulate(Telemetry::CYCLE_COLLECTOR_CONTENT_UNBIND,
                            uint32_t(PR_Now() - start) / PR_USEC_PER_MSEC);
    }
    nsCycleCollector_dispatchDeferredDeletion();
    if (this == sContentUnbinder) {
      sContentUnbinder = nullptr;
      if (mNext) {
        nsRefPtr<ContentUnbinder> next;
        next.swap(mNext);
        sContentUnbinder = next;
        next->mLast = mLast;
        mLast = nullptr;
        NS_DispatchToMainThread(next);
      }
    }
    return NS_OK;
  }

  static void UnbindAll()
  {
    nsRefPtr<ContentUnbinder> ub = sContentUnbinder;
    sContentUnbinder = nullptr;
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

ContentUnbinder* ContentUnbinder::sContentUnbinder = nullptr;

void
FragmentOrElement::ClearContentUnbinder()
{
  ContentUnbinder::UnbindAll();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(FragmentOrElement)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(FragmentOrElement)
  nsINode::Unlink(tmp);

  // The XBL binding is removed by RemoveFromBindingManagerRunnable
  // which is dispatched in UnbindFromTree.

  if (tmp->HasProperties()) {
    if (tmp->IsHTMLElement() || tmp->IsSVGElement()) {
      nsIAtom*** props = Element::HTMLSVGPropertiesToTraverseAndUnlink();
      for (uint32_t i = 0; props[i]; ++i) {
        tmp->DeleteProperty(*props[i]);
      }
    }
  }

  // Unlink child content (and unbind our subtree).
  if (tmp->UnoptimizableCCNode() || !nsCCUncollectableMarker::sGeneration) {
    uint32_t childCount = tmp->mAttrsAndChildren.ChildCount();
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
          tmp->mFirstChild = nullptr;
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

  // Clear flag here because unlinking slots will clear the
  // containing shadow root pointer.
  tmp->UnsetFlags(NODE_IS_IN_SHADOW_TREE);

  // Unlink any DOM slots of interest.
  {
    nsDOMSlots *slots = tmp->GetExistingDOMSlots();
    if (slots) {
      slots->Unlink(tmp->IsXULElement());
    }
  }

  {
    nsIDocument *doc;
    if (!tmp->GetParentNode() && (doc = tmp->OwnerDoc())) {
      doc->BindingManager()->RemovedFromDocument(tmp, doc);
    }
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(FragmentOrElement)

void
FragmentOrElement::MarkUserData(void* aObject, nsIAtom* aKey, void* aChild,
                               void* aData)
{
  uint32_t* gen = static_cast<uint32_t*>(aData);
  xpc_MarkInCCGeneration(static_cast<nsISupports*>(aChild), *gen);
}

void
FragmentOrElement::MarkNodeChildren(nsINode* aNode)
{
  JSObject* o = GetJSObjectChild(aNode);
  if (o) {
    JS::ExposeObjectToActiveJS(o);
  }

  EventListenerManager* elm = aNode->GetExistingListenerManager();
  if (elm) {
    elm->MarkForCC();
  }

  if (aNode->HasProperties()) {
    nsIDocument* ownerDoc = aNode->OwnerDoc();
    ownerDoc->PropertyTable(DOM_USER_DATA)->
      Enumerate(aNode, FragmentOrElement::MarkUserData,
                &nsCCUncollectableMarker::sGeneration);
  }
}

nsINode*
FindOptimizableSubtreeRoot(nsINode* aNode)
{
  nsINode* p;
  while ((p = aNode->GetParentNode())) {
    if (aNode->UnoptimizableCCNode()) {
      return nullptr;
    }
    aNode = p;
  }

  if (aNode->UnoptimizableCCNode()) {
    return nullptr;
  }
  return aNode;
}

StaticAutoPtr<nsTHashtable<nsPtrHashKey<nsINode>>> gCCBlackMarkedNodes;

static PLDHashOperator
VisitBlackMarkedNode(nsPtrHashKey<nsINode>* aEntry, void*)
{
  nsINode* n = aEntry->GetKey();
  n->SetCCMarkedRoot(false);
  n->SetInCCBlackTree(false);
  return PL_DHASH_NEXT;
}

static void
ClearBlackMarkedNodes()
{
  if (!gCCBlackMarkedNodes) {
    return;
  }
  gCCBlackMarkedNodes->EnumerateEntries(VisitBlackMarkedNode, nullptr);
  gCCBlackMarkedNodes = nullptr;
}

// static
void
FragmentOrElement::RemoveBlackMarkedNode(nsINode* aNode)
{
  if (!gCCBlackMarkedNodes) {
    return;
  }
  gCCBlackMarkedNodes->RemoveEntry(aNode);
}

// static
bool
FragmentOrElement::CanSkipInCC(nsINode* aNode)
{
  // Don't try to optimize anything during shutdown.
  if (nsCCUncollectableMarker::sGeneration == 0) {
    return false;
  }

  //XXXsmaug Need to figure out in which cases Shadow DOM can be optimized out
  //         from the CC graph.
  nsIDocument* currentDoc = aNode->GetUncomposedDoc();
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
    gCCBlackMarkedNodes = new nsTHashtable<nsPtrHashKey<nsINode> >(1020);
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
    currentDoc = nullptr;
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
  gCCBlackMarkedNodes->PutEntry(root);

  if (!foundBlack) {
    return false;
  }

  if (currentDoc) {
    // Special case documents. If we know the document is black,
    // we can mark the document to be in CCGeneration.
    currentDoc->
      MarkUncollectableForCCGeneration(nsCCUncollectableMarker::sGeneration);
  } else {
    for (uint32_t i = 0; i < grayNodes.Length(); ++i) {
      nsINode* node = grayNodes[i];
      node->SetInCCBlackTree(true);
      gCCBlackMarkedNodes->PutEntry(node);
    }
  }

  // Subtree is black, we can remove non-gray purple nodes from
  // purple buffer.
  for (uint32_t i = 0; i < nodesToUnpurple.Length(); ++i) {
    nsIContent* purple = nodesToUnpurple[i];
    // Can't remove currently handled purple node.
    if (purple != aNode) {
      purple->RemovePurple();
    }
  }
  return !NeedsScriptTraverse(aNode);
}

nsAutoTArray<nsINode*, 1020>* gPurpleRoots = nullptr;
nsAutoTArray<nsIContent*, 1020>* gNodesToUnbind = nullptr;

void ClearCycleCollectorCleanupData()
{
  if (gPurpleRoots) {
    uint32_t len = gPurpleRoots->Length();
    for (uint32_t i = 0; i < len; ++i) {
      nsINode* n = gPurpleRoots->ElementAt(i);
      n->SetIsPurpleRoot(false);
    }
    delete gPurpleRoots;
    gPurpleRoots = nullptr;
  }
  if (gNodesToUnbind) {
    uint32_t len = gNodesToUnbind->Length();
    for (uint32_t i = 0; i < len; ++i) {
      nsIContent* c = gNodesToUnbind->ElementAt(i);
      c->SetIsPurpleRoot(false);
      ContentUnbinder::Append(c);
    }
    delete gNodesToUnbind;
    gNodesToUnbind = nullptr;
  }
}

static bool
ShouldClearPurple(nsIContent* aContent)
{
  if (aContent && aContent->IsPurple()) {
    return true;
  }

  JSObject* o = GetJSObjectChild(aContent);
  if (o && JS::ObjectIsMarkedGray(o)) {
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
  return aNode->IsElement() && aNode->AsElement()->GetXBLBinding();
}

// CanSkip checks if aNode is black, and if it is, returns
// true. If aNode is in a black DOM tree, CanSkip may also remove other objects
// from purple buffer and unmark event listeners and user data.
// If the root of the DOM tree is a document, less optimizations are done
// since checking the blackness of the current document is usually fast and we
// don't want slow down such common cases.
bool
FragmentOrElement::CanSkip(nsINode* aNode, bool aRemovingAllowed)
{
  // Don't try to optimize anything during shutdown.
  if (nsCCUncollectableMarker::sGeneration == 0) {
    return false;
  }

  bool unoptimizable = aNode->UnoptimizableCCNode();
  nsIDocument* currentDoc = aNode->GetUncomposedDoc();
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
    currentDoc = nullptr;
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
      for (uint32_t i = 0; i < nodesToClear.Length(); ++i) {
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
  for (uint32_t i = 0; i < nodesToClear.Length(); ++i) {
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
FragmentOrElement::CanSkipThis(nsINode* aNode)
{
  if (nsCCUncollectableMarker::sGeneration == 0) {
    return false;
  }
  if (aNode->IsBlack()) {
    return true;
  }
  nsIDocument* c = aNode->GetUncomposedDoc();
  return
    ((c && nsCCUncollectableMarker::InGeneration(c->GetMarkedCCGeneration())) ||
     aNode->InCCBlackTree()) && !NeedsScriptTraverse(aNode);
}

void
FragmentOrElement::InitCCCallbacks()
{
  nsCycleCollector_setForgetSkippableCallback(ClearCycleCollectorCleanupData);
  nsCycleCollector_setBeforeUnlinkCallback(ClearBlackMarkedNodes);
}

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(FragmentOrElement)
  return FragmentOrElement::CanSkip(tmp, aRemovingAllowed);
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_BEGIN(FragmentOrElement)
  return FragmentOrElement::CanSkipInCC(tmp);
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(FragmentOrElement)
  return FragmentOrElement::CanSkipThis(tmp);
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

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INTERNAL(FragmentOrElement)
  if (MOZ_UNLIKELY(cb.WantDebugInfo())) {
    char name[512];
    uint32_t nsid = tmp->GetNameSpaceID();
    nsAtomCString localName(tmp->NodeInfo()->NameAtom());
    nsAutoCString uri;
    if (tmp->OwnerDoc()->GetDocumentURI()) {
      tmp->OwnerDoc()->GetDocumentURI()->GetSpec(uri);
    }

    nsAutoString id;
    nsIAtom* idAtom = tmp->GetID();
    if (idAtom) {
      id.AppendLiteral(" id='");
      id.Append(nsDependentAtomString(idAtom));
      id.Append('\'');
    }

    nsAutoString classes;
    const nsAttrValue* classAttrValue = tmp->GetClasses();
    if (classAttrValue) {
      classes.AppendLiteral(" class='");
      nsAutoString classString;
      classAttrValue->ToString(classString);
      classString.ReplaceChar(char16_t('\n'), char16_t(' '));
      classes.Append(classString);
      classes.Append('\'');
    }

    nsAutoCString orphan;
    if (!tmp->IsInDoc() &&
        // Ignore xbl:content, which is never in the document and hence always
        // appears to be orphaned.
        !tmp->NodeInfo()->Equals(nsGkAtoms::content, kNameSpaceID_XBL)) {
      orphan.AppendLiteral(" (orphan)");
    }

    const char* nsuri = nsid < ArrayLength(kNSURIs) ? kNSURIs[nsid] : "";
    PR_snprintf(name, sizeof(name), "FragmentOrElement%s %s%s%s%s %s",
                nsuri,
                localName.get(),
                NS_ConvertUTF16toUTF8(id).get(),
                NS_ConvertUTF16toUTF8(classes).get(),
                orphan.get(),
                uri.get());
    cb.DescribeRefCountedNode(tmp->mRefCnt.get(), name);
  }
  else {
    NS_IMPL_CYCLE_COLLECTION_DESCRIBE(FragmentOrElement, tmp->mRefCnt.get())
  }

  // Always need to traverse script objects, so do that before we check
  // if we're uncollectable.
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS

  if (!nsINode::Traverse(tmp, cb)) {
    return NS_SUCCESS_INTERRUPTED_TRAVERSE;
  }

  tmp->OwnerDoc()->BindingManager()->Traverse(tmp, cb);

  if (tmp->HasProperties()) {
    if (tmp->IsHTMLElement() || tmp->IsSVGElement()) {
      nsIAtom*** props = Element::HTMLSVGPropertiesToTraverseAndUnlink();
      for (uint32_t i = 0; props[i]; ++i) {
        nsISupports* property =
          static_cast<nsISupports*>(tmp->GetProperty(*props[i]));
        cb.NoteXPCOMChild(property);
      }
    }
  }

  // Traverse attribute names and child content.
  {
    uint32_t i;
    uint32_t attrs = tmp->mAttrsAndChildren.AttrCount();
    for (i = 0; i < attrs; i++) {
      const nsAttrName* name = tmp->mAttrsAndChildren.AttrNameAt(i);
      if (!name->IsAtom()) {
        NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb,
                                           "mAttrsAndChildren[i]->NodeInfo()");
        cb.NoteNativeChild(name->NodeInfo(),
                           NS_CYCLE_COLLECTION_PARTICIPANT(NodeInfo));
      }
    }

    uint32_t kids = tmp->mAttrsAndChildren.ChildCount();
    for (i = 0; i < kids; i++) {
      NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mAttrsAndChildren[i]");
      cb.NoteXPCOMChild(tmp->mAttrsAndChildren.GetSafeChildAt(i));
    }
  }

  // Traverse any DOM slots of interest.
  {
    nsDOMSlots *slots = tmp->GetExistingDOMSlots();
    if (slots) {
      slots->Traverse(cb, tmp->IsXULElement());
    }
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END


NS_INTERFACE_MAP_BEGIN(FragmentOrElement)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(FragmentOrElement)
  NS_INTERFACE_MAP_ENTRY(Element)
  NS_INTERFACE_MAP_ENTRY(nsIContent)
  NS_INTERFACE_MAP_ENTRY(nsINode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventTarget)
  NS_INTERFACE_MAP_ENTRY(mozilla::dom::EventTarget)
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsISupportsWeakReference,
                                 new nsNodeSupportsWeakRefTearoff(this))
  // DOM bindings depend on the identity pointer being the
  // same as nsINode (which nsIContent inherits).
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIContent)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(FragmentOrElement)
NS_IMPL_CYCLE_COLLECTING_RELEASE_WITH_LAST_RELEASE(FragmentOrElement,
                                                   nsNodeUtils::LastRelease(this))

//----------------------------------------------------------------------

nsresult
FragmentOrElement::CopyInnerTo(FragmentOrElement* aDst)
{
  uint32_t i, count = mAttrsAndChildren.AttrCount();
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

const nsTextFragment*
FragmentOrElement::GetText()
{
  return nullptr;
}

uint32_t
FragmentOrElement::TextLength() const
{
  // We can remove this assertion if it turns out to be useful to be able
  // to depend on this returning 0
  NS_NOTREACHED("called FragmentOrElement::TextLength");

  return 0;
}

nsresult
FragmentOrElement::SetText(const char16_t* aBuffer, uint32_t aLength,
                          bool aNotify)
{
  NS_ERROR("called FragmentOrElement::SetText");

  return NS_ERROR_FAILURE;
}

nsresult
FragmentOrElement::AppendText(const char16_t* aBuffer, uint32_t aLength,
                             bool aNotify)
{
  NS_ERROR("called FragmentOrElement::AppendText");

  return NS_ERROR_FAILURE;
}

bool
FragmentOrElement::TextIsOnlyWhitespace()
{
  return false;
}

bool
FragmentOrElement::HasTextForTranslation()
{
  return false;
}

void
FragmentOrElement::AppendTextTo(nsAString& aResult)
{
  // We can remove this assertion if it turns out to be useful to be able
  // to depend on this appending nothing.
  NS_NOTREACHED("called FragmentOrElement::TextLength");
}

bool
FragmentOrElement::AppendTextTo(nsAString& aResult, const mozilla::fallible_t&)
{
  // We can remove this assertion if it turns out to be useful to be able
  // to depend on this appending nothing.
  NS_NOTREACHED("called FragmentOrElement::TextLength");

  return false;
}

uint32_t
FragmentOrElement::GetChildCount() const
{
  return mAttrsAndChildren.ChildCount();
}

nsIContent *
FragmentOrElement::GetChildAt(uint32_t aIndex) const
{
  return mAttrsAndChildren.GetSafeChildAt(aIndex);
}

nsIContent * const *
FragmentOrElement::GetChildArray(uint32_t* aChildCount) const
{
  return mAttrsAndChildren.GetChildArray(aChildCount);
}

int32_t
FragmentOrElement::IndexOf(const nsINode* aPossibleChild) const
{
  return mAttrsAndChildren.IndexOfChild(aPossibleChild);
}

// Try to keep the size of StringBuilder close to a jemalloc bucket size.
#define STRING_BUFFER_UNITS 1020

namespace {

// We put StringBuilder in the anonymous namespace to prevent anything outside
// this file from accidentally being linked against it.

class StringBuilder
{
private:
  class Unit
  {
  public:
    Unit() : mAtom(nullptr), mType(eUnknown), mLength(0)
    {
      MOZ_COUNT_CTOR(StringBuilder::Unit);
    }
    ~Unit()
    {
      if (mType == eString || mType == eStringWithEncode) {
        delete mString;
      }
      MOZ_COUNT_DTOR(StringBuilder::Unit);
    }

    enum Type
    {
      eUnknown,
      eAtom,
      eString,
      eStringWithEncode,
      eLiteral,
      eTextFragment,
      eTextFragmentWithEncode,
    };

    union
    {
      nsIAtom*              mAtom;
      const char*           mLiteral;
      nsAutoString*         mString;
      const nsTextFragment* mTextFragment;
    };
    Type     mType;
    uint32_t mLength;
  };
public:
  StringBuilder() : mLast(this), mLength(0)
  {
    MOZ_COUNT_CTOR(StringBuilder);
  }

  ~StringBuilder()
  {
    MOZ_COUNT_DTOR(StringBuilder);
  }

  void Append(nsIAtom* aAtom)
  {
    Unit* u = AddUnit();
    u->mAtom = aAtom;
    u->mType = Unit::eAtom;
    uint32_t len = aAtom->GetLength();
    u->mLength = len;
    mLength += len;
  }

  template<int N>
  void Append(const char (&aLiteral)[N])
  {
    Unit* u = AddUnit();
    u->mLiteral = aLiteral;
    u->mType = Unit::eLiteral;
    uint32_t len = N - 1;
    u->mLength = len;
    mLength += len;
  }

  template<int N>
  void Append(char (&aLiteral)[N])
  {
    Unit* u = AddUnit();
    u->mLiteral = aLiteral;
    u->mType = Unit::eLiteral;
    uint32_t len = N - 1;
    u->mLength = len;
    mLength += len;
  }

  void Append(const nsAString& aString)
  {
    Unit* u = AddUnit();
    u->mString = new nsAutoString(aString);
    u->mType = Unit::eString;
    uint32_t len = aString.Length();
    u->mLength = len;
    mLength += len;
  }

  void Append(nsAutoString* aString)
  {
    Unit* u = AddUnit();
    u->mString = aString;
    u->mType = Unit::eString;
    uint32_t len = aString->Length();
    u->mLength = len;
    mLength += len;
  }

  void AppendWithAttrEncode(nsAutoString* aString, uint32_t aLen)
  {
    Unit* u = AddUnit();
    u->mString = aString;
    u->mType = Unit::eStringWithEncode;
    u->mLength = aLen;
    mLength += aLen;
  }

  void Append(const nsTextFragment* aTextFragment)
  {
    Unit* u = AddUnit();
    u->mTextFragment = aTextFragment;
    u->mType = Unit::eTextFragment;
    uint32_t len = aTextFragment->GetLength();
    u->mLength = len;
    mLength += len;
  }

  void AppendWithEncode(const nsTextFragment* aTextFragment, uint32_t aLen)
  {
    Unit* u = AddUnit();
    u->mTextFragment = aTextFragment;
    u->mType = Unit::eTextFragmentWithEncode;
    u->mLength = aLen;
    mLength += aLen;
  }

  bool ToString(nsAString& aOut)
  {
    if (!aOut.SetCapacity(mLength, fallible)) {
      return false;
    }

    for (StringBuilder* current = this; current; current = current->mNext) {
      uint32_t len = current->mUnits.Length();
      for (uint32_t i = 0; i < len; ++i) {
        Unit& u = current->mUnits[i];
        switch (u.mType) {
          case Unit::eAtom:
            aOut.Append(nsDependentAtomString(u.mAtom));
            break;
          case Unit::eString:
            aOut.Append(*(u.mString));
            break;
          case Unit::eStringWithEncode:
            EncodeAttrString(*(u.mString), aOut);
            break;
          case Unit::eLiteral:
            aOut.AppendASCII(u.mLiteral, u.mLength);
            break;
          case Unit::eTextFragment:
            u.mTextFragment->AppendTo(aOut);
            break;
          case Unit::eTextFragmentWithEncode:
            EncodeTextFragment(u.mTextFragment, aOut);
            break;
          default:
            MOZ_CRASH("Unknown unit type?");
        }
      }
    }
    return true;
  }
private:
  Unit* AddUnit()
  {
    if (mLast->mUnits.Length() == STRING_BUFFER_UNITS) {
      new StringBuilder(this);
    }
    return mLast->mUnits.AppendElement();
  }

  explicit StringBuilder(StringBuilder* aFirst)
  : mLast(nullptr), mLength(0)
  {
    MOZ_COUNT_CTOR(StringBuilder);
    aFirst->mLast->mNext = this;
    aFirst->mLast = this;
  }

  void EncodeAttrString(const nsAutoString& aValue, nsAString& aOut)
  {
    const char16_t* c = aValue.BeginReading();
    const char16_t* end = aValue.EndReading();
    while (c < end) {
      switch (*c) {
      case '"':
        aOut.AppendLiteral("&quot;");
        break;
      case '&':
        aOut.AppendLiteral("&amp;");
        break;
      case 0x00A0:
        aOut.AppendLiteral("&nbsp;");
        break;
      default:
        aOut.Append(*c);
        break;
      }
      ++c;
    }
  }

  void EncodeTextFragment(const nsTextFragment* aValue, nsAString& aOut)
  {
    uint32_t len = aValue->GetLength();
    if (aValue->Is2b()) {
      const char16_t* data = aValue->Get2b();
      for (uint32_t i = 0; i < len; ++i) {
        const char16_t c = data[i];
        switch (c) {
          case '<':
            aOut.AppendLiteral("&lt;");
            break;
          case '>':
            aOut.AppendLiteral("&gt;");
            break;
          case '&':
            aOut.AppendLiteral("&amp;");
            break;
          case 0x00A0:
            aOut.AppendLiteral("&nbsp;");
            break;
          default:
            aOut.Append(c);
            break;
        }
      }
    } else {
      const char* data = aValue->Get1b();
      for (uint32_t i = 0; i < len; ++i) {
        const unsigned char c = data[i];
        switch (c) {
          case '<':
            aOut.AppendLiteral("&lt;");
            break;
          case '>':
            aOut.AppendLiteral("&gt;");
            break;
          case '&':
            aOut.AppendLiteral("&amp;");
            break;
          case 0x00A0:
            aOut.AppendLiteral("&nbsp;");
            break;
          default:
            aOut.Append(c);
            break;
        }
      }
    }
  }

  nsAutoTArray<Unit, STRING_BUFFER_UNITS> mUnits;
  nsAutoPtr<StringBuilder>                mNext;
  StringBuilder*                          mLast;
  // mLength is used only in the first StringBuilder object in the linked list.
  uint32_t                                mLength;
};

} // anonymous namespace

static void
AppendEncodedCharacters(const nsTextFragment* aText, StringBuilder& aBuilder)
{
  uint32_t extraSpaceNeeded = 0;
  uint32_t len = aText->GetLength();
  if (aText->Is2b()) {
    const char16_t* data = aText->Get2b();
    for (uint32_t i = 0; i < len; ++i) {
      const char16_t c = data[i];
      switch (c) {
        case '<':
          extraSpaceNeeded += ArrayLength("&lt;") - 2;
          break;
        case '>':
          extraSpaceNeeded += ArrayLength("&gt;") - 2;
          break;
        case '&':
          extraSpaceNeeded += ArrayLength("&amp;") - 2;
          break;
        case 0x00A0:
          extraSpaceNeeded += ArrayLength("&nbsp;") - 2;
          break;
        default:
          break;
      }
    }
  } else {
    const char* data = aText->Get1b();
    for (uint32_t i = 0; i < len; ++i) {
      const unsigned char c = data[i];
      switch (c) {
        case '<':
          extraSpaceNeeded += ArrayLength("&lt;") - 2;
          break;
        case '>':
          extraSpaceNeeded += ArrayLength("&gt;") - 2;
          break;
        case '&':
          extraSpaceNeeded += ArrayLength("&amp;") - 2;
          break;
        case 0x00A0:
          extraSpaceNeeded += ArrayLength("&nbsp;") - 2;
          break;
        default:
          break;
      }
    }
  }

  if (extraSpaceNeeded) {
    aBuilder.AppendWithEncode(aText, len + extraSpaceNeeded);
  } else {
    aBuilder.Append(aText);
  }
}

static void
AppendEncodedAttributeValue(nsAutoString* aValue, StringBuilder& aBuilder)
{
  const char16_t* c = aValue->BeginReading();
  const char16_t* end = aValue->EndReading();

  uint32_t extraSpaceNeeded = 0;
  while (c < end) {
    switch (*c) {
      case '"':
        extraSpaceNeeded += ArrayLength("&quot;") - 2;
        break;
      case '&':
        extraSpaceNeeded += ArrayLength("&amp;") - 2;
        break;
      case 0x00A0:
        extraSpaceNeeded += ArrayLength("&nbsp;") - 2;
        break;
      default:
        break;
    }
    ++c;
  }

  if (extraSpaceNeeded) {
    aBuilder.AppendWithAttrEncode(aValue, aValue->Length() + extraSpaceNeeded);
  } else {
    aBuilder.Append(aValue);
  }
}

static void
StartElement(Element* aContent, StringBuilder& aBuilder)
{
  nsIAtom* localName = aContent->NodeInfo()->NameAtom();
  int32_t tagNS = aContent->GetNameSpaceID();

  aBuilder.Append("<");
  if (aContent->IsHTMLElement() || aContent->IsSVGElement() ||
      aContent->IsMathMLElement()) {
    aBuilder.Append(localName);
  } else {
    aBuilder.Append(aContent->NodeName());
  }

  int32_t count = aContent->GetAttrCount();
  for (int32_t i = count; i > 0;) {
    --i;
    const nsAttrName* name = aContent->GetAttrNameAt(i);
    int32_t attNs = name->NamespaceID();
    nsIAtom* attName = name->LocalName();

    // Filter out any attribute starting with [-|_]moz
    nsDependentAtomString attrNameStr(attName);
    if (StringBeginsWith(attrNameStr, NS_LITERAL_STRING("_moz")) ||
        StringBeginsWith(attrNameStr, NS_LITERAL_STRING("-moz"))) {
      continue;
    }

    nsAutoString* attValue = new nsAutoString();
    aContent->GetAttr(attNs, attName, *attValue);

    // Filter out special case of <br type="_moz*"> used by the editor.
    // Bug 16988.  Yuck.
    if (localName == nsGkAtoms::br && tagNS == kNameSpaceID_XHTML &&
        attName == nsGkAtoms::type && attNs == kNameSpaceID_None &&
        StringBeginsWith(*attValue, NS_LITERAL_STRING("_moz"))) {
      delete attValue;
      continue;
    }

    aBuilder.Append(" ");

    if (MOZ_LIKELY(attNs == kNameSpaceID_None) ||
        (attNs == kNameSpaceID_XMLNS &&
         attName == nsGkAtoms::xmlns)) {
      // Nothing else required
    } else if (attNs == kNameSpaceID_XML) {
      aBuilder.Append("xml:");
    } else if (attNs == kNameSpaceID_XMLNS) {
      aBuilder.Append("xmlns:");
    } else if (attNs == kNameSpaceID_XLink) {
      aBuilder.Append("xlink:");
    } else {
      nsIAtom* prefix = name->GetPrefix();
      if (prefix) {
        aBuilder.Append(prefix);
        aBuilder.Append(":");
      }
    }

    aBuilder.Append(attName);
    aBuilder.Append("=\"");
    AppendEncodedAttributeValue(attValue, aBuilder);
    aBuilder.Append("\"");
  }

  aBuilder.Append(">");

  /*
  // Per HTML spec we should append one \n if the first child of
  // pre/textarea/listing is a textnode and starts with a \n.
  // But because browsers haven't traditionally had that behavior,
  // we're not changing our behavior either - yet.
  if (aContent->IsHTMLElement()) {
    if (localName == nsGkAtoms::pre || localName == nsGkAtoms::textarea ||
        localName == nsGkAtoms::listing) {
      nsIContent* fc = aContent->GetFirstChild();
      if (fc &&
          (fc->NodeType() == nsIDOMNode::TEXT_NODE ||
           fc->NodeType() == nsIDOMNode::CDATA_SECTION_NODE)) {
        const nsTextFragment* text = fc->GetText();
        if (text && text->GetLength() && text->CharAt(0) == char16_t('\n')) {
          aBuilder.Append("\n");
        }
      }
    }
  }*/
}

static inline bool
ShouldEscape(nsIContent* aParent)
{
  if (!aParent || !aParent->IsHTMLElement()) {
    return true;
  }

  static const nsIAtom* nonEscapingElements[] = {
    nsGkAtoms::style, nsGkAtoms::script, nsGkAtoms::xmp,
    nsGkAtoms::iframe, nsGkAtoms::noembed, nsGkAtoms::noframes,
    nsGkAtoms::plaintext,
    // Per the current spec noscript should be escaped in case
    // scripts are disabled or if document doesn't have
    // browsing context. However the latter seems to be a spec bug
    // and Gecko hasn't traditionally done the former.
    nsGkAtoms::noscript
  };
  static mozilla::BloomFilter<12, nsIAtom> sFilter;
  static bool sInitialized = false;
  if (!sInitialized) {
    sInitialized = true;
    for (uint32_t i = 0; i < ArrayLength(nonEscapingElements); ++i) {
      sFilter.add(nonEscapingElements[i]);
    }
  }

  nsIAtom* tag = aParent->NodeInfo()->NameAtom();
  if (sFilter.mightContain(tag)) {
    for (uint32_t i = 0; i < ArrayLength(nonEscapingElements); ++i) {
      if (tag == nonEscapingElements[i]) {
        return false;
      }
    }
  }
  return true;
}

static inline bool
IsVoidTag(nsIAtom* aTag)
{
  static const nsIAtom* voidElements[] = {
    nsGkAtoms::area, nsGkAtoms::base, nsGkAtoms::basefont,
    nsGkAtoms::bgsound, nsGkAtoms::br, nsGkAtoms::col,
    nsGkAtoms::embed, nsGkAtoms::frame,
    nsGkAtoms::hr, nsGkAtoms::img, nsGkAtoms::input,
    nsGkAtoms::keygen, nsGkAtoms::link, nsGkAtoms::meta,
    nsGkAtoms::param, nsGkAtoms::source, nsGkAtoms::track,
    nsGkAtoms::wbr
  };

  static mozilla::BloomFilter<12, nsIAtom> sFilter;
  static bool sInitialized = false;
  if (!sInitialized) {
    sInitialized = true;
    for (uint32_t i = 0; i < ArrayLength(voidElements); ++i) {
      sFilter.add(voidElements[i]);
    }
  }

  if (sFilter.mightContain(aTag)) {
    for (uint32_t i = 0; i < ArrayLength(voidElements); ++i) {
      if (aTag == voidElements[i]) {
        return true;
      }
    }
  }
  return false;
}

static inline bool
IsVoidTag(Element* aElement)
{
  if (!aElement->IsHTMLElement()) {
    return false;
  }
  return IsVoidTag(aElement->NodeInfo()->NameAtom());
}

/* static */
bool
FragmentOrElement::IsHTMLVoid(nsIAtom* aLocalName)
{
  return aLocalName && IsVoidTag(aLocalName);
}

static bool
Serialize(FragmentOrElement* aRoot, bool aDescendentsOnly, nsAString& aOut)
{
  nsINode* current = aDescendentsOnly ?
    nsNodeUtils::GetFirstChildOfTemplateOrNode(aRoot) : aRoot;

  if (!current) {
    return true;
  }

  StringBuilder builder;
  nsIContent* next;
  while (true) {
    bool isVoid = false;
    switch (current->NodeType()) {
      case nsIDOMNode::ELEMENT_NODE: {
        Element* elem = current->AsElement();
        StartElement(elem, builder);
        isVoid = IsVoidTag(elem);
        if (!isVoid &&
            (next = nsNodeUtils::GetFirstChildOfTemplateOrNode(current))) {
          current = next;
          continue;
        }
        break;
      }

      case nsIDOMNode::TEXT_NODE:
      case nsIDOMNode::CDATA_SECTION_NODE: {
        const nsTextFragment* text = static_cast<nsIContent*>(current)->GetText();
        nsIContent* parent = current->GetParent();
        if (ShouldEscape(parent)) {
          AppendEncodedCharacters(text, builder);
        } else {
          builder.Append(text);
        }
        break;
      }

      case nsIDOMNode::COMMENT_NODE: {
        builder.Append("<!--");
        builder.Append(static_cast<nsIContent*>(current)->GetText());
        builder.Append("-->");
        break;
      }

      case nsIDOMNode::DOCUMENT_TYPE_NODE: {
        builder.Append("<!DOCTYPE ");
        builder.Append(current->NodeName());
        builder.Append(">");
        break;
      }

      case nsIDOMNode::PROCESSING_INSTRUCTION_NODE: {
        builder.Append("<?");
        builder.Append(current->NodeName());
        builder.Append(" ");
        builder.Append(static_cast<nsIContent*>(current)->GetText());
        builder.Append(">");
        break;
      }
    }

    while (true) {
      if (!isVoid && current->NodeType() == nsIDOMNode::ELEMENT_NODE) {
        builder.Append("</");
        nsIContent* elem = static_cast<nsIContent*>(current);
        if (elem->IsHTMLElement() || elem->IsSVGElement() ||
            elem->IsMathMLElement()) {
          builder.Append(elem->NodeInfo()->NameAtom());
        } else {
          builder.Append(current->NodeName());
        }
        builder.Append(">");
      }
      isVoid = false;

      if (current == aRoot) {
        return builder.ToString(aOut);
      }

      if ((next = current->GetNextSibling())) {
        current = next;
        break;
      }

      current = current->GetParentNode();

      // Handle template element. If the parent is a template's content,
      // then adjust the parent to be the template element.
      if (current != aRoot &&
          current->NodeType() == nsIDOMNode::DOCUMENT_FRAGMENT_NODE) {
        DocumentFragment* frag = static_cast<DocumentFragment*>(current);
        nsIContent* fragHost = frag->GetHost();
        if (fragHost && nsNodeUtils::IsTemplateElement(fragHost)) {
          current = fragHost;
        }
      }

      if (aDescendentsOnly && current == aRoot) {
        return builder.ToString(aOut);
      }
    }
  }
}

void
FragmentOrElement::GetMarkup(bool aIncludeSelf, nsAString& aMarkup)
{
  aMarkup.Truncate();

  nsIDocument* doc = OwnerDoc();
  if (IsInHTMLDocument()) {
    Serialize(this, !aIncludeSelf, aMarkup);
    return;
  }

  nsAutoString contentType;
  doc->GetContentType(contentType);
  bool tryToCacheEncoder = !aIncludeSelf;

  nsCOMPtr<nsIDocumentEncoder> docEncoder = doc->GetCachedEncoder();
  if (!docEncoder) {
    docEncoder =
      do_CreateInstance(PromiseFlatCString(
        nsDependentCString(NS_DOC_ENCODER_CONTRACTID_BASE) +
        NS_ConvertUTF16toUTF8(contentType)
      ).get());
  }
  if (!docEncoder) {
    // This could be some type for which we create a synthetic document.  Try
    // again as XML
    contentType.AssignLiteral("application/xml");
    docEncoder = do_CreateInstance(NS_DOC_ENCODER_CONTRACTID_BASE "application/xml");
    // Don't try to cache the encoder since it would point to a different
    // contentType once it has been reinitialized.
    tryToCacheEncoder = false;
  }

  NS_ENSURE_TRUE_VOID(docEncoder);

  uint32_t flags = nsIDocumentEncoder::OutputEncodeBasicEntities |
                   // Output DOM-standard newlines
                   nsIDocumentEncoder::OutputLFLineBreak |
                   // Don't do linebreaking that's not present in
                   // the source
                   nsIDocumentEncoder::OutputRaw |
                   // Only check for mozdirty when necessary (bug 599983)
                   nsIDocumentEncoder::OutputIgnoreMozDirty;

  if (IsEditable()) {
    nsCOMPtr<Element> elem = do_QueryInterface(this);
    nsIEditor* editor = elem ? elem->GetEditorInternal() : nullptr;
    if (editor && editor->OutputsMozDirty()) {
      flags &= ~nsIDocumentEncoder::OutputIgnoreMozDirty;
    }
  }

  DebugOnly<nsresult> rv = docEncoder->NativeInit(doc, contentType, flags);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  if (aIncludeSelf) {
    docEncoder->SetNativeNode(this);
  } else {
    docEncoder->SetNativeContainerNode(this);
  }
  rv = docEncoder->EncodeToString(aMarkup);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  if (tryToCacheEncoder) {
    doc->SetCachedEncoder(docEncoder.forget());
  }
}

static bool
ContainsMarkup(const nsAString& aStr)
{
  // Note: we can't use FindCharInSet because null is one of the characters we
  // want to search for.
  const char16_t* start = aStr.BeginReading();
  const char16_t* end = aStr.EndReading();

  while (start != end) {
    char16_t c = *start;
    if (c == char16_t('<') ||
        c == char16_t('&') ||
        c == char16_t('\r') ||
        c == char16_t('\0')) {
      return true;
    }
    ++start;
  }

  return false;
}

void
FragmentOrElement::SetInnerHTMLInternal(const nsAString& aInnerHTML, ErrorResult& aError)
{
  FragmentOrElement* target = this;
  // Handle template case.
  if (nsNodeUtils::IsTemplateElement(target)) {
    DocumentFragment* frag =
      static_cast<HTMLTemplateElement*>(target)->Content();
    MOZ_ASSERT(frag);
    target = frag;
  }

  // Fast-path for strings with no markup. Limit this to short strings, to
  // avoid ContainsMarkup taking too long. The choice for 100 is based on
  // gut feeling.
  //
  // Don't do this for elements with a weird parser insertion mode, for
  // instance setting innerHTML = "" on a <html> element should add the
  // optional <head> and <body> elements.
  if (!target->HasWeirdParserInsertionMode() &&
      aInnerHTML.Length() < 100 && !ContainsMarkup(aInnerHTML)) {
    aError = nsContentUtils::SetNodeTextContent(target, aInnerHTML, false);
    return;
  }

  nsIDocument* doc = target->OwnerDoc();

  // Batch possible DOMSubtreeModified events.
  mozAutoSubtreeModified subtree(doc, nullptr);

  target->FireNodeRemovedForChildren();

  // Needed when innerHTML is used in combination with contenteditable
  mozAutoDocUpdate updateBatch(doc, UPDATE_CONTENT_MODEL, true);

  // Remove childnodes.
  uint32_t childCount = target->GetChildCount();
  nsAutoMutationBatch mb(target, true, false);
  for (uint32_t i = 0; i < childCount; ++i) {
    target->RemoveChildAt(0, true);
  }
  mb.RemovalDone();

  nsAutoScriptLoaderDisabler sld(doc);

  nsIAtom* contextLocalName = NodeInfo()->NameAtom();
  int32_t contextNameSpaceID = GetNameSpaceID();

  ShadowRoot* shadowRoot = ShadowRoot::FromNode(this);
  if (shadowRoot) {
    // Fix up the context to be the host of the ShadowRoot.
    contextLocalName = shadowRoot->GetHost()->NodeInfo()->NameAtom();
    contextNameSpaceID = shadowRoot->GetHost()->GetNameSpaceID();
  }

  if (doc->IsHTMLDocument()) {
    int32_t oldChildCount = target->GetChildCount();
    aError = nsContentUtils::ParseFragmentHTML(aInnerHTML,
                                               target,
                                               contextLocalName,
                                               contextNameSpaceID,
                                               doc->GetCompatibilityMode() ==
                                                 eCompatibility_NavQuirks,
                                               true);
    mb.NodesAdded();
    // HTML5 parser has notified, but not fired mutation events.
    nsContentUtils::FireMutationEventsForDirectParsing(doc, target,
                                                       oldChildCount);
  } else {
    nsRefPtr<DocumentFragment> df =
      nsContentUtils::CreateContextualFragment(target, aInnerHTML, true, aError);
    if (!aError.Failed()) {
      // Suppress assertion about node removal mutation events that can't have
      // listeners anyway, because no one has had the chance to register mutation
      // listeners on the fragment that comes from the parser.
      nsAutoScriptBlockerSuppressNodeRemoved scriptBlocker;

      static_cast<nsINode*>(target)->AppendChild(*df, aError);
      mb.NodesAdded();
    }
  }
}

nsINode::nsSlots*
FragmentOrElement::CreateSlots()
{
  return new nsDOMSlots();
}

void
FragmentOrElement::FireNodeRemovedForChildren()
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
       child && child->GetParentNode() == this;
       child = child->GetNextSibling()) {
    nsContentUtils::MaybeFireNodeRemoved(child, this, doc);
  }
}

size_t
FragmentOrElement::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = 0;
  n += nsIContent::SizeOfExcludingThis(aMallocSizeOf);
  n += mAttrsAndChildren.SizeOfExcludingThis(aMallocSizeOf);

  nsDOMSlots* slots = GetExistingDOMSlots();
  if (slots) {
    n += slots->SizeOfIncludingThis(aMallocSizeOf);
  }

  return n;
}

void
FragmentOrElement::SetIsElementInStyleScopeFlagOnSubtree(bool aInStyleScope)
{
  if (aInStyleScope && IsElementInStyleScope()) {
    return;
  }

  if (IsElement()) {
    SetIsElementInStyleScope(aInStyleScope);
    SetIsElementInStyleScopeFlagOnShadowTree(aInStyleScope);
  }

  nsIContent* n = GetNextNode(this);
  while (n) {
    if (n->IsElementInStyleScope()) {
      n = n->GetNextNonChildNode(this);
    } else {
      if (n->IsElement()) {
        n->SetIsElementInStyleScope(aInStyleScope);
        n->AsElement()->SetIsElementInStyleScopeFlagOnShadowTree(aInStyleScope);
      }
      n = n->GetNextNode(this);
    }
  }
}

void
FragmentOrElement::SetIsElementInStyleScopeFlagOnShadowTree(bool aInStyleScope)
{
  NS_ASSERTION(IsElement(), "calling SetIsElementInStyleScopeFlagOnShadowTree "
                            "on a non-Element is useless");
  ShadowRoot* shadowRoot = GetShadowRoot();
  while (shadowRoot) {
    shadowRoot->SetIsElementInStyleScopeFlagOnSubtree(aInStyleScope);
    shadowRoot = shadowRoot->GetOlderShadowRoot();
  }
}
