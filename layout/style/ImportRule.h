/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* class for CSS @import rules */

#ifndef mozilla_css_ImportRule_h__
#define mozilla_css_ImportRule_h__

#include "mozilla/Attributes.h"

#include "mozilla/MemoryReporting.h"
#include "mozilla/css/Rule.h"
#include "nsIDOMCSSImportRule.h"

class nsMediaList;
class nsString;

namespace mozilla {

class CSSStyleSheet;

namespace css {

class ImportRule final : public Rule,
                         public nsIDOMCSSImportRule
{
public:
  ImportRule(nsMediaList* aMedia, const nsString& aURLSpec,
             uint32_t aLineNumber, uint32_t aColumnNumber);
private:
  // for |Clone|
  ImportRule(const ImportRule& aCopy);
  ~ImportRule();
public:
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(ImportRule, mozilla::css::Rule)
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  DECL_STYLE_RULE_INHERIT

  using Rule::GetStyleSheet; // unhide since nsIDOMCSSImportRule has its own GetStyleSheet

  // Rule methods
#ifdef DEBUG
  virtual void List(FILE* out = stdout, int32_t aIndent = 0) const override;
#endif
  virtual int32_t GetType() const override;
  virtual already_AddRefed<Rule> Clone() const override;

  void SetSheet(CSSStyleSheet*);

  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const override;

  // nsIDOMCSSRule interface
  NS_DECL_NSIDOMCSSRULE

  // nsIDOMCSSImportRule interface
  NS_DECL_NSIDOMCSSIMPORTRULE

private:
  nsString  mURLSpec;
  RefPtr<nsMediaList> mMedia;
  RefPtr<CSSStyleSheet> mChildSheet;
};

} // namespace css
} // namespace mozilla

#endif /* mozilla_css_ImportRule_h__ */
