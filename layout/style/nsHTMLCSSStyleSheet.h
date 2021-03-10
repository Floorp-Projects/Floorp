/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * style sheet and style rule processor representing style attributes
 */

#ifndef nsHTMLCSSStyleSheet_h_
#define nsHTMLCSSStyleSheet_h_

#include "mozilla/Attributes.h"
#include "mozilla/MemoryReporting.h"

#include "nsTHashMap.h"

class nsRuleWalker;
struct MiscContainer;

namespace mozilla {
enum class PseudoStyleType : uint8_t;
namespace dom {
class Element;
}  // namespace dom
}  // namespace mozilla

class nsHTMLCSSStyleSheet final {
 public:
  nsHTMLCSSStyleSheet();

  NS_INLINE_DECL_REFCOUNTING(nsHTMLCSSStyleSheet)

  void CacheStyleAttr(const nsAString& aSerialized, MiscContainer* aValue);
  void EvictStyleAttr(const nsAString& aSerialized, MiscContainer* aValue);
  MiscContainer* LookupStyleAttr(const nsAString& aSerialized);

 private:
  ~nsHTMLCSSStyleSheet();

  nsHTMLCSSStyleSheet(const nsHTMLCSSStyleSheet& aCopy) = delete;
  nsHTMLCSSStyleSheet& operator=(const nsHTMLCSSStyleSheet& aCopy) = delete;

 protected:
  nsTHashMap<nsStringHashKey, MiscContainer*> mCachedStyleAttrs;
};

#endif /* !defined(nsHTMLCSSStyleSheet_h_) */
