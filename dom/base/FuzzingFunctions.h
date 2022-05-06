/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FuzzingFunctions
#define mozilla_dom_FuzzingFunctions

#include "mozilla/EventForwards.h"

class nsIWidget;

namespace mozilla {

class ErrorResult;
class TextInputProcessor;

namespace dom {

class GlobalObject;
struct KeyboardEventInit;

class FuzzingFunctions final {
 public:
  static void GarbageCollect(const GlobalObject&);

  static void GarbageCollectCompacting(const GlobalObject&);

  static void Crash(const GlobalObject& aGlobalObject,
                    const nsAString& aKeyValue);

  static void CycleCollect(const GlobalObject&);

  static void MemoryPressure(const GlobalObject&);

  static void SignalIPCReady(const GlobalObject&);

  static void EnableAccessibility(const GlobalObject&, ErrorResult& aRv);

  MOZ_CAN_RUN_SCRIPT_BOUNDARY static void SynthesizeKeyboardEvents(
      const GlobalObject& aGlobalObject, const nsAString& aKeyValue,
      const KeyboardEventInit& aKeyboardEvent, ErrorResult& aRv);

 private:
  /**
   * ActivateModifiers() activates aModifiers in the TextInputProcessor.
   *
   * @param aTextInputProcessor The TIP whose modifier state you want to change.
   * @param aModifiers          Modifiers which you want to activate.
   * @param aWidget             The widget which should be set to
   *                            WidgetKeyboardEvent.
   * @param aRv                 Returns error if TextInputProcessor fails to
   *                            dispatch a modifier key event.
   * @return                    Modifiers which are activated by the call.
   */
  MOZ_CAN_RUN_SCRIPT static Modifiers ActivateModifiers(
      TextInputProcessor* aTextInputProcessor, Modifiers aModifiers,
      nsIWidget* aWidget, ErrorResult& aRv);

  /**
   * InactivateModifiers() inactivates aModifiers in the TextInputProcessor.
   *
   * @param aTextInputProcessor The TIP whose modifier state you want to change.
   * @param aModifiers          Modifiers which you want to inactivate.
   * @param aWidget             The widget which should be set to
   *                            WidgetKeyboardEvent.
   * @param aRv                 Returns error if TextInputProcessor fails to
   *                            dispatch a modifier key event.
   * @return                    Modifiers which are inactivated by the call.
   */
  MOZ_CAN_RUN_SCRIPT static Modifiers InactivateModifiers(
      TextInputProcessor* aTextInputProcessor, Modifiers aModifiers,
      nsIWidget* aWidget, ErrorResult& aRv);
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_FuzzingFunctions
