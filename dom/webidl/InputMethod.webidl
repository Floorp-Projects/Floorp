/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[JSImplementation="@mozilla.org/b2g-inputmethod;1",
 NavigatorProperty="mozInputMethod",
 Pref="dom.mozInputMethod.enabled",
 CheckAnyPermissions="input input-manage"]
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

  [ChromeOnly]
  // Activate or decactive current input method window.
  void setActive(boolean isActive);

  // Add a dynamically declared input.
  //
  // The id must not be the same with any statically declared input in the app
  // manifest. If an input of the same id is already declared, the info of that
  // input will be updated.
  Promise<void> addInput(DOMString inputId, object inputManifest);

  // Remove a dynamically declared input.
  //
  // The id must not be the same with any statically declared input in the app
  // manifest. Silently resolves if the input is not previously declared;
  // rejects if attempt to remove a statically declared input.
  Promise<void> removeInput(DOMString id);

  // The following are internal methods for Firefox OS system app only.

  // Set the value on the currently focused element. This has to be used
  // for special situations where the value had to be chosen amongst a
  // list (type=month) or a widget (type=date, time, etc.).
  // If the value passed in parameter isn't valid (in the term of HTML5
  // Forms Validation), the value will simply be ignored by the element.
  [Throws]
  void setValue(DOMString value);

  // Select the <select> option specified by index.
  // If this method is called on a <select> that support multiple
  // selection, then the option specified by index will be added to
  // the selection.
  // If this method is called for a select that does not support multiple
  // selection the previous element will be unselected.
  [Throws]
  void setSelectedOption(long index);

  // Select the <select> options specified by indexes. All other options
  // will be deselected.
  // If this method is called for a <select> that does not support multiple
  // selection, then the last index specified in indexes will be selected.
  [Throws]
  void setSelectedOptions(sequence<long> indexes);

  [Throws]
  void removeFocus();
};

// Manages the list of IMEs, enables/disables IME and switches to an
// IME.
[JSImplementation="@mozilla.org/b2g-imm;1",
 Pref="dom.mozInputMethod.enabled"]
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
[JSImplementation="@mozilla.org/b2g-inputcontext;1",
 Pref="dom.mozInputMethod.enabled"]
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
   Promise<DOMString> getText(optional long offset, optional long length);
   // The start and stop position of the selection.
   readonly attribute long selectionStart;
   readonly attribute long selectionEnd;

   // The text before and after the begining of the selected text.
   readonly attribute DOMString? textBeforeCursor;
   readonly attribute DOMString? textAfterCursor;

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
    Promise<boolean> setSelectionRange(long start, long length);

    /* User moves the cursor, or changes the selection with other means. If the text around
     * cursor has changed, but the cursor has not been moved, the IME won't get notification.
     *
     * A dict is provided in the detail property of the event containing the new values, and
     * an "ownAction" property to denote the event is the result of our own mutation to
     * the input field.
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
    Promise<boolean> replaceSurroundingText(DOMString text, optional long offset, optional long length);

    /*
     *
     * Delete text around the cursor.
     * @param offset The offset from the cursor position where deletion starts.
     * @param length The length of text to delete.
     * TODO: maybe updateSurroundingText(DOMString beforeText, DOMString afterText); ?
     * @return boolean
     */
    Promise<boolean> deleteSurroundingText(long offset, long length);

    /*
     * Notifies when the text around the cursor is changed, due to either text
     * editing or cursor movement. If the cursor has been moved, but the text around has not
     * changed, the IME won't get notification.
     *
     * A dict is provided in the detail property of the event containing the new values, and
     * an "ownAction" property to denote the event is the result of our own mutation to
     * the input field.
     */
    attribute EventHandler onsurroundingtextchange;

    /*
     * Send a string/character with its key events. There are two ways of invocating 
     * the method for backward compability purpose.
     *
     * (1) The recommended way, allow specifying DOM level 3 properties like |code|.
     * @param dictOrKeyCode See MozInputMethodKeyboardEventDict.
     * @param charCode disregarded
     * @param modifiers disregarded
     * @param repeat disregarded
     *
     * (2) Deprecated, reserved for backward compability.
     * @param dictOrKeyCode keyCode of the key to send, should be one of the DOM_VK_ value in KeyboardEvent.
     * @param charCode charCode of the character, should be 0 for non-printable keys.
     * @param modifiers this paramater is no longer honored.
     * @param repeat indicates whether a key would be sent repeatedly.
     *
     * @return A promise. Resolve to true if succeeds.
     *                    Rejects to a string indicating the error.
     *
     * Note that, if you want to send a key n times repeatedly, make sure set
     * parameter repeat to true and invoke sendKey n times, and invoke keyup
     * after the end of the input.
     */
    Promise<boolean> sendKey((MozInputMethodRequiredKeyboardEventDict or long) dictOrKeyCode,
                             optional long charCode, 
                             optional long modifiers, 
                             optional boolean repeat);

    /*
     * Send a string/character with keydown, and keypress events.
     * keyup should be called afterwards to ensure properly sequence.
     *
     * @param dict See MozInputMethodKeyboardEventDict.
     *
     * @return A promise. Resolve to true if succeeds.
     *                    Rejects to a string indicating the error.
     */
    Promise<boolean> keydown(MozInputMethodRequiredKeyboardEventDict dict);

    /*
     * Send a keyup event. keydown should be called first to ensure properly sequence.
     *
     * @param dict See MozInputMethodKeyboardEventDict.
     *
     * @return A promise. Resolve to true if succeeds.
     *                    Rejects to a string indicating the error.
     *
     */
    Promise<boolean> keyup(MozInputMethodRequiredKeyboardEventDict dict);

    /*
     * Set current composing text. This method will start composition or update
     * composition if it has started. The composition will be started right
     * before the cursor position and any selected text will be replaced by the
     * composing text. When the composition is started, calling this method can
     * update the text and move cursor winthin the range of the composing text.
     * @param text The new composition text to show.
     * @param cursor The new cursor position relative to the start of the
     * composition text. The cursor should be positioned within the composition
     * text. This means the value should be >= 0 and <= the length of
     * composition text. Defaults to the lenght of composition text, i.e., the
     * cursor will be positioned after the composition text.
     * @param clauses The array of composition clause information. If not set,
     * only one clause is supported.
     * @param dict The properties of the keyboard event that cause the composition
     * to set. keydown or keyup event will be fired if it's necessary.
     * For compatibility, we recommend that you should always set this argument 
     * if it's caused by a key operation.
     * 
     * The composing text, which is shown with underlined style to distinguish
     * from the existing text, is used to compose non-ASCII characters from
     * keystrokes, e.g. Pinyin or Hiragana. The composing text is the
     * intermediate text to help input complex character and is not committed to
     * current input field. Therefore if any text operation other than
     * composition is performed, the composition will automatically end. Same
     * apply when the inputContext is lost during an unfinished composition
     * session.
     *
     * To finish composition and commit text to current input field, an IME
     * should call |endComposition|.
     */
    Promise<boolean> setComposition(DOMString text, 
                                    optional long cursor,
                                    optional sequence<CompositionClauseParameters> clauses,
                                    optional MozInputMethodKeyboardEventDict dict);

    /*
     * End composition, clear the composing text and commit given text to
     * current input field. The text will be committed before the cursor
     * position.
     * @param text The text to commited before cursor position. If empty string
     * is given, no text will be committed.
     * @param dict The properties of the keyboard event that cause the composition
     * to end. keydown or keyup event will be fired if it's necessary.
     * For compatibility, we recommend that you should always set this argument 
     * if it's caused by a key operation.
     *
     * Note that composition always ends automatically with nothing to commit if
     * the composition does not explicitly end by calling |endComposition|, but
     * is interrupted by |sendKey|, |setSelectionRange|,
     * |replaceSurroundingText|, |deleteSurroundingText|, user moving the
     * cursor, changing the focus, etc.
     */
    Promise<boolean> endComposition(optional DOMString text, 
                                    optional MozInputMethodKeyboardEventDict dict);
};

enum CompositionClauseSelectionType {
  "raw-input",
  "selected-raw-text",
  "converted-text",
  "selected-converted-text"
};

dictionary CompositionClauseParameters {
  DOMString selectionType = "raw-input";
  long length;
};

/*
 * A MozInputMethodKeyboardEventDictBase contains the following properties,
 * indicating the properties of the keyboard event caused.
 *
 * This is the base dictionary type for us to create two child types that could
 * be used as argument type in two types of methods, as WebIDL parser required.
 * 
 */
dictionary MozInputMethodKeyboardEventDictBase {
  /*
   * String/character to output, or a registered name of non-printable key.
   * (To be defined in the inheriting dictionary types.)
   */
  // DOMString key;
  /*
   * String/char indicating the virtual hardware key pressed. Optional.
   * Must be a value defined in
   * http://www.w3.org/TR/DOM-Level-3-Events-code/#keyboard-chording-virtual
   * If your keyboard app emulates physical keyboard layout, this value should
   * not be empty string. Otherwise, it should be empty string.
   */
  DOMString code = "";
  /*
   * keyCode of the keyboard event. Optional.
   * To be disregarded if |key| is an alphanumeric character.
   * If the key causes inputting a character and if your keyboard app emulates
   * a physical keyboard layout, this value should be same as the value used
   * by Firefox for desktop. If the key causes inputting an ASCII character
   * but if your keyboard app doesn't emulate any physical keyboard layouts,
   * the value should be proper value for the key value.
   */
  long? keyCode;
  /*
   * Indicates whether a key would be sent repeatedly. Optional.
   */
  boolean repeat = false;
  /*
   * Optional. True if |key| property is explicitly referring to a printable key.
   * When this is set, key will be printable even if the |key| value matches any
   * of the registered name of non-printable keys.
   */
  boolean printable = false;
};

/*
 * For methods like setComposition() and endComposition(), the optional
 * dictionary type argument is really optional when all of it's property
 * are optional.
 * This dictionary type is used to denote that argument.
 */
dictionary MozInputMethodKeyboardEventDict : MozInputMethodKeyboardEventDictBase {
  DOMString? key;
};

/*
 * For methods like keydown() and keyup(), the dictionary type argument is
 * considered required only if at least one of it's property is required.
 * This dictionary type is used to denote that argument.
 */
dictionary MozInputMethodRequiredKeyboardEventDict : MozInputMethodKeyboardEventDictBase {
  required DOMString key;
};
