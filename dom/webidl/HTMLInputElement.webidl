/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://www.whatwg.org/specs/web-apps/current-work/#the-input-element
 * http://www.whatwg.org/specs/web-apps/current-work/#other-elements,-attributes-and-apis
 * https://wicg.github.io/entries-api/#idl-index
 *
 * © Copyright 2004-2011 Apple Computer, Inc., Mozilla Foundation, and
 * Opera Software ASA. You are granted a license to use, reproduce
 * and create derivative works of this document.
 */

enum SelectionMode {
  "select",
  "start",
  "end",
  "preserve",
};

interface XULControllers;

[Exposed=Window]
interface HTMLInputElement : HTMLElement {
  [HTMLConstructor] constructor();

  [CEReactions, Pure, SetterThrows]
           attribute DOMString accept;
  [CEReactions, Pure, SetterThrows]
           attribute DOMString alt;
  [CEReactions, Pure, SetterThrows]
           attribute DOMString autocomplete;
  [CEReactions, Pure, SetterThrows]
           attribute boolean autofocus;
  [CEReactions, Pure, SetterThrows, Pref="dom.capture.enabled"]
           attribute DOMString capture;
  [CEReactions, Pure, SetterThrows]
           attribute boolean defaultChecked;
  [Pure]
           attribute boolean checked;
           // Bug 850337 - attribute DOMString dirName;
  [CEReactions, Pure, SetterThrows]
           attribute boolean disabled;
  readonly attribute HTMLFormElement? form;
  [Pure]
           attribute FileList? files;
  [CEReactions, Pure, SetterThrows]
           attribute DOMString formAction;
  [CEReactions, Pure, SetterThrows]
           attribute DOMString formEnctype;
  [CEReactions, Pure, SetterThrows]
           attribute DOMString formMethod;
  [CEReactions, Pure, SetterThrows]
           attribute boolean formNoValidate;
  [CEReactions, Pure, SetterThrows]
           attribute DOMString formTarget;
  [CEReactions, Pure, SetterThrows]
           attribute unsigned long height;
  [Pure]
           attribute boolean indeterminate;
  [Pure]
  readonly attribute HTMLElement? list;
  [CEReactions, Pure, SetterThrows]
           attribute DOMString max;
  [CEReactions, Pure, SetterThrows]
           attribute long maxLength;
  [CEReactions, Pure, SetterThrows]
           attribute DOMString min;
  [CEReactions, Pure, SetterThrows]
           attribute long minLength;
  [CEReactions, Pure, SetterThrows]
           attribute boolean multiple;
  [CEReactions, Pure, SetterThrows]
           attribute DOMString name;
  [CEReactions, Pure, SetterThrows]
           attribute DOMString pattern;
  [CEReactions, Pure, SetterThrows]
           attribute DOMString placeholder;
  [CEReactions, Pure, SetterThrows]
           attribute boolean readOnly;
  [CEReactions, Pure, SetterThrows]
           attribute boolean required;
  [CEReactions, Pure, SetterThrows]
           attribute unsigned long size;
  [CEReactions, Pure, SetterNeedsSubjectPrincipal=NonSystem, SetterThrows]
           attribute DOMString src;
  [CEReactions, Pure, SetterThrows]
           attribute DOMString step;
  [CEReactions, Pure, SetterThrows]
           attribute DOMString type;
  [CEReactions, Pure, SetterThrows]
           attribute DOMString defaultValue;
  [CEReactions, Pure, SetterThrows, NeedsCallerType]
           attribute [LegacyNullToEmptyString] DOMString value;
  [Throws]
           attribute object? valueAsDate;
  [Pure, SetterThrows]
           attribute unrestricted double valueAsNumber;
  [CEReactions, SetterThrows]
           attribute unsigned long width;

  [Throws]
  void stepUp(optional long n = 1);
  [Throws]
  void stepDown(optional long n = 1);

  [Pure]
  readonly attribute boolean willValidate;
  [Pure]
  readonly attribute ValidityState validity;
  [Throws]
  readonly attribute DOMString validationMessage;
  boolean checkValidity();
  boolean reportValidity();
  void setCustomValidity(DOMString error);

  readonly attribute NodeList? labels;

  void select();

  [Throws]
           attribute unsigned long? selectionStart;
  [Throws]
           attribute unsigned long? selectionEnd;
  [Throws]
           attribute DOMString? selectionDirection;
  [Throws]
  void setRangeText(DOMString replacement);
  [Throws]
  void setRangeText(DOMString replacement, unsigned long start,
    unsigned long end, optional SelectionMode selectionMode = "preserve");
  [Throws]
  void setSelectionRange(unsigned long start, unsigned long end, optional DOMString direction);

  [Throws, Pref="dom.input.showPicker"]
  void showPicker();

  // also has obsolete members
};

partial interface HTMLInputElement {
  [CEReactions, Pure, SetterThrows]
           attribute DOMString align;
  [CEReactions, Pure, SetterThrows]
           attribute DOMString useMap;
};

// Mozilla extensions

partial interface HTMLInputElement {
  [GetterThrows, ChromeOnly]
  readonly attribute XULControllers?       controllers;
  // Binaryname because we have a FragmentOrElement function named "TextLength()".
  [NeedsCallerType, BinaryName="inputTextLength"]
  readonly attribute long                  textLength;

  [Throws, ChromeOnly]
  sequence<DOMString> mozGetFileNameArray();

  [ChromeOnly, Throws]
  void mozSetFileNameArray(sequence<DOMString> fileNames);

  [ChromeOnly]
  void mozSetFileArray(sequence<File> files);

  // This method is meant to use for testing only.
  [ChromeOnly, Throws]
  void mozSetDirectory(DOMString directoryPath);

  // This method is meant to use for testing only.
  [ChromeOnly]
  void mozSetDndFilesAndDirectories(sequence<(File or Directory)> list);

  // This method is meant to use for testing only.
  [ChromeOnly, NewObject]
  Promise<sequence<(File or Directory)>> getFilesAndDirectories();

  boolean mozIsTextField(boolean aExcludePassword);

  [ChromeOnly]
  readonly attribute boolean hasBeenTypePassword;

  [ChromeOnly]
  attribute DOMString previewValue;

  // Last value entered by the user, not by a script.
  // NOTE(emilio): As of right now some execCommand triggered changes might be
  // considered interactive.
  [ChromeOnly]
  readonly attribute DOMString lastInteractiveValue;

  [ChromeOnly]
  // This function will return null if @autocomplete is not defined for the
  // current @type
  AutocompleteInfo? getAutocompleteInfo();

  [ChromeOnly]
  // The reveal password state for a type=password control.
  attribute boolean revealPassword;
};

interface mixin MozEditableElement {
  // Returns an nsIEditor instance which is associated with the element.
  // If the element can be associated with an editor but not yet created,
  // this creates new one automatically.
  [Pure, ChromeOnly, BinaryName="editorForBindings"]
  readonly attribute nsIEditor? editor;

  // Returns true if an nsIEditor instance has already been associated with
  // the element.
  [Pure, ChromeOnly]
  readonly attribute boolean hasEditor;

  // This is set to true if "input" event should be fired with InputEvent on
  // the element.  Otherwise, i.e., if "input" event should be fired with
  // Event, set to false.
  [ChromeOnly]
  readonly attribute boolean isInputEventTarget;

  // This is similar to set .value on nsIDOMInput/TextAreaElements, but handling
  // of the value change is closer to the normal user input, so 'change' event
  // for example will be dispatched when focusing out the element.
  [Func="IsChromeOrUAWidget", NeedsSubjectPrincipal]
  void setUserInput(DOMString input);
};

HTMLInputElement includes MozEditableElement;

HTMLInputElement includes MozImageLoadingContent;

// https://wicg.github.io/entries-api/#idl-index
partial interface HTMLInputElement {
  [Pref="dom.webkitBlink.filesystem.enabled", Frozen, Cached, Pure]
  readonly attribute sequence<FileSystemEntry> webkitEntries;

  [Pref="dom.webkitBlink.dirPicker.enabled", BinaryName="WebkitDirectoryAttr", SetterThrows]
          attribute boolean webkitdirectory;
};

dictionary DateTimeValue {
  long hour;
  long minute;
  long year;
  long month;
  long day;
};

partial interface HTMLInputElement {
  [ChromeOnly]
  DateTimeValue getDateTimeInputBoxValue();

  [ChromeOnly]
  readonly attribute Element? dateTimeBoxElement;

  [ChromeOnly, BinaryName="getMinimumAsDouble"]
  double getMinimum();

  [ChromeOnly, BinaryName="getMaximumAsDouble"]
  double getMaximum();

  [Func="IsChromeOrUAWidget"]
  void openDateTimePicker(optional DateTimeValue initialValue = {});

  [Func="IsChromeOrUAWidget"]
  void updateDateTimePicker(optional DateTimeValue value = {});

  [Func="IsChromeOrUAWidget"]
  void closeDateTimePicker();

  [Func="IsChromeOrUAWidget"]
  void setFocusState(boolean aIsFocused);

  [Func="IsChromeOrUAWidget"]
  void updateValidityState();

  [Func="IsChromeOrUAWidget", BinaryName="getStepAsDouble"]
  double getStep();

  [Func="IsChromeOrUAWidget", BinaryName="getStepBaseAsDouble"]
  double getStepBase();
};
