/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "XULPersist.h"

#include "nsIXULStore.h"
#include "nsIStringEnumerator.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "nsContentUtils.h"
#include "nsIAppWindow.h"

namespace mozilla::dom {

static bool IsRootElement(Element* aElement) {
  return aElement->OwnerDoc()->GetRootElement() == aElement;
}

// FIXME: This is a hack to differentiate "attribute is missing" from "attribute
// is present but empty". Use a newline to avoid virtually all collisions.
// Ideally the XUL store would be able to store this more reasonably.
constexpr auto kMissingAttributeToken = u"-moz-missing\n"_ns;

static bool ShouldPersistAttribute(Element* aElement, nsAtom* aAttribute) {
  if (IsRootElement(aElement)) {
    // This is not an element of the top document, its owner is
    // not an AppWindow. Persist it.
    if (aElement->OwnerDoc()->GetInProcessParentDocument()) {
      return true;
    }
    // The following attributes of xul:window should be handled in
    // AppWindow::SavePersistentAttributes instead of here.
    if (aAttribute == nsGkAtoms::screenX || aAttribute == nsGkAtoms::screenY ||
        aAttribute == nsGkAtoms::width || aAttribute == nsGkAtoms::height ||
        aAttribute == nsGkAtoms::sizemode) {
      return false;
    }
  }
  return true;
}

NS_IMPL_ISUPPORTS(XULPersist, nsIDocumentObserver)

XULPersist::XULPersist(Document* aDocument)
    : nsStubDocumentObserver(), mDocument(aDocument) {}

XULPersist::~XULPersist() = default;

void XULPersist::Init() {
  ApplyPersistentAttributes();
  mDocument->AddObserver(this);
}

void XULPersist::DropDocumentReference() {
  mDocument->RemoveObserver(this);
  mDocument = nullptr;
}

void XULPersist::AttributeChanged(dom::Element* aElement, int32_t aNameSpaceID,
                                  nsAtom* aAttribute, int32_t aModType,
                                  const nsAttrValue* aOldValue) {
  NS_ASSERTION(aElement->OwnerDoc() == mDocument, "unexpected doc");

  if (aNameSpaceID != kNameSpaceID_None) {
    return;
  }

  // See if there is anything we need to persist in the localstore.
  nsAutoString persist;
  // Persistence of attributes of xul:window is handled in AppWindow.
  if (aElement->GetAttr(nsGkAtoms::persist, persist) &&
      ShouldPersistAttribute(aElement, aAttribute) &&
      // XXXldb This should check that it's a token, not just a substring.
      persist.Find(nsDependentAtomString(aAttribute)) >= 0) {
    // Might not need this, but be safe for now.
    nsCOMPtr<nsIDocumentObserver> kungFuDeathGrip(this);
    nsContentUtils::AddScriptRunner(NewRunnableMethod<Element*, nsAtom*>(
        "dom::XULPersist::Persist", this, &XULPersist::Persist, aElement,
        aAttribute));
  }
}

void XULPersist::Persist(Element* aElement, nsAtom* aAttribute) {
  if (!mDocument) {
    return;
  }
  // For non-chrome documents, persistance is simply broken
  if (!mDocument->NodePrincipal()->IsSystemPrincipal()) {
    return;
  }

  if (!mLocalStore) {
    mLocalStore = do_GetService("@mozilla.org/xul/xulstore;1");
    if (NS_WARN_IF(!mLocalStore)) {
      return;
    }
  }

  nsAutoString id;

  aElement->GetAttr(nsGkAtoms::id, id);
  nsAtomString attrstr(aAttribute);

  nsAutoCString utf8uri;
  nsresult rv = mDocument->GetDocumentURI()->GetSpec(utf8uri);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  // Persisting attributes to top level windows is handled by AppWindow.
  if (IsRootElement(aElement)) {
    if (nsCOMPtr<nsIAppWindow> win =
            mDocument->GetAppWindowIfToplevelChrome()) {
      return;
    }
  }

  NS_ConvertUTF8toUTF16 uri(utf8uri);
  nsAutoString valuestr;
  if (!aElement->GetAttr(aAttribute, valuestr)) {
    valuestr = kMissingAttributeToken;
  }

  mLocalStore->SetValue(uri, id, attrstr, valuestr);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "value set");
}

nsresult XULPersist::ApplyPersistentAttributes() {
  if (!mDocument) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  // For non-chrome documents, persistance is simply broken
  if (!mDocument->NodePrincipal()->IsSystemPrincipal()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Add all of the 'persisted' attributes into the content
  // model.
  if (!mLocalStore) {
    mLocalStore = do_GetService("@mozilla.org/xul/xulstore;1");
    if (NS_WARN_IF(!mLocalStore)) {
      return NS_ERROR_NOT_INITIALIZED;
    }
  }

  nsCOMArray<Element> elements;

  nsAutoCString utf8uri;
  nsresult rv = mDocument->GetDocumentURI()->GetSpec(utf8uri);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  NS_ConvertUTF8toUTF16 uri(utf8uri);

  // Get a list of element IDs for which persisted values are available
  nsCOMPtr<nsIStringEnumerator> ids;
  rv = mLocalStore->GetIDsEnumerator(uri, getter_AddRefs(ids));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool hasmore;
  while (NS_SUCCEEDED(ids->HasMore(&hasmore)) && hasmore) {
    nsAutoString id;
    ids->GetNext(id);

    // We want to hold strong refs to the elements while applying
    // persistent attributes, just in case.
    const nsTArray<Element*>* allElements = mDocument->GetAllElementsForId(id);
    if (!allElements) {
      continue;
    }
    elements.Clear();
    elements.SetCapacity(allElements->Length());
    for (Element* element : *allElements) {
      elements.AppendObject(element);
    }

    rv = ApplyPersistentAttributesToElements(id, uri, elements);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

nsresult XULPersist::ApplyPersistentAttributesToElements(
    const nsAString& aID, const nsAString& aDocURI,
    nsCOMArray<Element>& aElements) {
  nsresult rv = NS_OK;
  // Get a list of attributes for which persisted values are available
  nsCOMPtr<nsIStringEnumerator> attrs;
  rv = mLocalStore->GetAttributeEnumerator(aDocURI, aID, getter_AddRefs(attrs));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool hasmore;
  while (NS_SUCCEEDED(attrs->HasMore(&hasmore)) && hasmore) {
    nsAutoString attrstr;
    attrs->GetNext(attrstr);

    nsAutoString value;
    rv = mLocalStore->GetValue(aDocURI, aID, attrstr, value);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    RefPtr<nsAtom> attr = NS_Atomize(attrstr);
    if (NS_WARN_IF(!attr)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    uint32_t cnt = aElements.Length();
    for (int32_t i = int32_t(cnt) - 1; i >= 0; --i) {
      Element* element = aElements.SafeElementAt(i);
      if (!element) {
        continue;
      }

      // Applying persistent attributes to top level windows is handled
      // by AppWindow.
      if (IsRootElement(element)) {
        if (nsCOMPtr<nsIAppWindow> win =
                mDocument->GetAppWindowIfToplevelChrome()) {
          continue;
        }
      }

      if (value == kMissingAttributeToken) {
        Unused << element->UnsetAttr(kNameSpaceID_None, attr, true);
      } else {
        Unused << element->SetAttr(kNameSpaceID_None, attr, value, true);
      }
    }
  }

  return NS_OK;
}

}  // namespace mozilla::dom
