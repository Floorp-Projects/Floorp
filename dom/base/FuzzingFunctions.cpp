/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FuzzingFunctions.h"

#include "nsJSEnvironment.h"
#include "js/GCAPI.h"
#include "mozilla/dom/KeyboardEvent.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/TextEvents.h"
#include "mozilla/TextInputProcessor.h"
#include "nsFocusManager.h"
#include "nsIAccessibilityService.h"
#include "nsPIDOMWindow.h"
#include "xpcAccessibilityService.h"

namespace mozilla {
namespace dom {

/* static */
void FuzzingFunctions::GarbageCollect(const GlobalObject&) {
  nsJSContext::GarbageCollectNow(JS::GCReason::COMPONENT_UTILS,
                                 nsJSContext::NonIncrementalGC,
                                 nsJSContext::NonShrinkingGC);
}

/* static */
void FuzzingFunctions::GarbageCollectCompacting(const GlobalObject&) {
  nsJSContext::GarbageCollectNow(JS::GCReason::COMPONENT_UTILS,
                                 nsJSContext::NonIncrementalGC,
                                 nsJSContext::ShrinkingGC);
}

/* static */
void FuzzingFunctions::CycleCollect(const GlobalObject&) {
  nsJSContext::CycleCollectNow();
}

void FuzzingFunctions::MemoryPressure(const GlobalObject&) {
  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  os->NotifyObservers(nullptr, "memory-pressure", u"heap-minimize");
}

/* static */
void FuzzingFunctions::EnableAccessibility(const GlobalObject&,
                                           ErrorResult& aRv) {
  RefPtr<nsIAccessibilityService> a11y;
  nsresult rv;

  rv = NS_GetAccessibilityService(getter_AddRefs(a11y));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

struct ModifierKey final {
  Modifier mModifier;
  KeyNameIndex mKeyNameIndex;
  bool mLockable;

  ModifierKey(Modifier aModifier, KeyNameIndex aKeyNameIndex, bool aLockable)
      : mModifier(aModifier),
        mKeyNameIndex(aKeyNameIndex),
        mLockable(aLockable) {}
};

static const ModifierKey kModifierKeys[] = {
    ModifierKey(MODIFIER_ALT, KEY_NAME_INDEX_Alt, false),
    ModifierKey(MODIFIER_ALTGRAPH, KEY_NAME_INDEX_AltGraph, false),
    ModifierKey(MODIFIER_CONTROL, KEY_NAME_INDEX_Control, false),
    ModifierKey(MODIFIER_FN, KEY_NAME_INDEX_Fn, false),
    ModifierKey(MODIFIER_META, KEY_NAME_INDEX_Meta, false),
    ModifierKey(MODIFIER_OS, KEY_NAME_INDEX_OS, false),
    ModifierKey(MODIFIER_SHIFT, KEY_NAME_INDEX_Shift, false),
    ModifierKey(MODIFIER_SYMBOL, KEY_NAME_INDEX_Symbol, false),
    ModifierKey(MODIFIER_CAPSLOCK, KEY_NAME_INDEX_CapsLock, true),
    ModifierKey(MODIFIER_FNLOCK, KEY_NAME_INDEX_FnLock, true),
    ModifierKey(MODIFIER_NUMLOCK, KEY_NAME_INDEX_NumLock, true),
    ModifierKey(MODIFIER_SCROLLLOCK, KEY_NAME_INDEX_ScrollLock, true),
    ModifierKey(MODIFIER_SYMBOLLOCK, KEY_NAME_INDEX_SymbolLock, true),
};

/* static */
Modifiers FuzzingFunctions::ActivateModifiers(
    TextInputProcessor* aTextInputProcessor, Modifiers aModifiers,
    nsIWidget* aWidget, ErrorResult& aRv) {
  MOZ_ASSERT(aTextInputProcessor);

  if (aModifiers == MODIFIER_NONE) {
    return MODIFIER_NONE;
  }

  // We don't want to dispatch modifier key event from here.  In strictly
  // speaking, all necessary modifiers should be activated with dispatching
  // each modifier key event.  However, we cannot keep storing
  // TextInputProcessor instance for multiple SynthesizeKeyboardEvents() calls.
  // So, if some callers need to emulate modifier key events, they should do
  // it by themselves.
  uint32_t flags = nsITextInputProcessor::KEY_NON_PRINTABLE_KEY |
                   nsITextInputProcessor::KEY_DONT_DISPATCH_MODIFIER_KEY_EVENT;

  Modifiers activatedModifiers = MODIFIER_NONE;
  Modifiers activeModifiers = aTextInputProcessor->GetActiveModifiers();
  for (const ModifierKey& kModifierKey : kModifierKeys) {
    if (!(kModifierKey.mModifier & aModifiers)) {
      continue;  // Not requested modifier.
    }
    if (kModifierKey.mModifier & activeModifiers) {
      continue;  // Already active, do nothing.
    }
    WidgetKeyboardEvent event(true, eVoidEvent, aWidget);
    // mKeyCode will be computed by TextInputProcessor automatically.
    event.mKeyNameIndex = kModifierKey.mKeyNameIndex;
    aRv = aTextInputProcessor->Keydown(event, flags);
    if (NS_WARN_IF(aRv.Failed())) {
      return activatedModifiers;
    }
    if (kModifierKey.mLockable) {
      aRv = aTextInputProcessor->Keyup(event, flags);
      if (NS_WARN_IF(aRv.Failed())) {
        return activatedModifiers;
      }
    }
    activatedModifiers |= kModifierKey.mModifier;
  }
  return activatedModifiers;
}

/* static */
Modifiers FuzzingFunctions::InactivateModifiers(
    TextInputProcessor* aTextInputProcessor, Modifiers aModifiers,
    nsIWidget* aWidget, ErrorResult& aRv) {
  MOZ_ASSERT(aTextInputProcessor);

  if (aModifiers == MODIFIER_NONE) {
    return MODIFIER_NONE;
  }

  // We don't want to dispatch modifier key event from here.  In strictly
  // speaking, all necessary modifiers should be activated with dispatching
  // each modifier key event.  However, we cannot keep storing
  // TextInputProcessor instance for multiple SynthesizeKeyboardEvents() calls.
  // So, if some callers need to emulate modifier key events, they should do
  // it by themselves.
  uint32_t flags = nsITextInputProcessor::KEY_NON_PRINTABLE_KEY |
                   nsITextInputProcessor::KEY_DONT_DISPATCH_MODIFIER_KEY_EVENT;

  Modifiers inactivatedModifiers = MODIFIER_NONE;
  Modifiers activeModifiers = aTextInputProcessor->GetActiveModifiers();
  for (const ModifierKey& kModifierKey : kModifierKeys) {
    if (!(kModifierKey.mModifier & aModifiers)) {
      continue;  // Not requested modifier.
    }
    if (kModifierKey.mModifier & activeModifiers) {
      continue;  // Already active, do nothing.
    }
    WidgetKeyboardEvent event(true, eVoidEvent, aWidget);
    // mKeyCode will be computed by TextInputProcessor automatically.
    event.mKeyNameIndex = kModifierKey.mKeyNameIndex;
    if (kModifierKey.mLockable) {
      aRv = aTextInputProcessor->Keydown(event, flags);
      if (NS_WARN_IF(aRv.Failed())) {
        return inactivatedModifiers;
      }
    }
    aRv = aTextInputProcessor->Keyup(event, flags);
    if (NS_WARN_IF(aRv.Failed())) {
      return inactivatedModifiers;
    }
    inactivatedModifiers |= kModifierKey.mModifier;
  }
  return inactivatedModifiers;
}

/* static */
void FuzzingFunctions::SynthesizeKeyboardEvents(
    const GlobalObject& aGlobalObject, const nsAString& aKeyValue,
    const KeyboardEventInit& aDict, ErrorResult& aRv) {
  // Prepare keyboard event to synthesize first.
  uint32_t flags = 0;
  // Don't modify the given dictionary since caller may want to modify
  // a part of it and call this with it again.
  WidgetKeyboardEvent event(true, eVoidEvent, nullptr);
  event.mKeyCode = aDict.mKeyCode;
  event.mCharCode = 0;  // Ignore.
  event.mKeyNameIndex = WidgetKeyboardEvent::GetKeyNameIndex(aKeyValue);
  if (event.mKeyNameIndex == KEY_NAME_INDEX_USE_STRING) {
    event.mKeyValue = aKeyValue;
  }
  // code value should be empty string or one of valid code value.
  event.mCodeNameIndex =
      aDict.mCode.IsEmpty()
          ? CODE_NAME_INDEX_UNKNOWN
          : WidgetKeyboardEvent::GetCodeNameIndex(aDict.mCode);
  if (NS_WARN_IF(event.mCodeNameIndex == CODE_NAME_INDEX_USE_STRING)) {
    // Meaning that the code value is specified but it's not a known code
    // value.  TextInputProcessor does not support synthesizing keyboard
    // events with unknown code value.  So, returns error now.
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return;
  }
  event.mLocation = aDict.mLocation;
  event.mIsRepeat = aDict.mRepeat;

#define SET_MODIFIER(aName, aValue) \
  if (aDict.m##aName) {             \
    event.mModifiers |= aValue;     \
  }

  SET_MODIFIER(CtrlKey, MODIFIER_CONTROL)
  SET_MODIFIER(ShiftKey, MODIFIER_SHIFT)
  SET_MODIFIER(AltKey, MODIFIER_ALT)
  SET_MODIFIER(MetaKey, MODIFIER_META)
  SET_MODIFIER(ModifierAltGraph, MODIFIER_ALTGRAPH)
  SET_MODIFIER(ModifierCapsLock, MODIFIER_CAPSLOCK)
  SET_MODIFIER(ModifierFn, MODIFIER_FN)
  SET_MODIFIER(ModifierFnLock, MODIFIER_FNLOCK)
  SET_MODIFIER(ModifierNumLock, MODIFIER_NUMLOCK)
  SET_MODIFIER(ModifierOS, MODIFIER_OS)
  SET_MODIFIER(ModifierScrollLock, MODIFIER_SCROLLLOCK)
  SET_MODIFIER(ModifierSymbol, MODIFIER_SYMBOL)
  SET_MODIFIER(ModifierSymbolLock, MODIFIER_SYMBOLLOCK)

#undef SET_MODIFIER

  // If we could distinguish whether the caller specified 0 explicitly or
  // not, we would skip computing the key location when it's specified
  // explicitly.  However, this caller probably won't test tricky keyboard
  // events, so, it must be enough even though caller cannot set location
  // to 0.
  Maybe<uint32_t> maybeNonStandardLocation;
  if (!event.mLocation) {
    maybeNonStandardLocation = mozilla::Some(event.mLocation);
  }

  // If the key is a printable key and |.code| and/or |.keyCode| value is
  // not specified as non-zero explicitly, let's assume that the caller
  // emulates US-English keyboard's behavior (because otherwise, caller
  // should set both values.
  if (event.mKeyNameIndex == KEY_NAME_INDEX_USE_STRING) {
    if (event.mCodeNameIndex == CODE_NAME_INDEX_UNKNOWN) {
      event.mCodeNameIndex =
          TextInputProcessor::GuessCodeNameIndexOfPrintableKeyInUSEnglishLayout(
              event.mKeyValue, maybeNonStandardLocation);
      MOZ_ASSERT(event.mCodeNameIndex != CODE_NAME_INDEX_USE_STRING);
    }
    if (!event.mKeyCode) {
      event.mKeyCode =
          TextInputProcessor::GuessKeyCodeOfPrintableKeyInUSEnglishLayout(
              event.mKeyValue, maybeNonStandardLocation);
      if (!event.mKeyCode) {
        // Prevent to recompute keyCode in TextInputProcessor.
        flags |= nsITextInputProcessor::KEY_KEEP_KEYCODE_ZERO;
      }
    }
  }
  // If the key is a non-printable key, we can compute |.code| value of
  // usual keyboard of the platform.  Note that |.keyCode| value for
  // non-printable key will be computed by TextInputProcessor.  So, we need
  // to take care only |.code| value here.
  else if (event.mCodeNameIndex == CODE_NAME_INDEX_UNKNOWN) {
    event.mCodeNameIndex =
        WidgetKeyboardEvent::ComputeCodeNameIndexFromKeyNameIndex(
            event.mKeyNameIndex, maybeNonStandardLocation);
  }

  // Synthesize keyboard events in a DOM window which is in-process top one.
  // For emulating user input, this is better than dispatching the events in
  // the caller's DOM window because this approach can test the path redirecting
  // the events to focused subdocument too.  However, for now, we cannot
  // dispatch it via another process without big changes.  Therefore, we should
  // use in-process top window instead.  If you need to test the path in the
  // parent process to, please file a feature request bug.
  nsCOMPtr<nsPIDOMWindowInner> windowInner =
      do_QueryInterface(aGlobalObject.GetAsSupports());
  if (!windowInner) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return;
  }

  nsPIDOMWindowOuter* inProcessTopWindowOuter =
      windowInner->GetInProcessScriptableTop();
  if (NS_WARN_IF(!inProcessTopWindowOuter)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsIDocShell* docShell = inProcessTopWindowOuter->GetDocShell();
  if (NS_WARN_IF(!docShell)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  RefPtr<nsPresContext> presContext = docShell->GetPresContext();
  if (NS_WARN_IF(!presContext)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  event.mWidget = presContext->GetRootWidget();
  if (NS_WARN_IF(!event.mWidget)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsCOMPtr<nsPIDOMWindowInner> inProcessTopWindowInner =
      inProcessTopWindowOuter->EnsureInnerWindow();
  if (NS_WARN_IF(!inProcessTopWindowInner)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  RefPtr<TextInputProcessor> textInputProcessor = new TextInputProcessor();
  bool beganInputTransaction = false;
  aRv = textInputProcessor->BeginInputTransactionForFuzzing(
      inProcessTopWindowInner, nullptr, &beganInputTransaction);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
  if (NS_WARN_IF(!beganInputTransaction)) {
    // This is possible if a keyboard event listener or something tries to
    // dispatch next keyboard events during dispatching a keyboard event via
    // TextInputProcessor.
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  // First, activate necessary modifiers.
  // MOZ_KnownLive(event.mWidget) is safe because `event` is an instance in
  // the stack, and `mWidget` is `nsCOMPtr<nsIWidget>`.
  Modifiers activatedModifiers = ActivateModifiers(
      textInputProcessor, event.mModifiers, MOZ_KnownLive(event.mWidget), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  // Then, dispatch keydown and keypress.
  aRv = textInputProcessor->Keydown(event, flags);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  // Then, dispatch keyup.
  aRv = textInputProcessor->Keyup(event, flags);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  // Finally, inactivate some modifiers which are activated by this call.
  // MOZ_KnownLive(event.mWidget) is safe because `event` is an instance in
  // the stack, and `mWidget` is `nsCOMPtr<nsIWidget>`.
  InactivateModifiers(textInputProcessor, activatedModifiers,
                      MOZ_KnownLive(event.mWidget), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  // Unfortunately, we cannot keep storing modifier state in the
  // TextInputProcessor since if we store it into a static variable,
  // we need to take care of resetting it when the caller wants.
  // However, that makes API more complicated.  So, until they need
  // to want
}

}  // namespace dom
}  // namespace mozilla
