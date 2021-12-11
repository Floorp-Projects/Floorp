/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLFormControlsCollection_h
#define mozilla_dom_HTMLFormControlsCollection_h

#include "nsIHTMLCollection.h"
#include "nsInterfaceHashtable.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"

class nsGenericHTMLFormElement;
class nsIContent;
class nsIFormControl;
template <class T>
class RefPtr;

namespace mozilla::dom {
class Element;
class HTMLFormElement;
class HTMLImageElement;
class OwningRadioNodeListOrElement;
template <typename>
struct Nullable;

class HTMLFormControlsCollection final : public nsIHTMLCollection,
                                         public nsWrapperCache {
 public:
  explicit HTMLFormControlsCollection(HTMLFormElement* aForm);

  void DropFormReference();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  virtual uint32_t Length() override;
  virtual Element* GetElementAt(uint32_t index) override;
  virtual nsINode* GetParentObject() override;

  virtual Element* GetFirstNamedElement(const nsAString& aName,
                                        bool& aFound) override;

  void NamedGetter(const nsAString& aName, bool& aFound,
                   Nullable<OwningRadioNodeListOrElement>& aResult);
  void NamedItem(const nsAString& aName,
                 Nullable<OwningRadioNodeListOrElement>& aResult) {
    bool dummy;
    NamedGetter(aName, dummy, aResult);
  }
  virtual void GetSupportedNames(nsTArray<nsString>& aNames) override;

  nsresult AddElementToTable(nsGenericHTMLFormElement* aChild,
                             const nsAString& aName);
  nsresult AddImageElementToTable(HTMLImageElement* aChild,
                                  const nsAString& aName);
  nsresult RemoveElementFromTable(nsGenericHTMLFormElement* aChild,
                                  const nsAString& aName);
  nsresult IndexOfContent(nsIContent* aContent, int32_t* aIndex);

  nsISupports* NamedItemInternal(const nsAString& aName);

  /**
   * Create a sorted list of form control elements. This list is sorted
   * in document order and contains the controls in the mElements and
   * mNotInElements list. This function does not add references to the
   * elements.
   *
   * @param aControls The list of sorted controls[out].
   * @return NS_OK or NS_ERROR_OUT_OF_MEMORY.
   */
  nsresult GetSortedControls(
      nsTArray<RefPtr<nsGenericHTMLFormElement>>& aControls) const;

  // nsWrapperCache
  using nsWrapperCache::GetWrapperPreserveColor;
  using nsWrapperCache::PreserveWrapper;
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

 protected:
  virtual ~HTMLFormControlsCollection();
  virtual JSObject* GetWrapperPreserveColorInternal() override {
    return nsWrapperCache::GetWrapperPreserveColor();
  }
  virtual void PreserveWrapperInternal(
      nsISupports* aScriptObjectHolder) override {
    nsWrapperCache::PreserveWrapper(aScriptObjectHolder);
  }

 public:
  static bool ShouldBeInElements(nsIFormControl* aFormControl);

  HTMLFormElement* mForm;  // WEAK - the form owns me

  nsTArray<nsGenericHTMLFormElement*>
      mElements;  // Holds WEAK references - bug 36639

  // This array holds on to all form controls that are not contained
  // in mElements (form.elements in JS, see ShouldBeInFormControl()).
  // This is needed to properly clean up the bi-directional references
  // (both weak and strong) between the form and its form controls.

  nsTArray<nsGenericHTMLFormElement*> mNotInElements;  // Holds WEAK references

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(HTMLFormControlsCollection)

 protected:
  // Drop all our references to the form elements
  void Clear();

  // A map from an ID or NAME attribute to the form control(s), this
  // hash holds strong references either to the named form control, or
  // to a list of named form controls, in the case where this hash
  // holds on to a list of named form controls the list has weak
  // references to the form control.

  nsInterfaceHashtable<nsStringHashKey, nsISupports> mNameLookupTable;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_HTMLFormControlsCollection_h
