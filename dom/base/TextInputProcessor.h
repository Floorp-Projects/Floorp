/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_textinputprocessor_h_
#define mozilla_dom_textinputprocessor_h_

#include "mozilla/Attributes.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/EventForwards.h"
#include "mozilla/Maybe.h"
#include "mozilla/TextEventDispatcher.h"
#include "mozilla/TextEventDispatcherListener.h"
#include "nsITextInputProcessor.h"
#include "nsITextInputProcessorCallback.h"
#include "nsTArray.h"

class nsPIDOMWindowInner;

namespace mozilla {

namespace dom {
class KeyboardEvent;
}  // namespace dom

class TextInputProcessor final : public nsITextInputProcessor,
                                 public widget::TextEventDispatcherListener {
  typedef mozilla::widget::IMENotification IMENotification;
  typedef mozilla::widget::IMENotificationRequests IMENotificationRequests;
  typedef mozilla::widget::TextEventDispatcher TextEventDispatcher;

 public:
  TextInputProcessor();

  NS_DECL_ISUPPORTS
  NS_DECL_NSITEXTINPUTPROCESSOR

  // TextEventDispatcherListener
  MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHOD
  NotifyIME(TextEventDispatcher* aTextEventDispatcher,
            const IMENotification& aNotification) override;

  NS_IMETHOD_(IMENotificationRequests) GetIMENotificationRequests() override;

  NS_IMETHOD_(void)
  OnRemovedFrom(TextEventDispatcher* aTextEventDispatcher) override;

  NS_IMETHOD_(void)
  WillDispatchKeyboardEvent(TextEventDispatcher* aTextEventDispatcher,
                            WidgetKeyboardEvent& aKeyboardEvent,
                            uint32_t aIndexOfKeypress, void* aData) override;

  /**
   * TextInputProcessor manages modifier key state.  E.g., when it dispatches
   * a modifier keydown event, activates proper modifier state and when it
   * dispatches a modifier keyup event, inactivates proper modifier state.
   * This returns all active modifiers in the instance.
   */
  Modifiers GetActiveModifiers() const {
    return mModifierKeyDataArray ? mModifierKeyDataArray->GetActiveModifiers()
                                 : MODIFIER_NONE;
  }

  /**
   * This begins transaction for fuzzing.  This must be called only by
   * FuzzingFunctions since this skips the permission check.
   * See explanation of nsITextInputProcessor::BeginInputTransaction() for
   * the detail.
   */
  nsresult BeginInputTransactionForFuzzing(
      nsPIDOMWindowInner* aWindow, nsITextInputProcessorCallback* aCallback,
      bool* aSucceeded);

  /**
   * The following Keydown() and KeyUp() are same as nsITextInputProcessor's
   * same name methods except the type of event class.  See explanation in
   * nsITextInputProcessor for the detail.
   */
  MOZ_CAN_RUN_SCRIPT nsresult Keydown(const WidgetKeyboardEvent& aKeyboardEvent,
                                      uint32_t aKeyFlags,
                                      uint32_t* aConsumedFlags = nullptr);
  nsresult Keyup(const WidgetKeyboardEvent& aKeyboardEvent, uint32_t aKeyFlags,
                 bool* aDoDefault = nullptr);

  /**
   * GuessCodeNameIndexOfPrintableKeyInUSEnglishLayout() returns CodeNameIndex
   * of a printable key which is in usual keyboard of the platform and when
   * active keyboard layout is US-English.
   * Note that this does not aware of option key mapping on macOS.
   *
   * @param aKeyValue          The key value. Must be a character which can
   *                           be inputted with US-English keyboard layout.
   * @param aLocation          The location of the key.  This is important
   *                           to distinguish whether the key is in Standard
   *                           or Numpad. If this is not some, treated as
   *                           Standard.
   * @return                   Returns CODE_NAME_INDEX_UNKNOWN if there is
   *                           no proper key.
   */
  static CodeNameIndex GuessCodeNameIndexOfPrintableKeyInUSEnglishLayout(
      const nsAString& aKeyValue, const Maybe<uint32_t>& aLocation);

  /**
   * GuessKeyCodeOfPrintableKeyInUSEnglishLayout() returns a key code value
   * of a printable key which is in usual keyboard of the platform and when
   * active keyboard layout is US-English.
   * Note that this does not aware of option key mapping on macOS.
   *
   * @param aKeyValue          The key value. Must be a character which can
   *                           be inputted with US-English keyboard layout.
   * @param aLocation          The location of the key.  This is important
   *                           to distinguish whether the key is in Standard
   *                           or Numpad. If this is not some, treated as
   *                           Standard.
   * @return                   Returns 0 if there is no proper key to input
   *                           aKeyValue with US-English keyboard layout.
   */
  static uint32_t GuessKeyCodeOfPrintableKeyInUSEnglishLayout(
      const nsAString& aKeyValue, const Maybe<uint32_t>& aLocation);

 protected:
  virtual ~TextInputProcessor();

 private:
  bool IsComposing() const;
  nsresult BeginInputTransactionInternal(
      mozIDOMWindow* aWindow, nsITextInputProcessorCallback* aCallback,
      bool aForTests, bool& aSucceeded);
  MOZ_CAN_RUN_SCRIPT nsresult CommitCompositionInternal(
      const WidgetKeyboardEvent* aKeyboardEvent = nullptr,
      uint32_t aKeyFlags = 0, const nsAString* aCommitString = nullptr,
      bool* aSucceeded = nullptr);
  MOZ_CAN_RUN_SCRIPT nsresult
  CancelCompositionInternal(const WidgetKeyboardEvent* aKeyboardEvent = nullptr,
                            uint32_t aKeyFlags = 0);
  MOZ_CAN_RUN_SCRIPT nsresult
  KeydownInternal(const WidgetKeyboardEvent& aKeyboardEvent, uint32_t aKeyFlags,
                  bool aAllowToDispatchKeypress, uint32_t& aConsumedFlags);
  nsresult KeyupInternal(const WidgetKeyboardEvent& aKeyboardEvent,
                         uint32_t aKeyFlags, bool& aDoDefault);
  nsresult IsValidStateForComposition();
  void UnlinkFromTextEventDispatcher();
  nsresult PrepareKeyboardEventToDispatch(WidgetKeyboardEvent& aKeyboardEvent,
                                          uint32_t aKeyFlags);
  /**
   * InitEditCommands() initializes edit commands of aKeyboardEvent.
   * This must be called only in a content process, and aKeyboardEvent must
   * be used only for `eKeyPress` event.
   */
  MOZ_CAN_RUN_SCRIPT nsresult
  InitEditCommands(WidgetKeyboardEvent& aKeyboardEvent) const;

  bool IsValidEventTypeForComposition(
      const WidgetKeyboardEvent& aKeyboardEvent) const;
  nsresult PrepareKeyboardEventForComposition(
      dom::KeyboardEvent* aDOMKeyEvent, uint32_t& aKeyFlags,
      uint8_t aOptionalArgc, WidgetKeyboardEvent*& aKeyboardEvent);

  struct EventDispatcherResult {
    nsresult mResult;
    bool mDoDefault;
    bool mCanContinue;

    EventDispatcherResult()
        : mResult(NS_OK), mDoDefault(true), mCanContinue(true) {}
  };
  MOZ_CAN_RUN_SCRIPT EventDispatcherResult MaybeDispatchKeydownForComposition(
      const WidgetKeyboardEvent* aKeyboardEvent, uint32_t aKeyFlags);
  EventDispatcherResult MaybeDispatchKeyupForComposition(
      const WidgetKeyboardEvent* aKeyboardEvent, uint32_t aKeyFlags);

  /**
   * AutoPendingCompositionResetter guarantees to clear all pending composition
   * data in its destructor.
   */
  class MOZ_STACK_CLASS AutoPendingCompositionResetter {
   public:
    explicit AutoPendingCompositionResetter(TextInputProcessor* aTIP);
    ~AutoPendingCompositionResetter();

   private:
    RefPtr<TextInputProcessor> mTIP;
  };

  /**
   * TextInputProcessor manages modifier state both with .key and .code.
   * For example, left shift key up shouldn't cause inactivating shift state
   * while right shift key is being pressed.
   */
  struct ModifierKeyData {
    // One of modifier key name
    KeyNameIndex mKeyNameIndex;
    // Any code name is allowed.
    CodeNameIndex mCodeNameIndex;
    // A modifier key flag which is activated by the key.
    Modifiers mModifier;

    explicit ModifierKeyData(const WidgetKeyboardEvent& aKeyboardEvent);

    bool operator==(const ModifierKeyData& aOther) const {
      return mKeyNameIndex == aOther.mKeyNameIndex &&
             mCodeNameIndex == aOther.mCodeNameIndex;
    }
  };

  class ModifierKeyDataArray : public nsTArray<ModifierKeyData> {
    NS_INLINE_DECL_REFCOUNTING(ModifierKeyDataArray)

   public:
    Modifiers GetActiveModifiers() const;
    void ActivateModifierKey(const ModifierKeyData& aModifierKeyData);
    void InactivateModifierKey(const ModifierKeyData& aModifierKeyData);
    void ToggleModifierKey(const ModifierKeyData& aModifierKeyData);

   private:
    virtual ~ModifierKeyDataArray() = default;
  };

  void EnsureModifierKeyDataArray() {
    if (mModifierKeyDataArray) {
      return;
    }
    mModifierKeyDataArray = new ModifierKeyDataArray();
  }
  void ActivateModifierKey(const ModifierKeyData& aModifierKeyData) {
    EnsureModifierKeyDataArray();
    mModifierKeyDataArray->ActivateModifierKey(aModifierKeyData);
  }
  void InactivateModifierKey(const ModifierKeyData& aModifierKeyData) {
    if (!mModifierKeyDataArray) {
      return;
    }
    mModifierKeyDataArray->InactivateModifierKey(aModifierKeyData);
  }
  void ToggleModifierKey(const ModifierKeyData& aModifierKeyData) {
    EnsureModifierKeyDataArray();
    mModifierKeyDataArray->ToggleModifierKey(aModifierKeyData);
  }

  TextEventDispatcher* mDispatcher;  // [Weak]
  nsCOMPtr<nsITextInputProcessorCallback> mCallback;
  RefPtr<ModifierKeyDataArray> mModifierKeyDataArray;

  bool mForTests;
};

}  // namespace mozilla

#endif  // #ifndef mozilla_dom_textinputprocessor_h_
