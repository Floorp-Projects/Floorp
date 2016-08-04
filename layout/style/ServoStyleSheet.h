/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ServoStyleSheet_h
#define mozilla_ServoStyleSheet_h

#include "mozilla/dom/SRIMetadata.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ServoBindingHelpers.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/StyleSheetHandle.h"
#include "mozilla/StyleSheetInfo.h"
#include "nsStringFwd.h"

namespace mozilla {

/**
 * CSS style sheet object that is a wrapper for a Servo Stylesheet.
 */
class ServoStyleSheet : public StyleSheet
                      , public StyleSheetInfo
{
public:
  ServoStyleSheet(css::SheetParsingMode aParsingMode,
                  CORSMode aCORSMode,
                  net::ReferrerPolicy aReferrerPolicy,
                  const dom::SRIMetadata& aIntegrity);

  NS_INLINE_DECL_REFCOUNTING(ServoStyleSheet)

  bool IsApplicable() const;
  bool HasRules() const;

  nsIDocument* GetOwningDocument() const;
  void SetOwningDocument(nsIDocument* aDocument);

  StyleSheetHandle GetParentSheet() const;
  void AppendStyleSheet(StyleSheetHandle aSheet);

  void ParseSheet(const nsAString& aInput,
                  nsIURI* aSheetURI,
                  nsIURI* aBaseURI,
                  nsIPrincipal* aSheetPrincipal,
                  uint32_t aLineNumber);

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const;

#ifdef DEBUG
  void List(FILE* aOut = stdout, int32_t aIndex = 0) const;
#endif

  RawServoStyleSheet* RawSheet() const { return mSheet; }

protected:
  ~ServoStyleSheet();

private:
  void DropSheet();

  RefPtr<RawServoStyleSheet> mSheet;
};

} // namespace mozilla

#endif // mozilla_ServoStyleSheet_h
