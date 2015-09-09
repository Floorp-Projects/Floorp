/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
 * InputMethod API implements a bridge between the web content hosting an input
 * element and the input content (a.k.a. input app, virtual keyboard app,
 * or IME). This API is input content facing in order for it to interact with
 * the remote input element.
 * The API also contains a few Gaia System app only methods
 * (marked with "input-manage" permission) for Gaia System app to interact with
 * some types of inputs and to regulate the input apps.
 */
[JSImplementation="@mozilla.org/b2g-inputmethod;1",
 NavigatorProperty="mozInputMethod",
 Pref="dom.mozInputMethod.enabled",
 CheckAnyPermissions="input input-manage"]
interface MozInputMethod : EventTarget {
  /**
   * Activate or decactive current API instance.
   * Gaia System app call this method via BrowserElement#setInputMethodActive.
   */
  [ChromeOnly]
  void setActive(boolean isActive);

  /**
   * InputMethodManager contain a few global methods expose to input apps.
   */
  readonly attribute MozInputMethodManager mgmt;

  /**
   * Fired when the input context changes, include changes from and to null.
   * The new InputContext instance will be available in the event
   * object under |inputcontext| property.  When it changes to null it
   * means the app (the user of this API) no longer has the control of
   * the original focused input field.
   * Note that if the app saves the original context, it might get
   * void; implementation decides when to void the input context.
   */
  [CheckAnyPermissions="input"]
  attribute EventHandler oninputcontextchange;

  /**
   * An "input context" is mapped to a text field that the app is
   * allow to mutate. This attribute should be null when there is no
   * text field currently focused.
   */
  [CheckAnyPermissions="input"]
  readonly attribute MozInputContext? inputcontext;

  /**
   * Add a dynamically declared input.
   *
   * The id must not be the same with any statically declared input in the app
   * manifest. If an input of the same id is already declared, the info of that
   * input will be updated.
   */
  [CheckAnyPermissions="input"]
  Promise<void> addInput(DOMString inputId,
                         MozInputMethodInputManifest inputManifest);

  /**
   * Remove a dynamically declared input.
   *
   * The id must not be the same with any statically declared input in the app
   * manifest. Silently resolves if the input is not previously declared;
   * rejects if attempt to remove a statically declared input.
   */
  [CheckAnyPermissions="input"]
  Promise<void> removeInput(DOMString id);

  /**
   * Remove focus from the current input, usable by Gaia System app, globally,
   * regardless of the current focus state.
   */
  [CheckAnyPermissions="input-manage"]
  void removeFocus();

  /**
   * The following are internal methods for Firefox OS System app only,
   * for handling the "option" group inputs.
   */

  /**
   * Set the value on the currently focused element. This has to be used
   * for special situations where the value had to be chosen amongst a
   * list (type=month) or a widget (type=date, time, etc.).
   * If the value passed in parameter isn't valid (in the term of HTML5
   * Forms Validation), the value will simply be ignored by the element.
   */
  [CheckAnyPermissions="input-manage"]
  void setValue(DOMString value);

  /**
   * Select the <select> option specified by index.
   * If this method is called on a <select> that support multiple
   * selection, then the option specified by index will be added to
   * the selection.
   * If this method is called for a select that does not support multiple
   * selection the previous element will be unselected.
   */
  [CheckAnyPermissions="input-manage"]
  void setSelectedOption(long index);

  /**
   * Select the <select> options specified by indexes. All other options
   * will be deselected.
   * If this method is called for a <select> that does not support multiple
   * selection, then the last index specified in indexes will be selected.
   */
  [CheckAnyPermissions="input-manage"]
  void setSelectedOptions(sequence<long> indexes);
};

/**
 * InputMethodManager contains a few of the global methods for the input app.
 */
[JSImplementation="@mozilla.org/b2g-imm;1",
 Pref="dom.mozInputMethod.enabled",
 CheckAnyPermissions="input"]
interface MozInputMethodManager {
  /**
   * Ask the OS to show a list of available inputs for users to switch from.
   * OS should sliently ignore this request if the app is currently not the
   * active one.
   */
  void showAll();

  /**
   * Ask the OS to switch away from the current active input app.
   * OS should sliently ignore this request if the app is currently not the
   * active one.
   */
  void next();

  /**
   * If this method returns true, it is recommented that the input app provides
   * a shortcut that would invoke the next() method above, for easy switching
   * between inputs -- i.e. show a "global" button on screen if the input app
   * implements an on-screen virtual keyboard.
   *
   * The returning value is depend on the inputType of the current input context.
   */
  boolean supportsSwitching();

  /**
   * Ask the OS to remove the input focus, will cause the lost of input context.
   * OS should sliently ignore this request if the app is currently not the
   * active one.
   */
  void hide();
};

/**
 * The input context, which consists of attributes and information of current
 * input field. It also hosts the methods available to the keyboard app to
 * mutate the input field represented. An "input context" gets void when the
 * app is no longer allowed to interact with the text field,
 * e.g., the text field does no longer exist, the app is being switched to
 * background, and etc.
 */
[JSImplementation="@mozilla.org/b2g-inputcontext;1",
 Pref="dom.mozInputMethod.enabled",
 CheckAnyPermissions="input"]
interface MozInputContext: EventTarget {
  /**
   * Type of the InputContext. See MozInputMethodInputContextTypes
   */
  readonly attribute MozInputMethodInputContextTypes? type;

  /**
   * InputType of the InputContext. See MozInputMethodInputContextInputTypes.
   */
  readonly attribute MozInputMethodInputContextInputTypes? inputType;

  /**
   * The inputmode string, representing the inputmode of the input.
   * See http://www.whatwg.org/specs/web-apps/current-work/multipage/association-of-controls-and-forms.html#input-modalities:-the-inputmode-attribute
   */
  readonly attribute DOMString? inputMode;

  /**
   * The primary language for the input field.
   * It is the value of HTMLElement.lang.
   * See http://www.whatwg.org/specs/web-apps/current-work/multipage/elements.html#htmlelement
   */
  readonly attribute DOMString? lang;

  /**
   * Get the whole text content of the input field.
   * @return DOMString
   */
  Promise<DOMString> getText(optional long offset, optional long length);

  /**
   * The start and stop position of the current selection.
   */
  readonly attribute long selectionStart;
  readonly attribute long selectionEnd;

  /**
   * The text before and after the begining of the selected text.
   */
  readonly attribute DOMString? textBeforeCursor;
  readonly attribute DOMString? textAfterCursor;

  /**
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

  /**
   * Commit text to current input field and replace text around
   * cursor position. It will clear the current composition.
   *
   * @param text The string to be replaced with.
   * @param offset The offset from the cursor position where replacing starts. Defaults to 0.
   * @param length The length of text to replace. Defaults to 0.
   * @return boolean
   */
  Promise<boolean> replaceSurroundingText(DOMString text, optional long offset, optional long length);

  /**
   * Delete text around the cursor.
   * @param offset The offset from the cursor position where deletion starts.
   * @param length The length of text to delete.
   * TODO: maybe updateSurroundingText(DOMString beforeText, DOMString afterText); ?
   * @return boolean
   */
  Promise<boolean> deleteSurroundingText(long offset, long length);

  /**
   * Notifies when the text around the cursor is changed, due to either text
   * editing or cursor movement. If the cursor has been moved, but the text around has not
   * changed, the IME won't get notification.
   *
   * A dict is provided in the detail property of the event containing the new values, and
   * an "ownAction" property to denote the event is the result of our own mutation to
   * the input field.
   */
  attribute EventHandler onsurroundingtextchange;

  /**
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

  /**
   * Send a string/character with keydown, and keypress events.
   * keyup should be called afterwards to ensure properly sequence.
   *
   * @param dict See MozInputMethodKeyboardEventDict.
   *
   * @return A promise. Resolve to true if succeeds.
   *                    Rejects to a string indicating the error.
   */
  Promise<boolean> keydown(MozInputMethodRequiredKeyboardEventDict dict);

  /**
   * Send a keyup event. keydown should be called first to ensure properly sequence.
   *
   * @param dict See MozInputMethodKeyboardEventDict.
   *
   * @return A promise. Resolve to true if succeeds.
   *                    Rejects to a string indicating the error.
   */
  Promise<boolean> keyup(MozInputMethodRequiredKeyboardEventDict dict);

  /**
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

  /**
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
  CompositionClauseSelectionType selectionType = "raw-input";
  long length;
};

/**
 * Types are HTML tag names of the inputs that is explosed with InputContext,
 * *and* the special keyword "contenteditable" for contenteditable element.
 */
enum MozInputMethodInputContextTypes {
  "input", "textarea", "contenteditable"
  /**
   * <select> is managed by the API but it's not exposed through InputContext
   * yet.
   */
  // "select"
};

/**
 * InputTypes of the input that InputContext is representing. The value
 * is inferred from the type attribute of input element.
 *
 * See https://html.spec.whatwg.org/multipage/forms.html#states-of-the-type-attribute
 *
 * They are divided into groups -- an layout/input capable of handling one type
 * in the group is considered as capable of handling all of the types in the
 * same group.
 * The layout/input that could handle type "text" is considered as the fallback
 * if none of layout/input installed can handle a specific type.
 *
 * Groups and fallbacks is enforced in Gaia System app currently.
 *
 * Noted that the actual virtual keyboard to show, for example in the case of
 * Gaia Keyboard app, will also depend on the inputMode of the input element.
 */
enum MozInputMethodInputContextInputTypes {
  /**
   * Group "text". Be reminded that contenteditable element is assigned with
   * an input type of "textarea".
   */
  "text", "search", "textarea",
  /**
   * Group "number"
   */
  "number", "tel",
  /**
   * Group "url"
   */
  "url",
  /**
   * Group "email"
   */
  "email",
  /**
   * Group "password".
   * An non-Latin alphabet layout/input should not be able to handle this type.
   */
  "password"
  /**
   * Group "option". These types are handled by System app itself currently, and
   * not exposed and allowed to handled with input context.
   */
  //"datetime", "date", "month", "week", "time", "datetime-local", "color",
  /**
   * These types are ignored by the API even though they are valid
   * HTMLInputElement#type.
   */
  //"checkbox", "radio", "file", "submit", "image", "range", "reset", "button"
};

/**
 * An input app can host multiple inputs (a.k.a. layouts) and the capability of
 * the input is described by the input manifest.
 */
dictionary MozInputMethodInputManifest {
  required DOMString launch_path;
  required DOMString name;
  DOMString? description;
  sequence<MozInputMethodInputContextInputTypes> types;
};

/**
 * A MozInputMethodKeyboardEventDictBase contains the following properties,
 * indicating the properties of the keyboard event caused.
 *
 * This is the base dictionary type for us to create two child types that could
 * be used as argument type in two types of methods, as WebIDL parser required.
 *
 */
dictionary MozInputMethodKeyboardEventDictBase {
  /**
   * String/character to output, or a registered name of non-printable key.
   * (To be defined in the inheriting dictionary types.)
   */
  // DOMString key;
  /**
   * String/char indicating the virtual hardware key pressed. Optional.
   * Must be a value defined in
   * http://www.w3.org/TR/DOM-Level-3-Events-code/#keyboard-chording-virtual
   * If your keyboard app emulates physical keyboard layout, this value should
   * not be empty string. Otherwise, it should be empty string.
   */
  DOMString code = "";
  /**
   * keyCode of the keyboard event. Optional.
   * To be disregarded if |key| is an alphanumeric character.
   * If the key causes inputting a character and if your keyboard app emulates
   * a physical keyboard layout, this value should be same as the value used
   * by Firefox for desktop. If the key causes inputting an ASCII character
   * but if your keyboard app doesn't emulate any physical keyboard layouts,
   * the value should be proper value for the key value.
   */
  long? keyCode;
  /**
   * Indicates whether a key would be sent repeatedly. Optional.
   */
  boolean repeat = false;
  /**
   * Optional. True if |key| property is explicitly referring to a printable key.
   * When this is set, key will be printable even if the |key| value matches any
   * of the registered name of non-printable keys.
   */
  boolean printable = false;
};

/**
 * For methods like setComposition() and endComposition(), the optional
 * dictionary type argument is really optional when all of it's property
 * are optional.
 * This dictionary type is used to denote that argument.
 */
dictionary MozInputMethodKeyboardEventDict : MozInputMethodKeyboardEventDictBase {
  DOMString? key;
};

/**
 * For methods like keydown() and keyup(), the dictionary type argument is
 * considered required only if at least one of it's property is required.
 * This dictionary type is used to denote that argument.
 */
dictionary MozInputMethodRequiredKeyboardEventDict : MozInputMethodKeyboardEventDictBase {
  required DOMString key;
};
