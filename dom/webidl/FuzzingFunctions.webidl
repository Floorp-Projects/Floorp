/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/*
 * Various functions useful for automated fuzzing that are enabled
 * only in --enable-fuzzing builds, because they may be dangerous to
 * enable on untrusted pages.
*/

[Pref="fuzzing.enabled",
 Exposed=Window]
namespace FuzzingFunctions {
  /**
   * Synchronously perform a garbage collection.
   */
  undefined garbageCollect();

  /**
   * Synchronously perform a compacting garbage collection.
   */
  undefined garbageCollectCompacting();

  /**
   * Trigger a forced crash.
   */
  undefined crash(optional DOMString reason = "");

  /**
   * Synchronously perform a cycle collection.
   */
  undefined cycleCollect();

  /**
   * Send a memory pressure event, causes shrinking GC, cycle collection and
   * other actions.
   */
  undefined memoryPressure();

  /**
   * Enable accessibility.
   */
  [Throws]
  undefined enableAccessibility();

  /**
   * Send IPC fuzzing ready event to parent.
   */
  undefined signalIPCReady();

  /**
   * synthesizeKeyboardEvents() synthesizes a set of "keydown",
   * "keypress" (only when it's necessary) and "keyup" events in top DOM window
   * in current process (and the synthesized events will be retargeted to
   * focused window/document/element).  I.e, this is currently not dispatched
   * via the main process if you call this in a content process.  Therefore, in
   * the case, some default action handlers which are only in the main process
   * will never run.  Note that this does not allow to synthesize keyboard
   * events if this is called from a keyboard event or composition event
   * listener.
   *
   * @param aKeyValue          If you want to synthesize non-printable key
   *                           events, you need to set one of key values
   *                           defined by "UI Events KeyboardEvent key Values".
   *                           You can check our current support values in
   *                           dom/events/KeyNameList.h
   *                           If you want to synthesize printable key events,
   *                           you can set any string value including empty
   *                           string.
   *                           Note that |key| value in aDictionary is always
   *                           ignored.
   * @param aDictionary        If you want to synthesize simple key press
   *                           without any modifiers, you can omit this.
   *                           Otherwise, specify this with proper values.
   *                           If |code| is omitted or empty string, this
   *                           guesses proper code value in US-English
   *                           keyboard.  Otherwise, the value must be empty
   *                           string or known code value defined by "UI Events
   *                           KeyboardEvent code Values".  You can check our
   *                           current support values in
   *                           dom/events/PhysicalKeyCodeNameList.h.
   *                           If |keyCode| is omitted or 0, this guesses
   *                           proper keyCode value in US-English keyboard.
   *                           If |location| is omitted or 0, this assumes
   *                           that left modifier key is pressed if aKeyValue
   *                           is one of such modifier keys.
   *                           |key|, |isComposing|, |charCode| and |which|
   *                           are always ignored.
   *                           Modifier states like |shiftKey|, |altKey|,
   *                           |modifierAltGraph|, |modifierCapsLock| and
   *                           |modifierNumLock| are not adjusted for
   *                           aKeyValue.  Please specify them manually if
   *                           necessary.
   *                           Note that this API does not allow to dispatch
   *                           known key events with empty |code| value and
   *                           0 |keyCode| value since it's unsual situation
   *                           especially 0 |keyCode| value with known key.
   *                           Note that when you specify only one of |code|
   *                           and |keyCode| value, the other will be guessed
   *                           from US-English keyboard layout.  So, if you
   *                           want to emulate key press with another keyboard
   *                           layout, you should specify both values.
   *
   * For example:
   *   // Synthesize "Tab" key events.
   *   synthesizeKeyboardEvents("Tab");
   *   // Synthesize Shift + Tab key events.
   *   synthesizeKeyboardEvents("Tab", { shiftKey: true });
   *   // Synthesize Control + A key events.
   *   synthesizeKeyboardEvents("a", { controlKey: true });
   *   // Synthesize Control + Shift + A key events.
   *   synthesizeKeyboardEvents("A", { controlKey: true,
   *                                   shitKey: true });
   *   // Synthesize "Enter" key on numpad.
   *   synthesizeKeyboardEvents("Enter", { code: "NumpadEnter" });
   *   // Synthesize right "Shift" key.
   *   synthesizeKeyboardEvents("Shift", { code: "ShiftRight" });
   *   // Synthesize "1" on numpad.
   *   synthesizeKeyboardEvents("1", { code: "Numpad1",
   *                                   modifierNumLock: true });
   *   // Synthesize "End" on numpad.
   *   synthesizeKeyboardEvents("End", { code: "Numpad1" });
   *   // Synthesize "%" key of US-English keyboard layout.
   *   synthesizeKeyboardEvents("%", { shiftKey: true });
   *   // Synthesize "*" key of Japanese keyboard layout.
   *   synthesizeKeyboardEvents("*", { code: "Quote",
   *                                   shiftKey: true,
   *                                   keyCode: KeyboardEvent.DOM_VK_COLON });
   */
  [Throws]
  undefined synthesizeKeyboardEvents(DOMString aKeyValue,
                                     optional KeyboardEventInit aDictionary = {});
};
