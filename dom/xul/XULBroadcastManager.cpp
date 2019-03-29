/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=4 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XULBroadcastManager.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/Logging.h"
#include "mozilla/dom/DocumentInlines.h"
#include "nsXULElement.h"

struct BroadcastListener {
  nsWeakPtr mListener;
  RefPtr<nsAtom> mAttribute;
};

struct BroadcasterMapEntry : public PLDHashEntryHdr {
  Element* mBroadcaster;  // [WEAK]
  nsTArray<BroadcastListener*>
      mListeners;  // [OWNING] of BroadcastListener objects
};

struct nsAttrNameInfo {
  nsAttrNameInfo(int32_t aNamespaceID, nsAtom* aName, nsAtom* aPrefix)
      : mNamespaceID(aNamespaceID), mName(aName), mPrefix(aPrefix) {}
  nsAttrNameInfo(const nsAttrNameInfo& aOther)
      : mNamespaceID(aOther.mNamespaceID),
        mName(aOther.mName),
        mPrefix(aOther.mPrefix) {}
  int32_t mNamespaceID;
  RefPtr<nsAtom> mName;
  RefPtr<nsAtom> mPrefix;
};

static void ClearBroadcasterMapEntry(PLDHashTable* aTable,
                                     PLDHashEntryHdr* aEntry) {
  BroadcasterMapEntry* entry = static_cast<BroadcasterMapEntry*>(aEntry);
  for (size_t i = entry->mListeners.Length() - 1; i != (size_t)-1; --i) {
    delete entry->mListeners[i];
  }
  entry->mListeners.Clear();

  // N.B. that we need to manually run the dtor because we
  // constructed the nsTArray object in-place.
  entry->mListeners.~nsTArray<BroadcastListener*>();
}

static bool CanBroadcast(int32_t aNameSpaceID, nsAtom* aAttribute) {
  // Don't push changes to the |id|, |persist|, |command| or
  // |observes| attribute.
  if (aNameSpaceID == kNameSpaceID_None) {
    if ((aAttribute == nsGkAtoms::id) || (aAttribute == nsGkAtoms::persist) ||
        (aAttribute == nsGkAtoms::command) ||
        (aAttribute == nsGkAtoms::observes)) {
      return false;
    }
  }
  return true;
}

namespace mozilla {
namespace dom {
static LazyLogModule sXULBroadCastManager("XULBroadcastManager");

/* static */
bool XULBroadcastManager::MayNeedListener(const Element& aElement) {
  if (aElement.NodeInfo()->Equals(nsGkAtoms::observes, kNameSpaceID_XUL)) {
    return true;
  }
  if (aElement.HasAttr(nsGkAtoms::observes)) {
    return true;
  }
  if (aElement.HasAttr(nsGkAtoms::command) &&
      !(aElement.NodeInfo()->Equals(nsGkAtoms::menuitem, kNameSpaceID_XUL) ||
        aElement.NodeInfo()->Equals(nsGkAtoms::key, kNameSpaceID_XUL))) {
    return true;
  }
  return false;
}

XULBroadcastManager::XULBroadcastManager(Document* aDocument)
    : mDocument(aDocument),
      mBroadcasterMap(nullptr),
      mHandlingDelayedAttrChange(false),
      mHandlingDelayedBroadcasters(false) {}

XULBroadcastManager::~XULBroadcastManager() { delete mBroadcasterMap; }

void XULBroadcastManager::DropDocumentReference(void) { mDocument = nullptr; }

void XULBroadcastManager::SynchronizeBroadcastListener(Element* aBroadcaster,
                                                       Element* aListener,
                                                       const nsAString& aAttr) {
  if (!nsContentUtils::IsSafeToRunScript()) {
    nsDelayedBroadcastUpdate delayedUpdate(aBroadcaster, aListener, aAttr);
    mDelayedBroadcasters.AppendElement(delayedUpdate);
    MaybeBroadcast();
    return;
  }
  bool notify = mHandlingDelayedBroadcasters;

  if (aAttr.EqualsLiteral("*")) {
    uint32_t count = aBroadcaster->GetAttrCount();
    nsTArray<nsAttrNameInfo> attributes(count);
    for (uint32_t i = 0; i < count; ++i) {
      const nsAttrName* attrName = aBroadcaster->GetAttrNameAt(i);
      int32_t nameSpaceID = attrName->NamespaceID();
      nsAtom* name = attrName->LocalName();

      // _Don't_ push the |id|, |ref|, or |persist| attribute's value!
      if (!CanBroadcast(nameSpaceID, name)) continue;

      attributes.AppendElement(
          nsAttrNameInfo(nameSpaceID, name, attrName->GetPrefix()));
    }

    count = attributes.Length();
    while (count-- > 0) {
      int32_t nameSpaceID = attributes[count].mNamespaceID;
      nsAtom* name = attributes[count].mName;
      nsAutoString value;
      if (aBroadcaster->GetAttr(nameSpaceID, name, value)) {
        aListener->SetAttr(nameSpaceID, name, attributes[count].mPrefix, value,
                           notify);
      }

#if 0
            // XXX we don't fire the |onbroadcast| handler during
            // initial hookup: doing so would potentially run the
            // |onbroadcast| handler before the |onload| handler,
            // which could define JS properties that mask XBL
            // properties, etc.
            ExecuteOnBroadcastHandlerFor(aBroadcaster, aListener, name);
#endif
    }
  } else {
    // Find out if the attribute is even present at all.
    RefPtr<nsAtom> name = NS_Atomize(aAttr);

    nsAutoString value;
    if (aBroadcaster->GetAttr(kNameSpaceID_None, name, value)) {
      aListener->SetAttr(kNameSpaceID_None, name, value, notify);
    } else {
      aListener->UnsetAttr(kNameSpaceID_None, name, notify);
    }

#if 0
        // XXX we don't fire the |onbroadcast| handler during initial
        // hookup: doing so would potentially run the |onbroadcast|
        // handler before the |onload| handler, which could define JS
        // properties that mask XBL properties, etc.
        ExecuteOnBroadcastHandlerFor(aBroadcaster, aListener, name);
#endif
  }
}

void XULBroadcastManager::AddListenerFor(Element& aBroadcaster,
                                         Element& aListener,
                                         const nsAString& aAttr,
                                         ErrorResult& aRv) {
  if (!mDocument) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsresult rv = nsContentUtils::CheckSameOrigin(mDocument, &aBroadcaster);

  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  rv = nsContentUtils::CheckSameOrigin(mDocument, &aListener);

  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  static const PLDHashTableOps gOps = {
      PLDHashTable::HashVoidPtrKeyStub, PLDHashTable::MatchEntryStub,
      PLDHashTable::MoveEntryStub, ClearBroadcasterMapEntry, nullptr};

  if (!mBroadcasterMap) {
    mBroadcasterMap = new PLDHashTable(&gOps, sizeof(BroadcasterMapEntry));
  }

  auto entry =
      static_cast<BroadcasterMapEntry*>(mBroadcasterMap->Search(&aBroadcaster));
  if (!entry) {
    entry = static_cast<BroadcasterMapEntry*>(
        mBroadcasterMap->Add(&aBroadcaster, fallible));

    if (!entry) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }

    entry->mBroadcaster = &aBroadcaster;

    // N.B. placement new to construct the nsTArray object in-place
    new (&entry->mListeners) nsTArray<BroadcastListener*>();
  }

  // Only add the listener if it's not there already!
  RefPtr<nsAtom> attr = NS_Atomize(aAttr);

  for (size_t i = entry->mListeners.Length() - 1; i != (size_t)-1; --i) {
    BroadcastListener* bl = entry->mListeners[i];
    nsCOMPtr<Element> blListener = do_QueryReferent(bl->mListener);

    if (blListener == &aListener && bl->mAttribute == attr) return;
  }

  BroadcastListener* bl = new BroadcastListener;
  bl->mListener = do_GetWeakReference(&aListener);
  bl->mAttribute = attr;

  entry->mListeners.AppendElement(bl);

  SynchronizeBroadcastListener(&aBroadcaster, &aListener, aAttr);
}

void XULBroadcastManager::RemoveListenerFor(Element& aBroadcaster,
                                            Element& aListener,
                                            const nsAString& aAttr) {
  // If we haven't added any broadcast listeners, then there sure
  // aren't any to remove.
  if (!mBroadcasterMap) return;

  auto entry =
      static_cast<BroadcasterMapEntry*>(mBroadcasterMap->Search(&aBroadcaster));
  if (entry) {
    RefPtr<nsAtom> attr = NS_Atomize(aAttr);
    for (size_t i = entry->mListeners.Length() - 1; i != (size_t)-1; --i) {
      BroadcastListener* bl = entry->mListeners[i];
      nsCOMPtr<Element> blListener = do_QueryReferent(bl->mListener);

      if (blListener == &aListener && bl->mAttribute == attr) {
        entry->mListeners.RemoveElementAt(i);
        delete bl;

        if (entry->mListeners.IsEmpty()) mBroadcasterMap->RemoveEntry(entry);

        break;
      }
    }
  }
}

nsresult XULBroadcastManager::ExecuteOnBroadcastHandlerFor(
    Element* aBroadcaster, Element* aListener, nsAtom* aAttr) {
  if (!mDocument) {
    return NS_OK;
  }
  // Now we execute the onchange handler in the context of the
  // observer. We need to find the observer in order to
  // execute the handler.

  for (nsIContent* child = aListener->GetFirstChild(); child;
       child = child->GetNextSibling()) {
    // Look for an <observes> element beneath the listener. This
    // ought to have an |element| attribute that refers to
    // aBroadcaster, and an |attribute| element that tells us what
    // attriubtes we're listening for.
    if (!child->IsXULElement(nsGkAtoms::observes)) continue;

    // Is this the element that was listening to us?
    nsAutoString listeningToID;
    child->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::element,
                                listeningToID);

    nsAutoString broadcasterID;
    aBroadcaster->GetAttr(kNameSpaceID_None, nsGkAtoms::id, broadcasterID);

    if (listeningToID != broadcasterID) continue;

    // We are observing the broadcaster, but is this the right
    // attribute?
    nsAutoString listeningToAttribute;
    child->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::attribute,
                                listeningToAttribute);

    if (!aAttr->Equals(listeningToAttribute) &&
        !listeningToAttribute.EqualsLiteral("*")) {
      continue;
    }

    // This is the right <observes> element. Execute the
    // |onbroadcast| event handler
    WidgetEvent event(true, eXULBroadcast);

    RefPtr<nsPresContext> presContext = mDocument->GetPresContext();
    if (presContext) {
      // Handle the DOM event
      nsEventStatus status = nsEventStatus_eIgnore;
      EventDispatcher::Dispatch(child, presContext, &event, nullptr, &status);
    }
  }

  return NS_OK;
}

void XULBroadcastManager::AttributeChanged(Element* aElement,
                                           int32_t aNameSpaceID,
                                           nsAtom* aAttribute) {
  if (!mDocument) {
    return;
  }
  NS_ASSERTION(aElement->OwnerDoc() == mDocument, "unexpected doc");

  // Synchronize broadcast listeners
  if (mBroadcasterMap && CanBroadcast(aNameSpaceID, aAttribute)) {
    auto entry =
        static_cast<BroadcasterMapEntry*>(mBroadcasterMap->Search(aElement));

    if (entry) {
      // We've got listeners: push the value.
      nsAutoString value;
      bool attrSet = aElement->GetAttr(kNameSpaceID_None, aAttribute, value);

      for (size_t i = entry->mListeners.Length() - 1; i != (size_t)-1; --i) {
        BroadcastListener* bl = entry->mListeners[i];
        if ((bl->mAttribute == aAttribute) ||
            (bl->mAttribute == nsGkAtoms::_asterisk)) {
          nsCOMPtr<Element> listenerEl = do_QueryReferent(bl->mListener);
          if (listenerEl) {
            nsAutoString currentValue;
            bool hasAttr = listenerEl->GetAttr(kNameSpaceID_None, aAttribute,
                                               currentValue);
            // We need to update listener only if we're
            // (1) removing an existing attribute,
            // (2) adding a new attribute or
            // (3) changing the value of an attribute.
            bool needsAttrChange =
                attrSet != hasAttr || !value.Equals(currentValue);
            nsDelayedBroadcastUpdate delayedUpdate(aElement, listenerEl,
                                                   aAttribute, value, attrSet,
                                                   needsAttrChange);

            size_t index = mDelayedAttrChangeBroadcasts.IndexOf(
                delayedUpdate, 0, nsDelayedBroadcastUpdate::Comparator());
            if (index != mDelayedAttrChangeBroadcasts.NoIndex) {
              if (mHandlingDelayedAttrChange) {
                NS_WARNING("Broadcasting loop!");
                continue;
              }
              mDelayedAttrChangeBroadcasts.RemoveElementAt(index);
            }

            mDelayedAttrChangeBroadcasts.AppendElement(delayedUpdate);
          }
        }
      }
    }
  }
}

void XULBroadcastManager::MaybeBroadcast() {
  // Only broadcast when not in an update and when safe to run scripts.
  if (mDocument && mDocument->UpdateNestingLevel() == 0 &&
      (mDelayedAttrChangeBroadcasts.Length() ||
       mDelayedBroadcasters.Length())) {
    if (!nsContentUtils::IsSafeToRunScript()) {
      if (mDocument) {
        nsContentUtils::AddScriptRunner(
            NewRunnableMethod("dom::XULBroadcastManager::MaybeBroadcast", this,
                              &XULBroadcastManager::MaybeBroadcast));
      }
      return;
    }
    if (!mHandlingDelayedAttrChange) {
      mHandlingDelayedAttrChange = true;
      for (uint32_t i = 0; i < mDelayedAttrChangeBroadcasts.Length(); ++i) {
        nsAtom* attrName = mDelayedAttrChangeBroadcasts[i].mAttrName;
        if (mDelayedAttrChangeBroadcasts[i].mNeedsAttrChange) {
          nsCOMPtr<Element> listener =
              mDelayedAttrChangeBroadcasts[i].mListener;
          const nsString& value = mDelayedAttrChangeBroadcasts[i].mAttr;
          if (mDelayedAttrChangeBroadcasts[i].mSetAttr) {
            listener->SetAttr(kNameSpaceID_None, attrName, value, true);
          } else {
            listener->UnsetAttr(kNameSpaceID_None, attrName, true);
          }
        }
        ExecuteOnBroadcastHandlerFor(
            mDelayedAttrChangeBroadcasts[i].mBroadcaster,
            mDelayedAttrChangeBroadcasts[i].mListener, attrName);
      }
      mDelayedAttrChangeBroadcasts.Clear();
      mHandlingDelayedAttrChange = false;
    }

    uint32_t length = mDelayedBroadcasters.Length();
    if (length) {
      bool oldValue = mHandlingDelayedBroadcasters;
      mHandlingDelayedBroadcasters = true;
      nsTArray<nsDelayedBroadcastUpdate> delayedBroadcasters;
      mDelayedBroadcasters.SwapElements(delayedBroadcasters);
      for (uint32_t i = 0; i < length; ++i) {
        SynchronizeBroadcastListener(delayedBroadcasters[i].mBroadcaster,
                                     delayedBroadcasters[i].mListener,
                                     delayedBroadcasters[i].mAttr);
      }
      mHandlingDelayedBroadcasters = oldValue;
    }
  }
}

nsresult XULBroadcastManager::FindBroadcaster(Element* aElement,
                                              Element** aListener,
                                              nsString& aBroadcasterID,
                                              nsString& aAttribute,
                                              Element** aBroadcaster) {
  NodeInfo* ni = aElement->NodeInfo();
  *aListener = nullptr;
  *aBroadcaster = nullptr;

  if (ni->Equals(nsGkAtoms::observes, kNameSpaceID_XUL)) {
    // It's an <observes> element, which means that the actual
    // listener is the _parent_ node. This element should have an
    // 'element' attribute that specifies the ID of the
    // broadcaster element, and an 'attribute' element, which
    // specifies the name of the attribute to observe.
    nsIContent* parent = aElement->GetParent();
    if (!parent) {
      // <observes> is the root element
      return NS_FINDBROADCASTER_NOT_FOUND;
    }

    *aListener = Element::FromNode(parent);
    NS_IF_ADDREF(*aListener);

    aElement->GetAttr(kNameSpaceID_None, nsGkAtoms::element, aBroadcasterID);
    if (aBroadcasterID.IsEmpty()) {
      return NS_FINDBROADCASTER_NOT_FOUND;
    }
    aElement->GetAttr(kNameSpaceID_None, nsGkAtoms::attribute, aAttribute);
  } else {
    // It's a generic element, which means that we'll use the
    // value of the 'observes' attribute to determine the ID of
    // the broadcaster element, and we'll watch _all_ of its
    // values.
    aElement->GetAttr(kNameSpaceID_None, nsGkAtoms::observes, aBroadcasterID);

    // Bail if there's no aBroadcasterID
    if (aBroadcasterID.IsEmpty()) {
      // Try the command attribute next.
      aElement->GetAttr(kNameSpaceID_None, nsGkAtoms::command, aBroadcasterID);
      if (!aBroadcasterID.IsEmpty()) {
        // We've got something in the command attribute.  We
        // only treat this as a normal broadcaster if we are
        // not a menuitem or a key.

        if (ni->Equals(nsGkAtoms::menuitem, kNameSpaceID_XUL) ||
            ni->Equals(nsGkAtoms::key, kNameSpaceID_XUL)) {
          return NS_FINDBROADCASTER_NOT_FOUND;
        }
      } else {
        return NS_FINDBROADCASTER_NOT_FOUND;
      }
    }

    *aListener = aElement;
    NS_ADDREF(*aListener);

    aAttribute.Assign('*');
  }

  // Make sure we got a valid listener.
  NS_ENSURE_TRUE(*aListener, NS_ERROR_UNEXPECTED);

  // Try to find the broadcaster element in the document.
  Document* doc = aElement->GetComposedDoc();
  if (doc) {
    *aBroadcaster = doc->GetElementById(aBroadcasterID);
  }

  // The broadcaster element is missing.
  if (!*aBroadcaster) {
    return NS_FINDBROADCASTER_NOT_FOUND;
  }

  NS_ADDREF(*aBroadcaster);

  return NS_FINDBROADCASTER_FOUND;
}

nsresult XULBroadcastManager::UpdateListenerHookup(Element* aElement,
                                                   HookupAction aAction) {
  // Resolve a broadcaster hookup. Look at the element that we're
  // trying to resolve: it could be an '<observes>' element, or just
  // a vanilla element with an 'observes' attribute on it.
  nsresult rv;

  nsCOMPtr<Element> listener;
  nsAutoString broadcasterID;
  nsAutoString attribute;
  nsCOMPtr<Element> broadcaster;

  rv = FindBroadcaster(aElement, getter_AddRefs(listener), broadcasterID,
                       attribute, getter_AddRefs(broadcaster));
  switch (rv) {
    case NS_FINDBROADCASTER_NOT_FOUND:
      return NS_OK;
    case NS_FINDBROADCASTER_FOUND:
      break;
    default:
      return rv;
  }

  NS_ENSURE_ARG(broadcaster && listener);
  if (aAction == eHookupAdd) {
    ErrorResult domRv;
    AddListenerFor(*broadcaster, *listener, attribute, domRv);
    if (domRv.Failed()) {
      return domRv.StealNSResult();
    }
  } else {
    RemoveListenerFor(*broadcaster, *listener, attribute);
  }

  // Tell the world we succeeded
  if (MOZ_LOG_TEST(sXULBroadCastManager, LogLevel::Debug)) {
    nsCOMPtr<nsIContent> content = listener;
    NS_ASSERTION(content != nullptr, "not an nsIContent");
    if (!content) {
      return rv;
    }

    nsAutoCString attributeC, broadcasteridC;
    LossyCopyUTF16toASCII(attribute, attributeC);
    LossyCopyUTF16toASCII(broadcasterID, broadcasteridC);
    MOZ_LOG(sXULBroadCastManager, LogLevel::Debug,
            ("xul: broadcaster hookup <%s attribute='%s'> to %s",
             nsAtomCString(content->NodeInfo()->NameAtom()).get(),
             attributeC.get(), broadcasteridC.get()));
  }

  return NS_OK;
}

nsresult XULBroadcastManager::AddListener(Element* aElement) {
  return UpdateListenerHookup(aElement, eHookupAdd);
}

nsresult XULBroadcastManager::RemoveListener(Element* aElement) {
  return UpdateListenerHookup(aElement, eHookupRemove);
}

}  // namespace dom
}  // namespace mozilla
