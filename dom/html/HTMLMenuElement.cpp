/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLMenuElement.h"

#include "mozilla/BasicEvents.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/dom/HTMLMenuElementBinding.h"
#include "mozilla/dom/HTMLMenuItemElement.h"
#include "nsIMenuBuilder.h"
#include "nsAttrValueInlines.h"
#include "nsContentUtils.h"
#include "nsIURI.h"

#define HTMLMENUBUILDER_CONTRACTID "@mozilla.org/content/html-menu-builder;1"

NS_IMPL_NS_NEW_HTML_ELEMENT(Menu)

namespace mozilla {
namespace dom {

enum MenuType : uint8_t
{
  MENU_TYPE_CONTEXT = 1,
  MENU_TYPE_TOOLBAR
};

static const nsAttrValue::EnumTable kMenuTypeTable[] = {
  { "context", MENU_TYPE_CONTEXT },
  { "toolbar", MENU_TYPE_TOOLBAR },
  { nullptr, 0 }
};

static const nsAttrValue::EnumTable* kMenuDefaultType =
  &kMenuTypeTable[1];

enum SeparatorType
{
  ST_TRUE_INIT = -1,
  ST_FALSE = 0,
  ST_TRUE = 1
};



HTMLMenuElement::HTMLMenuElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo), mType(MENU_TYPE_TOOLBAR)
{
}

HTMLMenuElement::~HTMLMenuElement()
{
}

NS_IMPL_ELEMENT_CLONE(HTMLMenuElement)


void
HTMLMenuElement::SendShowEvent()
{
  nsCOMPtr<nsIDocument> document = GetComposedDoc();
  if (!document) {
    return;
  }

  WidgetEvent event(true, eShow);
  event.mFlags.mBubbles = false;
  event.mFlags.mCancelable = false;

  RefPtr<nsPresContext> presContext = document->GetPresContext();
  if (!presContext) {
    return;
  }

  nsEventStatus status = nsEventStatus_eIgnore;
  EventDispatcher::Dispatch(static_cast<nsIContent*>(this), presContext,
                            &event, nullptr, &status);
}

already_AddRefed<nsIMenuBuilder>
HTMLMenuElement::CreateBuilder()
{
  if (mType != MENU_TYPE_CONTEXT) {
    return nullptr;
  }

  nsCOMPtr<nsIMenuBuilder> builder = do_CreateInstance(HTMLMENUBUILDER_CONTRACTID);
  NS_WARNING_ASSERTION(builder, "No builder available");
  return builder.forget();
}

void
HTMLMenuElement::Build(nsIMenuBuilder* aBuilder)
{
  if (!aBuilder) {
    return;
  }

  BuildSubmenu(EmptyString(), this, aBuilder);
}

nsresult
HTMLMenuElement::AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                              const nsAttrValue* aValue,
                              const nsAttrValue* aOldValue,
                              nsIPrincipal* aSubjectPrincipal,
                              bool aNotify)
{
  if (aNameSpaceID == kNameSpaceID_None && aName == nsGkAtoms::type) {
    if (aValue) {
      mType = aValue->GetEnumValue();
    } else {
      mType = kMenuDefaultType->value;
    }
  }

  return nsGenericHTMLElement::AfterSetAttr(aNameSpaceID, aName, aValue,
                                            aOldValue, aSubjectPrincipal, aNotify);
}

bool
HTMLMenuElement::ParseAttribute(int32_t aNamespaceID,
                                nsAtom* aAttribute,
                                const nsAString& aValue,
                                nsIPrincipal* aMaybeScriptedPrincipal,
                                nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None && aAttribute == nsGkAtoms::type) {
    return aResult.ParseEnumValue(aValue, kMenuTypeTable, false,
                                  kMenuDefaultType);
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aMaybeScriptedPrincipal, aResult);
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
    nsGenericHTMLElement* element = nsGenericHTMLElement::FromNode(child);
    if (!element) {
      continue;
    }

    if (child->IsHTMLElement(nsGkAtoms::menuitem)) {
      HTMLMenuItemElement* menuitem = HTMLMenuItemElement::FromNode(child);

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
    } else if (child->IsHTMLElement(nsGkAtoms::hr)) {
      aBuilder->AddSeparator();
    } else if (child->IsHTMLElement(nsGkAtoms::menu) && !element->IsHidden()) {
      if (child->AsElement()->HasAttr(kNameSpaceID_None, nsGkAtoms::label)) {
        nsAutoString label;
        child->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::label, label);

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
HTMLMenuElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLMenuElement_Binding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
