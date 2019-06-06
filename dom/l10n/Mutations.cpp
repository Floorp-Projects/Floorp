#include "Mutations.h"
#include "mozilla/dom/DocumentInlines.h"

namespace mozilla {
namespace dom {
namespace l10n {

NS_IMPL_CYCLE_COLLECTION_CLASS(Mutations)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(Mutations)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPendingElements)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPendingElementsHash)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(Mutations)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPendingElements)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPendingElementsHash)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Mutations)
  NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(Mutations)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Mutations)

Mutations::Mutations(DOMLocalization* aDOMLocalization)
    : mDOMLocalization(aDOMLocalization) {
  mObserving = true;
}

void Mutations::AttributeChanged(dom::Element* aElement, int32_t aNameSpaceID,
                                 nsAtom* aAttribute, int32_t aModType,
                                 const nsAttrValue* aOldValue) {
  if (!mObserving) {
    return;
  }
  Document* uncomposedDoc = aElement->GetUncomposedDoc();
  if (uncomposedDoc) {
    if (aNameSpaceID == kNameSpaceID_None &&
        (aAttribute == nsGkAtoms::datal10nid ||
         aAttribute == nsGkAtoms::datal10nargs)) {
      L10nElementChanged(aElement);
    }
  }
}

void Mutations::ContentAppended(nsIContent* aChild) {
  if (!mObserving) {
    return;
  }
  ErrorResult rv;
  Sequence<OwningNonNull<Element>> elements;

  nsINode* node = aChild;
  while (node) {
    if (node->IsElement()) {
      Element* elem = node->AsElement();

      Document* uncomposedDoc = elem->GetUncomposedDoc();
      if (uncomposedDoc) {
        DOMLocalization::GetTranslatables(*node, elements, rv);
      }
    }

    node = node->GetNextSibling();
  }

  for (auto& elem : elements) {
    L10nElementChanged(elem);
  }
}

void Mutations::ContentInserted(nsIContent* aChild) {
  if (!mObserving) {
    return;
  }
  ErrorResult rv;
  Sequence<OwningNonNull<Element>> elements;

  if (!aChild->IsElement()) {
    return;
  }
  Element* elem = aChild->AsElement();

  Document* uncomposedDoc = elem->GetUncomposedDoc();
  if (!uncomposedDoc) {
    return;
  }
  DOMLocalization::GetTranslatables(*aChild, elements, rv);

  for (auto& elem : elements) {
    L10nElementChanged(elem);
  }
}

void Mutations::L10nElementChanged(Element* aElement) {
  if (!mPendingElementsHash.Contains(aElement)) {
    mPendingElements.AppendElement(aElement);
    mPendingElementsHash.PutEntry(aElement);
  }

  if (!mRefreshObserver) {
    StartRefreshObserver();
  }
}

void Mutations::PauseObserving() { mObserving = false; }

void Mutations::ResumeObserving() { mObserving = true; }

void Mutations::WillRefresh(mozilla::TimeStamp aTime) {
  StopRefreshObserver();
  FlushPendingTranslations();
}

void Mutations::FlushPendingTranslations() {
  if (!mDOMLocalization) {
    return;
  }

  ErrorResult rv;

  Sequence<OwningNonNull<Element>> elements;

  for (auto& elem : mPendingElements) {
    if (!elem->HasAttr(kNameSpaceID_None, nsGkAtoms::datal10nid)) {
      continue;
    }

    elements.AppendElement(*elem, fallible);
  }

  mPendingElementsHash.Clear();
  mPendingElements.Clear();

  RefPtr<Promise> promise = mDOMLocalization->TranslateElements(elements, rv);
}

void Mutations::Disconnect() {
  StopRefreshObserver();
  mDOMLocalization = nullptr;
}

void Mutations::StartRefreshObserver() {
  if (!mDOMLocalization || mRefreshObserver) {
    return;
  }

  if (!mRefreshDriver) {
    nsPIDOMWindowInner* innerWindow =
        mDOMLocalization->GetParentObject()->AsInnerWindow();
    Document* doc = innerWindow ? innerWindow->GetExtantDoc() : nullptr;
    if (doc) {
      nsPresContext* ctx = doc->GetPresContext();
      if (ctx) {
        mRefreshDriver = ctx->RefreshDriver();
      }
    }
  }

  // If we can't start the refresh driver, it means
  // that the presContext is not available yet.
  // In that case, we'll trigger the flush of pending
  // elements in Document::CreatePresShell.
  if (mRefreshDriver) {
    mRefreshDriver->AddRefreshObserver(this, FlushType::Style);
    mRefreshObserver = true;
  } else {
    NS_WARNING("[l10n][mutations] Failed to start a refresh observer.");
  }
}

void Mutations::StopRefreshObserver() {
  if (!mDOMLocalization) {
    return;
  }

  if (mRefreshDriver) {
    mRefreshDriver->RemoveRefreshObserver(this, FlushType::Style);
    mRefreshObserver = false;
  }
}

void Mutations::OnCreatePresShell() {
  if (!mPendingElements.IsEmpty()) {
    StartRefreshObserver();
  }
}

}  // namespace l10n
}  // namespace dom
}  // namespace mozilla
