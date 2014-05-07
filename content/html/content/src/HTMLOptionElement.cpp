/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLOptionElement.h"
#include "mozilla/dom/HTMLOptionElementBinding.h"
#include "mozilla/dom/HTMLSelectElement.h"
#include "nsIDOMHTMLOptGroupElement.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsIFormControl.h"
#include "nsIForm.h"
#include "nsIDOMNode.h"
#include "nsIDOMHTMLCollection.h"
#include "nsISelectControlFrame.h"

// Notify/query select frame for selected state
#include "nsIFormControlFrame.h"
#include "nsIDocument.h"
#include "nsIDOMHTMLSelectElement.h"
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

HTMLOptionElement::HTMLOptionElement(already_AddRefed<nsINodeInfo>& aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo),
    mSelectedChanged(false),
    mIsSelected(false),
    mIsInSetDefaultSelected(false)
{
  // We start off enabled
  AddStatesSilently(NS_EVENT_STATE_ENABLED);
}

HTMLOptionElement::~HTMLOptionElement()
{
}

NS_IMPL_ISUPPORTS_INHERITED(HTMLOptionElement, nsGenericHTMLElement,
                            nsIDOMHTMLOptionElement)

NS_IMPL_ELEMENT_CLONE(HTMLOptionElement)


NS_IMETHODIMP
HTMLOptionElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  NS_IF_ADDREF(*aForm = GetForm());
  return NS_OK;
}

mozilla::dom::HTMLFormElement*
HTMLOptionElement::GetForm()
{
  HTMLSelectElement* selectControl = GetSelect();
  return selectControl ? selectControl->GetForm() : nullptr;
}

void
HTMLOptionElement::SetSelectedInternal(bool aValue, bool aNotify)
{
  mSelectedChanged = true;
  mIsSelected = aValue;

  // When mIsInSetDefaultSelected is true, the state change will be handled by
  // SetAttr/UnsetAttr.
  if (!mIsInSetDefaultSelected) {
    UpdateState(aNotify);
  }
}

NS_IMETHODIMP
HTMLOptionElement::GetSelected(bool* aValue)
{
  NS_ENSURE_ARG_POINTER(aValue);
  *aValue = Selected();
  return NS_OK;
}

NS_IMETHODIMP
HTMLOptionElement::SetSelected(bool aValue)
{
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

  return NS_OK;
}

NS_IMPL_BOOL_ATTR(HTMLOptionElement, DefaultSelected, selected)
// GetText returns a whitespace compressed .textContent value.
NS_IMPL_STRING_ATTR_WITH_FALLBACK(HTMLOptionElement, Label, label, GetText)
NS_IMPL_STRING_ATTR_WITH_FALLBACK(HTMLOptionElement, Value, value, GetText)
NS_IMPL_BOOL_ATTR(HTMLOptionElement, Disabled, disabled)

NS_IMETHODIMP
HTMLOptionElement::GetIndex(int32_t* aIndex)
{
  *aIndex = Index();
  return NS_OK;
}

int32_t
HTMLOptionElement::Index()
{
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
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
    options->GetOptionIndex(this, 0, true, &index)));
  return index;
}

bool
HTMLOptionElement::Selected() const
{
  // If we haven't been explictly selected or deselected, use our default value
  if (!mSelectedChanged) {
    return DefaultSelected();
  }

  return mIsSelected;
}

bool
HTMLOptionElement::DefaultSelected() const
{
  return HasAttr(kNameSpaceID_None, nsGkAtoms::selected);
}

nsChangeHint
HTMLOptionElement::GetAttributeChangeHint(const nsIAtom* aAttribute,
                                          int32_t aModType) const
{
  nsChangeHint retval =
      nsGenericHTMLElement::GetAttributeChangeHint(aAttribute, aModType);

  if (aAttribute == nsGkAtoms::label ||
      aAttribute == nsGkAtoms::text) {
    NS_UpdateHint(retval, NS_STYLE_HINT_REFLOW);
  }
  return retval;
}

nsresult
HTMLOptionElement::BeforeSetAttr(int32_t aNamespaceID, nsIAtom* aName,
                                 const nsAttrValueOrString* aValue,
                                 bool aNotify)
{
  nsresult rv = nsGenericHTMLElement::BeforeSetAttr(aNamespaceID, aName,
                                                    aValue, aNotify);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aNamespaceID != kNameSpaceID_None || aName != nsGkAtoms::selected ||
      mSelectedChanged) {
    return NS_OK;
  }

  bool defaultSelected = aValue;
  // First make sure we actually set our mIsSelected state to reflect our new
  // defaultSelected state.  If that turns out to be wrong,
  // SetOptionsSelectedByIndex will fix it up.  But otherwise we can end up in a
  // situation where mIsSelected is still false, but mSelectedChanged becomes
  // true (later in this method, when we compare mIsSelected to
  // defaultSelected), and then we start returning false for Selected() even
  // though we're actually selected.
  mIsSelected = defaultSelected;

  // We just changed out selected state (since we look at the "selected"
  // attribute when mSelectedChanged is false).  Let's tell our select about
  // it.
  HTMLSelectElement* selectInt = GetSelect();
  if (!selectInt) {
    return NS_OK;
  }

  NS_ASSERTION(!mSelectedChanged, "Shouldn't be here");

  bool inSetDefaultSelected = mIsInSetDefaultSelected;
  mIsInSetDefaultSelected = true;

  int32_t index = Index();
  uint32_t mask = HTMLSelectElement::SET_DISABLED;
  if (defaultSelected) {
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
  // more than once; make sure our mSelectedChanged state is set correctly.
  mSelectedChanged = mIsSelected != defaultSelected;

  return NS_OK;
}

nsresult
HTMLOptionElement::AfterSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                const nsAttrValue* aValue, bool aNotify)
{
  if (aNameSpaceID == kNameSpaceID_None &&
      aName == nsGkAtoms::value && Selected()) {
    // Since this option is selected, changing value
    // may have changed missing validity state of the
    // Select element
    HTMLSelectElement* select = GetSelect();
    if (select) {
      select->UpdateValueMissingValidityState();
    }
  }

  return nsGenericHTMLElement::AfterSetAttr(aNameSpaceID, aName,
                                            aValue, aNotify);
}

NS_IMETHODIMP
HTMLOptionElement::GetText(nsAString& aText)
{
  nsAutoString text;

  nsIContent* child = nsINode::GetFirstChild();
  while (child) {
    if (child->NodeType() == nsIDOMNode::TEXT_NODE ||
        child->NodeType() == nsIDOMNode::CDATA_SECTION_NODE) {
      child->AppendTextTo(text);
    }
    if (child->IsHTML(nsGkAtoms::script) || child->IsSVG(nsGkAtoms::script)) {
      child = child->GetNextNonChildNode(this);
    } else {
      child = child->GetNextNode(this);
    }
  }

  // XXX No CompressWhitespace for nsAString.  Sad.
  text.CompressWhitespace(true, true);
  aText = text;

  return NS_OK;
}

NS_IMETHODIMP
HTMLOptionElement::SetText(const nsAString& aText)
{
  return nsContentUtils::SetNodeTextContent(this, aText, true);
}

nsresult
HTMLOptionElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument, aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  // Our new parent might change :disabled/:enabled state.
  UpdateState(false);

  return NS_OK;
}

void
HTMLOptionElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);

  // Our previous parent could have been involved in :disabled/:enabled state.
  UpdateState(false);
}

EventStates
HTMLOptionElement::IntrinsicState() const
{
  EventStates state = nsGenericHTMLElement::IntrinsicState();
  if (Selected()) {
    state |= NS_EVENT_STATE_CHECKED;
  }
  if (DefaultSelected()) {
    state |= NS_EVENT_STATE_DEFAULT;
  }

  // An <option> is disabled if it has @disabled set or if it's <optgroup> has
  // @disabled set.
  if (HasAttr(kNameSpaceID_None, nsGkAtoms::disabled)) {
    state |= NS_EVENT_STATE_DISABLED;
    state &= ~NS_EVENT_STATE_ENABLED;
  } else {
    nsIContent* parent = GetParent();
    if (parent && parent->IsHTML(nsGkAtoms::optgroup) &&
        parent->HasAttr(kNameSpaceID_None, nsGkAtoms::disabled)) {
      state |= NS_EVENT_STATE_DISABLED;
      state &= ~NS_EVENT_STATE_ENABLED;
    } else {
      state &= ~NS_EVENT_STATE_DISABLED;
      state |= NS_EVENT_STATE_ENABLED;
    }
  }

  return state;
}

// Get the select content element that contains this option
HTMLSelectElement*
HTMLOptionElement::GetSelect()
{
  nsIContent* parent = this;
  while ((parent = parent->GetParent()) &&
         parent->IsHTML()) {
    HTMLSelectElement* select = HTMLSelectElement::FromContent(parent);
    if (select) {
      return select;
    }
    if (parent->Tag() != nsGkAtoms::optgroup) {
      break;
    }
  }

  return nullptr;
}

already_AddRefed<HTMLOptionElement>
HTMLOptionElement::Option(const GlobalObject& aGlobal,
                          const Optional<nsAString>& aText,
                          const Optional<nsAString>& aValue,
                          const Optional<bool>& aDefaultSelected,
                          const Optional<bool>& aSelected, ErrorResult& aError)
{
  nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(aGlobal.GetAsSupports());
  nsIDocument* doc;
  if (!win || !(doc = win->GetExtantDoc())) {
    aError.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  already_AddRefed<nsINodeInfo> nodeInfo =
    doc->NodeInfoManager()->GetNodeInfo(nsGkAtoms::option, nullptr,
                                        kNameSpaceID_XHTML,
                                        nsIDOMNode::ELEMENT_NODE);

  nsRefPtr<HTMLOptionElement> option = new HTMLOptionElement(nodeInfo);

  if (aText.WasPassed()) {
    // Create a new text node and append it to the option
    nsRefPtr<nsTextNode> textContent =
      new nsTextNode(option->NodeInfo()->NodeInfoManager());

    textContent->SetText(aText.Value(), false);

    aError = option->AppendChildTo(textContent, false);
    if (aError.Failed()) {
      return nullptr;
    }

    if (aValue.WasPassed()) {
      // Set the value attribute for this element. We're calling SetAttr
      // directly because we want to pass aNotify == false.
      aError = option->SetAttr(kNameSpaceID_None, nsGkAtoms::value,
                               aValue.Value(), false);
      if (aError.Failed()) {
        return nullptr;
      }

      if (aDefaultSelected.WasPassed()) {
        if (aDefaultSelected.Value()) {
          // We're calling SetAttr directly because we want to pass
          // aNotify == false.
          aError = option->SetAttr(kNameSpaceID_None, nsGkAtoms::selected,
                                   EmptyString(), false);
          if (aError.Failed()) {
            return nullptr;
          }
        }

        if (aSelected.WasPassed()) {
          option->SetSelected(aSelected.Value(), aError);
          if (aError.Failed()) {
            return nullptr;
          }
        }
      }
    }
  }

  return option.forget();
}

nsresult
HTMLOptionElement::CopyInnerTo(Element* aDest)
{
  nsresult rv = nsGenericHTMLElement::CopyInnerTo(aDest);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aDest->OwnerDoc()->IsStaticDocument()) {
    static_cast<HTMLOptionElement*>(aDest)->SetSelected(Selected());
  }
  return NS_OK;
}

JSObject*
HTMLOptionElement::WrapNode(JSContext* aCx)
{
  return HTMLOptionElementBinding::Wrap(aCx, this);
}

} // namespace dom
} // namespace mozilla
