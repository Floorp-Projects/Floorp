/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLMenuItemElement.h"

#include "mozilla/BasicEvents.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/dom/HTMLMenuItemElementBinding.h"
#include "nsAttrValueInlines.h"
#include "nsContentUtils.h"


NS_IMPL_NS_NEW_HTML_ELEMENT_CHECK_PARSER(MenuItem)

namespace mozilla {
namespace dom {

// First bits are needed for the menuitem type.
#define NS_CHECKED_IS_TOGGLED (1 << 2)
#define NS_ORIGINAL_CHECKED_VALUE (1 << 3)
#define NS_MENUITEM_TYPE(bits) ((bits) & ~( \
  NS_CHECKED_IS_TOGGLED | NS_ORIGINAL_CHECKED_VALUE))

enum CmdType : uint8_t
{
  CMD_TYPE_MENUITEM = 1,
  CMD_TYPE_CHECKBOX,
  CMD_TYPE_RADIO
};

static const nsAttrValue::EnumTable kMenuItemTypeTable[] = {
  { "menuitem", CMD_TYPE_MENUITEM },
  { "checkbox", CMD_TYPE_CHECKBOX },
  { "radio", CMD_TYPE_RADIO },
  { nullptr, 0 }
};

static const nsAttrValue::EnumTable* kMenuItemDefaultType =
  &kMenuItemTypeTable[0];

// A base class inherited by all radio visitors.
class Visitor
{
public:
  Visitor() { }
  virtual ~Visitor() { }

  /**
   * Visit a node in the tree. This is meant to be called on all radios in a
   * group, sequentially. If the method returns false then the iteration is
   * stopped.
   */
  virtual bool Visit(HTMLMenuItemElement* aMenuItem) = 0;
};

// Find the selected radio, see GetSelectedRadio().
class GetCheckedVisitor : public Visitor
{
public:
  explicit GetCheckedVisitor(HTMLMenuItemElement** aResult)
    : mResult(aResult)
    { }
  virtual bool Visit(HTMLMenuItemElement* aMenuItem) override
  {
    if (aMenuItem->IsChecked()) {
      *mResult = aMenuItem;
      return false;
    }
    return true;
  }
protected:
  HTMLMenuItemElement** mResult;
};

// Deselect all radios except the one passed to the constructor.
class ClearCheckedVisitor : public Visitor
{
public:
  explicit ClearCheckedVisitor(HTMLMenuItemElement* aExcludeMenuItem)
    : mExcludeMenuItem(aExcludeMenuItem)
    { }
  virtual bool Visit(HTMLMenuItemElement* aMenuItem) override
  {
    if (aMenuItem != mExcludeMenuItem && aMenuItem->IsChecked()) {
      aMenuItem->ClearChecked();
    }
    return true;
  }
protected:
  HTMLMenuItemElement* mExcludeMenuItem;
};

// Get current value of the checked dirty flag. The same value is stored on all
// radios in the group, so we need to check only the first one.
class GetCheckedDirtyVisitor : public Visitor
{
public:
  GetCheckedDirtyVisitor(bool* aCheckedDirty,
                         HTMLMenuItemElement* aExcludeMenuItem)
    : mCheckedDirty(aCheckedDirty),
      mExcludeMenuItem(aExcludeMenuItem)
    { }
  virtual bool Visit(HTMLMenuItemElement* aMenuItem) override
  {
    if (aMenuItem == mExcludeMenuItem) {
      return true;
    }
    *mCheckedDirty = aMenuItem->IsCheckedDirty();
    return false;
  }
protected:
  bool* mCheckedDirty;
  HTMLMenuItemElement* mExcludeMenuItem;
};

// Set checked dirty to true on all radios in the group.
class SetCheckedDirtyVisitor : public Visitor
{
public:
  SetCheckedDirtyVisitor()
    { }
  virtual bool Visit(HTMLMenuItemElement* aMenuItem) override
  {
    aMenuItem->SetCheckedDirty();
    return true;
  }
};

// A helper visitor that is used to combine two operations (visitors) to avoid
// iterating over radios twice.
class CombinedVisitor : public Visitor
{
public:
  CombinedVisitor(Visitor* aVisitor1, Visitor* aVisitor2)
    : mVisitor1(aVisitor1), mVisitor2(aVisitor2),
      mContinue1(true), mContinue2(true)
    { }
  virtual bool Visit(HTMLMenuItemElement* aMenuItem) override
  {
    if (mContinue1) {
      mContinue1 = mVisitor1->Visit(aMenuItem);
    }
    if (mContinue2) {
      mContinue2 = mVisitor2->Visit(aMenuItem);
    }
    return mContinue1 || mContinue2;
  }
protected:
  Visitor* mVisitor1;
  Visitor* mVisitor2;
  bool mContinue1;
  bool mContinue2;
};


HTMLMenuItemElement::HTMLMenuItemElement(
  already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo, FromParser aFromParser)
  : nsGenericHTMLElement(aNodeInfo),
    mType(kMenuItemDefaultType->value),
    mParserCreating(false),
    mShouldInitChecked(false),
    mCheckedDirty(false),
    mChecked(false)
{
  mParserCreating = aFromParser;
}

HTMLMenuItemElement::~HTMLMenuItemElement()
{
}


NS_IMPL_ISUPPORTS_INHERITED0(HTMLMenuItemElement, nsGenericHTMLElement)

//NS_IMPL_ELEMENT_CLONE(HTMLMenuItemElement)

nsresult
HTMLMenuItemElement::Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult,
                           bool aPreallocateArrays) const
{
  *aResult = nullptr;
  already_AddRefed<mozilla::dom::NodeInfo> ni = RefPtr<mozilla::dom::NodeInfo>(aNodeInfo).forget();
  RefPtr<HTMLMenuItemElement> it =
    new HTMLMenuItemElement(ni, NOT_FROM_PARSER);
  nsresult rv = const_cast<HTMLMenuItemElement*>(this)->CopyInnerTo(it, aPreallocateArrays);
  if (NS_SUCCEEDED(rv)) {
    switch (mType) {
      case CMD_TYPE_CHECKBOX:
      case CMD_TYPE_RADIO:
        if (mCheckedDirty) {
          // We no longer have our original checked state.  Set our
          // checked state on the clone.
          it->mCheckedDirty = true;
          it->mChecked = mChecked;
        }
        break;
    }

    it.forget(aResult);
  }

  return rv;
}

void
HTMLMenuItemElement::GetType(DOMString& aValue)
{
  GetEnumAttr(nsGkAtoms::type, kMenuItemDefaultType->tag, aValue);
}

void
HTMLMenuItemElement::SetChecked(bool aChecked)
{
  bool checkedChanged = mChecked != aChecked;

  mChecked = aChecked;

  if (mType == CMD_TYPE_RADIO) {
    if (checkedChanged) {
      if (mCheckedDirty) {
        ClearCheckedVisitor visitor(this);
        WalkRadioGroup(&visitor);
      } else {
        ClearCheckedVisitor visitor1(this);
        SetCheckedDirtyVisitor visitor2;
        CombinedVisitor visitor(&visitor1, &visitor2);
        WalkRadioGroup(&visitor);
      }
    } else if (!mCheckedDirty) {
      SetCheckedDirtyVisitor visitor;
      WalkRadioGroup(&visitor);
    }
  } else {
    mCheckedDirty = true;
  }
}

nsresult
HTMLMenuItemElement::GetEventTargetParent(EventChainPreVisitor& aVisitor)
{
  if (aVisitor.mEvent->mMessage == eMouseClick) {

    bool originalCheckedValue = false;
    switch (mType) {
      case CMD_TYPE_CHECKBOX:
        originalCheckedValue = mChecked;
        SetChecked(!originalCheckedValue);
        aVisitor.mItemFlags |= NS_CHECKED_IS_TOGGLED;
        break;
      case CMD_TYPE_RADIO:
        // casting back to Element* here to resolve nsISupports ambiguity.
        Element* supports = GetSelectedRadio();
        aVisitor.mItemData = supports;

        originalCheckedValue = mChecked;
        if (!originalCheckedValue) {
          SetChecked(true);
          aVisitor.mItemFlags |= NS_CHECKED_IS_TOGGLED;
        }
        break;
    }

    if (originalCheckedValue) {
      aVisitor.mItemFlags |= NS_ORIGINAL_CHECKED_VALUE;
    }

    // We must cache type because mType may change during JS event.
    aVisitor.mItemFlags |= mType;
  }

  return nsGenericHTMLElement::GetEventTargetParent(aVisitor);
}

nsresult
HTMLMenuItemElement::PostHandleEvent(EventChainPostVisitor& aVisitor)
{
  // Check to see if the event was cancelled.
  if (aVisitor.mEvent->mMessage == eMouseClick &&
      aVisitor.mItemFlags & NS_CHECKED_IS_TOGGLED &&
      aVisitor.mEventStatus == nsEventStatus_eConsumeNoDefault) {
    bool originalCheckedValue =
      !!(aVisitor.mItemFlags & NS_ORIGINAL_CHECKED_VALUE);
    uint8_t oldType = NS_MENUITEM_TYPE(aVisitor.mItemFlags);

    nsCOMPtr<nsIContent> content(do_QueryInterface(aVisitor.mItemData));
    RefPtr<HTMLMenuItemElement> selectedRadio = HTMLMenuItemElement::FromContentOrNull(content);
    if (selectedRadio) {
      selectedRadio->SetChecked(true);
      if (mType != CMD_TYPE_RADIO) {
        SetChecked(false);
      }
    } else if (oldType == CMD_TYPE_CHECKBOX) {
      SetChecked(originalCheckedValue);
    }
  }

  return NS_OK;
}

nsresult
HTMLMenuItemElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                                nsIContent* aBindingParent,
                                bool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument, aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);

  if (NS_SUCCEEDED(rv) && aDocument && mType == CMD_TYPE_RADIO) {
    AddedToRadioGroup();
  }

  return rv;
}

bool
HTMLMenuItemElement::ParseAttribute(int32_t aNamespaceID,
                                    nsAtom* aAttribute,
                                    const nsAString& aValue,
                                    nsIPrincipal* aMaybeScriptedPrincipal,
                                    nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::type) {
      return aResult.ParseEnumValue(aValue, kMenuItemTypeTable, false,
                                    kMenuItemDefaultType);
    }

    if (aAttribute == nsGkAtoms::radiogroup) {
      aResult.ParseAtom(aValue);
      return true;
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aMaybeScriptedPrincipal, aResult);
}

void
HTMLMenuItemElement::DoneCreatingElement()
{
  mParserCreating = false;

  if (mShouldInitChecked) {
    InitChecked();
    mShouldInitChecked = false;
  }
}

void
HTMLMenuItemElement::GetText(nsAString& aText)
{
  nsAutoString text;
  nsContentUtils::GetNodeTextContent(this, false, text);

  text.CompressWhitespace(true, true);
  aText = text;
}

nsresult
HTMLMenuItemElement::AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                  const nsAttrValue* aValue,
                                  const nsAttrValue* aOldValue,
                                  nsIPrincipal* aSubjectPrincipal,
                                  bool aNotify)
{
  if (aNameSpaceID == kNameSpaceID_None) {
    // Handle type changes first, since some of the later conditions in this
    // method look at mType and want to see the new value.
    if (aName == nsGkAtoms::type) {
      if (aValue) {
        mType = aValue->GetEnumValue();
      } else {
        mType = kMenuItemDefaultType->value;
      }
    }

    if ((aName == nsGkAtoms::radiogroup || aName == nsGkAtoms::type) &&
        mType == CMD_TYPE_RADIO &&
        !mParserCreating) {
      if (IsInUncomposedDoc() && GetParent()) {
        AddedToRadioGroup();
      }
    }

    // Checked must be set no matter what type of menuitem it is, since
    // GetChecked() must reflect the new value
    if (aName == nsGkAtoms::checked &&
        !mCheckedDirty) {
      if (mParserCreating) {
        mShouldInitChecked = true;
      } else {
        InitChecked();
      }
    }
  }

  return nsGenericHTMLElement::AfterSetAttr(aNameSpaceID, aName, aValue,
                                            aOldValue, aSubjectPrincipal, aNotify);
}

void
HTMLMenuItemElement::WalkRadioGroup(Visitor* aVisitor)
{
  nsIContent* parent = GetParent();
  if (!parent) {
    aVisitor->Visit(this);
    return;
  }

  BorrowedAttrInfo info1(GetAttrInfo(kNameSpaceID_None,
                               nsGkAtoms::radiogroup));
  bool info1Empty = !info1.mValue || info1.mValue->IsEmptyString();

  for (nsIContent* cur = parent->GetFirstChild();
       cur;
       cur = cur->GetNextSibling()) {
    HTMLMenuItemElement* menuitem = HTMLMenuItemElement::FromContent(cur);

    if (!menuitem || menuitem->GetType() != CMD_TYPE_RADIO) {
      continue;
    }

    BorrowedAttrInfo info2(menuitem->GetAttrInfo(kNameSpaceID_None,
                                           nsGkAtoms::radiogroup));
    bool info2Empty = !info2.mValue || info2.mValue->IsEmptyString();

    if (info1Empty != info2Empty ||
        (info1.mValue && info2.mValue && !info1.mValue->Equals(*info2.mValue))) {
      continue;
    }

    if (!aVisitor->Visit(menuitem)) {
      break;
    }
  }
}

HTMLMenuItemElement*
HTMLMenuItemElement::GetSelectedRadio()
{
  HTMLMenuItemElement* result = nullptr;

  GetCheckedVisitor visitor(&result);
  WalkRadioGroup(&visitor);

  return result;
}

void
HTMLMenuItemElement::AddedToRadioGroup()
{
  bool checkedDirty = mCheckedDirty;
  if (mChecked) {
    ClearCheckedVisitor visitor1(this);
    GetCheckedDirtyVisitor visitor2(&checkedDirty, this);
    CombinedVisitor visitor(&visitor1, &visitor2);
    WalkRadioGroup(&visitor);
  } else {
    GetCheckedDirtyVisitor visitor(&checkedDirty, this);
    WalkRadioGroup(&visitor);
  }
  mCheckedDirty = checkedDirty;
}

void
HTMLMenuItemElement::InitChecked()
{
  bool defaultChecked = DefaultChecked();
  mChecked = defaultChecked;
  if (mType == CMD_TYPE_RADIO) {
    ClearCheckedVisitor visitor(this);
    WalkRadioGroup(&visitor);
  }
}

JSObject*
HTMLMenuItemElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLMenuItemElementBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla

#undef NS_ORIGINAL_CHECKED_VALUE
