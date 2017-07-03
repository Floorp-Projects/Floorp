/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of CSSImportRule for stylo */

#ifndef mozilla_ServoImportRule_h
#define mozilla_ServoImportRule_h

#include "mozilla/dom/CSSImportRule.h"
#include "mozilla/ServoBindingTypes.h"

namespace mozilla {

class ServoStyleSheet;
class ServoMediaList;

class ServoImportRule final : public dom::CSSImportRule
{
public:
  ServoImportRule(RefPtr<RawServoImportRule> aRawRule,
                  uint32_t aLine, uint32_t aColumn);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ServoImportRule, dom::CSSImportRule)

  // unhide since nsIDOMCSSImportRule has its own GetStyleSheet and GetMedia
  using dom::CSSImportRule::GetStyleSheet;
  using dom::CSSImportRule::GetMedia;

#ifdef DEBUG
  void List(FILE* out = stdout, int32_t aIndent = 0) const final;
#endif
  already_AddRefed<css::Rule> Clone() const final;
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const final;

  // nsIDOMCSSImportRule interface
  NS_IMETHOD GetHref(nsAString& aHref) final;

  // WebIDL interface
  void GetCssTextImpl(nsAString& aCssText) const override;
  dom::MediaList* GetMedia() const final;
  StyleSheet* GetStyleSheet() const final;

private:
  ~ServoImportRule();

  RefPtr<RawServoImportRule> mRawRule;
  RefPtr<ServoStyleSheet> mChildSheet;
};

} // namespace mozilla

#endif // mozilla_ServoImportRule_h
