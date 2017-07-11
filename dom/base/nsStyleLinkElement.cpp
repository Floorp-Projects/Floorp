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
#include "nsIDOMComment.h"
#include "nsIDOMNode.h"
#include "nsIDOMStyleSheet.h"
#include "nsUnicharUtils.h"
#include "nsCRT.h"
#include "nsXPCOMCIDInternal.h"
#include "nsUnicharInputStream.h"
#include "nsContentUtils.h"
#include "nsStyleUtil.h"
#include "nsQueryObject.h"
#include "nsIContentPolicyBase.h"
#include "nsMimeTypes.h"
#include "imgLoader.h"
#include "MediaContainerType.h"
#include "DecoderDoctorDiagnostics.h"
#include "DecoderTraits.h"
#include "MediaList.h"
#include "nsAttrValueInlines.h"

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

NS_IMETHODIMP
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

  return NS_OK;
}

NS_IMETHODIMP_(StyleSheet*)
nsStyleLinkElement::GetStyleSheet()
{
  return mStyleSheet;
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
  else if (aLink.EqualsLiteral("prerender"))
    return nsStyleLinkElement::ePRERENDER;
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

// We will use official mime-types from:
// https://www.iana.org/assignments/media-types/media-types.xhtml#font
// We do not support old deprecated mime-types for preload feature.
// (We currectly do not support font/collection)
static uint32_t StyleLinkElementFontMimeTypesNum = 5;
static const char* StyleLinkElementFontMimeTypes[] = {
  "font/otf",
  "font/sfnt",
  "font/ttf",
  "font/woff",
  "font/woff2"
};

bool
IsFontMimeType(const nsAString& aType)
{
  if (aType.IsEmpty()) {
    return true;
  }
  for (uint32_t i = 0; i < StyleLinkElementFontMimeTypesNum; i++) {
    if (aType.EqualsASCII(StyleLinkElementFontMimeTypes[i])) {
      return true;
    }
  }
  return false;
}

bool
nsStyleLinkElement::CheckPreloadAttrs(const nsAttrValue& aAs,
                                      const nsAString& aType,
                                      const nsAString& aMedia,
                                      nsIDocument* aDocument)
{
  nsContentPolicyType policyType = Link::AsValueToContentPolicy(aAs);
  if (policyType == nsIContentPolicy::TYPE_INVALID) {
    return false;
  }

  // Check if media attribute is valid.
  if (!aMedia.IsEmpty()) {
    RefPtr<MediaList> mediaList = MediaList::Create(aDocument->GetStyleBackendType(),
                                                    aMedia);
    nsIPresShell* shell = aDocument->GetShell();
    if (!shell) {
      return false;
    }

    nsPresContext* presContext = shell->GetPresContext();
    if (!presContext) {
      return false;
    }
    if (!mediaList->Matches(presContext)) {
      return false;
    }
  }

  if (aType.IsEmpty()) {
    return true;
  }

  nsString type = nsString(aType);
  ToLowerCase(type);

  if (policyType == nsIContentPolicy::TYPE_OTHER) {
    return true;

  } else if (policyType == nsIContentPolicy::TYPE_MEDIA) {
    if (aAs.GetEnumValue() == DESTINATION_TRACK) {
      if (type.EqualsASCII("text/vtt")) {
        return true;
      } else {
        return false;
      }
    }
    Maybe<MediaContainerType> mimeType = MakeMediaContainerType(aType);
    if (!mimeType) {
      return false;
    }
    DecoderDoctorDiagnostics diagnostics;
    CanPlayStatus status = DecoderTraits::CanHandleContainerType(*mimeType,
                                                                 &diagnostics);
    // Preload if this return CANPLAY_YES and CANPLAY_MAYBE.
    if (status == CANPLAY_NO) {
      return false;
    } else {
      return true;
    }

  } else if (policyType == nsIContentPolicy::TYPE_FONT) {
    if (IsFontMimeType(type)) {
      return true;
    } else {
      return false;
    }

  } else if (policyType == nsIContentPolicy::TYPE_IMAGE) {
    if (imgLoader::SupportImageWithMimeType(NS_ConvertUTF16toUTF8(type).get(),
                                            AcceptedMimeTypes::IMAGES_AND_DOCUMENTS)) {
      return true;
    } else {
      return false;
    }

  } else if (policyType == nsIContentPolicy::TYPE_SCRIPT) {
    if (nsContentUtils::IsJavascriptMIMEType(type)) {
      return true;
    } else {
      return false;
    }

  } else if (policyType == nsIContentPolicy::TYPE_STYLESHEET) {
    if (type.EqualsASCII("text/css")) {
      return true;
    } else {
      return false;
    }
  }
  return false;
}

NS_IMETHODIMP
nsStyleLinkElement::UpdateStyleSheet(nsICSSLoaderObserver* aObserver,
                                     bool* aWillNotify,
                                     bool* aIsAlternate,
                                     bool aForceReload)
{
  if (aForceReload) {
    // We remove this stylesheet from the cache to load a new version.
    nsCOMPtr<nsIContent> thisContent;
    CallQueryInterface(this, getter_AddRefs(thisContent));
    nsCOMPtr<nsIDocument> doc = thisContent->IsInShadowTree() ?
      thisContent->OwnerDoc() : thisContent->GetUncomposedDoc();
    if (doc && doc->CSSLoader()->GetEnabled() &&
        mStyleSheet && !mStyleSheet->IsInline()) {
      doc->CSSLoader()->ObsoleteSheet(mStyleSheet->GetOriginalURI());
    }
  }
  return DoUpdateStyleSheet(nullptr, nullptr, aObserver, aWillNotify,
                            aIsAlternate, aForceReload);
}

nsresult
nsStyleLinkElement::UpdateStyleSheetInternal(nsIDocument *aOldDocument,
                                             ShadowRoot *aOldShadowRoot,
                                             bool aForceUpdate)
{
  bool notify, alternate;
  return DoUpdateStyleSheet(aOldDocument, aOldShadowRoot, nullptr, &notify,
                            &alternate, aForceUpdate);
}

static bool
IsScopedStyleElement(nsIContent* aContent)
{
  // This is quicker than, say, QIing aContent to nsStyleLinkElement
  // and then calling its virtual GetStyleSheetInfo method to find out
  // if it is scoped.
  return (aContent->IsHTMLElement(nsGkAtoms::style) ||
          aContent->IsSVGElement(nsGkAtoms::style)) &&
         aContent->HasAttr(kNameSpaceID_None, nsGkAtoms::scoped) &&
         aContent->OwnerDoc()->IsScopedStyleEnabled();
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

nsresult
nsStyleLinkElement::DoUpdateStyleSheet(nsIDocument* aOldDocument,
                                       ShadowRoot* aOldShadowRoot,
                                       nsICSSLoaderObserver* aObserver,
                                       bool* aWillNotify,
                                       bool* aIsAlternate,
                                       bool aForceUpdate)
{
  *aWillNotify = false;

  nsCOMPtr<nsIContent> thisContent;
  CallQueryInterface(this, getter_AddRefs(thisContent));

  // All instances of nsStyleLinkElement should implement nsIContent.
  NS_ENSURE_TRUE(thisContent, NS_ERROR_FAILURE);

  if (thisContent->IsInAnonymousSubtree() &&
      thisContent->IsAnonymousContentInSVGUseSubtree()) {
    // Stylesheets in <use>-cloned subtrees are disabled until we figure out
    // how they should behave.
    return NS_OK;
  }

  // Check for a ShadowRoot because link elements are inert in a
  // ShadowRoot.
  ShadowRoot* containingShadow = thisContent->GetContainingShadow();
  if (thisContent->IsHTMLElement(nsGkAtoms::link) &&
      (aOldShadowRoot || containingShadow)) {
    return NS_OK;
  }

  Element* oldScopeElement = nullptr;
  if (mStyleSheet) {
    if (mStyleSheet->IsServo()) {
      MOZ_ASSERT(!aOldDocument->IsScopedStyleEnabled(),
                 "stylo: ServoStyleSheets don't support <style scoped>");
    } else {
      oldScopeElement = mStyleSheet->AsGecko()->GetScopeElement();
    }
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
    if (oldScopeElement) {
      UpdateIsElementInStyleScopeFlagOnSubtree(oldScopeElement);
    }
  }

  // When static documents are created, stylesheets are cloned manually.
  if (mDontLoadStyle || !mUpdatesEnabled ||
      thisContent->OwnerDoc()->IsStaticDocument()) {
    return NS_OK;
  }

  nsCOMPtr<nsIDocument> doc = thisContent->IsInShadowTree() ?
    thisContent->OwnerDoc() : thisContent->GetUncomposedDoc();
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
    scopeElement->SetIsElementInStyleScopeFlagOnSubtree(true);
  }

  bool doneLoading = false;
  nsresult rv = NS_OK;
  if (isInline) {
    nsAutoString text;
    if (!nsContentUtils::GetNodeTextContent(thisContent, false, text, fallible)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    MOZ_ASSERT(thisContent->NodeInfo()->NameAtom() != nsGkAtoms::link,
               "<link> is not 'inline', and needs different CSP checks");
    if (!nsStyleUtil::CSPAllowsInlineStyle(thisContent,
                                           thisContent->NodePrincipal(),
                                           doc->GetDocumentURI(),
                                           mLineNumber, text, &rv))
      return rv;

    // Parse the style sheet.
    rv = doc->CSSLoader()->
      LoadInlineStyle(thisContent, text, mLineNumber, title, media,
                      scopeElement, aObserver, &doneLoading, &isAlternate);
  }
  else {
    nsAutoString integrity;
    thisContent->GetAttr(kNameSpaceID_None, nsGkAtoms::integrity, integrity);
    if (!integrity.IsEmpty()) {
      MOZ_LOG(SRILogHelper::GetSriLog(), mozilla::LogLevel::Debug,
              ("nsStyleLinkElement::DoUpdateStyleSheet, integrity=%s",
               NS_ConvertUTF16toUTF8(integrity).get()));
    }

    // if referrer attributes are enabled in preferences, load the link's referrer
    // attribute. If the link does not provide a referrer attribute, ignore this
    // and use the document's referrer policy

    net::ReferrerPolicy referrerPolicy = GetLinkReferrerPolicy();
    if (referrerPolicy == net::RP_Unset) {
      referrerPolicy = doc->GetReferrerPolicy();
    }

    // XXXbz clone the URI here to work around content policies modifying URIs.
    nsCOMPtr<nsIURI> clonedURI;
    uri->Clone(getter_AddRefs(clonedURI));
    NS_ENSURE_TRUE(clonedURI, NS_ERROR_OUT_OF_MEMORY);
    rv = doc->CSSLoader()->
      LoadStyleLink(thisContent, clonedURI, title, media, isAlternate,
                    GetCORSMode(), referrerPolicy, integrity,
                    aObserver, &isAlternate);
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

  if (mStyleSheet->IsServo()) {
    // XXXheycam ServoStyleSheets don't support <style scoped>.
    NS_ERROR("stylo: ServoStyleSheets don't support <style scoped>");
    return;
  }

  CSSStyleSheet* sheet = mStyleSheet->AsGecko();

  nsCOMPtr<nsIContent> thisContent;
  CallQueryInterface(this, getter_AddRefs(thisContent));

  Element* oldScopeElement = sheet->GetScopeElement();
  Element* newScopeElement = aIsNowScoped ?
                               thisContent->GetParentElement() :
                               nullptr;

  if (oldScopeElement == newScopeElement) {
    return;
  }

  nsIDocument* document = thisContent->GetOwnerDocument();

  if (thisContent->IsInShadowTree()) {
    ShadowRoot* containingShadow = thisContent->GetContainingShadow();
    containingShadow->RemoveSheet(mStyleSheet);

    sheet->SetScopeElement(newScopeElement);

    containingShadow->InsertSheet(mStyleSheet, thisContent);
  } else {
    document->BeginUpdate(UPDATE_STYLE);
    document->RemoveStyleSheet(mStyleSheet);

    sheet->SetScopeElement(newScopeElement);

    document->AddStyleSheet(mStyleSheet);
    document->EndUpdate(UPDATE_STYLE);
  }

  if (oldScopeElement) {
    UpdateIsElementInStyleScopeFlagOnSubtree(oldScopeElement);
  }
  if (newScopeElement) {
    newScopeElement->SetIsElementInStyleScopeFlagOnSubtree(true);
  }
}
