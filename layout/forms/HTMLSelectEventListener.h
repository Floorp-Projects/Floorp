/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_HTMLSelectEventListener_h
#define mozilla_HTMLSelectEventListener_h

#include "nsIDOMEventListener.h"
#include "nsStubMutationObserver.h"

class nsIFrame;
class nsListControlFrame;

namespace mozilla {

namespace dom {
class HTMLSelectElement;
class HTMLOptionElement;
class Event;
}  // namespace dom

/**
 * HTMLSelectEventListener
 * This class is responsible for propagating events to the select element while
 * it has a frame.
 * Frames are not refcounted so they can't be used as event listeners.
 */

class HTMLSelectEventListener final : public nsStubMutationObserver,
                                      public nsIDOMEventListener {
 public:
  enum class SelectType : uint8_t { Listbox, Combobox };
  HTMLSelectEventListener(dom::HTMLSelectElement& aElement,
                          SelectType aSelectType)
      : mElement(&aElement), mIsCombobox(aSelectType == SelectType::Combobox) {
    Attach();
  }

  NS_DECL_ISUPPORTS

  // For comboboxes, we need to keep the list up to date when options change.
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED

  // nsIDOMEventListener
  MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHOD HandleEvent(dom::Event*) override;

  void Attach();
  void Detach();

  dom::HTMLOptionElement* GetCurrentOption() const;

  MOZ_CAN_RUN_SCRIPT void FireOnInputAndOnChange();

 private:
  // This is always guaranteed to be > 0, but callers want signed integers so we
  // do the cast for them.
  int32_t ItemsPerPage() const;

  nsListControlFrame* GetListControlFrame() const;

  MOZ_CAN_RUN_SCRIPT nsresult KeyDown(dom::Event*);
  MOZ_CAN_RUN_SCRIPT nsresult KeyPress(dom::Event*);
  MOZ_CAN_RUN_SCRIPT nsresult MouseDown(dom::Event*);
  MOZ_CAN_RUN_SCRIPT nsresult MouseUp(dom::Event*);
  MOZ_CAN_RUN_SCRIPT nsresult MouseMove(dom::Event*);

  void AdjustIndexForDisabledOpt(int32_t aStartIndex, int32_t& aNewIndex,
                                 int32_t aNumOptions, int32_t aDoAdjustInc,
                                 int32_t aDoAdjustIncNext);
  bool IsOptionInteractivelySelectable(uint32_t aIndex) const;
  int32_t GetEndSelectionIndex() const;

  MOZ_CAN_RUN_SCRIPT
  void PostHandleKeyEvent(int32_t aNewIndex, uint32_t aCharCode, bool aIsShift,
                          bool aIsControlOrMeta);

  /**
   * Return the first non-disabled option starting at aFromIndex (inclusive).
   * @param aFoundIndex if non-null, set to the index of the returned option
   */
  dom::HTMLOptionElement* GetNonDisabledOptionFrom(
      int32_t aFromIndex, int32_t* aFoundIndex = nullptr) const;

  void ComboboxMightHaveChanged();
  void OptionValueMightHaveChanged(nsIContent* aMutatingNode);

  ~HTMLSelectEventListener();

  RefPtr<dom::HTMLSelectElement> mElement;
  const bool mIsCombobox;
  bool mButtonDown = false;
  bool mControlSelectMode = false;
};

}  // namespace mozilla

#endif  // mozilla_HTMLSelectEventListener_h
