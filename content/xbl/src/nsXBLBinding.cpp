/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=79: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsIAtom.h"
#include "nsXBLDocumentInfo.h"
#include "nsIInputStream.h"
#include "nsINameSpaceManager.h"
#include "nsHashtable.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsIChannel.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsNetUtil.h"
#include "plstr.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsContentUtils.h"
#include "ChildIterator.h"
#include "nsCxPusher.h"
#ifdef MOZ_XUL
#include "nsIXULDocument.h"
#endif
#include "nsIXMLContentSink.h"
#include "nsContentCID.h"
#include "mozilla/dom/XMLDocument.h"
#include "jsapi.h"
#include "nsXBLService.h"
#include "nsIXPConnect.h"
#include "nsIScriptContext.h"
#include "nsCRT.h"

// Event listeners
#include "nsEventListenerManager.h"
#include "nsIDOMEventListener.h"
#include "nsAttrName.h"

#include "nsGkAtoms.h"

#include "nsXBLPrototypeHandler.h"

#include "nsXBLPrototypeBinding.h"
#include "nsXBLBinding.h"
#include "nsIPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsGUIEvent.h"
#include "mozilla/dom/XBLChildrenElement.h"

#include "prprf.h"
#include "nsNodeUtils.h"
#include "nsJSUtils.h"

// Nasty hack.  Maybe we could move some of the classinfo utility methods
// (e.g. WrapNative) over to nsContentUtils?
#include "nsDOMClassInfo.h"

#include "mozilla/dom/Element.h"

using namespace mozilla;
using namespace mozilla::dom;

// Helper classes

/***********************************************************************/
//
// The JS class for XBLBinding
//
static void
XBLFinalize(JSFreeOp *fop, JSObject *obj)
{
  nsXBLDocumentInfo* docInfo =
    static_cast<nsXBLDocumentInfo*>(::JS_GetPrivate(obj));
  nsContentUtils::DeferredFinalize(static_cast<nsIScriptGlobalObjectOwner*>(docInfo));
  
  nsXBLJSClass* c = static_cast<nsXBLJSClass*>(::JS_GetClass(obj));
  c->Drop();
}

static JSBool
XBLEnumerate(JSContext *cx, JS::Handle<JSObject*> obj)
{
  nsXBLPrototypeBinding* protoBinding =
    static_cast<nsXBLPrototypeBinding*>(::JS_GetReservedSlot(obj, 0).toPrivate());
  MOZ_ASSERT(protoBinding);

  return protoBinding->ResolveAllFields(cx, obj);
}

uint64_t nsXBLJSClass::sIdCount = 0;

nsXBLJSClass::nsXBLJSClass(const nsAFlatCString& aClassName,
                           const nsCString& aKey)
{
  memset(this, 0, sizeof(nsXBLJSClass));
  next = prev = static_cast<JSCList*>(this);
  name = ToNewCString(aClassName);
  flags =
    JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS |
    JSCLASS_NEW_RESOLVE |
    // Our one reserved slot holds the relevant nsXBLPrototypeBinding
    JSCLASS_HAS_RESERVED_SLOTS(1);
  addProperty = getProperty = ::JS_PropertyStub;
  delProperty = ::JS_DeletePropertyStub;
  setProperty = ::JS_StrictPropertyStub;
  enumerate = XBLEnumerate;
  resolve = JS_ResolveStub;
  convert = ::JS_ConvertStub;
  finalize = XBLFinalize;
  mKey = aKey;
}

nsrefcnt
nsXBLJSClass::Destroy()
{
  NS_ASSERTION(next == prev && prev == static_cast<JSCList*>(this),
               "referenced nsXBLJSClass is on LRU list already!?");

  if (nsXBLService::gClassTable) {
    nsCStringKey key(mKey);
    (nsXBLService::gClassTable)->Remove(&key);
    mKey.Truncate();
  }

  if (nsXBLService::gClassLRUListLength >= nsXBLService::gClassLRUListQuota) {
    // Over LRU list quota, just unhash and delete this class.
    delete this;
  } else {
    // Put this most-recently-used class on end of the LRU-sorted freelist.
    JSCList* mru = static_cast<JSCList*>(this);
    JS_APPEND_LINK(mru, &nsXBLService::gClassLRUList);
    nsXBLService::gClassLRUListLength++;
  }

  return 0;
}

// Implementation /////////////////////////////////////////////////////////////////

// Constructors/Destructors
nsXBLBinding::nsXBLBinding(nsXBLPrototypeBinding* aBinding)
  : mMarkedForDeath(false),
    mPrototypeBinding(aBinding)
{
  NS_ASSERTION(mPrototypeBinding, "Must have a prototype binding!");
  // Grab a ref to the document info so the prototype binding won't die
  NS_ADDREF(mPrototypeBinding->XBLDocumentInfo());
}


nsXBLBinding::~nsXBLBinding(void)
{
  if (mContent) {
    nsXBLBinding::UninstallAnonymousContent(mContent->OwnerDoc(), mContent);
  }
  nsXBLDocumentInfo* info = mPrototypeBinding->XBLDocumentInfo();
  NS_RELEASE(info);
}

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsXBLBinding)
  // XXX Probably can't unlink mPrototypeBinding->XBLDocumentInfo(), because
  //     mPrototypeBinding is weak.
  if (tmp->mContent) {
    nsXBLBinding::UninstallAnonymousContent(tmp->mContent->OwnerDoc(),
                                            tmp->mContent);
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mContent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mNextBinding)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDefaultInsertionPoint)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mInsertionPoints)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mAnonymousContentList)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsXBLBinding)
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb,
                                     "mPrototypeBinding->XBLDocumentInfo()");
  cb.NoteXPCOMChild(static_cast<nsIScriptGlobalObjectOwner*>(
                      tmp->mPrototypeBinding->XBLDocumentInfo()));
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mContent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mNextBinding)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDefaultInsertionPoint)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mInsertionPoints)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAnonymousContentList)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(nsXBLBinding, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(nsXBLBinding, Release)

void
nsXBLBinding::SetBaseBinding(nsXBLBinding* aBinding)
{
  if (mNextBinding) {
    NS_ERROR("Base XBL binding is already defined!");
    return;
  }

  mNextBinding = aBinding; // Comptr handles rel/add
}

nsXBLBinding*
nsXBLBinding::GetBindingWithContent()
{
  if (mContent) {
    return this;
  }

  return mNextBinding ? mNextBinding->GetBindingWithContent() : nullptr;
}

void
nsXBLBinding::InstallAnonymousContent(nsIContent* aAnonParent, nsIContent* aElement,
                                      bool aChromeOnlyContent)
{
  // We need to ensure two things.
  // (1) The anonymous content should be fooled into thinking it's in the bound
  // element's document, assuming that the bound element is in a document
  // Note that we don't change the current doc of aAnonParent here, since that
  // quite simply does not matter.  aAnonParent is just a way of keeping refs
  // to all its kids, which are anonymous content from the point of view of
  // aElement.
  // (2) The children's parent back pointer should not be to this synthetic root
  // but should instead point to the enclosing parent element.
  nsIDocument* doc = aElement->GetCurrentDoc();
  bool allowScripts = AllowScripts();

  nsAutoScriptBlocker scriptBlocker;
  for (nsIContent* child = aAnonParent->GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    child->UnbindFromTree();
    if (aChromeOnlyContent) {
      child->SetFlags(NODE_CHROME_ONLY_ACCESS |
                      NODE_IS_ROOT_OF_CHROME_ONLY_ACCESS);
    }
    nsresult rv =
      child->BindToTree(doc, aElement, mBoundElement, allowScripts);
    if (NS_FAILED(rv)) {
      // Oh, well... Just give up.
      // XXXbz This really shouldn't be a void method!
      child->UnbindFromTree();
      return;
    }        

    child->SetFlags(NODE_IS_ANONYMOUS);

#ifdef MOZ_XUL
    // To make XUL templates work (and other goodies that happen when
    // an element is added to a XUL document), we need to notify the
    // XUL document using its special API.
    nsCOMPtr<nsIXULDocument> xuldoc(do_QueryInterface(doc));
    if (xuldoc)
      xuldoc->AddSubtreeToDocument(child);
#endif
  }
}

void
nsXBLBinding::UninstallAnonymousContent(nsIDocument* aDocument,
                                        nsIContent* aAnonParent)
{
  nsAutoScriptBlocker scriptBlocker;
  // Hold a strong ref while doing this, just in case.
  nsCOMPtr<nsIContent> anonParent = aAnonParent;
#ifdef MOZ_XUL
  nsCOMPtr<nsIXULDocument> xuldoc =
    do_QueryInterface(aDocument);
#endif
  for (nsIContent* child = aAnonParent->GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    child->UnbindFromTree();
#ifdef MOZ_XUL
    if (xuldoc) {
      xuldoc->RemoveSubtreeFromDocument(child);
    }
#endif
  }
}

void
nsXBLBinding::SetBoundElement(nsIContent* aElement)
{
  mBoundElement = aElement;
  if (mNextBinding)
    mNextBinding->SetBoundElement(aElement);
}

bool
nsXBLBinding::HasStyleSheets() const
{
  // Find out if we need to re-resolve style.  We'll need to do this
  // if we have additional stylesheets in our binding document.
  if (mPrototypeBinding->HasStyleSheets())
    return true;

  return mNextBinding ? mNextBinding->HasStyleSheets() : false;
}

void
nsXBLBinding::GenerateAnonymousContent()
{
  NS_ASSERTION(!nsContentUtils::IsSafeToRunScript(),
               "Someone forgot a script blocker");

  // Fetch the content element for this binding.
  nsIContent* content =
    mPrototypeBinding->GetImmediateChild(nsGkAtoms::content);

  if (!content) {
    // We have no anonymous content.
    if (mNextBinding)
      mNextBinding->GenerateAnonymousContent();

    return;
  }
     
  // Find out if we're really building kids or if we're just
  // using the attribute-setting shorthand hack.
  uint32_t contentCount = content->GetChildCount();

  // Plan to build the content by default.
  bool hasContent = (contentCount > 0);
  if (hasContent) {
    nsIDocument* doc = mBoundElement->OwnerDoc();

    nsCOMPtr<nsINode> clonedNode;
    nsCOMArray<nsINode> nodesWithProperties;
    nsNodeUtils::Clone(content, true, doc->NodeInfoManager(),
                       nodesWithProperties, getter_AddRefs(clonedNode));
    mContent = clonedNode->AsElement();

    // Search for <xbl:children> elements in the XBL content. In the presence
    // of multiple default insertion points, we use the last one in document
    // order.
    for (nsIContent* child = mContent; child; child = child->GetNextNode(mContent)) {
      if (child->NodeInfo()->Equals(nsGkAtoms::children, kNameSpaceID_XBL)) {
        XBLChildrenElement* point = static_cast<XBLChildrenElement*>(child);
        if (point->IsDefaultInsertion()) {
          mDefaultInsertionPoint = point;
        } else {
          mInsertionPoints.AppendElement(point);
        }
      }
    }

    // Do this after looking for <children> as this messes up the parent
    // pointer which would make the GetNextNode call above fail
    InstallAnonymousContent(mContent, mBoundElement,
                            mPrototypeBinding->ChromeOnlyContent());

    // Insert explicit children into insertion points
    if (mDefaultInsertionPoint && mInsertionPoints.IsEmpty()) {
      ExplicitChildIterator iter(mBoundElement);
      for (nsIContent* child = iter.GetNextChild(); child; child = iter.GetNextChild()) {
        mDefaultInsertionPoint->AppendInsertedChild(child);
      }
    } else {
      // It is odd to come into this code if mInsertionPoints is not empty, but
      // we need to make sure to do the compatibility hack below if the bound
      // node has any non <xul:template> or <xul:observes> children.
      ExplicitChildIterator iter(mBoundElement);
      for (nsIContent* child = iter.GetNextChild(); child; child = iter.GetNextChild()) {
        XBLChildrenElement* point = FindInsertionPointForInternal(child);
        if (point) {
          point->AppendInsertedChild(child);
        } else {
          nsINodeInfo *ni = child->NodeInfo();
          if (ni->NamespaceID() != kNameSpaceID_XUL ||
              (!ni->Equals(nsGkAtoms::_template) &&
               !ni->Equals(nsGkAtoms::observes))) {
            // Compatibility hack. For some reason the original XBL
            // implementation dropped the content of a binding if any child of
            // the bound element didn't match any of the <children> in the
            // binding. This became a pseudo-API that we have to maintain.

            // Undo InstallAnonymousContent
            UninstallAnonymousContent(doc, mContent);

            // Clear out our children elements to avoid dangling references.
            ClearInsertionPoints();

            // Pretend as though there was no content in the binding.
            mContent = nullptr;
            return;
          }
        }
      }
    }

    // Set binding parent on default content if need
    if (mDefaultInsertionPoint) {
      mDefaultInsertionPoint->MaybeSetupDefaultContent();
    }
    for (uint32_t i = 0; i < mInsertionPoints.Length(); ++i) {
      mInsertionPoints[i]->MaybeSetupDefaultContent();
    }

    mPrototypeBinding->SetInitialAttributes(mBoundElement, mContent);
  }

  // Always check the content element for potential attributes.
  // This shorthand hack always happens, even when we didn't
  // build anonymous content.
  const nsAttrName* attrName;
  for (uint32_t i = 0; (attrName = content->GetAttrNameAt(i)); ++i) {
    int32_t namespaceID = attrName->NamespaceID();
    // Hold a strong reference here so that the atom doesn't go away during
    // UnsetAttr.
    nsCOMPtr<nsIAtom> name = attrName->LocalName();

    if (name != nsGkAtoms::includes) {
      if (!nsContentUtils::HasNonEmptyAttr(mBoundElement, namespaceID, name)) {
        nsAutoString value2;
        content->GetAttr(namespaceID, name, value2);
        mBoundElement->SetAttr(namespaceID, name, attrName->GetPrefix(),
                               value2, false);
      }
    }

    // Conserve space by wiping the attributes off the clone.
    if (mContent)
      mContent->UnsetAttr(namespaceID, name, false);
  }
}

XBLChildrenElement*
nsXBLBinding::FindInsertionPointFor(nsIContent* aChild)
{
  // XXX We should get rid of this function as it causes us to traverse the
  // binding chain multiple times
  if (mContent) {
    return FindInsertionPointForInternal(aChild);
  }
  
  return mNextBinding ? mNextBinding->FindInsertionPointFor(aChild)
                      : nullptr;
}

XBLChildrenElement*
nsXBLBinding::FindInsertionPointForInternal(nsIContent* aChild)
{
  for (uint32_t i = 0; i < mInsertionPoints.Length(); ++i) {
    XBLChildrenElement* point = mInsertionPoints[i];
    if (point->Includes(aChild)) {
      return point;
    }
  }
  
  return mDefaultInsertionPoint;
}

void
nsXBLBinding::ClearInsertionPoints()
{
  if (mDefaultInsertionPoint) {
    mDefaultInsertionPoint->ClearInsertedChildren();
  }

  for (uint32_t i = 0; i < mInsertionPoints.Length(); ++i) {
    mInsertionPoints[i]->ClearInsertedChildren();
  }
}

nsAnonymousContentList*
nsXBLBinding::GetAnonymousNodeList()
{
  if (!mContent) {
    return mNextBinding ? mNextBinding->GetAnonymousNodeList() : nullptr;
  }

  if (!mAnonymousContentList) {
    mAnonymousContentList = new nsAnonymousContentList(mContent);
  }

  return mAnonymousContentList;
}

void
nsXBLBinding::InstallEventHandlers()
{
  // Don't install handlers if scripts aren't allowed.
  if (AllowScripts()) {
    // Fetch the handlers prototypes for this binding.
    nsXBLPrototypeHandler* handlerChain = mPrototypeBinding->GetPrototypeHandlers();

    if (handlerChain) {
      nsEventListenerManager* manager =
        mBoundElement->GetListenerManager(true);
      if (!manager)
        return;

      bool isChromeDoc =
        nsContentUtils::IsChromeDoc(mBoundElement->OwnerDoc());
      bool isChromeBinding = mPrototypeBinding->IsChrome();
      nsXBLPrototypeHandler* curr;
      for (curr = handlerChain; curr; curr = curr->GetNextHandler()) {
        // Fetch the event type.
        nsCOMPtr<nsIAtom> eventAtom = curr->GetEventName();
        if (!eventAtom ||
            eventAtom == nsGkAtoms::keyup ||
            eventAtom == nsGkAtoms::keydown ||
            eventAtom == nsGkAtoms::keypress)
          continue;

        nsXBLEventHandler* handler = curr->GetEventHandler();
        if (handler) {
          // Figure out if we're using capturing or not.
          dom::EventListenerFlags flags;
          flags.mCapture = (curr->GetPhase() == NS_PHASE_CAPTURING);

          // If this is a command, add it in the system event group
          if ((curr->GetType() & (NS_HANDLER_TYPE_XBL_COMMAND |
                                  NS_HANDLER_TYPE_SYSTEM)) &&
              (isChromeBinding || mBoundElement->IsInNativeAnonymousSubtree())) {
            flags.mInSystemGroup = true;
          }

          bool hasAllowUntrustedAttr = curr->HasAllowUntrustedAttr();
          if ((hasAllowUntrustedAttr && curr->AllowUntrustedEvents()) ||
              (!hasAllowUntrustedAttr && !isChromeDoc)) {
            flags.mAllowUntrustedEvents = true;
          }

          manager->AddEventListenerByType(handler,
                                          nsDependentAtomString(eventAtom),
                                          flags);
        }
      }

      const nsCOMArray<nsXBLKeyEventHandler>* keyHandlers =
        mPrototypeBinding->GetKeyEventHandlers();
      int32_t i;
      for (i = 0; i < keyHandlers->Count(); ++i) {
        nsXBLKeyEventHandler* handler = keyHandlers->ObjectAt(i);
        handler->SetIsBoundToChrome(isChromeDoc);

        nsAutoString type;
        handler->GetEventName(type);

        // If this is a command, add it in the system event group, otherwise 
        // add it to the standard event group.

        // Figure out if we're using capturing or not.
        dom::EventListenerFlags flags;
        flags.mCapture = (handler->GetPhase() == NS_PHASE_CAPTURING);

        if ((handler->GetType() & (NS_HANDLER_TYPE_XBL_COMMAND |
                                   NS_HANDLER_TYPE_SYSTEM)) &&
            (isChromeBinding || mBoundElement->IsInNativeAnonymousSubtree())) {
          flags.mInSystemGroup = true;
        }

        // For key handlers we have to set mAllowUntrustedEvents flag.
        // Whether the handling of the event is allowed or not is handled in
        // nsXBLKeyEventHandler::HandleEvent
        flags.mAllowUntrustedEvents = true;

        manager->AddEventListenerByType(handler, type, flags);
      }
    }
  }

  if (mNextBinding)
    mNextBinding->InstallEventHandlers();
}

nsresult
nsXBLBinding::InstallImplementation()
{
  // Always install the base class properties first, so that
  // derived classes can reference the base class properties.

  if (mNextBinding) {
    nsresult rv = mNextBinding->InstallImplementation();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // iterate through each property in the prototype's list and install the property.
  if (AllowScripts())
    return mPrototypeBinding->InstallImplementation(this);

  return NS_OK;
}

nsIAtom*
nsXBLBinding::GetBaseTag(int32_t* aNameSpaceID)
{
  nsIAtom *tag = mPrototypeBinding->GetBaseTag(aNameSpaceID);
  if (!tag && mNextBinding)
    return mNextBinding->GetBaseTag(aNameSpaceID);

  return tag;
}

void
nsXBLBinding::AttributeChanged(nsIAtom* aAttribute, int32_t aNameSpaceID,
                               bool aRemoveFlag, bool aNotify)
{
  // XXX Change if we ever allow multiple bindings in a chain to contribute anonymous content
  if (!mContent) {
    if (mNextBinding)
      mNextBinding->AttributeChanged(aAttribute, aNameSpaceID,
                                     aRemoveFlag, aNotify);
  } else {
    mPrototypeBinding->AttributeChanged(aAttribute, aNameSpaceID, aRemoveFlag,
                                        mBoundElement, mContent, aNotify);
  }
}

void
nsXBLBinding::ExecuteAttachedHandler()
{
  if (mNextBinding)
    mNextBinding->ExecuteAttachedHandler();

  if (AllowScripts())
    mPrototypeBinding->BindingAttached(mBoundElement);
}

void
nsXBLBinding::ExecuteDetachedHandler()
{
  if (AllowScripts())
    mPrototypeBinding->BindingDetached(mBoundElement);

  if (mNextBinding)
    mNextBinding->ExecuteDetachedHandler();
}

void
nsXBLBinding::UnhookEventHandlers()
{
  nsXBLPrototypeHandler* handlerChain = mPrototypeBinding->GetPrototypeHandlers();

  if (handlerChain) {
    nsEventListenerManager* manager =
      mBoundElement->GetListenerManager(false);
    if (!manager) {
      return;
    }
                                      
    bool isChromeBinding = mPrototypeBinding->IsChrome();
    nsXBLPrototypeHandler* curr;
    for (curr = handlerChain; curr; curr = curr->GetNextHandler()) {
      nsXBLEventHandler* handler = curr->GetCachedEventHandler();
      if (!handler) {
        continue;
      }
      
      nsCOMPtr<nsIAtom> eventAtom = curr->GetEventName();
      if (!eventAtom ||
          eventAtom == nsGkAtoms::keyup ||
          eventAtom == nsGkAtoms::keydown ||
          eventAtom == nsGkAtoms::keypress)
        continue;

      // Figure out if we're using capturing or not.
      dom::EventListenerFlags flags;
      flags.mCapture = (curr->GetPhase() == NS_PHASE_CAPTURING);

      // If this is a command, remove it from the system event group,
      // otherwise remove it from the standard event group.

      if ((curr->GetType() & (NS_HANDLER_TYPE_XBL_COMMAND |
                              NS_HANDLER_TYPE_SYSTEM)) &&
          (isChromeBinding || mBoundElement->IsInNativeAnonymousSubtree())) {
        flags.mInSystemGroup = true;
      }

      manager->RemoveEventListenerByType(handler,
                                         nsDependentAtomString(eventAtom),
                                         flags);
    }

    const nsCOMArray<nsXBLKeyEventHandler>* keyHandlers =
      mPrototypeBinding->GetKeyEventHandlers();
    int32_t i;
    for (i = 0; i < keyHandlers->Count(); ++i) {
      nsXBLKeyEventHandler* handler = keyHandlers->ObjectAt(i);

      nsAutoString type;
      handler->GetEventName(type);

      // Figure out if we're using capturing or not.
      dom::EventListenerFlags flags;
      flags.mCapture = (handler->GetPhase() == NS_PHASE_CAPTURING);

      // If this is a command, remove it from the system event group, otherwise 
      // remove it from the standard event group.

      if ((handler->GetType() & (NS_HANDLER_TYPE_XBL_COMMAND | NS_HANDLER_TYPE_SYSTEM)) &&
          (isChromeBinding || mBoundElement->IsInNativeAnonymousSubtree())) {
        flags.mInSystemGroup = true;
      }

      manager->RemoveEventListenerByType(handler, type, flags);
    }
  }
}

static void
UpdateInsertionParent(XBLChildrenElement* aPoint,
                      nsIContent* aOldBoundElement)
{
  if (aPoint->IsDefaultInsertion()) {
    return;
  }

  for (size_t i = 0; i < aPoint->InsertedChildrenLength(); ++i) {
    nsIContent* child = aPoint->mInsertedChildren[i];

    MOZ_ASSERT(child->GetParentNode());

    // Here, we're iterating children that we inserted. There are two cases:
    // either |child| is an explicit child of |aOldBoundElement| and is no
    // longer inserted anywhere or it's a child of a <children> element
    // parented to |aOldBoundElement|. In the former case, the child is no
    // longer inserted anywhere, so we set its insertion parent to null. In the
    // latter case, the child is now inserted into |aOldBoundElement| from some
    // binding above us, so we set its insertion parent to aOldBoundElement.
    if (child->GetParentNode() == aOldBoundElement) {
      child->SetXBLInsertionParent(nullptr);
    } else {
      child->SetXBLInsertionParent(aOldBoundElement);
    }
  }
}

void
nsXBLBinding::ChangeDocument(nsIDocument* aOldDocument, nsIDocument* aNewDocument)
{
  if (aOldDocument == aNewDocument)
    return;

  // Now the binding dies.  Unhook our prototypes.
  if (mPrototypeBinding->HasImplementation()) {
    nsCOMPtr<nsIScriptGlobalObject> global =  do_QueryInterface(
                                                                aOldDocument->GetScopeObject());
    if (global) {
      nsCOMPtr<nsIScriptContext> context = global->GetContext();
      if (context) {
        JSContext *cx = context->GetNativeContext();

        nsCxPusher pusher;
        pusher.Push(cx);

        // scope might be null if we've cycle-collected the global
        // object, since the Unlink phase of cycle collection happens
        // after JS GC finalization.  But in that case, we don't care
        // about fixing the prototype chain, since everything's going
        // away immediately.
        JS::Rooted<JSObject*> scope(cx, global->GetGlobalJSObject());
        JS::Rooted<JSObject*> scriptObject(cx, mBoundElement->GetWrapper());
        if (scope && scriptObject) {
          // XXX Stay in sync! What if a layered binding has an
          // <interface>?!
          // XXXbz what does that comment mean, really?  It seems to date
          // back to when there was such a thing as an <interface>, whever
          // that was...

          // Find the right prototype.
          JSAutoCompartment ac(cx, scriptObject);

          JS::Rooted<JSObject*> base(cx, scriptObject);
          JS::Rooted<JSObject*> proto(cx);
          for ( ; true; base = proto) { // Will break out on null proto
            if (!JS_GetPrototype(cx, base, &proto)) {
              return;
            }
            if (!proto) {
              break;
            }

            JSClass* clazz = ::JS_GetClass(proto);
            if (!clazz ||
                (~clazz->flags &
                 (JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS)) ||
                JSCLASS_RESERVED_SLOTS(clazz) != 1 ||
                clazz->finalize != XBLFinalize) {
              // Clearly not the right class
              continue;
            }

            nsRefPtr<nsXBLDocumentInfo> docInfo =
              static_cast<nsXBLDocumentInfo*>(::JS_GetPrivate(proto));
            if (!docInfo) {
              // Not the proto we seek
              continue;
            }

            JS::Value protoBinding = ::JS_GetReservedSlot(proto, 0);

            if (JSVAL_TO_PRIVATE(protoBinding) != mPrototypeBinding) {
              // Not the right binding
              continue;
            }

            // Alright!  This is the right prototype.  Pull it out of the
            // proto chain.
            JS::Rooted<JSObject*> grandProto(cx);
            if (!JS_GetPrototype(cx, proto, &grandProto)) {
              return;
            }
            ::JS_SetPrototype(cx, base, grandProto);
            break;
          }

          mPrototypeBinding->UndefineFields(cx, scriptObject);

          // Don't remove the reference from the document to the
          // wrapper here since it'll be removed by the element
          // itself when that's taken out of the document.
        }
      }
    }
  }

  // Remove our event handlers
  UnhookEventHandlers();

  {
    nsAutoScriptBlocker scriptBlocker;

    // Then do our ancestors.  This reverses the construction order, so that at
    // all times things are consistent as far as everyone is concerned.
    if (mNextBinding) {
      mNextBinding->ChangeDocument(aOldDocument, aNewDocument);
    }

    // Update the anonymous content.
    // XXXbz why not only for style bindings?
    if (mContent) {
      nsXBLBinding::UninstallAnonymousContent(aOldDocument, mContent);
    }

    // Now that we've unbound our anonymous content from the tree and updated
    // its binding parent, update the insertion parent for content inserted
    // into our <children> elements.
    if (mDefaultInsertionPoint) {
      UpdateInsertionParent(mDefaultInsertionPoint, mBoundElement);
    }

    for (size_t i = 0; i < mInsertionPoints.Length(); ++i) {
      UpdateInsertionParent(mInsertionPoints[i], mBoundElement);
    }

    // Now that our inserted children no longer think they're inserted
    // anywhere, make sure our internal state reflects that as well.
    ClearInsertionPoints();
  }
}

bool
nsXBLBinding::InheritsStyle() const
{
  // XXX Will have to change if we ever allow multiple bindings to contribute anonymous content.
  // Most derived binding with anonymous content determines style inheritance for now.

  // XXX What about bindings with <content> but no kids, e.g., my treecell-text binding?
  if (mContent)
    return mPrototypeBinding->InheritsStyle();
  
  if (mNextBinding)
    return mNextBinding->InheritsStyle();

  return true;
}

void
nsXBLBinding::WalkRules(nsIStyleRuleProcessor::EnumFunc aFunc, void* aData)
{
  if (mNextBinding)
    mNextBinding->WalkRules(aFunc, aData);

  nsIStyleRuleProcessor *rules = mPrototypeBinding->GetRuleProcessor();
  if (rules)
    (*aFunc)(rules, aData);
}

// Internal helper methods ////////////////////////////////////////////////////////////////

// static
nsresult
nsXBLBinding::DoInitJSClass(JSContext *cx, JS::Handle<JSObject*> global,
                            JS::Handle<JSObject*> obj,
                            const nsAFlatCString& aClassName,
                            nsXBLPrototypeBinding* aProtoBinding,
                            JS::MutableHandle<JSObject*> aClassObject,
                            bool* aNew)
{
  // First ensure our JS class is initialized.
  nsAutoCString className(aClassName);
  nsAutoCString xblKey(aClassName);

  JSAutoCompartment ac(cx, global);

  JS::Rooted<JSObject*> parent_proto(cx, nullptr);
  nsXBLJSClass* c = nullptr;
  if (obj) {
    // Retrieve the current prototype of obj.
    if (!JS_GetPrototype(cx, obj, &parent_proto)) {
      return NS_ERROR_FAILURE;
    }
    if (parent_proto) {
      // We need to create a unique classname based on aClassName and
      // id.  Append a space (an invalid URI character) to ensure that
      // we don't have accidental collisions with the case when parent_proto is
      // null and aClassName ends in some bizarre numbers (yeah, it's unlikely).
      JS::Rooted<jsid> parent_proto_id(cx);
      if (!::JS_GetObjectId(cx, parent_proto, parent_proto_id.address())) {
        // Probably OOM
        return NS_ERROR_OUT_OF_MEMORY;
      }

      // One space, maybe "0x", at most 16 chars (on a 64-bit system) of long,
      // and a null-terminator (which PR_snprintf ensures is there even if the
      // string representation of what we're printing does not fit in the buffer
      // provided).
      char buf[20];
      if (sizeof(jsid) == 4) {
        PR_snprintf(buf, sizeof(buf), " %lx", parent_proto_id.get());
      } else {
        MOZ_ASSERT(sizeof(jsid) == 8);
        PR_snprintf(buf, sizeof(buf), " %llx", parent_proto_id.get());
      }
      xblKey.Append(buf);
      nsCStringKey key(xblKey);

      c = static_cast<nsXBLJSClass*>(nsXBLService::gClassTable->Get(&key));
      if (c) {
        className.Assign(c->name);
      } else {
        char buf[20];
        PR_snprintf(buf, sizeof(buf), " %llx", nsXBLJSClass::NewId());
        className.Append(buf);
      }
    }
  }

  JS::Rooted<JSObject*> proto(cx);
  JS::Rooted<JS::Value> val(cx);

  if (!::JS_LookupPropertyWithFlags(cx, global, className.get(), 0, val.address()))
    return NS_ERROR_OUT_OF_MEMORY;

  if (val.isObject()) {
    *aNew = false;
    proto = &val.toObject();
  } else {
    // We need to initialize the class.
    *aNew = true;

    nsCStringKey key(xblKey);
    if (!c) {
      c = static_cast<nsXBLJSClass*>(nsXBLService::gClassTable->Get(&key));
    }
    if (c) {
      // If c is on the LRU list (i.e., not linked to itself), remove it now!
      JSCList* link = static_cast<JSCList*>(c);
      if (c->next != link) {
        JS_REMOVE_AND_INIT_LINK(link);
        nsXBLService::gClassLRUListLength--;
      }
    } else {
      if (JS_CLIST_IS_EMPTY(&nsXBLService::gClassLRUList)) {
        // We need to create a struct for this class.
        c = new nsXBLJSClass(className, xblKey);

        if (!c)
          return NS_ERROR_OUT_OF_MEMORY;
      } else {
        // Pull the least recently used class struct off the list.
        JSCList* lru = (nsXBLService::gClassLRUList).next;
        JS_REMOVE_AND_INIT_LINK(lru);
        nsXBLService::gClassLRUListLength--;

        // Remove any mapping from the old name to the class struct.
        c = static_cast<nsXBLJSClass*>(lru);
        nsCStringKey oldKey(c->Key());
        (nsXBLService::gClassTable)->Remove(&oldKey);

        // Change the class name and we're done.
        nsMemory::Free((void*) c->name);
        c->name = ToNewCString(className);
        c->SetKey(xblKey);
      }

      // Add c to our table.
      (nsXBLService::gClassTable)->Put(&key, (void*)c);
    }

    // The prototype holds a strong reference to its class struct.
    c->Hold();

    // Make a new object prototyped by parent_proto and parented by global.
    proto = ::JS_InitClass(cx,                  // context
                           global,              // global object
                           parent_proto,        // parent proto 
                           c,                   // JSClass
                           nullptr,              // JSNative ctor
                           0,                   // ctor args
                           nullptr,              // proto props
                           nullptr,              // proto funcs
                           nullptr,              // ctor props (static)
                           nullptr);             // ctor funcs (static)
    if (!proto) {
      // This will happen if we're OOM or if the security manager
      // denies defining the new class...

      (nsXBLService::gClassTable)->Remove(&key);

      c->Drop();

      return NS_ERROR_OUT_OF_MEMORY;
    }

    // Keep this proto binding alive while we're alive.  Do this first so that
    // we can guarantee that in XBLFinalize this will be non-null.
    // Note that we can't just store aProtoBinding in the private and
    // addref/release the nsXBLDocumentInfo through it, because cycle
    // collection doesn't seem to work right if the private is not an
    // nsISupports.
    nsXBLDocumentInfo* docInfo = aProtoBinding->XBLDocumentInfo();
    ::JS_SetPrivate(proto, docInfo);
    NS_ADDREF(docInfo);

    ::JS_SetReservedSlot(proto, 0, PRIVATE_TO_JSVAL(aProtoBinding));
  }

  aClassObject.set(proto);

  if (obj) {
    // Set the prototype of our object to be the new class.
    if (!::JS_SetPrototype(cx, obj, proto)) {
      return NS_ERROR_FAILURE;
    }
  }

  return NS_OK;
}

bool
nsXBLBinding::AllowScripts()
{
  if (!mPrototypeBinding->GetAllowScripts())
    return false;

  // Nasty hack.  Use the JSContext of the bound node, since the
  // security manager API expects to get the docshell type from
  // that.  But use the nsIPrincipal of our document.
  nsIScriptSecurityManager* mgr = nsContentUtils::GetSecurityManager();
  if (!mgr) {
    return false;
  }

  nsIDocument* doc = mBoundElement ? mBoundElement->OwnerDoc() : nullptr;
  if (!doc) {
    return false;
  }

  nsCOMPtr<nsIScriptGlobalObject> global = do_QueryInterface(doc->GetWindow());
  if (!global) {
    return false;
  }

  nsCOMPtr<nsIScriptContext> context = global->GetContext();
  if (!context) {
    return false;
  }
  
  AutoPushJSContext cx(context->GetNativeContext());

  nsCOMPtr<nsIDocument> ourDocument =
    mPrototypeBinding->XBLDocumentInfo()->GetDocument();
  bool canExecute;
  nsresult rv =
    mgr->CanExecuteScripts(cx, ourDocument->NodePrincipal(), &canExecute);
  return NS_SUCCEEDED(rv) && canExecute;
}

nsXBLBinding*
nsXBLBinding::RootBinding()
{
  if (mNextBinding)
    return mNextBinding->RootBinding();

  return this;
}

bool
nsXBLBinding::ResolveAllFields(JSContext *cx, JS::Handle<JSObject*> obj) const
{
  if (!mPrototypeBinding->ResolveAllFields(cx, obj)) {
    return false;
  }

  if (mNextBinding) {
    return mNextBinding->ResolveAllFields(cx, obj);
  }

  return true;
}

bool
nsXBLBinding::LookupMember(JSContext* aCx, JS::HandleId aId,
                           JSPropertyDescriptor* aDesc)
{
  // We should never enter this function with a pre-filled property descriptor.
  MOZ_ASSERT(!aDesc->obj);

  // Get the string as an nsString before doing anything, so we can make
  // convenient comparisons during our search.
  if (!JSID_IS_STRING(aId)) {
    return true;
  }
  nsDependentJSString name(aId);

  // We have a weak reference to our bound element, so make sure it's alive.
  if (!mBoundElement || !mBoundElement->GetWrapper()) {
    return false;
  }

  // Get the scope of mBoundElement and the associated XBL scope. We should only
  // be calling into this machinery if we're running in a separate XBL scope.
  JS::Rooted<JSObject*> boundScope(aCx,
    js::GetGlobalForObjectCrossCompartment(mBoundElement->GetWrapper()));
  JS::Rooted<JSObject*> xblScope(aCx, xpc::GetXBLScope(aCx, boundScope));
  NS_ENSURE_TRUE(xblScope, false);
  MOZ_ASSERT(boundScope != xblScope);

  // Enter the xbl scope and invoke the internal version.
  {
    JSAutoCompartment ac(aCx, xblScope);
    JS::RootedId id(aCx, aId);
    if (!JS_WrapId(aCx, id.address()) ||
        !LookupMemberInternal(aCx, name, id, aDesc, xblScope))
    {
      return false;
    }
  }

  // Wrap into the caller's scope.
  return JS_WrapPropertyDescriptor(aCx, aDesc);
}

bool
nsXBLBinding::LookupMemberInternal(JSContext* aCx, nsString& aName,
                                   JS::HandleId aNameAsId,
                                   JSPropertyDescriptor* aDesc,
                                   JS::Handle<JSObject*> aXBLScope)
{
  // First, see if we have a JSClass. If we don't, it means that this binding
  // doesn't have a class object, and thus doesn't have any members. Skip it.
  if (!mJSClass) {
    if (!mNextBinding) {
      return true;
    }
    return mNextBinding->LookupMemberInternal(aCx, aName, aNameAsId,
                                              aDesc, aXBLScope);
  }

  // Find our class object. It's in a protected scope and permanent just in case,
  // so should be there no matter what.
  JS::RootedValue classObject(aCx);
  if (!JS_GetProperty(aCx, aXBLScope, mJSClass->name, &classObject)) {
    return false;
  }

  // The bound element may have been adoped by a document and have a different
  // wrapper (and different xbl scope) than when the binding was applied, in
  // this case getting the class object will fail. Behave as if the class
  // object did not exist.
  if (classObject.isUndefined()) {
    return true;
  }

  MOZ_ASSERT(classObject.isObject());

  // Look for the property on this binding. If it's not there, try the next
  // binding on the chain.
  nsXBLProtoImpl* impl = mPrototypeBinding->GetImplementation();
  if (impl && !impl->LookupMember(aCx, aName, aNameAsId, aDesc,
                                  &classObject.toObject()))
  {
    return false;
  }
  if (aDesc->obj || !mNextBinding) {
    return true;
  }

  return mNextBinding->LookupMemberInternal(aCx, aName, aNameAsId, aDesc,
                                            aXBLScope);
}

bool
nsXBLBinding::HasField(nsString& aName)
{
  // See if this binding has such a field.
  return mPrototypeBinding->FindField(aName) ||
    (mNextBinding && mNextBinding->HasField(aName));
}

void
nsXBLBinding::MarkForDeath()
{
  mMarkedForDeath = true;
  ExecuteDetachedHandler();
}

bool
nsXBLBinding::ImplementsInterface(REFNSIID aIID) const
{
  return mPrototypeBinding->ImplementsInterface(aIID) ||
    (mNextBinding && mNextBinding->ImplementsInterface(aIID));
}
