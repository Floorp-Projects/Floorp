/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsGUIEvent.h"
#include "nsEventDispatcher.h"
#include "nsHTMLMenuItemElement.h"

using namespace mozilla::dom;

// First bits are needed for the menuitem type.
#define NS_CHECKED_IS_TOGGLED (1 << 2)
#define NS_ORIGINAL_CHECKED_VALUE (1 << 3)
#define NS_MENUITEM_TYPE(bits) ((bits) & ~( \
  NS_CHECKED_IS_TOGGLED | NS_ORIGINAL_CHECKED_VALUE))

enum CmdType                                                                 
{                                                                            
  CMD_TYPE_MENUITEM = 1,
  CMD_TYPE_CHECKBOX,
  CMD_TYPE_RADIO
};

static const nsAttrValue::EnumTable kMenuItemTypeTable[] = {
  { "menuitem", CMD_TYPE_MENUITEM },
  { "checkbox", CMD_TYPE_CHECKBOX },
  { "radio", CMD_TYPE_RADIO },
  { 0 }
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
  virtual PRBool Visit(nsHTMLMenuItemElement* aMenuItem) = 0;
};

// Find the selected radio, see GetSelectedRadio().
class GetCheckedVisitor : public Visitor
{
public:
  GetCheckedVisitor(nsHTMLMenuItemElement** aResult)
    : mResult(aResult)
    { }
  virtual PRBool Visit(nsHTMLMenuItemElement* aMenuItem)
  {
    if (aMenuItem->IsChecked()) {
      *mResult = aMenuItem;
      return PR_FALSE;
    }
    return PR_TRUE;
  }
protected:
  nsHTMLMenuItemElement** mResult;
};

// Deselect all radios except the one passed to the constructor.
class ClearCheckedVisitor : public Visitor
{
public:
  ClearCheckedVisitor(nsHTMLMenuItemElement* aExcludeMenuItem)
    : mExcludeMenuItem(aExcludeMenuItem)
    { }
  virtual PRBool Visit(nsHTMLMenuItemElement* aMenuItem)
  {
    if (aMenuItem != mExcludeMenuItem && aMenuItem->IsChecked()) {
      aMenuItem->ClearChecked();
    }
    return PR_TRUE;
  }
protected:
  nsHTMLMenuItemElement* mExcludeMenuItem;
};

// Get current value of the checked dirty flag. The same value is stored on all
// radios in the group, so we need to check only the first one.
class GetCheckedDirtyVisitor : public Visitor
{
public:
  GetCheckedDirtyVisitor(PRBool* aCheckedDirty,
                         nsHTMLMenuItemElement* aExcludeMenuItem)
    : mCheckedDirty(aCheckedDirty),
      mExcludeMenuItem(aExcludeMenuItem)
    { }
  virtual PRBool Visit(nsHTMLMenuItemElement* aMenuItem)
  {
    if (aMenuItem == mExcludeMenuItem) {
      return PR_TRUE;
    }
    *mCheckedDirty = aMenuItem->IsCheckedDirty();
    return PR_FALSE;
  }
protected:
  PRBool* mCheckedDirty;
  nsHTMLMenuItemElement* mExcludeMenuItem;
};

// Set checked dirty to true on all radios in the group.
class SetCheckedDirtyVisitor : public Visitor
{
public:
  SetCheckedDirtyVisitor()
    { }
  virtual PRBool Visit(nsHTMLMenuItemElement* aMenuItem)
  {
    aMenuItem->SetCheckedDirty();
    return PR_TRUE;
  }
};

// A helper visitor that is used to combine two operations (visitors) to avoid
// iterating over radios twice.
class CombinedVisitor : public Visitor
{
public:
  CombinedVisitor(Visitor* aVisitor1, Visitor* aVisitor2)
    : mVisitor1(aVisitor1), mVisitor2(aVisitor2),
      mContinue1(PR_TRUE), mContinue2(PR_TRUE)
    { }
  virtual PRBool Visit(nsHTMLMenuItemElement* aMenuItem)
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
  PRPackedBool mContinue1;
  PRPackedBool mContinue2;
};


NS_IMPL_NS_NEW_HTML_ELEMENT_CHECK_PARSER(MenuItem)

nsHTMLMenuItemElement::nsHTMLMenuItemElement(
  already_AddRefed<nsINodeInfo> aNodeInfo, FromParser aFromParser)
  : nsGenericHTMLElement(aNodeInfo),
    mType(kMenuItemDefaultType->value),
    mParserCreating(false),
    mShouldInitChecked(false),
    mCheckedDirty(false),
    mChecked(false)
{
  mParserCreating = aFromParser;
}

nsHTMLMenuItemElement::~nsHTMLMenuItemElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLMenuItemElement, nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsHTMLMenuItemElement, nsGenericElement)


DOMCI_NODE_DATA(HTMLMenuItemElement, nsHTMLMenuItemElement)

// QueryInterface implementation for nsHTMLMenuItemElement
NS_INTERFACE_TABLE_HEAD(nsHTMLMenuItemElement)
  NS_HTML_CONTENT_INTERFACE_TABLE2(nsHTMLMenuItemElement,
                                   nsIDOMHTMLCommandElement,
                                   nsIDOMHTMLMenuItemElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLMenuItemElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLMenuItemElement)

//NS_IMPL_ELEMENT_CLONE(nsHTMLMenuItemElement)
nsresult
nsHTMLMenuItemElement::Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const
{
  *aResult = nsnull;
  nsCOMPtr<nsINodeInfo> ni = aNodeInfo;
  nsHTMLMenuItemElement *it = new nsHTMLMenuItemElement(ni.forget(),
                                                        NOT_FROM_PARSER);
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsINode> kungFuDeathGrip = it;
  nsresult rv = CopyInnerTo(it);
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

    kungFuDeathGrip.swap(*aResult);
  }

  return rv;
}


NS_IMPL_ENUM_ATTR_DEFAULT_VALUE(nsHTMLMenuItemElement, Type, type,
                                kMenuItemDefaultType->tag)
NS_IMPL_STRING_ATTR(nsHTMLMenuItemElement, Label, label)
NS_IMPL_URI_ATTR(nsHTMLMenuItemElement, Icon, icon)
NS_IMPL_BOOL_ATTR(nsHTMLMenuItemElement, Disabled, disabled)
NS_IMPL_BOOL_ATTR(nsHTMLMenuItemElement, DefaultChecked, checked)
//NS_IMPL_BOOL_ATTR(nsHTMLMenuItemElement, Checked, checked)
NS_IMPL_STRING_ATTR(nsHTMLMenuItemElement, Radiogroup, radiogroup)

NS_IMETHODIMP
nsHTMLMenuItemElement::GetChecked(PRBool* aChecked)
{
  *aChecked = mChecked;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLMenuItemElement::SetChecked(PRBool aChecked)
{
  PRBool checkedChanged = mChecked != aChecked;

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

  return NS_OK;
}

nsresult
nsHTMLMenuItemElement::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
{
  if (aVisitor.mEvent->message == NS_MOUSE_CLICK) {

    PRBool originalCheckedValue = PR_FALSE;
    switch (mType) {
      case CMD_TYPE_CHECKBOX:
        originalCheckedValue = mChecked;
        SetChecked(!originalCheckedValue);
        aVisitor.mItemFlags |= NS_CHECKED_IS_TOGGLED;
        break;
      case CMD_TYPE_RADIO:
        nsCOMPtr<nsIDOMHTMLMenuItemElement> selectedRadio = GetSelectedRadio();
        aVisitor.mItemData = selectedRadio;

        originalCheckedValue = mChecked;
        if (!originalCheckedValue) {
          SetChecked(PR_TRUE);
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

  return nsGenericHTMLElement::PreHandleEvent(aVisitor);
}

nsresult
nsHTMLMenuItemElement::PostHandleEvent(nsEventChainPostVisitor& aVisitor)
{
  // Check to see if the event was cancelled.
  if (aVisitor.mEvent->message == NS_MOUSE_CLICK &&
      aVisitor.mItemFlags & NS_CHECKED_IS_TOGGLED &&
      aVisitor.mEventStatus == nsEventStatus_eConsumeNoDefault) {
    PRBool originalCheckedValue =
      !!(aVisitor.mItemFlags & NS_ORIGINAL_CHECKED_VALUE);
    PRUint8 oldType = NS_MENUITEM_TYPE(aVisitor.mItemFlags);

    nsCOMPtr<nsIDOMHTMLMenuItemElement> selectedRadio =
      do_QueryInterface(aVisitor.mItemData);
    if (selectedRadio) {
      selectedRadio->SetChecked(PR_TRUE);
      if (mType != CMD_TYPE_RADIO) {
        SetChecked(PR_FALSE);
      }
    } else if (oldType == CMD_TYPE_CHECKBOX) {
      SetChecked(originalCheckedValue);
    }
  }

  return NS_OK;
}

nsresult
nsHTMLMenuItemElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                                  nsIContent* aBindingParent,
                                  PRBool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument, aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);

  if (NS_SUCCEEDED(rv) && aDocument && mType == CMD_TYPE_RADIO) {
    AddedToRadioGroup();
  }

  return rv;
}

PRBool
nsHTMLMenuItemElement::ParseAttribute(PRInt32 aNamespaceID,
                                      nsIAtom* aAttribute,
                                      const nsAString& aValue,
                                      nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::type) {
      PRBool success = aResult.ParseEnumValue(aValue, kMenuItemTypeTable,
                                              PR_FALSE);
      if (success) {
        mType = aResult.GetEnumValue();
      } else {
        mType = kMenuItemDefaultType->value;
      }

      return success;
    }

    if (aAttribute == nsGkAtoms::radiogroup) {
      aResult.ParseAtom(aValue);
      return PR_TRUE;
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

void
nsHTMLMenuItemElement::DoneCreatingElement()
{
  mParserCreating = false;

  if (mShouldInitChecked) {
    InitChecked();
    mShouldInitChecked = false;
  }
}

nsresult
nsHTMLMenuItemElement::AfterSetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                                    const nsAString* aValue, PRBool aNotify)
{
  if (aNameSpaceID == kNameSpaceID_None) {
    if ((aName == nsGkAtoms::radiogroup || aName == nsGkAtoms::type) &&
        mType == CMD_TYPE_RADIO &&
        !mParserCreating) {
      if (IsInDoc() && GetParent()) {
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
                                            aNotify);
}

void
nsHTMLMenuItemElement::WalkRadioGroup(Visitor* aVisitor)
{
  nsIContent* parent = GetParent();
  if (!parent) {
    aVisitor->Visit(this);
    return;
  }

  nsAttrInfo info1(GetAttrInfo(kNameSpaceID_None,
                               nsGkAtoms::radiogroup));
  PRBool info1Empty = !info1.mValue || info1.mValue->IsEmptyString();

  for (nsIContent* cur = parent->GetFirstChild();
       cur;
       cur = cur->GetNextSibling()) {
    nsHTMLMenuItemElement* menuitem = nsHTMLMenuItemElement::FromContent(cur);

    if (!menuitem || menuitem->GetType() != CMD_TYPE_RADIO) {
      continue;
    }

    nsAttrInfo info2(menuitem->GetAttrInfo(kNameSpaceID_None,
                                           nsGkAtoms::radiogroup));
    PRBool info2Empty = !info2.mValue || info2.mValue->IsEmptyString();

    if (info1Empty != info2Empty ||
        info1.mValue && info2.mValue && !info1.mValue->Equals(*info2.mValue)) {
      continue;
    }

    if (!aVisitor->Visit(menuitem)) {
      break;
    }
  }
}

nsHTMLMenuItemElement*
nsHTMLMenuItemElement::GetSelectedRadio()
{
  nsHTMLMenuItemElement* result = nsnull;

  GetCheckedVisitor visitor(&result);
  WalkRadioGroup(&visitor);

  return result;
}

void
nsHTMLMenuItemElement::AddedToRadioGroup()
{
  PRBool checkedDirty = mCheckedDirty;
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
nsHTMLMenuItemElement::InitChecked()
{
  PRBool defaultChecked;
  GetDefaultChecked(&defaultChecked);
  mChecked = defaultChecked;
  if (mType == CMD_TYPE_RADIO) {
    ClearCheckedVisitor visitor(this);
    WalkRadioGroup(&visitor);
  }
}
