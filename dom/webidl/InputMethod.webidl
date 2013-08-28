/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[JSImplementation="@mozilla.org/b2g-inputmethod;1",
 NavigatorProperty="mozInputMethod"]
interface MozInputMethod : EventTarget {
  // Input Method Manager contain a few global methods expose to apps
  readonly attribute MozInputMethodManager mgmt;

  // Fired when the input context changes, include changes from and to null.
  // The new InputContext instance will be available in the event
  // object under |inputcontext| property.  When it changes to null it
  // means the app (the user of this API) no longer has the control of
  // the original focused input field.
  // Note that if the app saves the original context, it might get
  // void; implementation decides when to void the input context.
  attribute EventHandler oninputcontextchange;

  // An "input context" is mapped to a text field that the app is
  // allow to mutate.  this attribute should be null when there is no
  // text field currently focused.
  readonly attribute MozInputContext? inputcontext;
};

// Manages the list of IMEs, enables/disables IME and switches to an
// IME.
[JSImplementation="@mozilla.org/b2g-imm;1"]
interface MozInputMethodManager {
  // Ask the OS to show a list of available IMEs for users to switch from.
  // OS should ignore this request if the app is currently not the active one.
  void showAll();

  // Ask the OS to switch away from the current active Keyboard app.
  // OS should ignore this request if the app is currently not the active one.
  void next();

  // To know if the OS supports IME switching or not.
  // Use case: let the keyboard app knows if it is necessary to show the "IME switching"
  // (globe) button. We have a use case that when there is only one IME enabled, we
  // should not show the globe icon.
  boolean supportsSwitching();

  // Ask the OS to hide the current active Keyboard app. (was: |removeFocus()|)
  // OS should ignore this request if the app is currently not the active one.
  // The OS will void the current input context (if it exists).
  // This method belong to |mgmt| because we would like to allow Keyboard to access to
  // this method w/o a input context.
  void hide();
};

// The input context, which consists of attributes and information of current input field.
// It also hosts the methods available to the keyboard app to mutate the input field represented.
// An "input context" gets void when the app is no longer allowed to interact with the text field,
// e.g., the text field does no longer exist, the app is being switched to background, and etc.
[JSImplementation="@mozilla.org/b2g-inputcontext;1"]
interface MozInputContext: EventTarget {
   // The tag name of input field, which is enum of "input", "textarea", or "contenteditable"
   readonly attribute DOMString? type;
   // The type of the input field, which is enum of text, number, password, url, search, email, and so on.
   // See http://www.whatwg.org/specs/web-apps/current-work/multipage/states-of-the-type-attribute.html#states-of-the-type-attribute
   readonly attribute DOMString? inputType;
   /*
    * The inputmode string, representing the input mode.
    * See http://www.whatwg.org/specs/web-apps/current-work/multipage/association-of-controls-and-forms.html#input-modalities:-the-inputmode-attribute
    */
   readonly attribute DOMString? inputMode;
   /*
    * The primary language for the input field.
    * It is the value of HTMLElement.lang.
    * See http://www.whatwg.org/specs/web-apps/current-work/multipage/elements.html#htmlelement
    */
   readonly attribute DOMString? lang;
   /*
    * Get the whole text content of the input field.
    * @return DOMString
    */
   Promise getText(optional long offset, optional long length);
   // The start and stop position of the selection.
   readonly attribute long selectionStart;
   readonly attribute long selectionEnd;

    /*
     * Set the selection range of the the editable text.
     * Note: This method cannot be used to move the cursor during composition. Calling this
     * method will cancel composition.
     * @param start The beginning of the selected text.
     * @param length The length of the selected text.
     *
     * Note that the start position should be less or equal to the end position.
     * To move the cursor, set the start and end position to the same value.
     *
     * @return boolean
     */
    Promise setSelectionRange(long start, long length);

    /* User moves the cursor, or changes the selection with other means. If the text around
     * cursor has changed, but the cursor has not been moved, the IME won't get notification.
     */
    attribute EventHandler onselectionchange;

    /*
     * Commit text to current input field and replace text around
     * cursor position. It will clear the current composition.
     *
     * @param text The string to be replaced with.
     * @param offset The offset from the cursor position where replacing starts. Defaults to 0.
     * @param length The length of text to replace. Defaults to 0.
     * @return boolean
     */
     Promise replaceSurroundingText(DOMString text, optional long offset, optional long length);

    /*
     *
     * Delete text around the cursor.
     * @param offset The offset from the cursor position where deletion starts.
     * @param length The length of text to delete.
     * TODO: maybe updateSurroundingText(DOMString beforeText, DOMString afterText); ?
     * @return boolean
     */
    Promise deleteSurroundingText(long offset, long length);

    /*
    * Notifies when the text around the cursor is changed, due to either text
    * editing or cursor movement. If the cursor has been moved, but the text around has not
    * changed, the IME won't get notification.
    *
    * The event handler function is specified as:
    * @param beforeString Text before and including cursor position.
    * @param afterString Text after and excluing cursor position.
    * function(DOMString beforeText, DOMString afterText) {
    * ...
    *  }
    */
    attribute EventHandler onsurroundingtextchange;

    /*
      * send a character with its key events.
      * @param modifiers see http://mxr.mozilla.org/mozilla-central/source/dom/interfaces/base/nsIDOMWindowUtils.idl#206
      * @return true if succeeds. Otherwise false if the input context becomes void.
      * Alternative: sendKey(KeyboardEvent event), but we will likely
      * waste memory for creating the KeyboardEvent object.
      */
    Promise sendKey(long keyCode, long charCode, long modifiers);

    /*
     * Set current composition. It will start or update composition.
     * @param cursor Position in the text of the cursor.
     *
     * The API implementation should automatically ends the composition
     * session (with event and confirm the current composition) if
     * endComposition is never called. Same apply when the inputContext is lost
     * during a unfinished composition session.
     */
    Promise setComposition(DOMString text, long cursor);

    /*
     * End composition and actually commit the text. (was |commitText(text, offset, length)|)
     * Ending the composition with an empty string will not send any text.
     * Note that if composition always ends automatically (with the current composition committed)
     * if the composition did not explicitly with |endComposition()| but was interrupted with
     * |sendKey()|, |setSelectionRange()|, user moving the cursor, or remove the focus, etc.
     *
     * @param text The text
     */
    Promise endComposition(DOMString text);
};
