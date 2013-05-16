/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* class for CSS @import rules */

#ifndef mozilla_css_ImportRule_h__
#define mozilla_css_ImportRule_h__

#include "mozilla/Attributes.h"

#include "mozilla/css/Rule.h"
#include "nsIDOMCSSImportRule.h"
#include "nsCSSRules.h"

class nsMediaList;
class nsString;

namespace mozilla {
namespace css {

class ImportRule MOZ_FINAL : public Rule,
                             public nsIDOMCSSImportRule
{
public:
  ImportRule(nsMediaList* aMedia, const nsString& aURLSpec);
private:
  // for |Clone|
  ImportRule(const ImportRule& aCopy);
  ~ImportRule();
public:
  NS_DECL_ISUPPORTS

  DECL_STYLE_RULE_INHERIT

#ifdef HAVE_CPP_AMBIGUITY_RESOLVING_USING
  using Rule::GetStyleSheet; // unhide since nsIDOMCSSImportRule has its own GetStyleSheet
#endif

  // nsIStyleRule methods
#ifdef DEBUG
  virtual void List(FILE* out = stdout, int32_t aIndent = 0) const MOZ_OVERRIDE;
#endif

  // Rule methods
  virtual int32_t GetType() const;
  virtual already_AddRefed<Rule> Clone() const;

  void SetSheet(nsCSSStyleSheet*);

  virtual size_t SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const;

  // nsIDOMCSSRule interface
  NS_DECL_NSIDOMCSSRULE

  // nsIDOMCSSImportRule interface
  NS_DECL_NSIDOMCSSIMPORTRULE

private:
  nsString  mURLSpec;
  nsRefPtr<nsMediaList> mMedia;
  nsRefPtr<nsCSSStyleSheet> mChildSheet;
};

} // namespace css
} // namespace mozilla

#endif /* mozilla_css_ImportRule_h__ */
