/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLOptionElement.h"

#include "HTMLOptGroupElement.h"
#include "mozilla/dom/HTMLOptionElementBinding.h"
#include "mozilla/dom/HTMLSelectElement.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsIFormControl.h"
#include "nsIForm.h"
#include "nsISelectControlFrame.h"

// Notify/query select frame for selected state
#include "nsIFormControlFrame.h"
#include "mozilla/dom/Document.h"
#include "nsNodeInfoManager.h"
#include "nsCOMPtr.h"
#include "mozilla/EventStates.h"
#include "nsContentCreatorFunctions.h"
#include "mozAutoDocUpdate.h"
#include "nsTextNode.h"

/**
 * Implementation of &lt;option&gt;
 */

NS_IMPL_NS_NEW_HTML_ELEMENT(Option)

namespace mozilla {
namespace dom {

HTMLOptionElement::HTMLOptionElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : nsGenericHTMLElement(std::move(aNodeInfo)),
      mSelectedChanged(false),
      mIsSelected(false),
      mIsInSetDefaultSelected(false) {
  // We start off enabled
  AddStatesSilently(NS_EVENT_STATE_ENABLED);
}

HTMLOptionElement::~HTMLOptionElement() = default;

NS_IMPL_ELEMENT_CLONE(HTMLOptionElement)

mozilla::dom::HTMLFormElement* HTMLOptionElement::GetForm() {
  HTMLSelectElement* selectControl = GetSelect();
  return selectControl ? selectControl->GetForm() : nullptr;
}

void HTMLOptionElement::SetSelectedInternal(bool aValue, bool aNotify) {
  mSelectedChanged = true;
  mIsSelected = aValue;

  // When mIsInSetDefaultSelected is true, the state change will be handled by
  // SetAttr/UnsetAttr.
  if (!mIsInSetDefaultSelected) {
    UpdateState(aNotify);
  }
}

void HTMLOptionElement::OptGroupDisabledChanged(bool aNotify) {
  UpdateDisabledState(aNotify);
}

void HTMLOptionElement::UpdateDisabledState(bool aNotify) {
  bool isDisabled = HasAttr(kNameSpaceID_None, nsGkAtoms::disabled);

  if (!isDisabled) {
    nsIContent* parent = GetParent();
    if (auto optGroupElement = HTMLOptGroupElement::FromNodeOrNull(parent)) {
      isDisabled = optGroupElement->IsDisabled();
    }
  }

  EventStates disabledStates;
  if (isDisabled) {
    disabledStates |= NS_EVENT_STATE_DISABLED;
  } else {
    disabledStates |= NS_EVENT_STATE_ENABLED;
  }

  EventStates oldDisabledStates = State() & DISABLED_STATES;
  EventStates changedStates = disabledStates ^ oldDisabledStates;

  if (!changedStates.IsEmpty()) {
    ToggleStates(changedStates, aNotify);
  }
}

void HTMLOptionElement::SetSelected(bool aValue) {
  // Note: The select content obj maintains all the PresState
  // so defer to it to get the answer
  HTMLSelectElement* selectInt = GetSelect();
  if (selectInt) {
    int32_t index = Index();
    uint32_t mask = HTMLSelectElement::SET_DISABLED | HTMLSelectElement::NOTIFY;
    if (aValue) {
      mask |= HTMLSelectElement::IS_SELECTED;
    }

    // This should end up calling SetSelectedInternal
    selectInt->SetOptionsSelectedByIndex(index, index, mask);
  } else {
    SetSelectedInternal(aValue, true);
  }
}

int32_t HTMLOptionElement::Index() {
  static int32_t defaultIndex = 0;

  // Only select elements can contain a list of options.
  HTMLSelectElement* selectElement = GetSelect();
  if (!selectElement) {
    return defaultIndex;
  }

  HTMLOptionsCollection* options = selectElement->GetOptions();
  if (!options) {
    return defaultIndex;
  }

  int32_t index = defaultIndex;
  MOZ_ALWAYS_SUCCEEDS(options->GetOptionIndex(this, 0, true, &index));
  return index;
}

nsChangeHint HTMLOptionElement::GetAttributeChangeHint(const nsAtom* aAttribute,
                                                       int32_t aModType) const {
  nsChangeHint retval =
      nsGenericHTMLElement::GetAttributeChangeHint(aAttribute, aModType);

  if (aAttribute == nsGkAtoms::label) {
    retval |= nsChangeHint_ReconstructFrame;
  } else if (aAttribute == nsGkAtoms::text) {
    retval |= NS_STYLE_HINT_REFLOW;
  }
  return retval;
}

nsresult HTMLOptionElement::BeforeSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                          const nsAttrValueOrString* aValue,
                                          bool aNotify) {
  nsresult rv =
      nsGenericHTMLElement::BeforeSetAttr(aNamespaceID, aName, aValue, aNotify);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aNamespaceID != kNameSpaceID_None || aName != nsGkAtoms::selected ||
      mSelectedChanged) {
    return NS_OK;
  }

  // We just changed out selected state (since we look at the "selected"
  // attribute when mSelectedChanged is false).  Let's tell our select about
  // it.
  HTMLSelectElement* selectInt = GetSelect();
  if (!selectInt) {
    // If option is a child of select, SetOptionsSelectedByIndex will set
    // mIsSelected if needed.
    mIsSelected = aValue;
    return NS_OK;
  }

  NS_ASSERTION(!mSelectedChanged, "Shouldn't be here");

  bool inSetDefaultSelected = mIsInSetDefaultSelected;
  mIsInSetDefaultSelected = true;

  int32_t index = Index();
  uint32_t mask = HTMLSelectElement::SET_DISABLED;
  if (aValue) {
    mask |= HTMLSelectElement::IS_SELECTED;
  }

  if (aNotify) {
    mask |= HTMLSelectElement::NOTIFY;
  }

  // This can end up calling SetSelectedInternal if our selected state needs to
  // change, which we will allow to take effect so that parts of
  // SetOptionsSelectedByIndex that might depend on it working don't get
  // confused.
  selectInt->SetOptionsSelectedByIndex(index, index, mask);

  // Now reset our members; when we finish the attr set we'll end up with the
  // rigt selected state.
  mIsInSetDefaultSelected = inSetDefaultSelected;
  // mIsSelected might have been changed by SetOptionsSelectedByIndex.  Possibly
  // more than once; make sure our mSelectedChanged state is set back correctly.
  mSelectedChanged = false;

  return NS_OK;
}

nsresult HTMLOptionElement::AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                         const nsAttrValue* aValue,
                                         const nsAttrValue* aOldValue,
                                         nsIPrincipal* aSubjectPrincipal,
                                         bool aNotify) {
  if (aNameSpaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::disabled) {
      UpdateDisabledState(aNotify);
    }

    if (aName == nsGkAtoms::value && Selected()) {
      // Since this option is selected, changing value
      // may have changed missing validity state of the
      // Select element
      HTMLSelectElement* select = GetSelect();
      if (select) {
        select->UpdateValueMissingValidityState();
      }
    }
  }

  return nsGenericHTMLElement::AfterSetAttr(
      aNameSpaceID, aName, aValue, aOldValue, aSubjectPrincipal, aNotify);
}

void HTMLOptionElement::GetText(nsAString& aText) {
  nsAutoString text;

  nsIContent* child = nsINode::GetFirstChild();
  while (child) {
    if (Text* textChild = child->GetAsText()) {
      textChild->AppendTextTo(text);
    }
    if (child->IsHTMLElement(nsGkAtoms::script) ||
        child->IsSVGElement(nsGkAtoms::script)) {
      child = child->GetNextNonChildNode(this);
    } else {
      child = child->GetNextNode(this);
    }
  }

  // XXX No CompressWhitespace for nsAString.  Sad.
  text.CompressWhitespace(true, true);
  aText = text;
}

void HTMLOptionElement::SetText(const nsAString& aText, ErrorResult& aRv) {
  aRv = nsContentUtils::SetNodeTextContent(this, aText, true);
}

nsresult HTMLOptionElement::BindToTree(BindContext& aContext,
                                       nsINode& aParent) {
  nsresult rv = nsGenericHTMLElement::BindToTree(aContext, aParent);
  NS_ENSURE_SUCCESS(rv, rv);

  // Our new parent might change :disabled/:enabled state.
  UpdateDisabledState(false);

  return NS_OK;
}

void HTMLOptionElement::UnbindFromTree(bool aNullParent) {
  nsGenericHTMLElement::UnbindFromTree(aNullParent);

  // Our previous parent could have been involved in :disabled/:enabled state.
  UpdateDisabledState(false);
}

EventStates HTMLOptionElement::IntrinsicState() const {
  EventStates state = nsGenericHTMLElement::IntrinsicState();
  if (Selected()) {
    state |= NS_EVENT_STATE_CHECKED;
  }
  if (DefaultSelected()) {
    state |= NS_EVENT_STATE_DEFAULT;
  }

  return state;
}

// Get the select content element that contains this option
HTMLSelectElement* HTMLOptionElement::GetSelect() {
  nsIContent* parent = GetParent();
  if (!parent) {
    return nullptr;
  }

  HTMLSelectElement* select = HTMLSelectElement::FromNode(parent);
  if (select) {
    return select;
  }

  if (!parent->IsHTMLElement(nsGkAtoms::optgroup)) {
    return nullptr;
  }

  return HTMLSelectElement::FromNodeOrNull(parent->GetParent());
}

already_AddRefed<HTMLOptionElement> HTMLOptionElement::Option(
    const GlobalObject& aGlobal, const nsAString& aText,
    const Optional<nsAString>& aValue, bool aDefaultSelected, bool aSelected,
    ErrorResult& aError) {
  nsCOMPtr<nsPIDOMWindowInner> win = do_QueryInterface(aGlobal.GetAsSupports());
  Document* doc;
  if (!win || !(doc = win->GetExtantDoc())) {
    aError.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<mozilla::dom::NodeInfo> nodeInfo = doc->NodeInfoManager()->GetNodeInfo(
      nsGkAtoms::option, nullptr, kNameSpaceID_XHTML, ELEMENT_NODE);

  auto* nim = nodeInfo->NodeInfoManager();
  RefPtr<HTMLOptionElement> option =
      new (nim) HTMLOptionElement(nodeInfo.forget());

  if (!aText.IsEmpty()) {
    // Create a new text node and append it to the option
    RefPtr<nsTextNode> textContent = new (option->NodeInfo()->NodeInfoManager())
        nsTextNode(option->NodeInfo()->NodeInfoManager());

    textContent->SetText(aText, false);

    aError = option->AppendChildTo(textContent, false);
    if (aError.Failed()) {
      return nullptr;
    }
  }

  if (aValue.WasPassed()) {
    // Set the value attribute for this element. We're calling SetAttr
    // directly because we want to pass aNotify == false.
    aError = option->SetAttr(kNameSpaceID_None, nsGkAtoms::value,
                             aValue.Value(), false);
    if (aError.Failed()) {
      return nullptr;
    }
  }

  if (aDefaultSelected) {
    // We're calling SetAttr directly because we want to pass
    // aNotify == false.
    aError = option->SetAttr(kNameSpaceID_None, nsGkAtoms::selected,
                             EmptyString(), false);
    if (aError.Failed()) {
      return nullptr;
    }
  }

  option->SetSelected(aSelected);
  option->SetSelectedChanged(false);

  return option.forget();
}

nsresult HTMLOptionElement::CopyInnerTo(Element* aDest) {
  nsresult rv = nsGenericHTMLElement::CopyInnerTo(aDest);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aDest->OwnerDoc()->IsStaticDocument()) {
    static_cast<HTMLOptionElement*>(aDest)->SetSelected(Selected());
  }
  return NS_OK;
}

JSObject* HTMLOptionElement::WrapNode(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return HTMLOptionElement_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace dom
}  // namespace mozilla
