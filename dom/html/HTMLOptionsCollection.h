/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_dom_HTMLOptionsCollection_h
#define mozilla_dom_HTMLOptionsCollection_h

#include "mozilla/Attributes.h"
#include "nsIHTMLCollection.h"
#include "nsWrapperCache.h"

#include "mozilla/dom/HTMLOptionElement.h"
#include "nsCOMPtr.h"
#include "nsError.h"
#include "nsGenericHTMLElement.h"
#include "nsTArray.h"

namespace mozilla {
class ErrorResult;

namespace dom {

class DocGroup;
class HTMLElementOrLong;
class HTMLOptionElementOrHTMLOptGroupElement;
class HTMLSelectElement;

/**
 * The collection of options in the select (what you get back when you do
 * select.options in DOM)
 */
class HTMLOptionsCollection final : public nsIHTMLCollection,
                                    public nsWrapperCache {
  typedef HTMLOptionElementOrHTMLOptGroupElement HTMLOptionOrOptGroupElement;

 public:
  explicit HTMLOptionsCollection(HTMLSelectElement* aSelect);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  // nsWrapperCache
  using nsWrapperCache::GetWrapper;
  using nsWrapperCache::GetWrapperPreserveColor;
  using nsWrapperCache::PreserveWrapper;
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

 protected:
  virtual ~HTMLOptionsCollection() = default;

  virtual JSObject* GetWrapperPreserveColorInternal() override {
    return nsWrapperCache::GetWrapperPreserveColor();
  }
  virtual void PreserveWrapperInternal(
      nsISupports* aScriptObjectHolder) override {
    nsWrapperCache::PreserveWrapper(aScriptObjectHolder);
  }

 public:
  virtual uint32_t Length() override;
  virtual Element* GetElementAt(uint32_t aIndex) override;
  virtual nsINode* GetParentObject() override;
  DocGroup* GetDocGroup() const;

  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS_AMBIGUOUS(HTMLOptionsCollection,
                                                        nsIHTMLCollection)

  // Helpers for HTMLSelectElement
  /**
   * Insert an option
   * @param aOption the option to insert
   * @param aIndex the index to insert at
   */
  void InsertOptionAt(mozilla::dom::HTMLOptionElement* aOption,
                      uint32_t aIndex) {
    mElements.InsertElementAt(aIndex, aOption);
  }

  /**
   * Remove an option
   * @param aIndex the index of the option to remove
   */
  void RemoveOptionAt(uint32_t aIndex) { mElements.RemoveElementAt(aIndex); }

  /**
   * Get the option at the index
   * @param aIndex the index
   * @param aReturn the option returned [OUT]
   */
  mozilla::dom::HTMLOptionElement* ItemAsOption(uint32_t aIndex) {
    return mElements.SafeElementAt(aIndex, nullptr);
  }

  /**
   * Clears out all options
   */
  void Clear() { mElements.Clear(); }

  /**
   * Append an option to end of array
   */
  void AppendOption(mozilla::dom::HTMLOptionElement* aOption) {
    mElements.AppendElement(aOption);
  }

  /**
   * Finds the index of a given option element.
   * If the option isn't part of the collection, return NS_ERROR_FAILURE
   * without setting aIndex.
   *
   * @param aOption the option to get the index of
   * @param aStartIndex the index to start looking at
   * @param aForward TRUE to look forward, FALSE to look backward
   * @return the option index
   */
  nsresult GetOptionIndex(Element* aOption, int32_t aStartIndex, bool aForward,
                          int32_t* aIndex);

  HTMLOptionElement* GetNamedItem(const nsAString& aName) {
    bool dummy;
    return NamedGetter(aName, dummy);
  }
  HTMLOptionElement* NamedGetter(const nsAString& aName, bool& aFound);
  virtual Element* GetFirstNamedElement(const nsAString& aName,
                                        bool& aFound) override {
    return NamedGetter(aName, aFound);
  }
  void Add(const HTMLOptionOrOptGroupElement& aElement,
           const Nullable<HTMLElementOrLong>& aBefore, ErrorResult& aError);
  void Remove(int32_t aIndex);
  int32_t SelectedIndex();
  void SetSelectedIndex(int32_t aSelectedIndex);
  void IndexedSetter(uint32_t aIndex, HTMLOptionElement* aOption,
                     ErrorResult& aError);
  virtual void GetSupportedNames(nsTArray<nsString>& aNames) override;
  void SetLength(uint32_t aLength, ErrorResult& aError);

 private:
  /** The list of options (holds strong references).  This is infallible, so
   * various members such as InsertOptionAt are also infallible. */
  nsTArray<RefPtr<mozilla::dom::HTMLOptionElement> > mElements;
  /** The select element that contains this array */
  RefPtr<HTMLSelectElement> mSelect;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_HTMLOptionsCollection_h
