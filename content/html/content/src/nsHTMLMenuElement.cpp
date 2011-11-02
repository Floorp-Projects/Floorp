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

#include "nsHTMLMenuElement.h"

#include "nsIDOMHTMLMenuItemElement.h"
#include "nsXULContextMenuBuilder.h"
#include "nsGUIEvent.h"
#include "nsEventDispatcher.h"
#include "nsHTMLMenuItemElement.h"
#include "nsContentUtils.h"

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

NS_IMPL_NS_NEW_HTML_ELEMENT(Menu)


nsHTMLMenuElement::nsHTMLMenuElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo), mType(MENU_TYPE_LIST)
{
}

nsHTMLMenuElement::~nsHTMLMenuElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLMenuElement, nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsHTMLMenuElement, nsGenericElement)


DOMCI_NODE_DATA(HTMLMenuElement, nsHTMLMenuElement)

// QueryInterface implementation for nsHTMLMenuElement
NS_INTERFACE_TABLE_HEAD(nsHTMLMenuElement)
  NS_HTML_CONTENT_INTERFACE_TABLE2(nsHTMLMenuElement,
                                   nsIDOMHTMLMenuElement,
                                   nsIHTMLMenu)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLMenuElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLMenuElement)

NS_IMPL_ELEMENT_CLONE(nsHTMLMenuElement)

NS_IMPL_BOOL_ATTR(nsHTMLMenuElement, Compact, compact)
NS_IMPL_ENUM_ATTR_DEFAULT_VALUE(nsHTMLMenuElement, Type, type,
                                kMenuDefaultType->tag)
NS_IMPL_STRING_ATTR(nsHTMLMenuElement, Label, label)


NS_IMETHODIMP
nsHTMLMenuElement::SendShowEvent()
{
  NS_ENSURE_TRUE(nsContentUtils::IsCallerChrome(), NS_ERROR_DOM_SECURITY_ERR);

  nsCOMPtr<nsIDocument> document = GetCurrentDoc();
  if (!document) {
    return NS_ERROR_FAILURE;
  }

  nsEvent event(true, NS_SHOW_EVENT);
  event.flags |= NS_EVENT_FLAG_CANT_CANCEL | NS_EVENT_FLAG_CANT_BUBBLE;

  nsCOMPtr<nsIPresShell> shell = document->GetShell();
  if (!shell) {
    return NS_ERROR_FAILURE;
  }
 
  nsRefPtr<nsPresContext> presContext = shell->GetPresContext();
  nsEventStatus status = nsEventStatus_eIgnore;
  nsEventDispatcher::Dispatch(static_cast<nsIContent*>(this), presContext,
                              &event, nsnull, &status);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLMenuElement::CreateBuilder(nsIMenuBuilder** _retval)
{
  NS_ENSURE_TRUE(nsContentUtils::IsCallerChrome(), NS_ERROR_DOM_SECURITY_ERR);

  *_retval = nsnull;

  if (mType == MENU_TYPE_CONTEXT) {
    NS_ADDREF(*_retval = new nsXULContextMenuBuilder());
  }

  return NS_OK;
}


NS_IMETHODIMP
nsHTMLMenuElement::Build(nsIMenuBuilder* aBuilder)
{
  NS_ENSURE_TRUE(nsContentUtils::IsCallerChrome(), NS_ERROR_DOM_SECURITY_ERR);

  if (!aBuilder) {
    return NS_OK;
  }

  BuildSubmenu(EmptyString(), this, aBuilder);

  return NS_OK;
}


bool
nsHTMLMenuElement::ParseAttribute(PRInt32 aNamespaceID,
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
nsHTMLMenuElement::BuildSubmenu(const nsAString& aLabel,
                                nsIContent* aContent,
                                nsIMenuBuilder* aBuilder)
{
  aBuilder->OpenContainer(aLabel);

  PRInt8 separator = ST_TRUE_INIT;
  TraverseContent(aContent, aBuilder, separator);

  if (separator == ST_TRUE) {
    aBuilder->UndoAddSeparator();
  }

  aBuilder->CloseContainer();
}

// static
bool
nsHTMLMenuElement::CanLoadIcon(nsIContent* aContent, const nsAString& aIcon)
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
nsHTMLMenuElement::TraverseContent(nsIContent* aContent,
                                   nsIMenuBuilder* aBuilder,
                                   PRInt8& aSeparator)
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
      nsHTMLMenuItemElement* menuitem =
        nsHTMLMenuItemElement::FromContent(child);

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
nsHTMLMenuElement::AddSeparator(nsIMenuBuilder* aBuilder, PRInt8& aSeparator)
{
  if (aSeparator) {
    return;
  }
 
  aBuilder->AddSeparator();
  aSeparator = ST_TRUE;
}
