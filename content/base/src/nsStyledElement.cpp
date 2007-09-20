/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set tw=80 expandtab softtabstop=2 ts=2 sw=2: */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mats Palmgren <mats.palmgren@bredband.net>
 *   Daniel Kraft <d@domob.eu>
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

#include "nsStyledElement.h"
#include "nsGkAtoms.h"
#include "nsAttrValue.h"
#include "nsGenericElement.h"
#include "nsMutationEvent.h"
#include "nsDOMCSSDeclaration.h"
#include "nsICSSOMFactory.h"
#include "nsServiceManagerUtils.h"
#include "nsIDocument.h"
#include "nsICSSStyleRule.h"
#include "nsICSSParser.h"
#include "nsICSSLoader.h"

#ifdef MOZ_SVG
#include "nsIDOMSVGStylable.h"
#endif

//----------------------------------------------------------------------
// nsIContent methods

nsIAtom*
nsStyledElement::GetClassAttributeName() const
{
  return nsGkAtoms::_class;
}

nsIAtom*
nsStyledElement::GetIDAttributeName() const
{
  return nsGkAtoms::id;
}

const nsAttrValue*
nsStyledElement::GetClasses() const
{
  if (!HasFlag(NODE_MAY_HAVE_CLASS)) {
    return nsnull;
  }
  return mAttrsAndChildren.GetAttr(nsGkAtoms::_class);
}

PRBool
nsStyledElement::ParseAttribute(PRInt32 aNamespaceID, nsIAtom* aAttribute,
                                const nsAString& aValue, nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::style) {
      SetFlags(NODE_MAY_HAVE_STYLE);
      ParseStyleAttribute(this, aValue, aResult);
      return PR_TRUE;
    }
    if (aAttribute == nsGkAtoms::_class) {
      SetFlags(NODE_MAY_HAVE_CLASS);
#ifdef MOZ_SVG
      NS_ASSERTION(!nsCOMPtr<nsIDOMSVGStylable>(do_QueryInterface(this)),
                   "SVG code should have handled this 'class' attribute!");
#endif
      aResult.ParseAtomArray(aValue);
      return PR_TRUE;
    }
  }

  return nsStyledElementBase::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                             aResult);
}

NS_IMETHODIMP
nsStyledElement::SetInlineStyleRule(nsICSSStyleRule* aStyleRule, PRBool aNotify)
{
  SetFlags(NODE_MAY_HAVE_STYLE);
  PRBool modification = PR_FALSE;
  nsAutoString oldValueStr;

  PRBool hasListeners = aNotify &&
    nsContentUtils::HasMutationListeners(this,
                                         NS_EVENT_BITS_MUTATION_ATTRMODIFIED,
                                         this);

  // There's no point in comparing the stylerule pointers since we're always
  // getting a new stylerule here. And we can't compare the stringvalues of
  // the old and the new rules since both will point to the same declaration
  // and thus will be the same.
  if (hasListeners) {
    // save the old attribute so we can set up the mutation event properly
    // XXXbz if the old rule points to the same declaration as the new one,
    // this is getting the new attr value, not the old one....
    modification = GetAttr(kNameSpaceID_None, nsGkAtoms::style,
                           oldValueStr);
  }
  else if (aNotify && IsInDoc()) {
    modification = !!mAttrsAndChildren.GetAttr(nsGkAtoms::style);
  }

  nsAttrValue attrValue(aStyleRule);

  return SetAttrAndNotify(kNameSpaceID_None, nsGkAtoms::style, nsnull,
                          oldValueStr, attrValue, modification, hasListeners,
                          aNotify);
}

nsICSSStyleRule*
nsStyledElement::GetInlineStyleRule()
{
  if (!HasFlag(NODE_MAY_HAVE_STYLE)) {
    return nsnull;
  }
  const nsAttrValue* attrVal = mAttrsAndChildren.GetAttr(nsGkAtoms::style);

  if (attrVal && attrVal->Type() == nsAttrValue::eCSSStyleRule) {
    return attrVal->GetCSSStyleRuleValue();
  }

  return nsnull;
}

nsresult
nsStyledElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                            nsIContent* aBindingParent,
                            PRBool aCompileEventHandlers)
{
  nsresult rv = nsStyledElementBase::BindToTree(aDocument, aParent,
                                                aBindingParent,
                                                aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  // XXXbz if we already have a style attr parsed, this won't do
  // anything... need to fix that.
  ReparseStyleAttribute();

  return rv;
}

// ---------------------------------------------------------------
// Others and helpers

static nsICSSOMFactory* gCSSOMFactory = nsnull;
static NS_DEFINE_CID(kCSSOMFactoryCID, NS_CSSOMFACTORY_CID);

nsresult
nsStyledElement::GetStyle(nsIDOMCSSStyleDeclaration** aStyle)
{
  nsGenericElement::nsDOMSlots *slots = GetDOMSlots();
  NS_ENSURE_TRUE(slots, NS_ERROR_OUT_OF_MEMORY);

  if (!slots->mStyle) {
    // Just in case...
    ReparseStyleAttribute();

    nsresult rv;
    if (!gCSSOMFactory) {
      rv = CallGetService(kCSSOMFactoryCID, &gCSSOMFactory);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    rv = gCSSOMFactory->CreateDOMCSSAttributeDeclaration(this,
                                                 getter_AddRefs(slots->mStyle));
    NS_ENSURE_SUCCESS(rv, rv);
    SetFlags(NODE_MAY_HAVE_STYLE);
  }

  // Why bother with QI?
  NS_ADDREF(*aStyle = slots->mStyle);
  return NS_OK;
}

nsresult
nsStyledElement::ReparseStyleAttribute()
{
  if (!HasFlag(NODE_MAY_HAVE_STYLE)) {
    return NS_OK;
  }
  const nsAttrValue* oldVal = mAttrsAndChildren.GetAttr(nsGkAtoms::style);
  
  if (oldVal && oldVal->Type() != nsAttrValue::eCSSStyleRule) {
    nsAttrValue attrValue;
    nsAutoString stringValue;
    oldVal->ToString(stringValue);
    ParseStyleAttribute(this, stringValue, attrValue);
    // Don't bother going through SetInlineStyleRule, we don't want to fire off
    // mutation events or document notifications anyway
    nsresult rv = mAttrsAndChildren.SetAndTakeAttr(nsGkAtoms::style, attrValue);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  
  return NS_OK;
}

void
nsStyledElement::ParseStyleAttribute(nsIContent* aContent,
                                     const nsAString& aValue,
                                     nsAttrValue& aResult)
{
  nsresult result = NS_OK;
  nsIDocument* doc = aContent->GetOwnerDoc();

  if (doc) {
    PRBool isCSS = PR_TRUE; // assume CSS until proven otherwise

    if (!aContent->IsNativeAnonymous()) {  // native anonymous content
                                           // always assumes CSS
      nsAutoString styleType;
      doc->GetHeaderData(nsGkAtoms::headerContentStyleType, styleType);
      if (!styleType.IsEmpty()) {
        static const char textCssStr[] = "text/css";
        isCSS = (styleType.EqualsIgnoreCase(textCssStr, sizeof(textCssStr) - 1));
      }
    }

    if (isCSS) {
      nsICSSLoader* cssLoader = doc->CSSLoader();
      nsCOMPtr<nsICSSParser> cssParser;
      result = cssLoader->GetParserFor(nsnull, getter_AddRefs(cssParser));
      if (cssParser) {
        nsCOMPtr<nsIURI> baseURI = aContent->GetBaseURI();

        nsCOMPtr<nsICSSStyleRule> rule;
        result = cssParser->ParseStyleAttribute(aValue, doc->GetDocumentURI(),
                                                baseURI,
                                                aContent->NodePrincipal(),
                                                getter_AddRefs(rule));
        cssLoader->RecycleParser(cssParser);

        if (rule) {
          aResult.SetTo(rule);
          return;
        }
      }
    }
  }

  aResult.SetTo(aValue);
}


/* static */ void
nsStyledElement::Shutdown()
{
  NS_IF_RELEASE(gCSSOMFactory);
}
