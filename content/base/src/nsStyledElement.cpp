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
#include "nsDOMCSSAttrDeclaration.h"
#include "nsServiceManagerUtils.h"
#include "nsIDocument.h"
#include "nsICSSStyleRule.h"
#include "nsCSSParser.h"
#include "mozilla/css/Loader.h"
#include "nsIDOMMutationEvent.h"
#include "nsXULElement.h"

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

nsIAtom*
nsStyledElement::DoGetID() const
{
  NS_ASSERTION(HasFlag(NODE_HAS_ID), "Unexpected call");

  // The nullcheck here is needed because nsGenericElement::UnsetAttr calls
  // out to various code between removing the attribute and we get a chance to
  // clear the NODE_HAS_ID flag.

  const nsAttrValue* attr = mAttrsAndChildren.GetAttr(nsGkAtoms::id);

  return attr ? attr->GetAtomValue() : nsnull;
}

const nsAttrValue*
nsStyledElement::DoGetClasses() const
{
  NS_ASSERTION(HasFlag(NODE_MAY_HAVE_CLASS), "Unexpected call");
  return mAttrsAndChildren.GetAttr(nsGkAtoms::_class);
}

PRBool
nsStyledElement::ParseAttribute(PRInt32 aNamespaceID, nsIAtom* aAttribute,
                                const nsAString& aValue, nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::style) {
      SetFlags(NODE_MAY_HAVE_STYLE);
      ParseStyleAttribute(aValue, aResult, PR_FALSE);
      return PR_TRUE;
    }
    if (aAttribute == nsGkAtoms::_class) {
      SetFlags(NODE_MAY_HAVE_CLASS);
      aResult.ParseAtomArray(aValue);
      return PR_TRUE;
    }
    if (aAttribute == nsGkAtoms::id) {
      // Store id as an atom.  id="" means that the element has no id,
      // not that it has an emptystring as the id.
      RemoveFromIdTable();
      if (aValue.IsEmpty()) {
        UnsetFlags(NODE_HAS_ID);
        return PR_FALSE;
      }
      aResult.ParseAtom(aValue);
      SetFlags(NODE_HAS_ID);
      AddToIdTable(aResult.GetAtomValue());
      return PR_TRUE;
    }
  }

  return nsStyledElementBase::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                             aResult);
}

nsresult
nsStyledElement::UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                           PRBool aNotify)
{
  if (aAttribute == nsGkAtoms::id && aNameSpaceID == kNameSpaceID_None) {
    // Have to do this before clearing flag. See RemoveFromIdTable
    RemoveFromIdTable();
  }

  return nsGenericElement::UnsetAttr(aNameSpaceID, aAttribute, aNotify);
}

nsresult
nsStyledElement::AfterSetAttr(PRInt32 aNamespaceID, nsIAtom* aAttribute,
                              const nsAString* aValue, PRBool aNotify)
{
  if (aNamespaceID == kNameSpaceID_None && !aValue &&
      aAttribute == nsGkAtoms::id) {
    // The id has been removed when calling UnsetAttr but we kept it because
    // the id is used for some layout stuff between UnsetAttr and AfterSetAttr.
    // Now. the id is really removed so it would not be safe to keep this flag.
    UnsetFlags(NODE_HAS_ID);
  }

  return nsGenericElement::AfterSetAttr(aNamespaceID, aAttribute, aValue,
                                        aNotify);
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

  nsAttrValue attrValue(aStyleRule, nsnull);

  // XXXbz do we ever end up with ADDITION here?  I doubt it.
  PRUint8 modType = modification ?
    static_cast<PRUint8>(nsIDOMMutationEvent::MODIFICATION) :
    static_cast<PRUint8>(nsIDOMMutationEvent::ADDITION);

  return SetAttrAndNotify(kNameSpaceID_None, nsGkAtoms::style, nsnull,
                          oldValueStr, attrValue, modType, hasListeners,
                          aNotify, nsnull);
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

  if (aDocument && HasFlag(NODE_HAS_ID) && !GetBindingParent()) {
    aDocument->AddToIdTable(this, DoGetID());
  }

  if (!IsXUL()) {
    // XXXbz if we already have a style attr parsed, this won't do
    // anything... need to fix that.
    ReparseStyleAttribute(PR_FALSE);
  }

  return NS_OK;
}

void
nsStyledElement::UnbindFromTree(PRBool aDeep, PRBool aNullParent)
{
  nsIDocument* doc = nsnull;

  if (HasFlag(NODE_HAS_ID)) {
    doc = GetCurrentDoc();
  }

  nsStyledElementBase::UnbindFromTree(aDeep, aNullParent);

  // If we had a document (and an id), we should now inform the document that
  // we are no longer a valid id.
  // This is done _after_ the call to UnbindFromTree to make sure that id are
  // removed from the inner-most elements to the top-most.
  if (doc) {
    nsIAtom* id = DoGetID();
    if (id) {
      doc->RemoveFromIdTable(this, id);
    }
  }
}


// ---------------------------------------------------------------
// Others and helpers

nsIDOMCSSStyleDeclaration*
nsStyledElement::GetStyle(nsresult* retval)
{
  nsXULElement* xulElement = nsXULElement::FromContent(this);
  if (xulElement) {
    nsresult rv = xulElement->EnsureLocalStyle();
    if (NS_FAILED(rv)) {
      *retval = rv;
      return nsnull;
    }
  }
    
  nsGenericElement::nsDOMSlots *slots = GetDOMSlots();

  if (!slots->mStyle) {
    // Just in case...
    ReparseStyleAttribute(PR_TRUE);

    slots->mStyle = new nsDOMCSSAttributeDeclaration(this
#ifdef MOZ_SMIL
                                                     , PR_FALSE
#endif // MOZ_SMIL
                                                     );
    SetFlags(NODE_MAY_HAVE_STYLE);
  }

  *retval = NS_OK;
  return slots->mStyle;
}

nsresult
nsStyledElement::ReparseStyleAttribute(PRBool aForceInDataDoc)
{
  if (!HasFlag(NODE_MAY_HAVE_STYLE)) {
    return NS_OK;
  }
  const nsAttrValue* oldVal = mAttrsAndChildren.GetAttr(nsGkAtoms::style);
  
  if (oldVal && oldVal->Type() != nsAttrValue::eCSSStyleRule) {
    nsAttrValue attrValue;
    nsAutoString stringValue;
    oldVal->ToString(stringValue);
    ParseStyleAttribute(stringValue, attrValue, aForceInDataDoc);
    // Don't bother going through SetInlineStyleRule, we don't want to fire off
    // mutation events or document notifications anyway
    nsresult rv = mAttrsAndChildren.SetAndTakeAttr(nsGkAtoms::style, attrValue);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  
  return NS_OK;
}

void
nsStyledElement::ParseStyleAttribute(const nsAString& aValue,
                                     nsAttrValue& aResult,
                                     PRBool aForceInDataDoc)
{
  nsIDocument* doc = GetOwnerDoc();

  if (doc && (aForceInDataDoc ||
              !doc->IsLoadedAsData() ||
              doc->IsStaticDocument())) {
    PRBool isCSS = PR_TRUE; // assume CSS until proven otherwise

    if (!IsInNativeAnonymousSubtree()) {  // native anonymous content
                                          // always assumes CSS
      nsAutoString styleType;
      doc->GetHeaderData(nsGkAtoms::headerContentStyleType, styleType);
      if (!styleType.IsEmpty()) {
        static const char textCssStr[] = "text/css";
        isCSS = (styleType.EqualsIgnoreCase(textCssStr, sizeof(textCssStr) - 1));
      }
    }

    if (isCSS) {
      mozilla::css::Loader* cssLoader = doc->CSSLoader();
      nsCSSParser cssParser(cssLoader);
      if (cssParser) {
        nsCOMPtr<nsIURI> baseURI = GetBaseURI();

        nsCOMPtr<nsICSSStyleRule> rule;
        cssParser.ParseStyleAttribute(aValue, doc->GetDocumentURI(),
                                      baseURI,
                                      NodePrincipal(),
                                      getter_AddRefs(rule));
        if (rule) {
          aResult.SetTo(rule, &aValue);
          return;
        }
      }
    }
  }

  aResult.SetTo(aValue);
}
