/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of DOMTokenList specified by HTML5.
 */

#ifndef nsDOMTokenList_h___
#define nsDOMTokenList_h___

#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsDOMString.h"
#include "nsWhitespaceTokenizer.h"
#include "nsWrapperCache.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/DOMTokenListSupportedTokens.h"

namespace mozilla {
class ErrorResult;
namespace dom {
class DocGroup;
class Element;
}  // namespace dom
}  // namespace mozilla

class nsAttrValue;
class nsAtom;

// nsISupports must be on the primary inheritance chain

class nsDOMTokenList : public nsISupports, public nsWrapperCache {
 protected:
  using Element = mozilla::dom::Element;
  using DocGroup = mozilla::dom::DocGroup;
  using WhitespaceTokenizer =
      nsWhitespaceTokenizerTemplate<nsContentUtils::IsHTMLWhitespace>;

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(nsDOMTokenList)

  nsDOMTokenList(Element* aElement, nsAtom* aAttrAtom,
                 const mozilla::dom::DOMTokenListSupportedTokenArray = nullptr);

  virtual JSObject* WrapObject(JSContext* cx,
                               JS::Handle<JSObject*> aGivenProto) override;

  Element* GetParentObject() { return mElement; }

  DocGroup* GetDocGroup() const;

  uint32_t Length();
  void Item(uint32_t aIndex, nsAString& aResult) {
    bool found;
    IndexedGetter(aIndex, found, aResult);
    if (!found) {
      SetDOMStringToNull(aResult);
    }
  }
  void IndexedGetter(uint32_t aIndex, bool& aFound, nsAString& aResult);
  bool Contains(const nsAString& aToken);
  void Add(const nsAString& aToken, mozilla::ErrorResult& aError);
  void Add(const nsTArray<nsString>& aTokens, mozilla::ErrorResult& aError);
  void Remove(const nsAString& aToken, mozilla::ErrorResult& aError);
  void Remove(const nsTArray<nsString>& aTokens, mozilla::ErrorResult& aError);
  bool Replace(const nsAString& aToken, const nsAString& aNewToken,
               mozilla::ErrorResult& aError);
  bool Toggle(const nsAString& aToken,
              const mozilla::dom::Optional<bool>& force,
              mozilla::ErrorResult& aError);
  bool Supports(const nsAString& aToken, mozilla::ErrorResult& aError);

  void GetValue(nsAString& aResult);
  void SetValue(const nsAString& aValue, mozilla::ErrorResult& rv);

 protected:
  virtual ~nsDOMTokenList();

  nsresult CheckToken(const nsAString& aStr);
  nsresult CheckTokens(const nsTArray<nsString>& aStr);
  void AddInternal(const nsAttrValue* aAttr, const nsTArray<nsString>& aTokens);
  void RemoveInternal(const nsAttrValue* aAttr,
                      const nsTArray<nsString>& aTokens);
  bool ReplaceInternal(const nsAttrValue* aAttr, const nsAString& aToken,
                       const nsAString& aNewToken);
  inline const nsAttrValue* GetParsedAttr();

  nsCOMPtr<Element> mElement;
  RefPtr<nsAtom> mAttrAtom;
  const mozilla::dom::DOMTokenListSupportedTokenArray mSupportedTokens;
};

#endif  // nsDOMTokenList_h___
