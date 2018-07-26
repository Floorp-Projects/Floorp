/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Base class for all element classes and DocumentFragment.
 */

#include "mozilla/ArrayUtils.h"
#include "mozilla/Likely.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/StaticPtr.h"

#include "mozilla/dom/FragmentOrElement.h"

#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/DeclarationBlock.h"
#include "mozilla/EffectSet.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/EventStates.h"
#include "mozilla/HTMLEditor.h"
#include "mozilla/RestyleManager.h"
#include "mozilla/TextEditor.h"
#include "mozilla/TouchEvents.h"
#include "mozilla/URLExtraData.h"
#include "mozilla/dom/Attr.h"
#include "nsDOMAttributeMap.h"
#include "nsAtom.h"
#include "mozilla/dom/NodeInfo.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/TouchEvent.h"
#include "nsIDocumentInlines.h"
#include "nsIDocumentEncoder.h"
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
#include "nsDOMCID.h"
#include "nsIServiceManager.h"
#include "nsDOMCSSAttrDeclaration.h"
#include "nsNameSpaceManager.h"
#include "nsContentList.h"
#include "nsDOMTokenList.h"
#include "nsXBLPrototypeBinding.h"
#include "nsError.h"
#include "nsDOMString.h"
#include "nsIScriptSecurityManager.h"
#include "mozilla/InternalMutationEvent.h"
#include "mozilla/MouseEvents.h"
#include "nsNodeUtils.h"
#include "nsDocument.h"
#include "nsAttrValueOrString.h"
#include "nsQueryObject.h"
#ifdef MOZ_XUL
#include "nsXULElement.h"
#endif /* MOZ_XUL */
#include "nsFrameSelection.h"
#ifdef DEBUG
#include "nsRange.h"
#endif

#include "nsBindingManager.h"
#include "nsFrameLoader.h"
#include "nsXBLBinding.h"
#include "nsPIDOMWindow.h"
#include "nsPIBoxObject.h"
#include "nsSVGUtils.h"
#include "nsLayoutUtils.h"
#include "nsGkAtoms.h"
#include "nsContentUtils.h"
#include "nsTextFragment.h"
#include "nsContentCID.h"
#include "nsWindowSizes.h"

#include "nsIDOMEventListener.h"
#include "nsIWebNavigation.h"
#include "nsIBaseWindow.h"
#include "nsIWidget.h"

#include "nsNodeInfoManager.h"
#include "nsICategoryManager.h"
#include "nsGenericHTMLElement.h"
#include "nsContentCreatorFunctions.h"
#include "nsIControllers.h"
#include "nsView.h"
#include "nsViewManager.h"
#include "nsIScrollableFrame.h"
#include "ChildIterator.h"
#include "nsTextNode.h"
#include "mozilla/dom/NodeListBinding.h"

#include "nsCCUncollectableMarker.h"

#include "mozAutoDocUpdate.h"

#include "mozilla/Sprintf.h"
#include "nsDOMMutationObserver.h"
#include "nsWrapperCacheInlines.h"
#include "nsCycleCollector.h"
#include "xpcpublic.h"
#include "nsIScriptError.h"
#include "mozilla/Telemetry.h"

#include "mozilla/CORSMode.h"

#include "mozilla/dom/ShadowRoot.h"
#include "mozilla/dom/HTMLSlotElement.h"
#include "mozilla/dom/HTMLTemplateElement.h"
#include "mozilla/dom/SVGUseElement.h"

#include "nsStyledElement.h"
#include "nsIContentInlines.h"
#include "nsChildContentList.h"
#include "mozilla/BloomFilter.h"

#include "NodeUbiReporting.h"

using namespace mozilla;
using namespace mozilla::dom;

int32_t nsIContent::sTabFocusModel = eTabFocus_any;
bool nsIContent::sTabFocusModelAppliesToXUL = false;
uint64_t nsMutationGuard::sGeneration = 0;

NS_IMPL_CYCLE_COLLECTION_CLASS(nsIContent)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsIContent)
  MOZ_ASSERT_UNREACHABLE("Our subclasses don't call us");
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsIContent)
  MOZ_ASSERT_UNREACHABLE("Our subclasses don't call us");
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN(nsIContent)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  // Don't bother to QI to cycle collection, because our CC impl is
  // not doing anything anyway.
  NS_INTERFACE_MAP_ENTRY(nsIContent)
  NS_INTERFACE_MAP_ENTRY(nsINode)
  NS_INTERFACE_MAP_ENTRY(mozilla::dom::EventTarget)
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsISupportsWeakReference,
                                 new nsNodeSupportsWeakRefTearoff(this))
  // DOM bindings depend on the identity pointer being the
  // same as nsINode (which nsIContent inherits).
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_MAIN_THREAD_ONLY_CYCLE_COLLECTING_ADDREF(nsIContent)
NS_IMPL_MAIN_THREAD_ONLY_CYCLE_COLLECTING_RELEASE_WITH_LAST_RELEASE(nsIContent,
                                                                    nsNodeUtils::LastRelease(this))

nsIContent*
nsIContent::FindFirstNonChromeOnlyAccessContent() const
{
  // This handles also nested native anonymous content.
  for (const nsIContent *content = this; content;
       content = content->GetBindingParent()) {
    if (!content->ChromeOnlyAccess()) {
      // Oops, this function signature allows casting const to
      // non-const.  (Then again, so does GetChildAt_Deprecated(0)->GetParent().)
      return const_cast<nsIContent*>(content);
    }
  }
  return nullptr;
}

// https://dom.spec.whatwg.org/#dom-slotable-assignedslot
HTMLSlotElement*
nsIContent::GetAssignedSlotByMode() const
{
  /**
   * Get slotable's assigned slot for the result of
   * find a slot with open flag UNSET [1].
   *
   * [1] https://dom.spec.whatwg.org/#assign-a-slot
   */
  HTMLSlotElement* slot = GetAssignedSlot();
  if (!slot) {
    return nullptr;
  }

  MOZ_ASSERT(GetParent());
  MOZ_ASSERT(GetParent()->GetShadowRoot());

  /**
   * Additional check for open flag SET:
   *   If slotable’s parent’s shadow root's mode is not "open",
   *   then return null.
   */
  if (GetParent()->GetShadowRoot()->IsClosed()) {
    return nullptr;
  }

  return slot;
}

nsIContent::IMEState
nsIContent::GetDesiredIMEState()
{
  if (!IsEditable()) {
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
  nsPresContext* pc = doc->GetPresContext();
  if (!pc) {
    return IMEState(IMEState::DISABLED);
  }
  HTMLEditor* htmlEditor = nsContentUtils::GetHTMLEditor(pc);
  if (!htmlEditor) {
    return IMEState(IMEState::DISABLED);
  }
  IMEState state;
  htmlEditor->GetPreferredIMEState(&state);
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
  if (!IsEditable()) {
    return nullptr;
  }

  nsIDocument* doc = GetComposedDoc();
  if (!doc) {
    return nullptr;
  }

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

  RefPtr<nsAtom> name;
  if (!aNamespacePrefix.IsEmpty()) {
    name = NS_Atomize(aNamespacePrefix);
    NS_ENSURE_TRUE(name, NS_ERROR_OUT_OF_MEMORY);
  }
  else {
    name = nsGkAtoms::xmlns;
  }
  // Trace up the content parent chain looking for the namespace
  // declaration that declares aNamespacePrefix.
  const nsIContent* content = this;
  do {
    if (content->IsElement() &&
        content->AsElement()->GetAttr(kNameSpaceID_XMLNS, name, aNamespaceURI))
      return NS_OK;
  } while ((content = content->GetParent()));
  return NS_ERROR_FAILURE;
}

nsAtom*
nsIContent::GetLang() const
{
  for (const auto* content = this; content; content = content->GetParent()) {
    if (!content->IsElement()) {
      continue;
    }

    auto* element = content->AsElement();
    if (!element->GetAttrCount()) {
      continue;
    }

    // xml:lang has precedence over lang on HTML elements (see
    // XHTML1 section C.7).
    const nsAttrValue* attr =
      element->GetParsedAttr(nsGkAtoms::lang, kNameSpaceID_XML);
    if (!attr && element->SupportsLangAttr()) {
      attr = element->GetParsedAttr(nsGkAtoms::lang);
    }
    if (attr) {
      MOZ_ASSERT(attr->Type() == nsAttrValue::eAtom);
      MOZ_ASSERT(attr->GetAtomValue());
      return attr->GetAtomValue();
    }
  }

  return nullptr;
}

already_AddRefed<nsIURI>
nsIContent::GetBaseURI(bool aTryUseXHRDocBaseURI) const
{
  if (SVGUseElement* use = GetContainingSVGUseShadowHost()) {
    // XXX Ignore xml:base as we are removing it.
    if (URLExtraData* data = use->GetContentURLData()) {
      return do_AddRef(data->BaseURI());
    }
  }

  nsIDocument* doc = OwnerDoc();
  // Start with document base
  nsCOMPtr<nsIURI> base = doc->GetBaseURI(aTryUseXHRDocBaseURI);

  // Collect array of xml:base attribute values up the parent chain. This
  // is slightly slower for the case when there are xml:base attributes, but
  // faster for the far more common case of there not being any such
  // attributes.
  // Also check for SVG elements which require special handling
  AutoTArray<nsString, 5> baseAttrs;
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

    // Otherwise check for xml:base attribute
    if (elem->IsElement()) {
      elem->AsElement()->GetAttr(kNameSpaceID_XML, nsGkAtoms::base, attr);
      if (!attr.IsEmpty()) {
        baseAttrs.AppendElement(attr);
      }
    }
    elem = elem->GetParent();
  } while(elem);

  if (!baseAttrs.IsEmpty()) {
    doc->WarnOnceAbout(nsIDocument::eXMLBaseAttribute);
    // Now resolve against all xml:base attrs
    for (uint32_t i = baseAttrs.Length() - 1; i != uint32_t(-1); --i) {
      nsCOMPtr<nsIURI> newBase;
      nsresult rv = NS_NewURI(getter_AddRefs(newBase), baseAttrs[i],
                              doc->GetDocumentCharacterSet(), base);
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
  }

  return base.forget();
}

nsIURI*
nsIContent::GetBaseURIForStyleAttr() const
{
  if (SVGUseElement* use = GetContainingSVGUseShadowHost()) {
    if (URLExtraData* data = use->GetContentURLData()) {
      return data->BaseURI();
    }
  }
  // This also ignores the case that SVG inside XBL binding.
  // But it is probably fine.
  return OwnerDoc()->GetDocBaseURI();
}

already_AddRefed<URLExtraData>
nsIContent::GetURLDataForStyleAttr(nsIPrincipal* aSubjectPrincipal) const
{
  if (SVGUseElement* use = GetContainingSVGUseShadowHost()) {
    if (URLExtraData* data = use->GetContentURLData()) {
      return do_AddRef(data);
    }
  }
  if (aSubjectPrincipal && aSubjectPrincipal != NodePrincipal()) {
    // TODO: Cache this?
    return MakeAndAddRef<URLExtraData>(OwnerDoc()->GetDocBaseURI(),
                                       OwnerDoc()->GetDocumentURI(),
                                       aSubjectPrincipal);
  }
  // This also ignores the case that SVG inside XBL binding.
  // But it is probably fine.
  return do_AddRef(OwnerDoc()->DefaultStyleAttrURLData());
}

void
nsIContent::ConstructUbiNode(void* storage)
{
  JS::ubi::Concrete<nsIContent>::construct(storage, this);
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
         !aNode->HasKnownLiveWrapperAndDoesNotNeedTracing(aNode);
}

//----------------------------------------------------------------------

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsAttrChildContentList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsAttrChildContentList)

// If nsAttrChildContentList is changed so that any additional fields are
// traversed by the cycle collector, then CAN_SKIP must be updated to
// check that the additional fields are null.
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(nsAttrChildContentList)

// nsAttrChildContentList only ever has a single child, its wrapper, so if
// the wrapper is known-live, the list can't be part of a garbage cycle.
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(nsAttrChildContentList)
  return tmp->HasKnownLiveWrapper();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_BEGIN(nsAttrChildContentList)
  return tmp->HasKnownLiveWrapperAndDoesNotNeedTracing(tmp);
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

// CanSkipThis returns false to avoid problems with incomplete unlinking.
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(nsAttrChildContentList)
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END

NS_INTERFACE_TABLE_HEAD(nsAttrChildContentList)
  NS_WRAPPERCACHE_INTERFACE_TABLE_ENTRY
  NS_INTERFACE_TABLE(nsAttrChildContentList, nsINodeList)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(nsAttrChildContentList)
NS_INTERFACE_MAP_END

JSObject*
nsAttrChildContentList::WrapObject(JSContext *cx,
                                   JS::Handle<JSObject*> aGivenProto)
{
  return NodeList_Binding::Wrap(cx, this, aGivenProto);
}

uint32_t
nsAttrChildContentList::Length()
{
  return mNode ? mNode->GetChildCount() : 0;
}

nsIContent*
nsAttrChildContentList::Item(uint32_t aIndex)
{
  if (mNode) {
    return mNode->GetChildAt_Deprecated(aIndex);
  }

  return nullptr;
}

int32_t
nsAttrChildContentList::IndexOf(nsIContent* aContent)
{
  if (mNode) {
    return mNode->ComputeIndexOf(aContent);
  }

  return -1;
}

//----------------------------------------------------------------------
uint32_t
nsParentNodeChildContentList::Length()
{
  if (!mIsCacheValid && !ValidateCache()) {
    return 0;
  }

  MOZ_ASSERT(mIsCacheValid);

  return mCachedChildArray.Length();
}

nsIContent*
nsParentNodeChildContentList::Item(uint32_t aIndex)
{
  if (!mIsCacheValid && !ValidateCache()) {
    return nullptr;
  }

  MOZ_ASSERT(mIsCacheValid);

  return mCachedChildArray.SafeElementAt(aIndex, nullptr);
}

int32_t
nsParentNodeChildContentList::IndexOf(nsIContent* aContent)
{
  if (!mIsCacheValid && !ValidateCache()) {
    return -1;
  }

  MOZ_ASSERT(mIsCacheValid);

  return mCachedChildArray.IndexOf(aContent);
}

bool
nsParentNodeChildContentList::ValidateCache()
{
  MOZ_ASSERT(!mIsCacheValid);
  MOZ_ASSERT(mCachedChildArray.IsEmpty());

  nsINode* parent = GetParentObject();
  if (!parent) {
    return false;
  }

  for (nsIContent* node = parent->GetFirstChild(); node;
       node = node->GetNextSibling()) {
    mCachedChildArray.AppendElement(node);
  }
  mIsCacheValid = true;

  return true;
}

//----------------------------------------------------------------------

nsIHTMLCollection*
FragmentOrElement::Children()
{
  nsDOMSlots* slots = DOMSlots();

  if (!slots->mChildrenList) {
    slots->mChildrenList = new nsContentList(this, kNameSpaceID_Wildcard,
                                             nsGkAtoms::_asterisk, nsGkAtoms::_asterisk,
                                             false);
  }

  return slots->mChildrenList;
}


//----------------------------------------------------------------------

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
  }

  NS_ADDREF(*aInstancePtr = slots->mWeakReference);

  return NS_OK;
}

//----------------------------------------------------------------------

static const size_t MaxDOMSlotSizeAllowed =
#ifdef HAVE_64BIT_BUILD
128;
#else
64;
#endif

static_assert(sizeof(nsINode::nsSlots) <= MaxDOMSlotSizeAllowed,
              "DOM slots cannot be grown without consideration");
static_assert(sizeof(FragmentOrElement::nsDOMSlots) <= MaxDOMSlotSizeAllowed,
              "DOM slots cannot be grown without consideration");

void
nsIContent::nsExtendedContentSlots::UnlinkExtendedSlots()
{
  mBindingParent = nullptr;
  mXBLInsertionPoint = nullptr;
  mContainingShadow = nullptr;
  mAssignedSlot = nullptr;
}

void
nsIContent::nsExtendedContentSlots::TraverseExtendedSlots(nsCycleCollectionTraversalCallback& aCb)
{
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(aCb, "mExtendedSlots->mBindingParent");
  aCb.NoteXPCOMChild(NS_ISUPPORTS_CAST(nsIContent*, mBindingParent));

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(aCb, "mExtendedSlots->mContainingShadow");
  aCb.NoteXPCOMChild(NS_ISUPPORTS_CAST(nsIContent*, mContainingShadow));

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(aCb, "mExtendedSlots->mAssignedSlot");
  aCb.NoteXPCOMChild(NS_ISUPPORTS_CAST(nsIContent*, mAssignedSlot.get()));

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(aCb, "mExtendedSlots->mXBLInsertionPoint");
  aCb.NoteXPCOMChild(mXBLInsertionPoint.get());
}

nsIContent::nsExtendedContentSlots::nsExtendedContentSlots()
{
}

nsIContent::nsExtendedContentSlots::~nsExtendedContentSlots() = default;

FragmentOrElement::nsDOMSlots::nsDOMSlots()
  : nsIContent::nsContentSlots(),
    mDataset(nullptr)
{
  MOZ_COUNT_CTOR(nsDOMSlots);
}

FragmentOrElement::nsDOMSlots::~nsDOMSlots()
{
  MOZ_COUNT_DTOR(nsDOMSlots);

  if (mAttributeMap) {
    mAttributeMap->DropReference();
  }
}

void
FragmentOrElement::nsDOMSlots::Traverse(nsCycleCollectionTraversalCallback& aCb)
{
  nsIContent::nsContentSlots::Traverse(aCb);

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(aCb, "mSlots->mStyle");
  aCb.NoteXPCOMChild(mStyle.get());

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(aCb, "mSlots->mAttributeMap");
  aCb.NoteXPCOMChild(mAttributeMap.get());

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(aCb, "mSlots->mChildrenList");
  aCb.NoteXPCOMChild(NS_ISUPPORTS_CAST(nsINodeList*, mChildrenList));

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(aCb, "mSlots->mClassList");
  aCb.NoteXPCOMChild(mClassList.get());
}

void
FragmentOrElement::nsDOMSlots::Unlink()
{
  nsIContent::nsContentSlots::Unlink();
  mStyle = nullptr;
  if (mAttributeMap) {
    mAttributeMap->DropReference();
    mAttributeMap = nullptr;
  }
  mChildrenList = nullptr;
  mClassList = nullptr;
}

size_t
FragmentOrElement::nsDOMSlots::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);
  if (OwnsExtendedSlots()) {
    n += aMallocSizeOf(GetExtendedContentSlots());
  }

  if (mAttributeMap) {
    n += mAttributeMap->SizeOfIncludingThis(aMallocSizeOf);
  }

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - Superclass members (nsINode::nsSlots)
  // - mStyle
  // - mDataSet
  // - mSMILOverrideStyle
  // - mSMILOverrideStyleDeclaration
  // - mChildrenList
  // - mClassList

  // The following member are not measured:
  // - mControllers: because it is non-owning
  // - mBindingParent: because it is some ancestor element.
  return n;
}

FragmentOrElement::nsExtendedDOMSlots::nsExtendedDOMSlots() = default;

FragmentOrElement::nsExtendedDOMSlots::~nsExtendedDOMSlots()
{
}

void
FragmentOrElement::nsExtendedDOMSlots::UnlinkExtendedSlots()
{
  nsIContent::nsExtendedContentSlots::UnlinkExtendedSlots();

  // Don't clear mXBLBinding, it'll be done in
  // BindingManager::RemovedFromDocument from FragmentOrElement::Unlink.
  //
  // mShadowRoot will similarly be cleared explicitly from
  // FragmentOrElement::Unlink.
  mSMILOverrideStyle = nullptr;
  mControllers = nullptr;
  mLabelsList = nullptr;
  if (mCustomElementData) {
    mCustomElementData->Unlink();
    mCustomElementData = nullptr;
  }
}

void
FragmentOrElement::nsExtendedDOMSlots::TraverseExtendedSlots(nsCycleCollectionTraversalCallback& aCb)
{
  nsIContent::nsExtendedContentSlots::TraverseExtendedSlots(aCb);

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(aCb, "mExtendedSlots->mSMILOverrideStyle");
  aCb.NoteXPCOMChild(mSMILOverrideStyle.get());

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(aCb, "mExtendedSlots->mControllers");
  aCb.NoteXPCOMChild(mControllers);

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(aCb, "mExtendedSlots->mLabelsList");
  aCb.NoteXPCOMChild(NS_ISUPPORTS_CAST(nsINodeList*, mLabelsList));

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(aCb, "mExtendedSlots->mShadowRoot");
  aCb.NoteXPCOMChild(NS_ISUPPORTS_CAST(nsIContent*, mShadowRoot));

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(aCb, "mExtendedSlots->mXBLBinding");
  aCb.NoteNativeChild(mXBLBinding,
                     NS_CYCLE_COLLECTION_PARTICIPANT(nsXBLBinding));

  if (mCustomElementData) {
    mCustomElementData->Traverse(aCb);
  }
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
  MOZ_ASSERT(!IsInUncomposedDoc(),
             "Please remove this from the document properly");
  if (GetParent()) {
    NS_RELEASE(mParent);
  }
}

already_AddRefed<nsINodeList>
FragmentOrElement::GetChildren(uint32_t aFilter)
{
  RefPtr<nsSimpleContentList> list = new nsSimpleContentList(this);
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

already_AddRefed<nsINode>
FindChromeAccessOnlySubtreeOwner(EventTarget* aTarget)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aTarget);
  if (!node || !node->ChromeOnlyAccess()) {
    return node.forget();
  }

  if (!node->IsContent()) {
    return nullptr;
  }

  node = FindChromeAccessOnlySubtreeOwner(node->AsContent());
  return node.forget();
}

void
nsIContent::GetEventTargetParent(EventChainPreVisitor& aVisitor)
{
  //FIXME! Document how this event retargeting works, Bug 329124.
  aVisitor.mCanHandle = true;
  aVisitor.mMayHaveListenerManager = HasListenerManager();

  if (IsInShadowTree()) {
    aVisitor.mItemInShadowTree = true;
  }

  // Don't propagate mouseover and mouseout events when mouse is moving
  // inside chrome access only content.
  bool isAnonForEvents = IsRootOfChromeAccessOnlySubtree();
  aVisitor.mRootOfClosedTree = isAnonForEvents;
  if ((aVisitor.mEvent->mMessage == eMouseOver ||
       aVisitor.mEvent->mMessage == eMouseOut ||
       aVisitor.mEvent->mMessage == ePointerOver ||
       aVisitor.mEvent->mMessage == ePointerOut) &&
      // Check if we should stop event propagation when event has just been
      // dispatched or when we're about to propagate from
      // chrome access only subtree or if we are about to propagate out of
      // a shadow root to a shadow root host.
      ((this == aVisitor.mEvent->mOriginalTarget &&
        !ChromeOnlyAccess()) || isAnonForEvents)) {
     nsCOMPtr<nsIContent> relatedTarget =
       do_QueryInterface(aVisitor.mEvent->AsMouseEvent()->mRelatedTarget);
    if (relatedTarget &&
        relatedTarget->OwnerDoc() == OwnerDoc()) {

      // If current target is anonymous for events or we know that related
      // target is descendant of an element which is anonymous for events,
      // we may want to stop event propagation.
      // If this is the original target, aVisitor.mRelatedTargetIsInAnon
      // must be updated.
      if (isAnonForEvents || aVisitor.mRelatedTargetIsInAnon ||
          (aVisitor.mEvent->mOriginalTarget == this &&
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
                do_QueryInterface(aVisitor.mEvent->mOriginalTarget);
              nsAutoString ot, ct, rt;
              if (originalTarget) {
                originalTarget->NodeInfo()->NameAtom()->ToString(ot);
              }
              NodeInfo()->NameAtom()->ToString(ct);
              relatedTarget->NodeInfo()->NameAtom()->ToString(rt);
              printf("Stopping %s propagation:"
                     "\n\toriginalTarget=%s \n\tcurrentTarget=%s %s"
                     "\n\trelatedTarget=%s %s \n%s",
                     (aVisitor.mEvent->mMessage == eMouseOver)
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
              aVisitor.SetParentTarget(nullptr, false);
              // Event should not propagate to non-anon content.
              aVisitor.mCanHandle = isAnonForEvents;
              return;
            }
          }
        }
      }
    }
  }

  // Event parent is the assigned slot, if node is assigned, or node's parent
  // otherwise.
  HTMLSlotElement* slot = GetAssignedSlot();
  nsIContent* parent = slot ? slot : GetParent();

  // Event may need to be retargeted if this is the root of a native
  // anonymous content subtree or event is dispatched somewhere inside XBL.
  if (isAnonForEvents) {
#ifdef DEBUG
    // If a DOM event is explicitly dispatched using node.dispatchEvent(), then
    // all the events are allowed even in the native anonymous content..
    nsCOMPtr<nsIContent> t =
      do_QueryInterface(aVisitor.mEvent->mOriginalTarget);
    NS_ASSERTION(!t || !t->ChromeOnlyAccess() ||
                 aVisitor.mEvent->mClass != eMutationEventClass ||
                 aVisitor.mDOMEvent,
                 "Mutation event dispatched in native anonymous content!?!");
#endif
    aVisitor.mEventTargetAtParent = parent;
  } else if (parent && aVisitor.mOriginalTargetIsInAnon) {
    nsCOMPtr<nsIContent> content(do_QueryInterface(aVisitor.mEvent->mTarget));
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

  if (!aVisitor.mEvent->mFlags.mComposedInNativeAnonymousContent &&
      IsRootOfNativeAnonymousSubtree() && OwnerDoc()->GetWindow()) {
    aVisitor.SetParentTarget(OwnerDoc()->GetWindow()->GetParentTarget(), true);
  } else if (parent) {
    aVisitor.SetParentTarget(parent, false);
    if (slot) {
      ShadowRoot* root = slot->GetContainingShadow();
      if (root && root->IsClosed()) {
        aVisitor.mParentIsSlotInClosedTree = true;
      }
    }
  } else {
    aVisitor.SetParentTarget(GetComposedDoc(), false);
  }

  if (!ChromeOnlyAccess() && !aVisitor.mRelatedTargetRetargetedInCurrentScope) {
    // We don't support Shadow DOM in native anonymous content yet.
    aVisitor.mRelatedTargetRetargetedInCurrentScope = true;
    if (aVisitor.mEvent->mOriginalRelatedTarget) {
      // https://dom.spec.whatwg.org/#concept-event-dispatch
      // Step 3.
      // "Let relatedTarget be the result of retargeting event's relatedTarget
      //  against target if event's relatedTarget is non-null, and null
      //  otherwise."
      //
      // This is a bit complicated because the event might be from native
      // anonymous content, but we need to deal with non-native anonymous
      // content there.
      bool initialTarget = this == aVisitor.mEvent->mOriginalTarget;
      nsCOMPtr<nsINode> originalTargetAsNode;
      // Use of mOriginalTargetIsInAnon is an optimization here.
      if (!initialTarget && aVisitor.mOriginalTargetIsInAnon) {
        originalTargetAsNode =
          FindChromeAccessOnlySubtreeOwner(aVisitor.mEvent->mOriginalTarget);
        initialTarget = originalTargetAsNode == this;
      }
      if (initialTarget) {
        nsCOMPtr<nsINode> relatedTargetAsNode =
          FindChromeAccessOnlySubtreeOwner(aVisitor.mEvent->mOriginalRelatedTarget);
        if (!originalTargetAsNode) {
          originalTargetAsNode =
            do_QueryInterface(aVisitor.mEvent->mOriginalTarget);
        }

        if (relatedTargetAsNode && originalTargetAsNode) {
          nsINode* retargetedRelatedTarget =
            nsContentUtils::Retarget(relatedTargetAsNode, originalTargetAsNode);
          if (originalTargetAsNode == retargetedRelatedTarget &&
              retargetedRelatedTarget != relatedTargetAsNode) {
            // Step 4.
            // "If target is relatedTarget and target is not event's
            //  relatedTarget, then return true."
            aVisitor.IgnoreCurrentTargetBecauseOfShadowDOMRetargeting();
            // Old code relies on mTarget to point to the first element which
            // was not added to the event target chain because of mCanHandle
            // being false, but in Shadow DOM case mTarget really should
            // point to a node in Shadow DOM.
            aVisitor.mEvent->mTarget = aVisitor.mTargetInKnownToBeHandledScope;
            return;
          }

          // Part of step 5. Retargeting target has happened already higher
          // up in this method.
          // "Append to an event path with event, target, targetOverride,
          //  relatedTarget, and false."
          aVisitor.mRetargetedRelatedTarget = retargetedRelatedTarget;
        }
      } else {
        nsCOMPtr<nsINode> relatedTargetAsNode =
          FindChromeAccessOnlySubtreeOwner(aVisitor.mEvent->mOriginalRelatedTarget);
        if (relatedTargetAsNode) {
          // Step 11.3.
          // "Let relatedTarget be the result of retargeting event's
          // relatedTarget against parent if event's relatedTarget is non-null,
          // and null otherwise.".
          nsINode* retargetedRelatedTarget =
            nsContentUtils::Retarget(relatedTargetAsNode, this);
          nsCOMPtr<nsINode> targetInKnownToBeHandledScope =
            FindChromeAccessOnlySubtreeOwner(aVisitor.mTargetInKnownToBeHandledScope);
          // If aVisitor.mTargetInKnownToBeHandledScope wasn't nsINode,
          // targetInKnownToBeHandledScope will be null. This may happen when
          // dispatching event to Window object in a content page and
          // propagating the event to a chrome Element.
          if (targetInKnownToBeHandledScope &&
              nsContentUtils::ContentIsShadowIncludingDescendantOf(
                this, targetInKnownToBeHandledScope->SubtreeRoot())) {
            // Part of step 11.4.
            // "If target's root is a shadow-including inclusive ancestor of
            //  parent, then"
            // "...Append to an event path with event, parent, null, relatedTarget,
            // "   and slot-in-closed-tree."
            aVisitor.mRetargetedRelatedTarget = retargetedRelatedTarget;
          } else if (this == retargetedRelatedTarget) {
            // Step 11.5
            // "Otherwise, if parent and relatedTarget are identical, then set
            //  parent to null."
            aVisitor.IgnoreCurrentTargetBecauseOfShadowDOMRetargeting();
            // Old code relies on mTarget to point to the first element which
            // was not added to the event target chain because of mCanHandle
            // being false, but in Shadow DOM case mTarget really should
            // point to a node in Shadow DOM.
            aVisitor.mEvent->mTarget = aVisitor.mTargetInKnownToBeHandledScope;
            return;
          } else if (targetInKnownToBeHandledScope) {
            // Note, if targetInKnownToBeHandledScope is null,
            // mTargetInKnownToBeHandledScope could be Window object in content
            // page and we're in chrome document in the same process.

            // Step 11.6
            aVisitor.mRetargetedRelatedTarget = retargetedRelatedTarget;
          }
        }
      }
    }

    if (aVisitor.mEvent->mClass == eTouchEventClass) {
      // Retarget touch objects.
      MOZ_ASSERT(!aVisitor.mRetargetedTouchTargets.isSome());
      aVisitor.mRetargetedTouchTargets.emplace();
      WidgetTouchEvent* touchEvent = aVisitor.mEvent->AsTouchEvent();
      WidgetTouchEvent::TouchArray& touches = touchEvent->mTouches;
      for (uint32_t i = 0; i < touches.Length(); ++i) {
        Touch* touch = touches[i];
        EventTarget* originalTarget = touch->mOriginalTarget;
        EventTarget* touchTarget = originalTarget;
        nsCOMPtr<nsINode> targetAsNode = do_QueryInterface(originalTarget);
        if (targetAsNode) {
          EventTarget* retargeted = nsContentUtils::Retarget(targetAsNode, this);
          if (retargeted) {
            touchTarget = retargeted;
          }
        }
        aVisitor.mRetargetedTouchTargets->AppendElement(touchTarget);
        touch->mTarget = touchTarget;
      }
      MOZ_ASSERT(aVisitor.mRetargetedTouchTargets->Length() ==
                   touches.Length());
    }
  }

  if (slot) {
    // Inform that we're about to exit the current scope.
    aVisitor.mRelatedTargetRetargetedInCurrentScope = false;
  }
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

bool
FragmentOrElement::IsLink(nsIURI** aURI) const
{
  *aURI = nullptr;
  return false;
}

nsXBLBinding*
FragmentOrElement::DoGetXBLBinding() const
{
  MOZ_ASSERT(HasFlag(NODE_MAY_BE_IN_BINDING_MNGR));
  const nsExtendedDOMSlots* slots = GetExistingExtendedDOMSlots();
  return slots ? slots->mXBLBinding.get() : nullptr;
}

nsIContent*
nsIContent::GetContainingShadowHost() const
{
  if (mozilla::dom::ShadowRoot* shadow = GetContainingShadow()) {
    return shadow->GetHost();
  }
  return nullptr;
}

void
nsIContent::SetAssignedSlot(HTMLSlotElement* aSlot)
{
  MOZ_ASSERT(aSlot || GetExistingExtendedContentSlots());
  ExtendedContentSlots()->mAssignedSlot = aSlot;
}

void
nsIContent::SetXBLInsertionPoint(nsIContent* aContent)
{
  if (aContent) {
    nsExtendedContentSlots* slots = ExtendedContentSlots();
    SetFlags(NODE_MAY_BE_IN_BINDING_MNGR);
    slots->mXBLInsertionPoint = aContent;
  } else {
    if (nsExtendedContentSlots* slots = GetExistingExtendedContentSlots()) {
      slots->mXBLInsertionPoint = nullptr;
    }
  }
}

nsresult
FragmentOrElement::InsertChildBefore(nsIContent* aKid,
                                     nsIContent* aBeforeThis,
                                     bool aNotify)
{
  MOZ_ASSERT(aKid, "null ptr");

  int32_t index = aBeforeThis ? ComputeIndexOf(aBeforeThis) : GetChildCount();
  MOZ_ASSERT(index >= 0);

  return doInsertChildAt(aKid, index, aNotify, mAttrsAndChildren);
}

void
FragmentOrElement::RemoveChildNode(nsIContent* aKid, bool aNotify)
{
  // Let's keep the node alive.
  nsCOMPtr<nsIContent> kungFuDeathGrip = aKid;
  doRemoveChildAt(ComputeIndexOf(aKid), aNotify, aKid, mAttrsAndChildren);
}

void
FragmentOrElement::GetTextContentInternal(nsAString& aTextContent,
                                          OOMReporter& aError)
{
  if (!nsContentUtils::GetNodeTextContent(this, true, aTextContent, fallible)) {
    aError.ReportOOM();
  }
}

void
FragmentOrElement::SetTextContentInternal(const nsAString& aTextContent,
                                          nsIPrincipal* aSubjectPrincipal,
                                          ErrorResult& aError)
{
  aError = nsContentUtils::SetNodeTextContent(this, aTextContent, false);
}

void
FragmentOrElement::DestroyContent()
{
  // Drop any servo data. We do this before the RemovedFromDocument call below
  // so that it doesn't need to try to keep the style state sane when shuffling
  // around the flattened tree.
  //
  // TODO(emilio): I suspect this can be asserted against instead, with a bit of
  // effort to avoid calling nsDocument::Destroy with a shell...
  if (IsElement()) {
    AsElement()->ClearServoData();
  }

  nsIDocument* document = OwnerDoc();

  document->BindingManager()->RemovedFromDocument(this, document,
                                                  nsBindingManager::eRunDtor);
  document->ClearBoxObjectFor(this);

#ifdef DEBUG
  uint32_t oldChildCount = GetChildCount();
#endif

  for (nsIContent* child = GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    child->DestroyContent();
    MOZ_ASSERT(child->GetParent() == this,
               "Mutating the tree during XBL destructors is evil");
  }

  MOZ_ASSERT(oldChildCount == GetChildCount(),
             "Mutating the tree during XBL destructors is evil");

  if (ShadowRoot* shadowRoot = GetShadowRoot()) {
    shadowRoot->DestroyContent();
  }
}

void
FragmentOrElement::SaveSubtreeState()
{
  for (nsIContent* child = GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    child->SaveSubtreeState();
  }

  // FIXME(bug 1469277): Pretty sure this wants to dig into shadow trees as
  // well.
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
      InternalMutationEvent mutation(true, eLegacyNodeInserted);
      mutation.mRelatedNode = aParent;

      mozAutoSubtreeModified subtree(aDoc, aParent);
      (new AsyncEventDispatcher(childContent, mutation))->RunDOMEventWhenSafe();
    }
  }
}

//----------------------------------------------------------------------

// nsISupports implementation

#define SUBTREE_UNBINDINGS_PER_RUNNABLE 500

class ContentUnbinder : public Runnable
{
public:
  ContentUnbinder()
    : Runnable("ContentUnbinder")
  {
    mLast = this;
  }

  ~ContentUnbinder()
  {
    Run();
  }

  void UnbindSubtree(nsIContent* aNode)
  {
    if (aNode->NodeType() != nsINode::ELEMENT_NODE &&
        aNode->NodeType() != nsINode::DOCUMENT_FRAGMENT_NODE) {
      return;
    }
    FragmentOrElement* container = static_cast<FragmentOrElement*>(aNode);
    uint32_t childCount = container->mAttrsAndChildren.ChildCount();
    if (childCount) {
      // Invalidate cached array of child nodes
      container->InvalidateChildNodes();

      while (childCount-- > 0) {
        // Hold a strong ref to the node when we remove it, because we may be
        // the last reference to it.  We need to call TakeChildAt() and
        // update mFirstChild before calling UnbindFromTree, since this last
        // can notify various observers and they should really see consistent
        // tree state.
        // If this code changes, change the corresponding code in
        // FragmentOrElement's and nsDocument's unlink impls.
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

  NS_IMETHOD Run() override
  {
    nsAutoScriptBlocker scriptBlocker;
    uint32_t len = mSubtreeRoots.Length();
    if (len) {
      for (uint32_t i = 0; i < len; ++i) {
        UnbindSubtree(mSubtreeRoots[i]);
      }
      mSubtreeRoots.Clear();
    }
    nsCycleCollector_dispatchDeferredDeletion();
    if (this == sContentUnbinder) {
      sContentUnbinder = nullptr;
      if (mNext) {
        RefPtr<ContentUnbinder> next;
        next.swap(mNext);
        sContentUnbinder = next;
        next->mLast = mLast;
        mLast = nullptr;
        NS_IdleDispatchToCurrentThread(next.forget());
      }
    }
    return NS_OK;
  }

  static void UnbindAll()
  {
    RefPtr<ContentUnbinder> ub = sContentUnbinder;
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
      NS_IdleDispatchToCurrentThread(e.forget());
    }

    if (sContentUnbinder->mLast->mSubtreeRoots.Length() >=
        SUBTREE_UNBINDINGS_PER_RUNNABLE) {
      sContentUnbinder->mLast->mNext = new ContentUnbinder();
      sContentUnbinder->mLast = sContentUnbinder->mLast->mNext;
    }
    sContentUnbinder->mLast->mSubtreeRoots.AppendElement(aSubtreeRoot);
  }

private:
  AutoTArray<nsCOMPtr<nsIContent>,
               SUBTREE_UNBINDINGS_PER_RUNNABLE> mSubtreeRoots;
  RefPtr<ContentUnbinder>                     mNext;
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

// We purposefully don't UNLINK_BEGIN_INHERITED here.
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(FragmentOrElement)
  nsIContent::Unlink(tmp);

  // The XBL binding is removed by RemoveFromBindingManagerRunnable
  // which is dispatched in UnbindFromTree.

  if (tmp->HasProperties()) {
    if (tmp->IsElement()) {
      Element* elem = tmp->AsElement();
      elem->UnlinkIntersectionObservers();
    }

    if (tmp->IsHTMLElement() || tmp->IsSVGElement()) {
      nsStaticAtom*** props = Element::HTMLSVGPropertiesToTraverseAndUnlink();
      for (uint32_t i = 0; props[i]; ++i) {
        tmp->DeleteProperty(*props[i]);
      }
      if (tmp->MayHaveAnimations()) {
        nsAtom** effectProps = EffectSet::GetEffectSetPropertyAtoms();
        for (uint32_t i = 0; effectProps[i]; ++i) {
          tmp->DeleteProperty(effectProps[i]);
        }
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
        // If this code changes, change the corresponding code in nsDocument's
        // unlink impl and ContentUnbinder::UnbindSubtree.
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

  if (ShadowRoot* shadowRoot = tmp->GetShadowRoot()) {
    for (nsIContent* child = shadowRoot->GetFirstChild();
         child;
         child = child->GetNextSibling()) {
      child->UnbindFromTree(true, false);
    }

    shadowRoot->SetIsComposedDocParticipant(false);
    tmp->ExtendedDOMSlots()->mShadowRoot = nullptr;
  }

  nsIDocument* doc = tmp->OwnerDoc();
  doc->BindingManager()->RemovedFromDocument(tmp, doc,
                                             nsBindingManager::eDoNotRunDtor);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(FragmentOrElement)

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

static void
ClearBlackMarkedNodes()
{
  if (!gCCBlackMarkedNodes) {
    return;
  }
  for (auto iter = gCCBlackMarkedNodes->ConstIter(); !iter.Done();
       iter.Next()) {
    nsINode* n = iter.Get()->GetKey();
    n->SetCCMarkedRoot(false);
    n->SetInCCBlackTree(false);
  }
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

static bool
IsCertainlyAliveNode(nsINode* aNode, nsIDocument* aDoc)
{
  MOZ_ASSERT(aNode->GetComposedDoc() == aDoc);

  // Marked to be in-CC-generation or if the document is an svg image that's
  // being kept alive by the image cache. (Note that an svg image's internal
  // SVG document will receive an OnPageHide() call when it gets purged from
  // the image cache; hence, we use IsVisible() as a hint that the document is
  // actively being kept alive by the cache.)
  return nsCCUncollectableMarker::InGeneration(aDoc->GetMarkedCCGeneration()) ||
         (nsCCUncollectableMarker::sGeneration &&
          aDoc->IsBeingUsedAsImage() &&
          aDoc->IsVisible());
}

// static
bool
FragmentOrElement::CanSkipInCC(nsINode* aNode)
{
  // Don't try to optimize anything during shutdown.
  if (nsCCUncollectableMarker::sGeneration == 0) {
    return false;
  }

  nsIDocument* currentDoc = aNode->GetComposedDoc();
  if (currentDoc && IsCertainlyAliveNode(aNode, currentDoc)) {
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
  // from the purple buffer if the DOM tree is known-live.
  AutoTArray<nsIContent*, 1020> nodesToUnpurple;
  // grayNodes need script traverse, so they aren't removed from
  // the purple buffer, but are marked to be in known-live subtree so that
  // traverse is faster.
  AutoTArray<nsINode*, 1020> grayNodes;

  bool foundLiveWrapper = root->HasKnownLiveWrapper();
  if (root != currentDoc) {
    currentDoc = nullptr;
    if (NeedsScriptTraverse(root)) {
      grayNodes.AppendElement(root);
    } else if (static_cast<nsIContent*>(root)->IsPurple()) {
      nodesToUnpurple.AppendElement(static_cast<nsIContent*>(root));
    }
  }

  // Traverse the subtree and check if we could know without CC
  // that it is known-live.
  // Note, this traverse is non-virtual and inline, so it should be a lot faster
  // than CC's generic traverse.
  for (nsIContent* node = root->GetFirstChild(); node;
       node = node->GetNextNode(root)) {
    foundLiveWrapper = foundLiveWrapper || node->HasKnownLiveWrapper();
    if (foundLiveWrapper && currentDoc) {
      // If we can mark the whole document known-live, no need to optimize
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
  root->SetInCCBlackTree(foundLiveWrapper);
  gCCBlackMarkedNodes->PutEntry(root);

  if (!foundLiveWrapper) {
    return false;
  }

  if (currentDoc) {
    // Special case documents. If we know the document is known-live,
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

  // Subtree is known-live, we can remove non-gray purple nodes from
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

AutoTArray<nsINode*, 1020>* gPurpleRoots = nullptr;
AutoTArray<nsIContent*, 1020>* gNodesToUnbind = nullptr;

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
  MOZ_ASSERT(aContent);
  if (aContent->IsPurple()) {
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

// CanSkip checks if aNode is known-live, and if it is, returns true. If aNode
// is in a known-live DOM tree, CanSkip may also remove other objects from
// purple buffer and unmark event listeners and user data.  If the root of the
// DOM tree is a document, less optimizations are done since checking the
// liveness of the current document is usually fast and we don't want slow down
// such common cases.
bool
FragmentOrElement::CanSkip(nsINode* aNode, bool aRemovingAllowed)
{
  // Don't try to optimize anything during shutdown.
  if (nsCCUncollectableMarker::sGeneration == 0) {
    return false;
  }

  bool unoptimizable = aNode->UnoptimizableCCNode();
  nsIDocument* currentDoc = aNode->GetComposedDoc();
  if (currentDoc && IsCertainlyAliveNode(aNode, currentDoc) &&
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
  AutoTArray<nsIContent*, 1020> nodesToClear;

  bool foundLiveWrapper = root->HasKnownLiveWrapper();
  bool domOnlyCycle = false;
  if (root != currentDoc) {
    currentDoc = nullptr;
    if (!foundLiveWrapper) {
      domOnlyCycle = static_cast<nsIContent*>(root)->OwnedOnlyByTheDOMTree();
    }
    if (ShouldClearPurple(static_cast<nsIContent*>(root))) {
      nodesToClear.AppendElement(static_cast<nsIContent*>(root));
    }
  }

  // Traverse the subtree and check if we could know without CC
  // that it is known-live.
  // Note, this traverse is non-virtual and inline, so it should be a lot faster
  // than CC's generic traverse.
  for (nsIContent* node = root->GetFirstChild(); node;
       node = node->GetNextNode(root)) {
    foundLiveWrapper = foundLiveWrapper || node->HasKnownLiveWrapper();
    if (foundLiveWrapper) {
      domOnlyCycle = false;
      if (currentDoc) {
        // If we can mark the whole document live, no need to optimize
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
        // they are kept alive in a known-live tree or are in a DOM-only cycle.
        nodesToClear.AppendElement(node);
      }
    }
  }

  if (!currentDoc || !foundLiveWrapper) {
    root->SetIsPurpleRoot(true);
    if (domOnlyCycle) {
      if (!gNodesToUnbind) {
        gNodesToUnbind = new AutoTArray<nsIContent*, 1020>();
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
        gPurpleRoots = new AutoTArray<nsINode*, 1020>();
      }
      gPurpleRoots->AppendElement(root);
    }
  }

  if (!foundLiveWrapper) {
    return false;
  }

  if (currentDoc) {
    // Special case documents. If we know the document is known-live,
    // we can mark the document to be in CCGeneration.
    currentDoc->
      MarkUncollectableForCCGeneration(nsCCUncollectableMarker::sGeneration);
    MarkNodeChildren(currentDoc);
  }

  // Subtree is known-live, so we can remove purple nodes from
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
  if (aNode->HasKnownLiveWrapper()) {
    return true;
  }
  nsIDocument* c = aNode->GetComposedDoc();
  return
    ((c && IsCertainlyAliveNode(aNode, c)) || aNode->InCCBlackTree()) &&
    !NeedsScriptTraverse(aNode);
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

// We purposefully don't TRAVERSE_BEGIN_INHERITED here.  All the bits
// we should traverse should be added here or in nsINode::Traverse.
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INTERNAL(FragmentOrElement)
  if (MOZ_UNLIKELY(cb.WantDebugInfo())) {
    char name[512];
    uint32_t nsid = tmp->GetNameSpaceID();
    nsAtomCString localName(tmp->NodeInfo()->NameAtom());
    nsAutoCString uri;
    if (tmp->OwnerDoc()->GetDocumentURI()) {
      uri = tmp->OwnerDoc()->GetDocumentURI()->GetSpecOrDefault();
    }

    nsAutoString id;
    nsAtom* idAtom = tmp->GetID();
    if (idAtom) {
      id.AppendLiteral(" id='");
      id.Append(nsDependentAtomString(idAtom));
      id.Append('\'');
    }

    nsAutoString classes;
    const nsAttrValue* classAttrValue = tmp->IsElement() ?
      tmp->AsElement()->GetClasses() : nullptr;
    if (classAttrValue) {
      classes.AppendLiteral(" class='");
      nsAutoString classString;
      classAttrValue->ToString(classString);
      classString.ReplaceChar(char16_t('\n'), char16_t(' '));
      classes.Append(classString);
      classes.Append('\'');
    }

    nsAutoCString orphan;
    if (!tmp->IsInComposedDoc() &&
        // Ignore xbl:content, which is never in the document and hence always
        // appears to be orphaned.
        !tmp->NodeInfo()->Equals(nsGkAtoms::content, kNameSpaceID_XBL)) {
      orphan.AppendLiteral(" (orphan)");
    }

    const char* nsuri = nsid < ArrayLength(kNSURIs) ? kNSURIs[nsid] : "";
    SprintfLiteral(name, "FragmentOrElement%s %s%s%s%s %s",
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

  if (!nsIContent::Traverse(tmp, cb)) {
    return NS_SUCCESS_INTERRUPTED_TRAVERSE;
  }

  tmp->OwnerDoc()->BindingManager()->Traverse(tmp, cb);

  // Check that whenever we have effect properties, MayHaveAnimations is set.
#ifdef DEBUG
  nsAtom** effectProps = EffectSet::GetEffectSetPropertyAtoms();
  for (uint32_t i = 0; effectProps[i]; ++i) {
    MOZ_ASSERT_IF(tmp->GetProperty(effectProps[i]), tmp->MayHaveAnimations());
  }
#endif

  if (tmp->HasProperties()) {
    if (tmp->IsElement()) {
      Element* elem = tmp->AsElement();
      IntersectionObserverList* observers =
        static_cast<IntersectionObserverList*>(
          elem->GetProperty(nsGkAtoms::intersectionobserverlist)
        );
      if (observers) {
        for (auto iter = observers->Iter(); !iter.Done(); iter.Next()) {
          DOMIntersectionObserver* observer = iter.Key();
          cb.NoteXPCOMChild(observer);
        }
      }
    }
    if (tmp->IsHTMLElement() || tmp->IsSVGElement()) {
      nsStaticAtom*** props = Element::HTMLSVGPropertiesToTraverseAndUnlink();
      for (uint32_t i = 0; props[i]; ++i) {
        nsISupports* property =
          static_cast<nsISupports*>(tmp->GetProperty(*props[i]));
        cb.NoteXPCOMChild(property);
      }
      if (tmp->MayHaveAnimations()) {
        nsAtom** effectProps = EffectSet::GetEffectSetPropertyAtoms();
        for (uint32_t i = 0; effectProps[i]; ++i) {
          EffectSet* effectSet =
            static_cast<EffectSet*>(tmp->GetProperty(effectProps[i]));
          if (effectSet) {
            effectSet->Traverse(cb);
          }
        }
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
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END


NS_INTERFACE_MAP_BEGIN(FragmentOrElement)
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(FragmentOrElement)
NS_INTERFACE_MAP_END_INHERITING(nsIContent)

//----------------------------------------------------------------------

nsresult
FragmentOrElement::CopyInnerTo(FragmentOrElement* aDst,
                               bool aPreallocateChildren)
{
  nsresult rv = aDst->mAttrsAndChildren.EnsureCapacityToClone(mAttrsAndChildren,
                                                              aPreallocateChildren);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t i, count = mAttrsAndChildren.AttrCount();
  for (i = 0; i < count; ++i) {
    const nsAttrName* name = mAttrsAndChildren.AttrNameAt(i);
    const nsAttrValue* value = mAttrsAndChildren.AttrAt(i);
    nsAutoString valStr;
    value->ToString(valStr);
    rv = aDst->AsElement()->SetAttr(name->NamespaceID(), name->LocalName(),
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
  MOZ_ASSERT_UNREACHABLE("called FragmentOrElement::TextLength");

  return 0;
}

bool
FragmentOrElement::TextIsOnlyWhitespace()
{
  return false;
}

bool
FragmentOrElement::ThreadSafeTextIsOnlyWhitespace() const
{
  return false;
}

uint32_t
FragmentOrElement::GetChildCount() const
{
  return mAttrsAndChildren.ChildCount();
}

nsIContent *
FragmentOrElement::GetChildAt_Deprecated(uint32_t aIndex) const
{
  return mAttrsAndChildren.GetSafeChildAt(aIndex);
}

int32_t
FragmentOrElement::ComputeIndexOf(const nsINode* aPossibleChild) const
{
  return mAttrsAndChildren.IndexOfChild(aPossibleChild);
}

static inline bool
IsVoidTag(nsAtom* aTag)
{
  static const nsAtom* voidElements[] = {
    nsGkAtoms::area, nsGkAtoms::base, nsGkAtoms::basefont,
    nsGkAtoms::bgsound, nsGkAtoms::br, nsGkAtoms::col,
    nsGkAtoms::embed, nsGkAtoms::frame,
    nsGkAtoms::hr, nsGkAtoms::img, nsGkAtoms::input,
    nsGkAtoms::keygen, nsGkAtoms::link, nsGkAtoms::meta,
    nsGkAtoms::param, nsGkAtoms::source, nsGkAtoms::track,
    nsGkAtoms::wbr
  };

  static mozilla::BloomFilter<12, nsAtom> sFilter;
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

/* static */
bool
FragmentOrElement::IsHTMLVoid(nsAtom* aLocalName)
{
  return aLocalName && IsVoidTag(aLocalName);
}

void
FragmentOrElement::GetMarkup(bool aIncludeSelf, nsAString& aMarkup)
{
  aMarkup.Truncate();

  nsIDocument* doc = OwnerDoc();
  if (IsInHTMLDocument()) {
    nsContentUtils::SerializeNodeToMarkup(this, !aIncludeSelf, aMarkup);
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
    TextEditor* textEditor = elem ? elem->GetTextEditorInternal() : nullptr;
    if (textEditor && textEditor->OutputsMozDirty()) {
      flags &= ~nsIDocumentEncoder::OutputIgnoreMozDirty;
    }
  }

  DebugOnly<nsresult> rv = docEncoder->NativeInit(doc, contentType, flags);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  if (aIncludeSelf) {
    docEncoder->SetNode(this);
  } else {
    docEncoder->SetContainerNode(this);
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
  mozAutoDocUpdate updateBatch(doc, true);

  // Remove childnodes.
  nsAutoMutationBatch mb(target, true, false);
  while (target->HasChildren()) {
    target->RemoveChildNode(target->GetFirstChild(), true);
  }
  mb.RemovalDone();

  nsAutoScriptLoaderDisabler sld(doc);

  nsAtom* contextLocalName = NodeInfo()->NameAtom();
  int32_t contextNameSpaceID = GetNameSpaceID();

  if (ShadowRoot* shadowRoot = ShadowRoot::FromNode(this)) {
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
    RefPtr<DocumentFragment> df =
      nsContentUtils::CreateContextualFragment(target, aInnerHTML, true,
                                               aError);
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

void
FragmentOrElement::FireNodeRemovedForChildren()
{
  nsIDocument* doc = OwnerDoc();
  // Optimize the common case
  if (!nsContentUtils::
        HasMutationListeners(doc, NS_EVENT_BITS_MUTATION_NODEREMOVED)) {
    return;
  }

  nsCOMPtr<nsINode> child;
  for (child = GetFirstChild();
       child && child->GetParentNode() == this;
       child = child->GetNextSibling()) {
    nsContentUtils::MaybeFireNodeRemoved(child, this);
  }
}

void
FragmentOrElement::AddSizeOfExcludingThis(nsWindowSizes& aSizes,
                                          size_t* aNodeSize) const
{
  nsIContent::AddSizeOfExcludingThis(aSizes, aNodeSize);
  *aNodeSize +=
    mAttrsAndChildren.SizeOfExcludingThis(aSizes.mState.mMallocSizeOf);

  nsDOMSlots* slots = GetExistingDOMSlots();
  if (slots) {
    *aNodeSize += slots->SizeOfIncludingThis(aSizes.mState.mMallocSizeOf);
  }
}
