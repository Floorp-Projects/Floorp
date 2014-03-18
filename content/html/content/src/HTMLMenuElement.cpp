/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLMenuElement.h"

#include "mozilla/BasicEvents.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/dom/HTMLMenuElementBinding.h"
#include "mozilla/dom/HTMLMenuItemElement.h"
#include "nsAttrValueInlines.h"
#include "nsContentUtils.h"
#include "nsXULContextMenuBuilder.h"
#include "nsIURI.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Menu)

namespace mozilla {
namespace dom {

enum MenuType
{
  MENU_TYPE_CONTEXT = 1,
  MENU_TYPE_TOOLBAR,
  MENU_TYPE_LIST
};

static const nsAttrValue::EnumTable kMenuTypeTable[] = {
  { "context", MENU_TYPE_CONTEXT },
  { "toolbar", MENU_TYPE_TOOLBAR },
  { "list", MENU_TYPE_LIST },
  { 0 }
};

static const nsAttrValue::EnumTable* kMenuDefaultType =
  &kMenuTypeTable[2];

enum SeparatorType
{
  ST_TRUE_INIT = -1,
  ST_FALSE = 0,
  ST_TRUE = 1
};



HTMLMenuElement::HTMLMenuElement(already_AddRefed<nsINodeInfo>& aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo), mType(MENU_TYPE_LIST)
{
}

HTMLMenuElement::~HTMLMenuElement()
{
}

NS_IMPL_ISUPPORTS_INHERITED2(HTMLMenuElement, nsGenericHTMLElement,
                             nsIDOMHTMLMenuElement, nsIHTMLMenu)

NS_IMPL_ELEMENT_CLONE(HTMLMenuElement)

NS_IMPL_BOOL_ATTR(HTMLMenuElement, Compact, compact)
NS_IMPL_ENUM_ATTR_DEFAULT_VALUE(HTMLMenuElement, Type, type,
                                kMenuDefaultType->tag)
NS_IMPL_STRING_ATTR(HTMLMenuElement, Label, label)


NS_IMETHODIMP
HTMLMenuElement::SendShowEvent()
{
  NS_ENSURE_TRUE(nsContentUtils::IsCallerChrome(), NS_ERROR_DOM_SECURITY_ERR);

  nsCOMPtr<nsIDocument> document = GetCurrentDoc();
  if (!document) {
    return NS_ERROR_FAILURE;
  }

  WidgetEvent event(true, NS_SHOW_EVENT);
  event.mFlags.mBubbles = false;
  event.mFlags.mCancelable = false;

  nsCOMPtr<nsIPresShell> shell = document->GetShell();
  if (!shell) {
    return NS_ERROR_FAILURE;
  }
 
  nsRefPtr<nsPresContext> presContext = shell->GetPresContext();
  nsEventStatus status = nsEventStatus_eIgnore;
  EventDispatcher::Dispatch(static_cast<nsIContent*>(this), presContext,
                            &event, nullptr, &status);

  return NS_OK;
}

NS_IMETHODIMP
HTMLMenuElement::CreateBuilder(nsIMenuBuilder** _retval)
{
  NS_ENSURE_TRUE(nsContentUtils::IsCallerChrome(), NS_ERROR_DOM_SECURITY_ERR);

  *_retval = nullptr;

  if (mType == MENU_TYPE_CONTEXT) {
    NS_ADDREF(*_retval = new nsXULContextMenuBuilder());
  }

  return NS_OK;
}

already_AddRefed<nsIMenuBuilder>
HTMLMenuElement::CreateBuilder()
{
  if (mType != MENU_TYPE_CONTEXT) {
    return nullptr;
  }

  nsCOMPtr<nsIMenuBuilder> ret = new nsXULContextMenuBuilder();
  return ret.forget();
}

NS_IMETHODIMP
HTMLMenuElement::Build(nsIMenuBuilder* aBuilder)
{
  NS_ENSURE_TRUE(nsContentUtils::IsCallerChrome(), NS_ERROR_DOM_SECURITY_ERR);

  if (!aBuilder) {
    return NS_OK;
  }

  BuildSubmenu(EmptyString(), this, aBuilder);

  return NS_OK;
}


bool
HTMLMenuElement::ParseAttribute(int32_t aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None && aAttribute == nsGkAtoms::type) {
    bool success = aResult.ParseEnumValue(aValue, kMenuTypeTable,
                                            false);
    if (success) {
      mType = aResult.GetEnumValue();
    } else {
      mType = kMenuDefaultType->value;
    }

    return success;
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

void
HTMLMenuElement::BuildSubmenu(const nsAString& aLabel,
                              nsIContent* aContent,
                              nsIMenuBuilder* aBuilder)
{
  aBuilder->OpenContainer(aLabel);

  int8_t separator = ST_TRUE_INIT;
  TraverseContent(aContent, aBuilder, separator);

  if (separator == ST_TRUE) {
    aBuilder->UndoAddSeparator();
  }

  aBuilder->CloseContainer();
}

// static
bool
HTMLMenuElement::CanLoadIcon(nsIContent* aContent, const nsAString& aIcon)
{
  if (aIcon.IsEmpty()) {
    return false;
  }

  nsIDocument* doc = aContent->OwnerDoc();

  nsCOMPtr<nsIURI> baseURI = aContent->GetBaseURI();
  nsCOMPtr<nsIURI> uri;
  nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(uri), aIcon, doc,
                                            baseURI);

  if (!uri) {
    return false;
  }

  return nsContentUtils::CanLoadImage(uri, aContent, doc,
                                      aContent->NodePrincipal());
}

void
HTMLMenuElement::TraverseContent(nsIContent* aContent,
                                 nsIMenuBuilder* aBuilder,
                                 int8_t& aSeparator)
{
  nsCOMPtr<nsIContent> child;
  for (child = aContent->GetFirstChild(); child;
       child = child->GetNextSibling()) {
    nsGenericHTMLElement* element = nsGenericHTMLElement::FromContent(child);
    if (!element) {
      continue;
    }

    nsIAtom* tag = child->Tag();

    if (tag == nsGkAtoms::menuitem) {
      HTMLMenuItemElement* menuitem = HTMLMenuItemElement::FromContent(child);

      if (menuitem->IsHidden()) {
        continue;
      }

      nsAutoString label;
      menuitem->GetLabel(label);
      if (label.IsEmpty()) {
        continue;
      }

      nsAutoString icon;
      menuitem->GetIcon(icon);

      aBuilder->AddItemFor(menuitem, CanLoadIcon(child, icon));

      aSeparator = ST_FALSE;
    } else if (tag == nsGkAtoms::menu && !element->IsHidden()) {
      if (child->HasAttr(kNameSpaceID_None, nsGkAtoms::label)) {
        nsAutoString label;
        child->GetAttr(kNameSpaceID_None, nsGkAtoms::label, label);

        BuildSubmenu(label, child, aBuilder);

        aSeparator = ST_FALSE;
      } else {
        AddSeparator(aBuilder, aSeparator);

        TraverseContent(child, aBuilder, aSeparator);

        AddSeparator(aBuilder, aSeparator);
      }
    }
  }
}

inline void
HTMLMenuElement::AddSeparator(nsIMenuBuilder* aBuilder, int8_t& aSeparator)
{
  if (aSeparator) {
    return;
  }
 
  aBuilder->AddSeparator();
  aSeparator = ST_TRUE;
}

JSObject*
HTMLMenuElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return HTMLMenuElementBinding::Wrap(aCx, aScope, this);
}

} // namespace dom
} // namespace mozilla
