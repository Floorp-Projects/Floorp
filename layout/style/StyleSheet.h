/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_StyleSheet_h
#define mozilla_StyleSheet_h

#include "mozilla/css/SheetParsingMode.h"
#include "mozilla/dom/CSSStyleSheetBinding.h"
#include "mozilla/net/ReferrerPolicy.h"
#include "mozilla/StyleBackendType.h"
#include "mozilla/CORSMode.h"

class nsIDocument;
class nsINode;

namespace mozilla {

class CSSStyleSheet;
class ServoStyleSheet;
struct StyleSheetInfo;

namespace dom {
class SRIMetadata;
} // namespace dom

/**
 * Superclass for data common to CSSStyleSheet and ServoStyleSheet.
 */
class StyleSheet
{
protected:
  StyleSheet(StyleBackendType aType, css::SheetParsingMode aParsingMode);
  StyleSheet(const StyleSheet& aCopy,
             nsIDocument* aDocumentToUse,
             nsINode* aOwningNodeToUse);

public:
  void SetOwningNode(nsINode* aOwningNode)
  {
    mOwningNode = aOwningNode;
  }

  css::SheetParsingMode ParsingMode() { return mParsingMode; }
  mozilla::dom::CSSStyleSheetParsingMode ParsingModeDOM();

  // The document this style sheet is associated with.  May be null
  nsIDocument* GetDocument() const { return mDocument; }

  /**
   * Whether the sheet is complete.
   */
  bool IsComplete() const;
  void SetComplete();

  bool IsGecko() const { return !IsServo(); }
  bool IsServo() const
  {
#ifdef MOZ_STYLO
    return mType == StyleBackendType::Servo;
#else
    return false;
#endif
  }

  // Only safe to call if the caller has verified that that |this| is of the
  // correct type.
  inline CSSStyleSheet* AsGecko();
  inline ServoStyleSheet* AsServo();
  inline const CSSStyleSheet* AsGecko() const;
  inline const ServoStyleSheet* AsServo() const;

  inline MozExternalRefCountType AddRef();
  inline MozExternalRefCountType Release();

  // Whether the sheet is for an inline <style> element.
  inline bool IsInline() const;

  inline nsIURI* GetSheetURI() const;
  /* Get the URI this sheet was originally loaded from, if any.  Can
     return null */
  inline nsIURI* GetOriginalURI() const;
  inline nsIURI* GetBaseURI() const;
  /**
   * SetURIs must be called on all sheets before parsing into them.
   * SetURIs may only be called while the sheet is 1) incomplete and 2)
   * has no rules in it
   */
  inline void SetURIs(nsIURI* aSheetURI, nsIURI* aOriginalSheetURI,
                      nsIURI* aBaseURI);

  /**
   * Whether the sheet is applicable.  A sheet that is not applicable
   * should never be inserted into a style set.  A sheet may not be
   * applicable for a variety of reasons including being disabled and
   * being incomplete.
   */
  inline bool IsApplicable() const;
  inline bool HasRules() const;

  // style sheet owner info
  nsIDocument* GetOwningDocument() const { return mDocument; }
  inline void SetOwningDocument(nsIDocument* aDocument);
  nsINode* GetOwnerNode() const { return mOwningNode; }
  inline StyleSheet* GetParentSheet() const;

  inline void AppendStyleSheet(StyleSheet* aSheet);

  // Principal() never returns a null pointer.
  inline nsIPrincipal* Principal() const;
  /**
   * SetPrincipal should be called on all sheets before parsing into them.
   * This can only be called once with a non-null principal.  Calling this with
   * a null pointer is allowed and is treated as a no-op.
   */
  inline void SetPrincipal(nsIPrincipal* aPrincipal);

  // Get this style sheet's CORS mode
  inline CORSMode GetCORSMode() const;
  // Get this style sheet's Referrer Policy
  inline net::ReferrerPolicy GetReferrerPolicy() const;
  // Get this style sheet's integrity metadata
  inline void GetIntegrity(dom::SRIMetadata& aResult) const;

  inline size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const;
#ifdef DEBUG
  inline void List(FILE* aOut = stdout, int32_t aIndex = 0) const;
#endif

private:
  // Get a handle to the various stylesheet bits which live on the 'inner' for
  // gecko stylesheets and live on the StyleSheet for Servo stylesheets.
  inline StyleSheetInfo& SheetInfo();
  inline const StyleSheetInfo& SheetInfo() const;

protected:
  nsIDocument*          mDocument; // weak ref; parents maintain this for their children
  nsINode*              mOwningNode; // weak ref

  // mParsingMode controls access to nonstandard style constructs that
  // are not safe for use on the public Web but necessary in UA sheets
  // and/or useful in user sheets.
  css::SheetParsingMode mParsingMode;

  StyleBackendType      mType;
  bool                  mDisabled;
};

} // namespace mozilla

#endif // mozilla_StyleSheet_h
