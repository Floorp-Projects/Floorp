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

#include "mozilla/dom/LinkStyle.h"

#include "mozilla/StyleSheet.h"
#include "mozilla/StyleSheetInlines.h"
#include "mozilla/css/Loader.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/FragmentOrElement.h"
#include "mozilla/dom/HTMLLinkElement.h"
#include "mozilla/dom/HTMLStyleElement.h"
#include "mozilla/dom/SVGStyleElement.h"
#include "mozilla/dom/ShadowRoot.h"
#include "mozilla/dom/SRILogHelper.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_dom.h"
#include "nsIContent.h"
#include "mozilla/dom/Document.h"
#include "nsUnicharUtils.h"
#include "nsCRT.h"
#include "nsXPCOMCIDInternal.h"
#include "nsUnicharInputStream.h"
#include "nsContentUtils.h"
#include "nsStyleUtil.h"
#include "nsQueryObject.h"

namespace mozilla::dom {

LinkStyle::SheetInfo::SheetInfo(
    const Document& aDocument, nsIContent* aContent,
    already_AddRefed<nsIURI> aURI,
    already_AddRefed<nsIPrincipal> aTriggeringPrincipal,
    already_AddRefed<nsIReferrerInfo> aReferrerInfo,
    mozilla::CORSMode aCORSMode, const nsAString& aTitle,
    const nsAString& aMedia, const nsAString& aIntegrity,
    const nsAString& aNonce, HasAlternateRel aHasAlternateRel,
    IsInline aIsInline, IsExplicitlyEnabled aIsExplicitlyEnabled,
    FetchPriority aFetchPriority)
    : mContent(aContent),
      mURI(aURI),
      mTriggeringPrincipal(aTriggeringPrincipal),
      mReferrerInfo(aReferrerInfo),
      mCORSMode(aCORSMode),
      mTitle(aTitle),
      mMedia(aMedia),
      mIntegrity(aIntegrity),
      mNonce(aNonce),
      mFetchPriority(aFetchPriority),
      mHasAlternateRel(aHasAlternateRel == HasAlternateRel::Yes),
      mIsInline(aIsInline == IsInline::Yes),
      mIsExplicitlyEnabled(aIsExplicitlyEnabled) {
  MOZ_ASSERT(!mIsInline || aContent);
  MOZ_ASSERT_IF(aContent, aContent->OwnerDoc() == &aDocument);
  MOZ_ASSERT(mReferrerInfo);
  MOZ_ASSERT(mIntegrity.IsEmpty() || !mIsInline,
             "Integrity only applies to <link>");
}

LinkStyle::SheetInfo::~SheetInfo() = default;
LinkStyle::LinkStyle() = default;

LinkStyle::~LinkStyle() { LinkStyle::SetStyleSheet(nullptr); }

StyleSheet* LinkStyle::GetSheetForBindings() const {
  if (mStyleSheet && mStyleSheet->IsComplete()) {
    return mStyleSheet;
  }
  return nullptr;
}

void LinkStyle::GetTitleAndMediaForElement(const Element& aSelf,
                                           nsString& aTitle, nsString& aMedia) {
  // Only honor title as stylesheet name for elements in the document (that is,
  // ignore for Shadow DOM), per [1] and [2]. See [3].
  //
  // [1]: https://html.spec.whatwg.org/#attr-link-title
  // [2]: https://html.spec.whatwg.org/#attr-style-title
  // [3]: https://github.com/w3c/webcomponents/issues/535
  if (aSelf.IsInUncomposedDoc()) {
    aSelf.GetAttr(nsGkAtoms::title, aTitle);
    aTitle.CompressWhitespace();
  }

  aSelf.GetAttr(nsGkAtoms::media, aMedia);
  // The HTML5 spec is formulated in terms of the CSSOM spec, which specifies
  // that media queries should be ASCII lowercased during serialization.
  //
  // FIXME(emilio): How does it matter? This is going to be parsed anyway, CSS
  // should take care of serializing it properly.
  nsContentUtils::ASCIIToLower(aMedia);
}

bool LinkStyle::IsCSSMimeTypeAttributeForStyleElement(const Element& aSelf) {
  // Per
  // https://html.spec.whatwg.org/multipage/semantics.html#the-style-element:update-a-style-block
  // step 4, for style elements we should only accept empty and "text/css" type
  // attribute values.
  nsAutoString type;
  aSelf.GetAttr(nsGkAtoms::type, type);
  return type.IsEmpty() || type.LowerCaseEqualsLiteral("text/css");
}

void LinkStyle::Unlink() { LinkStyle::SetStyleSheet(nullptr); }

void LinkStyle::Traverse(nsCycleCollectionTraversalCallback& cb) {
  LinkStyle* tmp = this;
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mStyleSheet);
}

void LinkStyle::SetStyleSheet(StyleSheet* aStyleSheet) {
  if (mStyleSheet) {
    mStyleSheet->SetOwningNode(nullptr);
  }

  mStyleSheet = aStyleSheet;
  if (mStyleSheet) {
    mStyleSheet->SetOwningNode(&AsContent());
  }
}

void LinkStyle::GetCharset(nsAString& aCharset) { aCharset.Truncate(); }

static uint32_t ToLinkMask(const nsAString& aLink) {
  // Keep this in sync with sSupportedRelValues in HTMLLinkElement.cpp
  uint32_t mask = 0;
  if (aLink.EqualsLiteral("prefetch")) {
    mask = LinkStyle::ePREFETCH;
  } else if (aLink.EqualsLiteral("dns-prefetch")) {
    mask = LinkStyle::eDNS_PREFETCH;
  } else if (aLink.EqualsLiteral("stylesheet")) {
    mask = LinkStyle::eSTYLESHEET;
  } else if (aLink.EqualsLiteral("next")) {
    mask = LinkStyle::eNEXT;
  } else if (aLink.EqualsLiteral("alternate")) {
    mask = LinkStyle::eALTERNATE;
  } else if (aLink.EqualsLiteral("preconnect")) {
    mask = LinkStyle::ePRECONNECT;
  } else if (aLink.EqualsLiteral("preload")) {
    mask = LinkStyle::ePRELOAD;
  } else if (aLink.EqualsLiteral("modulepreload")) {
    mask = LinkStyle::eMODULE_PRELOAD;
  }

  return mask;
}

uint32_t LinkStyle::ParseLinkTypes(const nsAString& aTypes) {
  uint32_t linkMask = 0;
  nsAString::const_iterator start, done;
  aTypes.BeginReading(start);
  aTypes.EndReading(done);
  if (start == done) return linkMask;

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
    } else {
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

Result<LinkStyle::Update, nsresult> LinkStyle::UpdateStyleSheetInternal(
    Document* aOldDocument, ShadowRoot* aOldShadowRoot,
    ForceUpdate aForceUpdate) {
  return DoUpdateStyleSheet(aOldDocument, aOldShadowRoot, nullptr,
                            aForceUpdate);
}

LinkStyle* LinkStyle::FromNode(Element& aElement) {
  nsAtom* name = aElement.NodeInfo()->NameAtom();
  if (name == nsGkAtoms::link) {
    MOZ_ASSERT(aElement.IsHTMLElement() == !!aElement.AsLinkStyle());
    return aElement.IsHTMLElement() ? static_cast<HTMLLinkElement*>(&aElement)
                                    : nullptr;
  }
  if (name == nsGkAtoms::style) {
    if (aElement.IsHTMLElement()) {
      MOZ_ASSERT(aElement.AsLinkStyle());
      return static_cast<HTMLStyleElement*>(&aElement);
    }
    if (aElement.IsSVGElement()) {
      MOZ_ASSERT(aElement.AsLinkStyle());
      return static_cast<SVGStyleElement*>(&aElement);
    }
  }
  MOZ_ASSERT(!aElement.AsLinkStyle());
  return nullptr;
}

void LinkStyle::BindToTree() {
  if (mUpdatesEnabled) {
    nsContentUtils::AddScriptRunner(NS_NewRunnableFunction(
        "LinkStyle::BindToTree",
        [this, pin = RefPtr{&AsContent()}] { UpdateStyleSheetInternal(); }));
  }
}

Result<LinkStyle::Update, nsresult> LinkStyle::DoUpdateStyleSheet(
    Document* aOldDocument, ShadowRoot* aOldShadowRoot,
    nsICSSLoaderObserver* aObserver, ForceUpdate aForceUpdate) {
  nsIContent& thisContent = AsContent();
  if (thisContent.IsInSVGUseShadowTree()) {
    // Stylesheets in <use>-cloned subtrees are disabled until we figure out
    // how they should behave.
    return Update{};
  }

  if (mStyleSheet && (aOldDocument || aOldShadowRoot)) {
    MOZ_ASSERT(!(aOldDocument && aOldShadowRoot),
               "ShadowRoot content is never in document, thus "
               "there should not be a old document and old "
               "ShadowRoot simultaneously.");

    // We're removing the link element from the document or shadow tree, unload
    // the stylesheet.
    //
    // We want to do this even if updates are disabled, since otherwise a sheet
    // with a stale linking element pointer will be hanging around -- not good!
    if (mStyleSheet->IsComplete()) {
      if (aOldShadowRoot) {
        aOldShadowRoot->RemoveStyleSheet(*mStyleSheet);
      } else {
        aOldDocument->RemoveStyleSheet(*mStyleSheet);
      }
    }

    SetStyleSheet(nullptr);
  }

  Document* doc = thisContent.GetComposedDoc();

  // Loader could be null during unlink, see bug 1425866.
  if (!doc || !doc->CSSLoader() || !doc->CSSLoader()->GetEnabled()) {
    return Update{};
  }

  // When static documents are created, stylesheets are cloned manually.
  if (!mUpdatesEnabled || doc->IsStaticDocument()) {
    return Update{};
  }

  Maybe<SheetInfo> info = GetStyleSheetInfo();
  if (aForceUpdate == ForceUpdate::No && mStyleSheet && info &&
      !info->mIsInline && info->mURI) {
    if (nsIURI* oldURI = mStyleSheet->GetSheetURI()) {
      bool equal;
      nsresult rv = oldURI->Equals(info->mURI, &equal);
      if (NS_SUCCEEDED(rv) && equal) {
        return Update{};
      }
    }
  }

  if (mStyleSheet) {
    if (mStyleSheet->IsComplete()) {
      if (thisContent.IsInShadowTree()) {
        ShadowRoot* containingShadow = thisContent.GetContainingShadow();
        // Could be null only during unlink.
        if (MOZ_LIKELY(containingShadow)) {
          containingShadow->RemoveStyleSheet(*mStyleSheet);
        }
      } else {
        doc->RemoveStyleSheet(*mStyleSheet);
      }
    }

    SetStyleSheet(nullptr);
  }

  if (!info) {
    return Update{};
  }

  if (!info->mURI && !info->mIsInline) {
    // If href is empty and this is not inline style then just bail
    return Update{};
  }

  if (info->mIsInline) {
    nsAutoString text;
    if (!nsContentUtils::GetNodeTextContent(&thisContent, false, text,
                                            fallible)) {
      return Err(NS_ERROR_OUT_OF_MEMORY);
    }

    MOZ_ASSERT(thisContent.NodeInfo()->NameAtom() != nsGkAtoms::link,
               "<link> is not 'inline', and needs different CSP checks");
    MOZ_ASSERT(thisContent.IsElement());
    nsresult rv = NS_OK;
    if (!nsStyleUtil::CSPAllowsInlineStyle(
            thisContent.AsElement(), doc, info->mTriggeringPrincipal,
            mLineNumber, mColumnNumber, text, &rv)) {
      if (NS_FAILED(rv)) {
        return Err(rv);
      }
      return Update{};
    }

    // Parse the style sheet.
    return doc->CSSLoader()->LoadInlineStyle(*info, text, aObserver);
  }
  if (thisContent.IsElement()) {
    nsAutoString integrity;
    thisContent.AsElement()->GetAttr(nsGkAtoms::integrity, integrity);
    if (!integrity.IsEmpty()) {
      MOZ_LOG(SRILogHelper::GetSriLog(), mozilla::LogLevel::Debug,
              ("LinkStyle::DoUpdateStyleSheet, integrity=%s",
               NS_ConvertUTF16toUTF8(integrity).get()));
    }
  }
  auto resultOrError = doc->CSSLoader()->LoadStyleLink(*info, aObserver);
  if (resultOrError.isErr()) {
    // Don't propagate LoadStyleLink() errors further than this, since some
    // consumers (e.g. nsXMLContentSink) will completely abort on innocuous
    // things like a stylesheet load being blocked by the security system.
    return Update{};
  }
  return resultOrError;
}

}  // namespace mozilla::dom
