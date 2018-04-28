/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A base class which implements nsIStyleSheetLinkingElement and can
 * be subclassed by various content nodes that want to load
 * stylesheets (<style>, <link>, processing instructions, etc).
 */

#include "nsStyleLinkElement.h"

#include "mozilla/StyleSheet.h"
#include "mozilla/StyleSheetInlines.h"
#include "mozilla/css/Loader.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/FragmentOrElement.h"
#include "mozilla/dom/HTMLLinkElement.h"
#include "mozilla/dom/ShadowRoot.h"
#include "mozilla/dom/SRILogHelper.h"
#include "mozilla/Preferences.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMNode.h"
#include "nsUnicharUtils.h"
#include "nsCRT.h"
#include "nsXPCOMCIDInternal.h"
#include "nsUnicharInputStream.h"
#include "nsContentUtils.h"
#include "nsStyleUtil.h"
#include "nsQueryObject.h"

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
  nsStyleLinkElement::SetStyleSheet(nullptr);
}

void
nsStyleLinkElement::Traverse(nsCycleCollectionTraversalCallback &cb)
{
  nsStyleLinkElement* tmp = this;
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mStyleSheet);
}

void
nsStyleLinkElement::SetStyleSheet(StyleSheet* aStyleSheet)
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
}

StyleSheet*
nsStyleLinkElement::GetStyleSheet()
{
  return mStyleSheet;
}

void
nsStyleLinkElement::InitStyleLinkElement(bool aDontLoadStyle)
{
  mDontLoadStyle = aDontLoadStyle;
}

void
nsStyleLinkElement::SetEnableUpdates(bool aEnableUpdates)
{
  mUpdatesEnabled = aEnableUpdates;
}

void
nsStyleLinkElement::GetCharset(nsAString& aCharset)
{
  aCharset.Truncate();
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

/* virtual */ uint32_t
nsStyleLinkElement::GetLineNumber()
{
  return mLineNumber;
}

static uint32_t ToLinkMask(const nsAString& aLink)
{
  // Keep this in sync with sRelValues in HTMLLinkElement.cpp
  if (aLink.EqualsLiteral("prefetch"))
    return nsStyleLinkElement::ePREFETCH;
  else if (aLink.EqualsLiteral("dns-prefetch"))
    return nsStyleLinkElement::eDNS_PREFETCH;
  else if (aLink.EqualsLiteral("stylesheet"))
    return nsStyleLinkElement::eSTYLESHEET;
  else if (aLink.EqualsLiteral("next"))
    return nsStyleLinkElement::eNEXT;
  else if (aLink.EqualsLiteral("alternate"))
    return nsStyleLinkElement::eALTERNATE;
  else if (aLink.EqualsLiteral("preconnect"))
    return nsStyleLinkElement::ePRECONNECT;
  else if (aLink.EqualsLiteral("preload"))
    return nsStyleLinkElement::ePRELOAD;
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

Result<nsStyleLinkElement::Update, nsresult>
nsStyleLinkElement::UpdateStyleSheet(nsICSSLoaderObserver* aObserver,
                                     ForceUpdate aForceUpdate)
{
  if (aForceUpdate == ForceUpdate::Yes) {
    // We remove this stylesheet from the cache to load a new version.
    nsCOMPtr<nsIContent> thisContent = do_QueryInterface(this);
    nsCOMPtr<nsIDocument> doc = thisContent->IsInShadowTree() ?
      thisContent->OwnerDoc() : thisContent->GetUncomposedDoc();
    if (doc && doc->CSSLoader()->GetEnabled() &&
        mStyleSheet && !mStyleSheet->IsInline()) {
      doc->CSSLoader()->ObsoleteSheet(mStyleSheet->GetOriginalURI());
    }
  }
  return DoUpdateStyleSheet(nullptr, nullptr, aObserver, aForceUpdate);
}

Result<nsStyleLinkElement::Update, nsresult>
nsStyleLinkElement::UpdateStyleSheetInternal(nsIDocument* aOldDocument,
                                             ShadowRoot* aOldShadowRoot,
                                             ForceUpdate aForceUpdate)
{
  return DoUpdateStyleSheet(
    aOldDocument, aOldShadowRoot, nullptr, aForceUpdate);
}

Result<nsStyleLinkElement::Update, nsresult>
nsStyleLinkElement::DoUpdateStyleSheet(nsIDocument* aOldDocument,
                                       ShadowRoot* aOldShadowRoot,
                                       nsICSSLoaderObserver* aObserver,
                                       ForceUpdate aForceUpdate)
{
  nsCOMPtr<nsIContent> thisContent = do_QueryInterface(this);
  // All instances of nsStyleLinkElement should implement nsIContent.
  MOZ_ASSERT(thisContent);

  if (thisContent->IsInAnonymousSubtree() &&
      thisContent->IsAnonymousContentInSVGUseSubtree()) {
    // Stylesheets in <use>-cloned subtrees are disabled until we figure out
    // how they should behave.
    return Update { };
  }

  // Check for a ShadowRoot because link elements are inert in a
  // ShadowRoot.
  ShadowRoot* containingShadow = thisContent->GetContainingShadow();
  if (thisContent->IsHTMLElement(nsGkAtoms::link) &&
      (aOldShadowRoot || containingShadow)) {
    return Update { };
  }

  if (mStyleSheet && (aOldDocument || aOldShadowRoot)) {
    MOZ_ASSERT(!(aOldDocument && aOldShadowRoot),
               "ShadowRoot content is never in document, thus "
               "there should not be a old document and old "
               "ShadowRoot simultaneously.");

    // We're removing the link element from the document or shadow tree,
    // unload the stylesheet.  We want to do this even if updates are
    // disabled, since otherwise a sheet with a stale linking element pointer
    // will be hanging around -- not good!
    if (aOldShadowRoot) {
      aOldShadowRoot->RemoveSheet(mStyleSheet);
    } else {
      aOldDocument->BeginUpdate(UPDATE_STYLE);
      aOldDocument->RemoveStyleSheet(mStyleSheet);
      aOldDocument->EndUpdate(UPDATE_STYLE);
    }

    nsStyleLinkElement::SetStyleSheet(nullptr);
  }

  // When static documents are created, stylesheets are cloned manually.
  if (mDontLoadStyle || !mUpdatesEnabled ||
      thisContent->OwnerDoc()->IsStaticDocument()) {
    return Update { };
  }

  nsCOMPtr<nsIDocument> doc = thisContent->IsInShadowTree() ?
    thisContent->OwnerDoc() : thisContent->GetUncomposedDoc();
  // Loader could be null during unlink, see bug 1425866.
  if (!doc || !doc->CSSLoader() || !doc->CSSLoader()->GetEnabled()) {
    return Update { };
  }

  bool isInline;
  nsCOMPtr<nsIPrincipal> triggeringPrincipal;
  nsCOMPtr<nsIURI> uri = GetStyleSheetURL(&isInline, getter_AddRefs(triggeringPrincipal));

  if (aForceUpdate == ForceUpdate::No && mStyleSheet && !isInline && uri) {
    if (nsIURI* oldURI = mStyleSheet->GetSheetURI()) {
      bool equal;
      nsresult rv = oldURI->Equals(uri, &equal);
      if (NS_SUCCEEDED(rv) && equal) {
        return Update { };
      }
    }
  }

  if (mStyleSheet) {
    if (thisContent->IsInShadowTree()) {
      ShadowRoot* containingShadow = thisContent->GetContainingShadow();
      containingShadow->RemoveSheet(mStyleSheet);
    } else {
      doc->BeginUpdate(UPDATE_STYLE);
      doc->RemoveStyleSheet(mStyleSheet);
      doc->EndUpdate(UPDATE_STYLE);
    }

    nsStyleLinkElement::SetStyleSheet(nullptr);
  }

  if (!uri && !isInline) {
    // If href is empty and this is not inline style then just bail
    return Update { };
  }

  nsAutoString title, type, media;
  bool hasAlternateRel;
  GetStyleSheetInfo(title, type, media, &hasAlternateRel);
  if (!type.LowerCaseEqualsLiteral("text/css")) {
    return Update { };
  }

  // Load the link's referrerpolicy attribute. If the link does not provide a
  // referrerpolicy attribute, ignore this and use the document's referrer
  // policy

  net::ReferrerPolicy referrerPolicy = GetLinkReferrerPolicy();
  if (referrerPolicy == net::RP_Unset) {
    referrerPolicy = doc->GetReferrerPolicy();
  }

  if (isInline) {
    nsAutoString text;
    if (!nsContentUtils::GetNodeTextContent(thisContent, false, text, fallible)) {
      return Err(NS_ERROR_OUT_OF_MEMORY);
    }


    MOZ_ASSERT(thisContent->NodeInfo()->NameAtom() != nsGkAtoms::link,
               "<link> is not 'inline', and needs different CSP checks");
    MOZ_ASSERT(thisContent->IsElement());
    nsresult rv = NS_OK;
    if (!nsStyleUtil::CSPAllowsInlineStyle(thisContent->AsElement(),
                                           thisContent->NodePrincipal(),
                                           triggeringPrincipal,
                                           doc->GetDocumentURI(),
                                           mLineNumber, text, &rv)) {
      if (NS_FAILED(rv)) {
        return Err(rv);
      }
      return Update { };
    }

    // Parse the style sheet.
    return doc->CSSLoader()->
      LoadInlineStyle(thisContent, text, triggeringPrincipal, mLineNumber,
                      title, media, referrerPolicy,
                      aObserver);
  }
  nsAutoString integrity;
  if (thisContent->IsElement()) {
    thisContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::integrity,
                                      integrity);
  }
  if (!integrity.IsEmpty()) {
    MOZ_LOG(SRILogHelper::GetSriLog(), mozilla::LogLevel::Debug,
            ("nsStyleLinkElement::DoUpdateStyleSheet, integrity=%s",
             NS_ConvertUTF16toUTF8(integrity).get()));
  }
  auto resultOrError =
    doc->CSSLoader()->LoadStyleLink(thisContent,
                                    uri,
                                    triggeringPrincipal,
                                    title,
                                    media,
                                    hasAlternateRel,
                                    GetCORSMode(),
                                    referrerPolicy,
                                    integrity,
                                    aObserver);
  if (resultOrError.isErr()) {
    // Don't propagate LoadStyleLink() errors further than this, since some
    // consumers (e.g. nsXMLContentSink) will completely abort on innocuous
    // things like a stylesheet load being blocked by the security system.
    return Update { };
  }
  return resultOrError;
}
