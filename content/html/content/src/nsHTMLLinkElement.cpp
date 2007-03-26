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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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
#include "nsIDOMHTMLLinkElement.h"
#include "nsIDOMLinkStyle.h"
#include "nsIDOMEventReceiver.h"
#include "nsGenericHTMLElement.h"
#include "nsILink.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsIDOMStyleSheet.h"
#include "nsIStyleSheet.h"
#include "nsIStyleSheetLinkingElement.h"
#include "nsStyleLinkElement.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsIDocument.h"
#include "nsIDOMEvent.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIDOMDocumentEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsParserUtils.h"
#include "nsContentUtils.h"
#include "nsPIDOMWindow.h"

class nsHTMLLinkElement : public nsGenericHTMLElement,
                          public nsIDOMHTMLLinkElement,
                          public nsILink,
                          public nsStyleLinkElement
{
public:
  nsHTMLLinkElement(nsINodeInfo *aNodeInfo);
  virtual ~nsHTMLLinkElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLLinkElement
  NS_DECL_NSIDOMHTMLLINKELEMENT

  // nsILink
  NS_IMETHOD    GetLinkState(nsLinkState &aState);
  NS_IMETHOD    SetLinkState(nsLinkState aState);
  NS_IMETHOD    GetHrefURI(nsIURI** aURI);
  NS_IMETHOD    LinkAdded();
  NS_IMETHOD    LinkRemoved();

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              PRBool aCompileEventHandlers);
  virtual void UnbindFromTree(PRBool aDeep = PR_TRUE,
                              PRBool aNullParent = PR_TRUE);
  void CreateAndDispatchEvent(nsIDocument* aDoc, const nsAString& aEventName);
  nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, PRBool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nsnull, aValue, aNotify);
  }
  virtual nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           PRBool aNotify);
  virtual nsresult UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                             PRBool aNotify);

  virtual nsresult PostHandleEvent(nsEventChainPostVisitor& aVisitor);
  virtual PRBool IsLink(nsIURI** aURI) const;
  virtual void GetLinkTarget(nsAString& aTarget);

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

protected:
  virtual void GetStyleSheetURL(PRBool* aIsInline,
                                nsIURI** aURI);
  virtual void GetStyleSheetInfo(nsAString& aTitle,
                                 nsAString& aType,
                                 nsAString& aMedia,
                                 PRBool* aIsAlternate);
 
  // The cached visited state
  nsLinkState mLinkState;
};


NS_IMPL_NS_NEW_HTML_ELEMENT(Link)


nsHTMLLinkElement::nsHTMLLinkElement(nsINodeInfo *aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo),
    mLinkState(eLinkState_Unknown)
{
}

nsHTMLLinkElement::~nsHTMLLinkElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLLinkElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLLinkElement, nsGenericElement) 


// QueryInterface implementation for nsHTMLLinkElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLLinkElement, nsGenericHTMLElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLLinkElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMLinkStyle)
  NS_INTERFACE_MAP_ENTRY(nsILink)
  NS_INTERFACE_MAP_ENTRY(nsIStyleSheetLinkingElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLLinkElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


NS_IMPL_ELEMENT_CLONE(nsHTMLLinkElement)


NS_IMETHODIMP
nsHTMLLinkElement::GetDisabled(PRBool* aDisabled)
{
  nsCOMPtr<nsIDOMStyleSheet> ss(do_QueryInterface(mStyleSheet));
  nsresult result = NS_OK;

  if (ss) {
    result = ss->GetDisabled(aDisabled);
  } else {
    *aDisabled = PR_FALSE;
  }

  return result;
}

NS_IMETHODIMP 
nsHTMLLinkElement::SetDisabled(PRBool aDisabled)
{
  nsCOMPtr<nsIDOMStyleSheet> ss(do_QueryInterface(mStyleSheet));
  nsresult result = NS_OK;

  if (ss) {
    result = ss->SetDisabled(aDisabled);
  }

  return result;
}


NS_IMPL_STRING_ATTR(nsHTMLLinkElement, Charset, charset)
NS_IMPL_URI_ATTR(nsHTMLLinkElement, Href, href)
NS_IMPL_STRING_ATTR(nsHTMLLinkElement, Hreflang, hreflang)
NS_IMPL_STRING_ATTR(nsHTMLLinkElement, Media, media)
NS_IMPL_STRING_ATTR(nsHTMLLinkElement, Rel, rel)
NS_IMPL_STRING_ATTR(nsHTMLLinkElement, Rev, rev)
NS_IMPL_STRING_ATTR(nsHTMLLinkElement, Target, target)
NS_IMPL_STRING_ATTR(nsHTMLLinkElement, Type, type)

nsresult
nsHTMLLinkElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              PRBool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument, aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  UpdateStyleSheet(nsnull);

  // XXXbz we really shouldn't fire the event until after we've finished with
  // the outermost BindToTree...  In particular, this can effectively cause us
  // to reenter this code, or for some part of the document to become unbound
  // inside the event!
  CreateAndDispatchEvent(aDocument, NS_LITERAL_STRING("DOMLinkAdded"));

  return rv;  
}

NS_IMETHODIMP
nsHTMLLinkElement::LinkAdded()
{
  CreateAndDispatchEvent(GetOwnerDoc(), NS_LITERAL_STRING("DOMLinkAdded"));
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLLinkElement::LinkRemoved()
{
  CreateAndDispatchEvent(GetOwnerDoc(), NS_LITERAL_STRING("DOMLinkRemoved"));
  return NS_OK;
}

void
nsHTMLLinkElement::UnbindFromTree(PRBool aDeep, PRBool aNullParent)
{
  nsCOMPtr<nsIDocument> oldDoc = GetCurrentDoc();
  if (oldDoc) {
    GetCurrentDoc()->ForgetLink(this);
    // If this link is ever reinserted into a document, it might
    // be under a different xml:base, so forget the cached state now
    mLinkState = eLinkState_Unknown;
  }

  // XXXbz we really shouldn't fire the event until after we've finished with
  // the outermost UnbindFromTree...  In particular, this can effectively cause
  // us to reenter this code, or to be bound to a different tree inside the
  // event!
  CreateAndDispatchEvent(oldDoc, NS_LITERAL_STRING("DOMLinkRemoved"));
  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);
  UpdateStyleSheet(oldDoc);
}

void
nsHTMLLinkElement::CreateAndDispatchEvent(nsIDocument* aDoc,
                                          const nsAString& aEventName)
{
  if (!aDoc)
    return;

  // In the unlikely case that both rev is specified *and* rel=stylesheet,
  // this code will cause the event to fire, on the principle that maybe the
  // page really does want to specify that it's author is a stylesheet. Since
  // this should never actually happen and the performance hit is minimal,
  // doing the "right" thing costs virtually nothing here, even if it doesn't
  // make much sense.
  static nsIContent::AttrValuesArray strings[] =
    {&nsGkAtoms::_empty, &nsGkAtoms::stylesheet, nsnull};

  if (!nsContentUtils::HasNonEmptyAttr(this, kNameSpaceID_None,
                                       nsGkAtoms::rev) &&
      FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::rel,
                      strings, eIgnoreCase) != ATTR_VALUE_NO_MATCH)
    return;

  nsContentUtils::DispatchTrustedEvent(aDoc,
                                       NS_STATIC_CAST(nsIContent*, this),
                                       aEventName, PR_TRUE, PR_TRUE);
}

nsresult
nsHTMLLinkElement::SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           PRBool aNotify)
{
  if (aName == nsGkAtoms::href && kNameSpaceID_None == aNameSpaceID) {
    nsIDocument* doc = GetCurrentDoc();
    if (doc) {
      doc->ForgetLink(this);
        // The change to 'href' will cause style reresolution which will
        // eventually recompute the link state and re-add this element
        // to the link map if necessary.
    }
    SetLinkState(eLinkState_Unknown);
  }

  nsresult rv = nsGenericHTMLElement::SetAttr(aNameSpaceID, aName, aPrefix,
                                              aValue, aNotify);
  if (NS_SUCCEEDED(rv)) {
    UpdateStyleSheet(nsnull, nsnull,
                     aNameSpaceID == kNameSpaceID_None &&
                     (aName == nsGkAtoms::rel ||
                      aName == nsGkAtoms::title ||
                      aName == nsGkAtoms::media ||
                      aName == nsGkAtoms::type));
  }

  return rv;
}

nsresult
nsHTMLLinkElement::UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                             PRBool aNotify)
{
  nsresult rv = nsGenericHTMLElement::UnsetAttr(aNameSpaceID, aAttribute,
                                                aNotify);
  if (NS_SUCCEEDED(rv)) {
    UpdateStyleSheet(nsnull, nsnull,
                     aNameSpaceID == kNameSpaceID_None &&
                     (aAttribute == nsGkAtoms::rel ||
                      aAttribute == nsGkAtoms::title ||
                      aAttribute == nsGkAtoms::media ||
                      aAttribute == nsGkAtoms::type));
  }

  return rv;
}

nsresult
nsHTMLLinkElement::PostHandleEvent(nsEventChainPostVisitor& aVisitor)
{
  return PostHandleEventForAnchors(aVisitor);
}

PRBool
nsHTMLLinkElement::IsLink(nsIURI** aURI) const
{
  return IsHTMLLink(aURI);
}

void
nsHTMLLinkElement::GetLinkTarget(nsAString& aTarget)
{
  GetAttr(kNameSpaceID_None, nsGkAtoms::target, aTarget);
  if (aTarget.IsEmpty()) {
    GetBaseTarget(aTarget);
  }
}

NS_IMETHODIMP
nsHTMLLinkElement::GetLinkState(nsLinkState &aState)
{
  aState = mLinkState;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLLinkElement::SetLinkState(nsLinkState aState)
{
  mLinkState = aState;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLLinkElement::GetHrefURI(nsIURI** aURI)
{
  return GetHrefURIForAnchors(aURI);
}

void
nsHTMLLinkElement::GetStyleSheetURL(PRBool* aIsInline,
                                    nsIURI** aURI)
{
  *aIsInline = PR_FALSE;
  GetHrefURIForAnchors(aURI);
  return;
}

void
nsHTMLLinkElement::GetStyleSheetInfo(nsAString& aTitle,
                                     nsAString& aType,
                                     nsAString& aMedia,
                                     PRBool* aIsAlternate)
{
  aTitle.Truncate();
  aType.Truncate();
  aMedia.Truncate();
  *aIsAlternate = PR_FALSE;

  nsAutoString rel;
  nsStringArray linkTypes(4);
  GetAttr(kNameSpaceID_None, nsGkAtoms::rel, rel);
  nsStyleLinkElement::ParseLinkTypes(rel, linkTypes);
  // Is it a stylesheet link?
  if (linkTypes.IndexOf(NS_LITERAL_STRING("stylesheet")) < 0) {
    return;
  }

  nsAutoString title;
  GetAttr(kNameSpaceID_None, nsGkAtoms::title, title);
  title.CompressWhitespace();
  aTitle.Assign(title);

  // If alternate, does it have title?
  if (-1 != linkTypes.IndexOf(NS_LITERAL_STRING("alternate"))) {
    if (aTitle.IsEmpty()) { // alternates must have title
      return;
    } else {
      *aIsAlternate = PR_TRUE;
    }
  }

  GetAttr(kNameSpaceID_None, nsGkAtoms::media, aMedia);
  ToLowerCase(aMedia); // HTML4.0 spec is inconsistent, make it case INSENSITIVE

  nsAutoString mimeType;
  nsAutoString notUsed;
  GetAttr(kNameSpaceID_None, nsGkAtoms::type, aType);
  nsParserUtils::SplitMimeType(aType, mimeType, notUsed);
  if (!mimeType.IsEmpty() && !mimeType.LowerCaseEqualsLiteral("text/css")) {
    return;
  }

  // If we get here we assume that we're loading a css file, so set the
  // type to 'text/css'
  aType.AssignLiteral("text/css");

  return;
}
