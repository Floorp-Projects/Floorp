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

#include "mozilla/dom/FragmentOrElement.h"

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
  return nullptr;
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
  NS_ENSURE_TRUE(IsEditableInternal(), nullptr);

  nsIDocument* doc = GetCurrentDoc();
  NS_ENSURE_TRUE(doc, nullptr);
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
    *aReturn = nullptr;

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

  return nullptr;
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

nsContentList*
FragmentOrElement::GetChildrenList()
{
  FragmentOrElement::nsDOMSlots *slots = DOMSlots();

  if (!slots->mChildrenList) {
    slots->mChildrenList = new nsContentList(this, kNameSpaceID_Wildcard, 
                                             nsGkAtoms::_asterix, nsGkAtoms::_asterix,
                                             false);
  }

  return slots->mChildrenList;
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
    mNode->GetSlots()->mWeakReference = nullptr;
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
  nsIContent* result = FragmentOrElement::doQuerySelector(mNode, aSelector, &rv);
  return result ? CallQueryInterface(result, aReturn) : rv;
}

NS_IMETHODIMP
nsNodeSelectorTearoff::QuerySelectorAll(const nsAString& aSelector,
                                        nsIDOMNodeList **aReturn)
{
  return FragmentOrElement::doQuerySelectorAll(mNode, aSelector, aReturn);
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
FragmentOrElement::nsDOMSlots::nsDOMSlots()
  : nsINode::nsSlots(),
    mDataset(nullptr),
    mBindingParent(nullptr)
{
}

FragmentOrElement::nsDOMSlots::~nsDOMSlots()
{
  if (mAttributeMap) {
    mAttributeMap->DropReference();
  }

  if (mClassList) {
    mClassList->DropReference();
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
  mChildrenList = nullptr;
  if (mClassList) {
    mClassList->DropReference();
    mClassList = nullptr;
  }
}

FragmentOrElement::FragmentOrElement(already_AddRefed<nsINodeInfo> aNodeInfo)
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

NS_IMETHODIMP
FragmentOrElement::GetNodeName(nsAString& aNodeName)
{
  aNodeName = NodeName();
  return NS_OK;
}

NS_IMETHODIMP
FragmentOrElement::GetLocalName(nsAString& aLocalName)
{
  aLocalName = LocalName();
  return NS_OK;
}

NS_IMETHODIMP
FragmentOrElement::GetNodeValue(nsAString& aNodeValue)
{
  SetDOMStringToNull(aNodeValue);

  return NS_OK;
}

NS_IMETHODIMP
FragmentOrElement::SetNodeValue(const nsAString& aNodeValue)
{
  // The DOM spec says that when nodeValue is defined to be null "setting it
  // has no effect", so we don't throw an exception.
  return NS_OK;
}

NS_IMETHODIMP
FragmentOrElement::GetNodeType(PRUint16* aNodeType)
{
  *aNodeType = NodeType();
  return NS_OK;
}

NS_IMETHODIMP
FragmentOrElement::GetNamespaceURI(nsAString& aNamespaceURI)
{
  return mNodeInfo->GetNamespaceURI(aNamespaceURI);
}

NS_IMETHODIMP
FragmentOrElement::GetPrefix(nsAString& aPrefix)
{
  mNodeInfo->GetPrefix(aPrefix);
  return NS_OK;
}

nsresult
FragmentOrElement::InternalIsSupported(nsISupports* aObject,
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
FragmentOrElement::IsSupported(const nsAString& aFeature,
                               const nsAString& aVersion,
                               bool* aReturn)
{
  return InternalIsSupported(this, aFeature, aVersion, aReturn);
}

NS_IMETHODIMP
FragmentOrElement::HasAttributes(bool* aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);

  *aReturn = GetAttrCount() > 0;

  return NS_OK;
}

NS_IMETHODIMP
FragmentOrElement::GetAttributes(nsIDOMNamedNodeMap** aAttributes)
{
  if (!IsElement()) {
    *aAttributes = nullptr;
    return NS_OK;
  }

  nsDOMSlots *slots = DOMSlots();

  if (!slots->mAttributeMap) {
    slots->mAttributeMap = new nsDOMAttributeMap(this->AsElement());
  }

  NS_ADDREF(*aAttributes = slots->mAttributeMap);

  return NS_OK;
}

nsresult
FragmentOrElement::HasChildNodes(bool* aReturn)
{
  *aReturn = mAttrsAndChildren.ChildCount() > 0;

  return NS_OK;
}

already_AddRefed<nsINodeList>
FragmentOrElement::GetChildren(PRUint32 aFilter)
{
  nsRefPtr<nsSimpleContentList> list = new nsSimpleContentList(this);
  if (!list) {
    return nullptr;
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
  nsINodeList *childList = nullptr;

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

  nsINodeList* returnList = nullptr;
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
FragmentOrElement::DoGetClasses() const
{
  NS_NOTREACHED("Shouldn't ever be called");
  return nullptr;
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

nsresult
FragmentOrElement::InsertChildAt(nsIContent* aKid,
                                PRUint32 aIndex,
                                bool aNotify)
{
  NS_PRECONDITION(aKid, "null ptr");

  return doInsertChildAt(aKid, aIndex, aNotify, mAttrsAndChildren);
}

void
FragmentOrElement::RemoveChildAt(PRUint32 aIndex, bool aNotify)
{
  nsCOMPtr<nsIContent> oldKid = mAttrsAndChildren.GetSafeChildAt(aIndex);
  NS_ASSERTION(oldKid == GetChildAt(aIndex), "Unexpected child in RemoveChildAt");

  if (oldKid) {
    doRemoveChildAt(aIndex, aNotify, oldKid, mAttrsAndChildren);
  }
}

NS_IMETHODIMP
FragmentOrElement::GetTextContent(nsAString &aTextContent)
{
  nsContentUtils::GetNodeTextContent(this, true, aTextContent);
  return NS_OK;
}

NS_IMETHODIMP
FragmentOrElement::SetTextContent(const nsAString& aTextContent)
{
  return nsContentUtils::SetNodeTextContent(this, aTextContent, false);
}

void
FragmentOrElement::DestroyContent()
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
FragmentOrElement::SaveSubtreeState()
{
  PRUint32 i, count = mAttrsAndChildren.ChildCount();
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

NS_IMPL_CYCLE_COLLECTION_CLASS(FragmentOrElement)

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
    FragmentOrElement* container = static_cast<FragmentOrElement*>(aNode);
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

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(FragmentOrElement)
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

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(FragmentOrElement)
  nsINode::Trace(tmp, aCallback, aClosure);
NS_IMPL_CYCLE_COLLECTION_TRACE_END

void
FragmentOrElement::MarkUserData(void* aObject, nsIAtom* aKey, void* aChild,
                               void* aData)
{
  PRUint32* gen = static_cast<PRUint32*>(aData);
  xpc_MarkInCCGeneration(static_cast<nsISupports*>(aChild), *gen);
}

void
FragmentOrElement::MarkUserDataHandler(void* aObject, nsIAtom* aKey,
                                      void* aChild, void* aData)
{
  xpc_TryUnmarkWrappedGrayObject(static_cast<nsISupports*>(aChild));
}

void
FragmentOrElement::MarkNodeChildren(nsINode* aNode)
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
      Enumerate(aNode, FragmentOrElement::MarkUserData,
                &nsCCUncollectableMarker::sGeneration);
    ownerDoc->PropertyTable(DOM_USER_DATA_HANDLER)->
      Enumerate(aNode, FragmentOrElement::MarkUserDataHandler,
                &nsCCUncollectableMarker::sGeneration);
  }
}

nsINode*
FindOptimizableSubtreeRoot(nsINode* aNode)
{
  nsINode* p;
  while ((p = aNode->GetNodeParent())) {
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

nsAutoTArray<nsINode*, 1020>* gCCBlackMarkedNodes = nullptr;

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
  gCCBlackMarkedNodes = nullptr;
}

// static
bool
FragmentOrElement::CanSkipInCC(nsINode* aNode)
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

nsAutoTArray<nsINode*, 1020>* gPurpleRoots = nullptr;
nsAutoTArray<nsIContent*, 1020>* gNodesToUnbind = nullptr;

void ClearCycleCollectorCleanupData()
{
  if (gPurpleRoots) {
    PRUint32 len = gPurpleRoots->Length();
    for (PRUint32 i = 0; i < len; ++i) {
      nsINode* n = gPurpleRoots->ElementAt(i);
      n->SetIsPurpleRoot(false);
    }
    delete gPurpleRoots;
    gPurpleRoots = nullptr;
  }
  if (gNodesToUnbind) {
    PRUint32 len = gNodesToUnbind->Length();
    for (PRUint32 i = 0; i < len; ++i) {
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
FragmentOrElement::CanSkip(nsINode* aNode, bool aRemovingAllowed)
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
FragmentOrElement::CanSkipThis(nsINode* aNode)
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
    PR_snprintf(name, sizeof(name), "FragmentOrElement%s %s%s%s %s",
                nsuri,
                localName.get(),
                NS_ConvertUTF16toUTF8(id).get(),
                NS_ConvertUTF16toUTF8(classes).get(),
                uri.get());
    cb.DescribeRefCountedNode(tmp->mRefCnt.get(), sizeof(FragmentOrElement),
                              name);
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


NS_INTERFACE_MAP_BEGIN(FragmentOrElement)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(FragmentOrElement)
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

NS_IMPL_CYCLE_COLLECTING_ADDREF(FragmentOrElement)
NS_IMPL_CYCLE_COLLECTING_RELEASE_WITH_DESTROY(FragmentOrElement,
                                              nsNodeUtils::LastRelease(this))

nsresult
FragmentOrElement::PostQueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  return OwnerDoc()->BindingManager()->GetBindingImplementation(this, aIID,
                                                                aInstancePtr);
}

//----------------------------------------------------------------------

nsresult
FragmentOrElement::CopyInnerTo(FragmentOrElement* aDst)
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

const nsTextFragment*
FragmentOrElement::GetText()
{
  return nullptr;
}

PRUint32
FragmentOrElement::TextLength() const
{
  // We can remove this assertion if it turns out to be useful to be able
  // to depend on this returning 0
  NS_NOTREACHED("called FragmentOrElement::TextLength");

  return 0;
}

nsresult
FragmentOrElement::SetText(const PRUnichar* aBuffer, PRUint32 aLength,
                          bool aNotify)
{
  NS_ERROR("called FragmentOrElement::SetText");

  return NS_ERROR_FAILURE;
}

nsresult
FragmentOrElement::AppendText(const PRUnichar* aBuffer, PRUint32 aLength,
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

void
FragmentOrElement::AppendTextTo(nsAString& aResult)
{
  // We can remove this assertion if it turns out to be useful to be able
  // to depend on this appending nothing.
  NS_NOTREACHED("called FragmentOrElement::TextLength");
}

PRUint32
FragmentOrElement::GetChildCount() const
{
  return mAttrsAndChildren.ChildCount();
}

nsIContent *
FragmentOrElement::GetChildAt(PRUint32 aIndex) const
{
  return mAttrsAndChildren.GetSafeChildAt(aIndex);
}

nsIContent * const *
FragmentOrElement::GetChildArray(PRUint32* aChildCount) const
{
  return mAttrsAndChildren.GetChildArray(aChildCount);
}

PRInt32
FragmentOrElement::IndexOf(nsINode* aPossibleChild) const
{
  return mAttrsAndChildren.IndexOfChild(aPossibleChild);
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
       child && child->GetNodeParent() == this;
       child = child->GetNextSibling()) {
    nsContentUtils::MaybeFireNodeRemoved(child, this, doc);
  }
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
      cur->mNext = nullptr;
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
  ElementHolder() : mElement(nullptr) {}
  void AppendElement(Element* aElement) {
    NS_ABORT_IF_FALSE(!mElement, "Should only get one element");
    mElement = aElement;
  }
  Element* mElement;
};

/* static */
nsIContent*
FragmentOrElement::doQuerySelector(nsINode* aRoot, const nsAString& aSelector,
                                  nsresult *aResult)
{
  NS_PRECONDITION(aResult, "Null out param?");

  ElementHolder holder;
  *aResult = FindMatchingElements<true>(aRoot, aSelector, holder);

  return holder.mElement;
}

/* static */
nsresult
FragmentOrElement::doQuerySelectorAll(nsINode* aRoot,
                                     const nsAString& aSelector,
                                     nsIDOMNodeList **aReturn)
{
  NS_PRECONDITION(aReturn, "Null out param?");

  nsSimpleContentList* contentList = new nsSimpleContentList(aRoot);
  NS_ENSURE_TRUE(contentList, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(*aReturn = contentList);
  
  return FindMatchingElements<false>(aRoot, aSelector, *contentList);
}


size_t
FragmentOrElement::SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf) const
{
  return nsIContent::SizeOfExcludingThis(aMallocSizeOf) +
         mAttrsAndChildren.SizeOfExcludingThis(aMallocSizeOf);
}
