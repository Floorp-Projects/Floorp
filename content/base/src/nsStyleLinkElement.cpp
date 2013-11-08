/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A base class which implements nsIStyleSheetLinkingElement and can
 * be subclassed by various content nodes that want to load
 * stylesheets (<style>, <link>, processing instructions, etc).
 */

#include "nsStyleLinkElement.h"

#include "mozilla/css/Loader.h"
#include "mozilla/dom/Element.h"
#include "nsCSSStyleSheet.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMComment.h"
#include "nsIDOMNode.h"
#include "nsIDOMStyleSheet.h"
#include "nsNetUtil.h"
#include "nsUnicharUtils.h"
#include "nsCRT.h"
#include "nsXPCOMCIDInternal.h"
#include "nsUnicharInputStream.h"
#include "nsContentUtils.h"
#include "nsStyleUtil.h"

using namespace mozilla;
using namespace mozilla::dom;

nsStyleLinkElement::nsStyleLinkElement()
  : mDontLoadStyle(false)
  , mUpdatesEnabled(true)
  , mLineNumber(1)
{
}

nsStyleLinkElement::~nsStyleLinkElement()
{
  nsStyleLinkElement::SetStyleSheet(nullptr);
}

void
nsStyleLinkElement::Unlink()
{
  mStyleSheet = nullptr;
}

void
nsStyleLinkElement::Traverse(nsCycleCollectionTraversalCallback &cb)
{
  nsStyleLinkElement* tmp = this;
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mStyleSheet);
}

NS_IMETHODIMP 
nsStyleLinkElement::SetStyleSheet(nsCSSStyleSheet* aStyleSheet)
{
  if (mStyleSheet) {
    mStyleSheet->SetOwningNode(nullptr);
  }

  mStyleSheet = aStyleSheet;
  if (mStyleSheet) {
    nsCOMPtr<nsINode> node = do_QueryObject(this);
    if (node) {
      mStyleSheet->SetOwningNode(node);
    }
  }
    
  return NS_OK;
}

NS_IMETHODIMP 
nsStyleLinkElement::GetStyleSheet(nsIStyleSheet*& aStyleSheet)
{
  aStyleSheet = mStyleSheet;
  NS_IF_ADDREF(aStyleSheet);

  return NS_OK;
}

NS_IMETHODIMP 
nsStyleLinkElement::InitStyleLinkElement(bool aDontLoadStyle)
{
  mDontLoadStyle = aDontLoadStyle;

  return NS_OK;
}

NS_IMETHODIMP
nsStyleLinkElement::SetEnableUpdates(bool aEnableUpdates)
{
  mUpdatesEnabled = aEnableUpdates;

  return NS_OK;
}

NS_IMETHODIMP
nsStyleLinkElement::GetCharset(nsAString& aCharset)
{
  // descendants have to implement this themselves
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* virtual */ void
nsStyleLinkElement::OverrideBaseURI(nsIURI* aNewBaseURI)
{
  NS_NOTREACHED("Base URI can't be overriden in this implementation "
                "of nsIStyleSheetLinkingElement.");
}

/* virtual */ void
nsStyleLinkElement::SetLineNumber(uint32_t aLineNumber)
{
  mLineNumber = aLineNumber;
}

uint32_t ToLinkMask(const nsAString& aLink)
{ 
  if (aLink.EqualsLiteral("prefetch"))
     return PREFETCH;
  else if (aLink.EqualsLiteral("dns-prefetch"))
     return DNS_PREFETCH;
  else if (aLink.EqualsLiteral("stylesheet"))
    return STYLESHEET;
  else if (aLink.EqualsLiteral("next"))
    return NEXT;
  else if (aLink.EqualsLiteral("alternate"))
    return ALTERNATE;
  else 
    return 0;
}

uint32_t nsStyleLinkElement::ParseLinkTypes(const nsAString& aTypes)
{
  uint32_t linkMask = 0;
  nsAString::const_iterator start, done;
  aTypes.BeginReading(start);
  aTypes.EndReading(done);
  if (start == done)
    return linkMask;

  nsAString::const_iterator current(start);
  bool inString = !nsContentUtils::IsHTMLWhitespace(*current);
  nsAutoString subString;
  
  while (current != done) {
    if (nsContentUtils::IsHTMLWhitespace(*current)) {
      if (inString) {
        nsContentUtils::ASCIIToLower(Substring(start, current), subString);
        linkMask |= ToLinkMask(subString);
        inString = false;
      }
    }
    else {
      if (!inString) {
        start = current;
        inString = true;
      }
    }
    ++current;
  }
  if (inString) {
    nsContentUtils::ASCIIToLower(Substring(start, current), subString);
    linkMask |= ToLinkMask(subString);
  }
  return linkMask;
}

NS_IMETHODIMP
nsStyleLinkElement::UpdateStyleSheet(nsICSSLoaderObserver* aObserver,
                                     bool* aWillNotify,
                                     bool* aIsAlternate)
{
  return DoUpdateStyleSheet(nullptr, aObserver, aWillNotify, aIsAlternate,
                            false);
}

nsresult
nsStyleLinkElement::UpdateStyleSheetInternal(nsIDocument *aOldDocument,
                                             bool aForceUpdate)
{
  bool notify, alternate;
  return DoUpdateStyleSheet(aOldDocument, nullptr, &notify, &alternate,
                            aForceUpdate);
}

static bool
IsScopedStyleElement(nsIContent* aContent)
{
  // This is quicker than, say, QIing aContent to nsStyleLinkElement
  // and then calling its virtual GetStyleSheetInfo method to find out
  // if it is scoped.
  return (aContent->IsHTML(nsGkAtoms::style) ||
          aContent->IsSVG(nsGkAtoms::style)) &&
         aContent->HasAttr(kNameSpaceID_None, nsGkAtoms::scoped);
}

static void
SetIsElementInStyleScopeFlagOnSubtree(Element* aElement)
{
  if (aElement->IsElementInStyleScope()) {
    return;
  }

  aElement->SetIsElementInStyleScope();

  nsIContent* n = aElement->GetNextNode(aElement);
  while (n) {
    if (n->IsElementInStyleScope()) {
      n = n->GetNextNonChildNode(aElement);
    } else {
      if (n->IsElement()) {
        n->SetIsElementInStyleScope();
      }
      n = n->GetNextNode(aElement);
    }
  }
}

static bool
HasScopedStyleSheetChild(nsIContent* aContent)
{
  for (nsIContent* n = aContent->GetFirstChild(); n; n = n->GetNextSibling()) {
    if (IsScopedStyleElement(n)) {
      return true;
    }
  }
  return false;
}

// Called when aElement has had a <style scoped> child removed.
static void
UpdateIsElementInStyleScopeFlagOnSubtree(Element* aElement)
{
  NS_ASSERTION(aElement->IsElementInStyleScope(),
               "only call UpdateIsElementInStyleScopeFlagOnSubtree on a "
               "subtree that has IsElementInStyleScope boolean flag set");

  if (HasScopedStyleSheetChild(aElement)) {
    return;
  }

  aElement->ClearIsElementInStyleScope();

  nsIContent* n = aElement->GetNextNode(aElement);
  while (n) {
    if (HasScopedStyleSheetChild(n)) {
      n = n->GetNextNonChildNode(aElement);
    } else {
      if (n->IsElement()) {
        n->ClearIsElementInStyleScope();
      }
      n = n->GetNextNode(aElement);
    }
  }
}

static Element*
GetScopeElement(nsIStyleSheet* aSheet)
{
  nsRefPtr<nsCSSStyleSheet> cssStyleSheet = do_QueryObject(aSheet);
  if (!cssStyleSheet) {
    return nullptr;
  }

  return cssStyleSheet->GetScopeElement();
}

nsresult
nsStyleLinkElement::DoUpdateStyleSheet(nsIDocument *aOldDocument,
                                       nsICSSLoaderObserver* aObserver,
                                       bool* aWillNotify,
                                       bool* aIsAlternate,
                                       bool aForceUpdate)
{
  *aWillNotify = false;

  nsCOMPtr<nsIContent> thisContent;
  CallQueryInterface(this, getter_AddRefs(thisContent));

  Element* oldScopeElement = GetScopeElement(mStyleSheet);

  if (mStyleSheet && aOldDocument) {
    // We're removing the link element from the document, unload the
    // stylesheet.  We want to do this even if updates are disabled, since
    // otherwise a sheet with a stale linking element pointer will be hanging
    // around -- not good!
    aOldDocument->BeginUpdate(UPDATE_STYLE);
    aOldDocument->RemoveStyleSheet(mStyleSheet);
    aOldDocument->EndUpdate(UPDATE_STYLE);
    nsStyleLinkElement::SetStyleSheet(nullptr);
    if (oldScopeElement) {
      UpdateIsElementInStyleScopeFlagOnSubtree(oldScopeElement);
    }
  }

  NS_ENSURE_TRUE(thisContent, NS_ERROR_FAILURE);

  // When static documents are created, stylesheets are cloned manually.
  if (mDontLoadStyle || !mUpdatesEnabled ||
      thisContent->OwnerDoc()->IsStaticDocument()) {
    return NS_OK;
  }

  nsCOMPtr<nsIDocument> doc = thisContent->GetDocument();

  if (!doc || !doc->CSSLoader()->GetEnabled()) {
    return NS_OK;
  }

  bool isInline;
  nsCOMPtr<nsIURI> uri = GetStyleSheetURL(&isInline);

  if (!aForceUpdate && mStyleSheet && !isInline && uri) {
    nsIURI* oldURI = mStyleSheet->GetSheetURI();
    if (oldURI) {
      bool equal;
      nsresult rv = oldURI->Equals(uri, &equal);
      if (NS_SUCCEEDED(rv) && equal) {
        return NS_OK; // We already loaded this stylesheet
      }
    }
  }

  if (mStyleSheet) {
    doc->BeginUpdate(UPDATE_STYLE);
    doc->RemoveStyleSheet(mStyleSheet);
    doc->EndUpdate(UPDATE_STYLE);
    nsStyleLinkElement::SetStyleSheet(nullptr);
  }

  if (!uri && !isInline) {
    return NS_OK; // If href is empty and this is not inline style then just bail
  }

  nsAutoString title, type, media;
  bool isScoped;
  bool isAlternate;

  GetStyleSheetInfo(title, type, media, &isScoped, &isAlternate);

  if (!type.LowerCaseEqualsLiteral("text/css")) {
    return NS_OK;
  }

  Element* scopeElement = isScoped ? thisContent->GetParentElement() : nullptr;
  if (scopeElement) {
    NS_ASSERTION(isInline, "non-inline style must not have scope element");
    SetIsElementInStyleScopeFlagOnSubtree(scopeElement);
  }

  bool doneLoading = false;
  nsresult rv = NS_OK;
  if (isInline) {
    nsAutoString text;
    nsContentUtils::GetNodeTextContent(thisContent, false, text);

    if (!nsStyleUtil::CSPAllowsInlineStyle(thisContent->NodePrincipal(),
                                           doc->GetDocumentURI(),
                                           mLineNumber, text, &rv))
      return rv;

    // Parse the style sheet.
    rv = doc->CSSLoader()->
      LoadInlineStyle(thisContent, text, mLineNumber, title, media,
                      scopeElement, aObserver, &doneLoading, &isAlternate);
  }
  else {
    // XXXbz clone the URI here to work around content policies modifying URIs.
    nsCOMPtr<nsIURI> clonedURI;
    uri->Clone(getter_AddRefs(clonedURI));
    NS_ENSURE_TRUE(clonedURI, NS_ERROR_OUT_OF_MEMORY);
    rv = doc->CSSLoader()->
      LoadStyleLink(thisContent, clonedURI, title, media, isAlternate,
                    GetCORSMode(), aObserver, &isAlternate);
    if (NS_FAILED(rv)) {
      // Don't propagate LoadStyleLink() errors further than this, since some
      // consumers (e.g. nsXMLContentSink) will completely abort on innocuous
      // things like a stylesheet load being blocked by the security system.
      doneLoading = true;
      isAlternate = false;
      rv = NS_OK;
    }
  }

  NS_ENSURE_SUCCESS(rv, rv);

  *aWillNotify = !doneLoading;
  *aIsAlternate = isAlternate;

  return NS_OK;
}

void
nsStyleLinkElement::UpdateStyleSheetScopedness(bool aIsNowScoped)
{
  if (!mStyleSheet) {
    return;
  }

  nsCOMPtr<nsIContent> thisContent;
  CallQueryInterface(this, getter_AddRefs(thisContent));

  Element* oldScopeElement = mStyleSheet->GetScopeElement();
  Element* newScopeElement = aIsNowScoped ?
                               thisContent->GetParentElement() :
                               nullptr;

  if (oldScopeElement == newScopeElement) {
    return;
  }

  nsIDocument* document = thisContent->GetOwnerDocument();

  document->BeginUpdate(UPDATE_STYLE);
  document->RemoveStyleSheet(mStyleSheet);

  mStyleSheet->SetScopeElement(newScopeElement);

  document->AddStyleSheet(mStyleSheet);
  document->EndUpdate(UPDATE_STYLE);

  if (oldScopeElement) {
    UpdateIsElementInStyleScopeFlagOnSubtree(oldScopeElement);
  }
  if (newScopeElement) {
    SetIsElementInStyleScopeFlagOnSubtree(newScopeElement);
  }
}
