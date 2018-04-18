/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ServoStyleSheet_h
#define mozilla_ServoStyleSheet_h

#include "mozilla/dom/SRIMetadata.h"
#include "mozilla/MozPromise.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ServoBindingTypes.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/StyleSheetInfo.h"
#include "mozilla/URLExtraData.h"
#include "nsCompatibility.h"
#include "nsStringFwd.h"

namespace mozilla {

class ServoCSSRuleList;
typedef MozPromise</* Dummy */ bool,
                   /* Dummy */ bool,
                   /* IsExclusive = */ true> StyleSheetParsePromise;

namespace css {
class Loader;
class LoaderReusableStyleSheets;
class SheetLoadData;
}

// -------------------------------
// Servo Style Sheet Inner Data Container
//

struct ServoStyleSheetInner final : public StyleSheetInfo
{
  ServoStyleSheetInner(CORSMode aCORSMode,
                       ReferrerPolicy aReferrerPolicy,
                       const dom::SRIMetadata& aIntegrity,
                       css::SheetParsingMode aParsingMode);
  ServoStyleSheetInner(ServoStyleSheetInner& aCopy,
                       ServoStyleSheet* aPrimarySheet);
  ~ServoStyleSheetInner();

  StyleSheetInfo* CloneFor(StyleSheet* aPrimarySheet) override;

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const;

  RefPtr<const RawServoStyleSheetContents> mContents;

  // XXX StyleSheetInfo already has mSheetURI, mBaseURI, and mPrincipal.
  // Can we somehow replace them with URLExtraData directly? The issue
  // is currently URLExtraData is immutable, but URIs in StyleSheetInfo
  // seems to be mutable, so we probably cannot set them altogether.
  // Also, this is mostly a duplicate reference of the same url data
  // inside RawServoStyleSheet. We may want to just use that instead.
  RefPtr<URLExtraData> mURLData;
};


/**
 * CSS style sheet object that is a wrapper for a Servo Stylesheet.
 */

// CID for the ServoStyleSheet class
// a6f31472-ab69-4beb-860f-c221431ead77
#define NS_SERVO_STYLE_SHEET_IMPL_CID     \
{ 0xa6f31472, 0xab69, 0x4beb, \
  { 0x86, 0x0f, 0xc2, 0x21, 0x43, 0x1e, 0xad, 0x77 } }


class ServoStyleSheet : public StyleSheet
{
public:
  ServoStyleSheet(css::SheetParsingMode aParsingMode,
                  CORSMode aCORSMode,
                  net::ReferrerPolicy aReferrerPolicy,
                  const dom::SRIMetadata& aIntegrity);

  already_AddRefed<ServoStyleSheet> CreateEmptyChildSheet(
      already_AddRefed<dom::MediaList> aMediaList) const;

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ServoStyleSheet, StyleSheet)

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_SERVO_STYLE_SHEET_IMPL_CID)

  bool HasRules() const;

  // Parses a stylesheet. The aLoadData argument corresponds to the
  // SheetLoadData for this stylesheet. It may be null in some cases.
  RefPtr<StyleSheetParsePromise>
  ParseSheet(css::Loader* aLoader,
             const nsACString& aBytes,
             nsIURI* aSheetURI,
             nsIURI* aBaseURI,
             nsIPrincipal* aSheetPrincipal,
             css::SheetLoadData* aLoadData,
             uint32_t aLineNumber,
             nsCompatibility aCompatMode,
             css::LoaderReusableStyleSheets* aReusableSheets = nullptr);

  // Similar to the above, but guarantees that parsing will be performed
  // synchronously.
  void
  ParseSheetSync(css::Loader* aLoader,
                 const nsACString& aBytes,
                 nsIURI* aSheetURI,
                 nsIURI* aBaseURI,
                 nsIPrincipal* aSheetPrincipal,
                 css::SheetLoadData* aLoadData,
                 uint32_t aLineNumber,
                 nsCompatibility aCompatMode,
                 css::LoaderReusableStyleSheets* aReusableSheets = nullptr);

  nsresult ReparseSheet(const nsAString& aInput);

  const RawServoStyleSheetContents* RawContents() const {
    return Inner()->mContents;
  }

  void SetContentsForImport(const RawServoStyleSheetContents* aContents) {
    MOZ_ASSERT(!Inner()->mContents);
    Inner()->mContents = aContents;
  }

  URLExtraData* URLData() const { return Inner()->mURLData; }

  void DidDirty() override {}

  already_AddRefed<StyleSheet> Clone(StyleSheet* aCloneParent,
    dom::CSSImportRule* aCloneOwnerRule,
    nsIDocument* aCloneDocument,
    nsINode* aCloneOwningNode) const final;

  // nsICSSLoaderObserver interface
  NS_IMETHOD StyleSheetLoaded(StyleSheet* aSheet, bool aWasAlternate,
                              nsresult aStatus) final;

  // Internal GetCssRules methods which do not have security check and
  // completeness check.
  ServoCSSRuleList* GetCssRulesInternal();

  // Returns the stylesheet's Servo origin as an OriginFlags value.
  OriginFlags GetOrigin();

protected:
  virtual ~ServoStyleSheet();

  void LastRelease();

  ServoStyleSheetInner* Inner() const
  {
    return static_cast<ServoStyleSheetInner*>(mInner);
  }

  // Internal methods which do not have security check and completeness check.
  uint32_t InsertRuleInternal(const nsAString& aRule,
                              uint32_t aIndex, ErrorResult& aRv);
  void DeleteRuleInternal(uint32_t aIndex, ErrorResult& aRv);
  nsresult InsertRuleIntoGroupInternal(const nsAString& aRule,
                                       css::GroupRule* aGroup,
                                       uint32_t aIndex);

  void EnabledStateChangedInternal() {}

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override;

private:
  ServoStyleSheet(const ServoStyleSheet& aCopy,
                  ServoStyleSheet* aParentToUse,
                  dom::CSSImportRule* aOwnerRuleToUse,
                  nsIDocument* aDocumentToUse,
                  nsINode* aOwningNodeToUse);

  // Common tail routine for the synchronous and asynchronous parsing paths.
  void FinishParse();

  void DropRuleList();

  // Take the recently cloned sheets from the `@import` rules, and reparent them
  // correctly to `aPrimarySheet`.
  void BuildChildListAfterInnerClone();

  RefPtr<ServoCSSRuleList> mRuleList;

  MozPromiseHolder<StyleSheetParsePromise> mParsePromise;

  friend class StyleSheet;
};

NS_DEFINE_STATIC_IID_ACCESSOR(ServoStyleSheet, NS_SERVO_STYLE_SHEET_IMPL_CID)

} // namespace mozilla

#endif // mozilla_ServoStyleSheet_h
