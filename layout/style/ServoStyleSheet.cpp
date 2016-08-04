/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ServoStyleSheet.h"
#include "mozilla/StyleBackendType.h"

namespace mozilla {

ServoStyleSheet::ServoStyleSheet(css::SheetParsingMode aParsingMode,
                                 CORSMode aCORSMode,
                                 net::ReferrerPolicy aReferrerPolicy,
                                 const dom::SRIMetadata& aIntegrity)
  : StyleSheet(StyleBackendType::Servo, aParsingMode)
  , StyleSheetInfo(aCORSMode, aReferrerPolicy, aIntegrity)
{
}

ServoStyleSheet::~ServoStyleSheet()
{
  DropSheet();
}

bool
ServoStyleSheet::IsApplicable() const
{
  return !mDisabled && mComplete;
}

bool
ServoStyleSheet::HasRules() const
{
  return Servo_StyleSheetHasRules(RawSheet());
}

nsIDocument*
ServoStyleSheet::GetOwningDocument() const
{
  return mDocument;
}

void
ServoStyleSheet::SetOwningDocument(nsIDocument* aDocument)
{
  // XXXheycam: Traverse to child ServoStyleSheets to set this, like
  // CSSStyleSheet::SetOwningDocument does.

  mDocument = aDocument;
}

StyleSheetHandle
ServoStyleSheet::GetParentSheet() const
{
  // XXXheycam: When we implement support for child sheets, we'll have
  // to fix SetOwningDocument to propagate the owning document down
  // to the children.
  MOZ_CRASH("stylo: not implemented");
}

void
ServoStyleSheet::AppendStyleSheet(StyleSheetHandle aSheet)
{
  // XXXheycam: When we implement support for child sheets, we'll have
  // to fix SetOwningDocument to propagate the owning document down
  // to the children.
  MOZ_CRASH("stylo: not implemented");
}

void
ServoStyleSheet::ParseSheet(const nsAString& aInput,
                            nsIURI* aSheetURI,
                            nsIURI* aBaseURI,
                            nsIPrincipal* aSheetPrincipal,
                            uint32_t aLineNumber)
{
  DropSheet();

  RefPtr<ThreadSafeURIHolder> base = new ThreadSafeURIHolder(aBaseURI);
  RefPtr<ThreadSafeURIHolder> referrer = new ThreadSafeURIHolder(aSheetURI);
  RefPtr<ThreadSafePrincipalHolder> principal =
    new ThreadSafePrincipalHolder(aSheetPrincipal);

  nsCString baseString;
  aBaseURI->GetSpec(baseString);

  NS_ConvertUTF16toUTF8 input(aInput);
  mSheet = already_AddRefed<RawServoStyleSheet>(Servo_StylesheetFromUTF8Bytes(
      reinterpret_cast<const uint8_t*>(input.get()), input.Length(),
      mParsingMode,
      reinterpret_cast<const uint8_t*>(baseString.get()), baseString.Length(),
      base, referrer, principal));
}

void
ServoStyleSheet::DropSheet()
{
  mSheet = nullptr;
}

size_t
ServoStyleSheet::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  MOZ_CRASH("stylo: not implemented");
}

#ifdef DEBUG
void
ServoStyleSheet::List(FILE* aOut, int32_t aIndex) const
{
  MOZ_CRASH("stylo: not implemented");
}
#endif

} // namespace mozilla
