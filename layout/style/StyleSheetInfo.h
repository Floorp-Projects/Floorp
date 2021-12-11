/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_StyleSheetInfo_h
#define mozilla_StyleSheetInfo_h

#include "mozilla/css/SheetParsingMode.h"
#include "mozilla/dom/SRIMetadata.h"
#include "mozilla/CORSMode.h"

#include "nsIReferrerInfo.h"

class nsIPrincipal;
class nsIURI;
struct RawServoStyleSheetContents;
struct StyleUseCounters;

namespace mozilla {
class StyleSheet;
struct URLExtraData;

/**
 * Struct for data common to CSSStyleSheetInner and ServoStyleSheet.
 */
struct StyleSheetInfo final {
  typedef dom::ReferrerPolicy ReferrerPolicy;

  StyleSheetInfo(CORSMode aCORSMode, const dom::SRIMetadata& aIntegrity,
                 css::SheetParsingMode aParsingMode);

  // FIXME(emilio): aCopy should be const.
  StyleSheetInfo(StyleSheetInfo& aCopy, StyleSheet* aPrimarySheet);

  ~StyleSheetInfo();

  StyleSheetInfo* CloneFor(StyleSheet* aPrimarySheet);

  void AddSheet(StyleSheet* aSheet);
  void RemoveSheet(StyleSheet* aSheet);

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const;

  // FIXME(emilio): most of this struct should be const, then we can remove the
  // duplication with the UrlExtraData member and such.
  nsCOMPtr<nsIURI> mSheetURI;          // for error reports, etc.
  nsCOMPtr<nsIURI> mOriginalSheetURI;  // for GetHref.  Can be null.
  nsCOMPtr<nsIURI> mBaseURI;           // for resolving relative URIs
  nsCOMPtr<nsIPrincipal> mPrincipal;
  CORSMode mCORSMode;
  // The ReferrerInfo of a stylesheet is used for its child sheets and loads
  // come from this stylesheet, so it is stored here.
  nsCOMPtr<nsIReferrerInfo> mReferrerInfo;
  dom::SRIMetadata mIntegrity;

  // Pointer to the list of child sheets. This is all fundamentally broken,
  // because each of the child sheets has a unique parent... We can only hope
  // (and currently this is the case) that any time page JS can get its hands on
  // a child sheet that means we've already ensured unique infos throughout its
  // parent chain and things are good.
  nsTArray<RefPtr<StyleSheet>> mChildren;

  AutoTArray<StyleSheet*, 8> mSheets;

  // If a SourceMap or X-SourceMap response header is seen, this is
  // the value.  If both are seen, SourceMap is preferred.  If neither
  // is seen, this will be an empty string.
  nsString mSourceMapURL;
  // This stores any source map URL that might have been seen in a
  // comment in the style sheet.  This is separate from mSourceMapURL
  // so that the value does not overwrite any value that might have
  // come from a response header.
  nsString mSourceMapURLFromComment;
  // This stores any source URL that might have been seen in a comment
  // in the style sheet.
  nsString mSourceURL;

  RefPtr<const RawServoStyleSheetContents> mContents;

  UniquePtr<StyleUseCounters> mUseCounters;

  // XXX We already have mSheetURI, mBaseURI, and mPrincipal.
  //
  // Can we somehow replace them with URLExtraData directly? The issue
  // is currently URLExtraData is immutable, but URIs in StyleSheetInfo
  // seems to be mutable, so we probably cannot set them altogether.
  // Also, this is mostly a duplicate reference of the same url data
  // inside RawServoStyleSheet. We may want to just use that instead.
  RefPtr<URLExtraData> mURLData;

#ifdef DEBUG
  bool mPrincipalSet;
#endif
};

}  // namespace mozilla

#endif  // mozilla_StyleSheetInfo_h
