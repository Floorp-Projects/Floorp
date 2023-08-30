/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/BasicEvents.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/Maybe.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/dom/CustomElementRegistry.h"
#include "mozilla/dom/HTMLFieldSetElement.h"
#include "mozilla/dom/HTMLFieldSetElementBinding.h"
#include "nsContentList.h"
#include "nsQueryObject.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(FieldSet)

namespace mozilla::dom {

HTMLFieldSetElement::HTMLFieldSetElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : nsGenericHTMLFormControlElement(std::move(aNodeInfo),
                                      FormControlType::Fieldset),
      mElements(nullptr),
      mFirstLegend(nullptr),
      mInvalidElementsCount(0) {
  // <fieldset> is always barred from constraint validation.
  SetBarredFromConstraintValidation(true);

  // We start out enabled and valid.
  AddStatesSilently(ElementState::ENABLED | ElementState::VALID);
}

HTMLFieldSetElement::~HTMLFieldSetElement() {
  uint32_t length = mDependentElements.Length();
  for (uint32_t i = 0; i < length; ++i) {
    mDependentElements[i]->ForgetFieldSet(this);
  }
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(HTMLFieldSetElement,
                                   nsGenericHTMLFormControlElement, mValidity,
                                   mElements)

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED(HTMLFieldSetElement,
                                             nsGenericHTMLFormControlElement,
                                             nsIConstraintValidation)

NS_IMPL_ELEMENT_CLONE(HTMLFieldSetElement)

bool HTMLFieldSetElement::IsDisabledForEvents(WidgetEvent* aEvent) {
  if (StaticPrefs::dom_forms_fieldset_disable_only_descendants_enabled()) {
    return false;
  }
  return IsElementDisabledForEvents(aEvent, nullptr);
}

// nsIContent
void HTMLFieldSetElement::GetEventTargetParent(EventChainPreVisitor& aVisitor) {
  // Do not process any DOM events if the element is disabled.
  aVisitor.mCanHandle = false;
  if (IsDisabledForEvents(aVisitor.mEvent)) {
    return;
  }

  nsGenericHTMLFormControlElement::GetEventTargetParent(aVisitor);
}

void HTMLFieldSetElement::AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                       const nsAttrValue* aValue,
                                       const nsAttrValue* aOldValue,
                                       nsIPrincipal* aSubjectPrincipal,
                                       bool aNotify) {
  if (aNameSpaceID == kNameSpaceID_None && aName == nsGkAtoms::disabled) {
    // This *has* to be called *before* calling FieldSetDisabledChanged on our
    // controls, as they may depend on our disabled state.
    UpdateDisabledState(aNotify);
  }

  return nsGenericHTMLFormControlElement::AfterSetAttr(
      aNameSpaceID, aName, aValue, aOldValue, aSubjectPrincipal, aNotify);
}

void HTMLFieldSetElement::GetType(nsAString& aType) const {
  aType.AssignLiteral("fieldset");
}

/* static */
bool HTMLFieldSetElement::MatchListedElements(Element* aElement,
                                              int32_t aNamespaceID,
                                              nsAtom* aAtom, void* aData) {
  nsCOMPtr<nsIFormControl> formControl = do_QueryInterface(aElement);
  return formControl;
}

nsIHTMLCollection* HTMLFieldSetElement::Elements() {
  if (!mElements) {
    mElements =
        new nsContentList(this, MatchListedElements, nullptr, nullptr, true);
  }

  return mElements;
}

// nsIFormControl

nsresult HTMLFieldSetElement::Reset() { return NS_OK; }

void HTMLFieldSetElement::InsertChildBefore(nsIContent* aChild,
                                            nsIContent* aBeforeThis,
                                            bool aNotify, ErrorResult& aRv) {
  bool firstLegendHasChanged = false;

  if (aChild->IsHTMLElement(nsGkAtoms::legend)) {
    if (!mFirstLegend) {
      mFirstLegend = aChild;
      // We do not want to notify the first time mFirstElement is set.
    } else {
      // If mFirstLegend is before aIndex, we do not change it.
      // Otherwise, mFirstLegend is now aChild.
      const Maybe<uint32_t> indexOfRef =
          aBeforeThis ? ComputeIndexOf(aBeforeThis) : Some(GetChildCount());
      const Maybe<uint32_t> indexOfFirstLegend = ComputeIndexOf(mFirstLegend);
      if ((indexOfRef.isSome() && indexOfFirstLegend.isSome() &&
           *indexOfRef <= *indexOfFirstLegend) ||
          // XXX Keep the odd traditional behavior for now.
          indexOfRef.isNothing()) {
        mFirstLegend = aChild;
        firstLegendHasChanged = true;
      }
    }
  }

  nsGenericHTMLFormControlElement::InsertChildBefore(aChild, aBeforeThis,
                                                     aNotify, aRv);
  if (aRv.Failed()) {
    return;
  }

  if (firstLegendHasChanged) {
    NotifyElementsForFirstLegendChange(aNotify);
  }
}

void HTMLFieldSetElement::RemoveChildNode(nsIContent* aKid, bool aNotify) {
  bool firstLegendHasChanged = false;

  if (mFirstLegend && aKid == mFirstLegend) {
    // If we are removing the first legend we have to found another one.
    nsIContent* child = mFirstLegend->GetNextSibling();
    mFirstLegend = nullptr;
    firstLegendHasChanged = true;

    for (; child; child = child->GetNextSibling()) {
      if (child->IsHTMLElement(nsGkAtoms::legend)) {
        mFirstLegend = child;
        break;
      }
    }
  }

  nsGenericHTMLFormControlElement::RemoveChildNode(aKid, aNotify);

  if (firstLegendHasChanged) {
    NotifyElementsForFirstLegendChange(aNotify);
  }
}

void HTMLFieldSetElement::AddElement(nsGenericHTMLFormElement* aElement) {
  mDependentElements.AppendElement(aElement);

  // If the element that we are adding aElement is a fieldset, then all the
  // invalid elements in aElement are also invalid elements of this.
  HTMLFieldSetElement* fieldSet = FromNode(aElement);
  if (fieldSet) {
    for (int32_t i = 0; i < fieldSet->mInvalidElementsCount; i++) {
      UpdateValidity(false);
    }
    return;
  }

  // If the element is a form-associated custom element, adding element might be
  // caused by FACE upgrade which won't trigger mutation observer, so mark
  // mElements dirty manually here.
  CustomElementData* data = aElement->GetCustomElementData();
  if (data && data->IsFormAssociated() && mElements) {
    mElements->SetDirty();
  }

  // We need to update the validity of the fieldset.
  nsCOMPtr<nsIConstraintValidation> cvElmt = do_QueryObject(aElement);
  if (cvElmt && cvElmt->IsCandidateForConstraintValidation() &&
      !cvElmt->IsValid()) {
    UpdateValidity(false);
  }

#if DEBUG
  int32_t debugInvalidElementsCount = 0;
  for (uint32_t i = 0; i < mDependentElements.Length(); i++) {
    HTMLFieldSetElement* fieldSet = FromNode(mDependentElements[i]);
    if (fieldSet) {
      debugInvalidElementsCount += fieldSet->mInvalidElementsCount;
      continue;
    }
    nsCOMPtr<nsIConstraintValidation> cvElmt =
        do_QueryObject(mDependentElements[i]);
    if (cvElmt && cvElmt->IsCandidateForConstraintValidation() &&
        !(cvElmt->IsValid())) {
      debugInvalidElementsCount += 1;
    }
  }
  MOZ_ASSERT(debugInvalidElementsCount == mInvalidElementsCount);
#endif
}

void HTMLFieldSetElement::RemoveElement(nsGenericHTMLFormElement* aElement) {
  mDependentElements.RemoveElement(aElement);

  // If the element that we are removing aElement is a fieldset, then all the
  // invalid elements in aElement are also removed from this.
  HTMLFieldSetElement* fieldSet = FromNode(aElement);
  if (fieldSet) {
    for (int32_t i = 0; i < fieldSet->mInvalidElementsCount; i++) {
      UpdateValidity(true);
    }
    return;
  }

  // We need to update the validity of the fieldset.
  nsCOMPtr<nsIConstraintValidation> cvElmt = do_QueryObject(aElement);
  if (cvElmt && cvElmt->IsCandidateForConstraintValidation() &&
      !cvElmt->IsValid()) {
    UpdateValidity(true);
  }

#if DEBUG
  int32_t debugInvalidElementsCount = 0;
  for (uint32_t i = 0; i < mDependentElements.Length(); i++) {
    HTMLFieldSetElement* fieldSet = FromNode(mDependentElements[i]);
    if (fieldSet) {
      debugInvalidElementsCount += fieldSet->mInvalidElementsCount;
      continue;
    }
    nsCOMPtr<nsIConstraintValidation> cvElmt =
        do_QueryObject(mDependentElements[i]);
    if (cvElmt && cvElmt->IsCandidateForConstraintValidation() &&
        !(cvElmt->IsValid())) {
      debugInvalidElementsCount += 1;
    }
  }
  MOZ_ASSERT(debugInvalidElementsCount == mInvalidElementsCount);
#endif
}

void HTMLFieldSetElement::UpdateDisabledState(bool aNotify) {
  nsGenericHTMLFormControlElement::UpdateDisabledState(aNotify);

  for (nsGenericHTMLFormElement* element : mDependentElements) {
    element->FieldSetDisabledChanged(aNotify);
  }
}

void HTMLFieldSetElement::NotifyElementsForFirstLegendChange(bool aNotify) {
  /**
   * NOTE: this could be optimized if only call when the fieldset is currently
   * disabled.
   * This should also make sure that mElements is set when we happen to be here.
   * However, this method shouldn't be called very often in normal use cases.
   */
  if (!mElements) {
    mElements =
        new nsContentList(this, MatchListedElements, nullptr, nullptr, true);
  }

  uint32_t length = mElements->Length(true);
  for (uint32_t i = 0; i < length; ++i) {
    static_cast<nsGenericHTMLFormElement*>(mElements->Item(i))
        ->FieldSetFirstLegendChanged(aNotify);
  }
}

void HTMLFieldSetElement::UpdateValidity(bool aElementValidity) {
  if (aElementValidity) {
    --mInvalidElementsCount;
  } else {
    ++mInvalidElementsCount;
  }

  MOZ_ASSERT(mInvalidElementsCount >= 0);

  // The fieldset validity has just changed if:
  // - there are no more invalid elements ;
  // - or there is one invalid elmement and an element just became invalid.
  if (!mInvalidElementsCount ||
      (mInvalidElementsCount == 1 && !aElementValidity)) {
    AutoStateChangeNotifier notifier(*this, true);
    RemoveStatesSilently(ElementState::VALID | ElementState::INVALID);
    AddStatesSilently(mInvalidElementsCount ? ElementState::INVALID
                                            : ElementState::VALID);
  }

  // We should propagate the change to the fieldset parent chain.
  if (mFieldSet) {
    mFieldSet->UpdateValidity(aElementValidity);
  }
}

JSObject* HTMLFieldSetElement::WrapNode(JSContext* aCx,
                                        JS::Handle<JSObject*> aGivenProto) {
  return HTMLFieldSetElement_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
