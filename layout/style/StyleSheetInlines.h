/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_StyleSheetInlines_h
#define mozilla_StyleSheetInlines_h

#include "mozilla/TypeTraits.h"
#include "mozilla/StyleSheetInfo.h"
#include "mozilla/ServoStyleSheet.h"
#include "mozilla/CSSStyleSheet.h"

namespace mozilla {

CSSStyleSheet*
StyleSheet::AsGecko()
{
  MOZ_ASSERT(IsGecko());
  return static_cast<CSSStyleSheet*>(this);
}

ServoStyleSheet*
StyleSheet::AsServo()
{
  MOZ_ASSERT(IsServo());
  return static_cast<ServoStyleSheet*>(this);
}

const CSSStyleSheet*
StyleSheet::AsGecko() const
{
  MOZ_ASSERT(IsGecko());
  return static_cast<const CSSStyleSheet*>(this);
}

const ServoStyleSheet*
StyleSheet::AsServo() const
{
  MOZ_ASSERT(IsServo());
  return static_cast<const ServoStyleSheet*>(this);
}

StyleSheetInfo&
StyleSheet::SheetInfo()
{
  if (IsServo()) {
    return AsServo()->mSheetInfo;
  }
  return *AsGecko()->mInner;
}

const StyleSheetInfo&
StyleSheet::SheetInfo() const
{
  if (IsServo()) {
    return AsServo()->mSheetInfo;
  }
  return *AsGecko()->mInner;
}

#define FORWARD_CONCRETE(method_, geckoargs_, servoargs_) \
  static_assert(!IsSame<decltype(&StyleSheet::method_), \
                        decltype(&CSSStyleSheet::method_)>::value, \
                "CSSStyleSheet should define its own " #method_); \
  static_assert(!IsSame<decltype(&StyleSheet::method_), \
                        decltype(&ServoStyleSheet::method_)>::value, \
                "ServoStyleSheet should define its own " #method_); \
  if (IsServo()) { \
    return AsServo()->method_ servoargs_; \
  } \
  return AsGecko()->method_ geckoargs_;

#define FORWARD(method_, args_) FORWARD_CONCRETE(method_, args_, args_)

MozExternalRefCountType
StyleSheet::AddRef()
{
  FORWARD(AddRef, ())
}

MozExternalRefCountType
StyleSheet::Release()
{
  FORWARD(Release, ())
}

bool
StyleSheet::IsInline() const
{
  return !SheetInfo().mOriginalSheetURI;
}

nsIURI*
StyleSheet::GetSheetURI() const
{
  return SheetInfo().mSheetURI;
}

nsIURI*
StyleSheet::GetOriginalURI() const
{
  return SheetInfo().mOriginalSheetURI;
}

nsIURI*
StyleSheet::GetBaseURI() const
{
  return SheetInfo().mBaseURI;
}

void
StyleSheet::SetURIs(nsIURI* aSheetURI, nsIURI* aOriginalSheetURI,
                    nsIURI* aBaseURI)
{
  NS_PRECONDITION(aSheetURI && aBaseURI, "null ptr");
  StyleSheetInfo& info = SheetInfo();
  MOZ_ASSERT(!HasRules() && !info.mComplete,
             "Can't call SetURIs on sheets that are complete or have rules");
  info.mSheetURI = aSheetURI;
  info.mOriginalSheetURI = aOriginalSheetURI;
  info.mBaseURI = aBaseURI;
}

bool
StyleSheet::IsApplicable() const
{
  return !mDisabled && SheetInfo().mComplete;
}

bool
StyleSheet::HasRules() const
{
  FORWARD(HasRules, ())
}

void
StyleSheet::SetOwningDocument(nsIDocument* aDocument)
{
  FORWARD(SetOwningDocument, (aDocument))
}

StyleSheet*
StyleSheet::GetParentSheet() const
{
  FORWARD(GetParentSheet, ())
}

void
StyleSheet::AppendStyleSheet(StyleSheet* aSheet)
{
  FORWARD_CONCRETE(AppendStyleSheet, (aSheet->AsGecko()), (aSheet->AsServo()))
}

nsIPrincipal*
StyleSheet::Principal() const
{
  return SheetInfo().mPrincipal;
}

void
StyleSheet::SetPrincipal(nsIPrincipal* aPrincipal)
{
  StyleSheetInfo& info = SheetInfo();
  NS_PRECONDITION(!info.mPrincipalSet, "Should only set principal once");
  if (aPrincipal) {
    info.mPrincipal = aPrincipal;
#ifdef DEBUG
    info.mPrincipalSet = true;
#endif
  }
}

CORSMode
StyleSheet::GetCORSMode() const
{
  return SheetInfo().mCORSMode;
}

net::ReferrerPolicy
StyleSheet::GetReferrerPolicy() const
{
  return SheetInfo().mReferrerPolicy;
}

void
StyleSheet::GetIntegrity(dom::SRIMetadata& aResult) const
{
  aResult = SheetInfo().mIntegrity;
}

size_t
StyleSheet::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  FORWARD(SizeOfIncludingThis, (aMallocSizeOf))
}

#ifdef DEBUG
void
StyleSheet::List(FILE* aOut, int32_t aIndex) const
{
  FORWARD(List, (aOut, aIndex))
}
#endif

#undef FORWARD
#undef FORWARD_CONCRETE

inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            RefPtr<StyleSheet>& aField,
                            const char* aName,
                            uint32_t aFlags = 0)
{
  if (aField && aField->IsGecko()) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(aCallback, aName);
    aCallback.NoteXPCOMChild(NS_ISUPPORTS_CAST(nsIDOMCSSStyleSheet*, aField->AsGecko()));
  }
}

inline void
ImplCycleCollectionUnlink(RefPtr<StyleSheet>& aField)
{
  aField = nullptr;
}

}

#endif // mozilla_StyleSheetInlines_h
