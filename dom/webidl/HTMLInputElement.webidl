/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://www.whatwg.org/specs/web-apps/current-work/#the-input-element
 * http://www.whatwg.org/specs/web-apps/current-work/#other-elements,-attributes-and-apis
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

interface nsIControllers;

interface HTMLInputElement : HTMLElement {
  [Pure, SetterThrows]
           attribute DOMString accept;
  [Pure, SetterThrows]
           attribute DOMString alt;
  [Pure, SetterThrows]
           attribute DOMString autocomplete;
  [Pure, SetterThrows]
           attribute boolean autofocus;
  [Pure, SetterThrows]
           attribute boolean defaultChecked;
  [Pure]
           attribute boolean checked;
           // Bug 850337 - attribute DOMString dirName;
  [Pure, SetterThrows]
           attribute boolean disabled;
  readonly attribute HTMLFormElement? form;
  [Pure]
  readonly attribute FileList? files;
  [Pure, SetterThrows]
           attribute DOMString formAction;
  [Pure, SetterThrows]
           attribute DOMString formEnctype;
  [Pure, SetterThrows]
           attribute DOMString formMethod;
  [Pure, SetterThrows]
           attribute boolean formNoValidate;
  [Pure, SetterThrows]
           attribute DOMString formTarget;
  [Pure, SetterThrows]
           attribute unsigned long height;
  [Pure]
           attribute boolean indeterminate;
  [Pure, SetterThrows, Pref="dom.forms.inputmode"]
           attribute DOMString inputMode;
  [Pure]
  readonly attribute HTMLElement? list;
  [Pure, SetterThrows]
           attribute DOMString max;
  [Pure, SetterThrows]
           attribute long maxLength;
  [Pure, SetterThrows]
           attribute DOMString min;
  [Pure, SetterThrows]
           attribute boolean multiple;
  [Pure, SetterThrows]
           attribute DOMString name;
  [Pure, SetterThrows]
           attribute DOMString pattern;
  [Pure, SetterThrows]
           attribute DOMString placeholder;
  [Pure, SetterThrows]
           attribute boolean readOnly;
  [Pure, SetterThrows]
           attribute boolean required;
  [Pure, SetterThrows]
           attribute unsigned long size;
  [Pure, SetterThrows]
           attribute DOMString src;
  [Pure, SetterThrows]
           attribute DOMString step;
  [Pure, SetterThrows]
           attribute DOMString type;
  [Pure, SetterThrows]
           attribute DOMString defaultValue;
  [Pure, TreatNullAs=EmptyString, Throws]
           attribute DOMString value;
  [Throws, Func="HTMLInputElement::ValueAsDateEnabled"]
           attribute Date? valueAsDate;
  [Pure, SetterThrows]
           attribute unrestricted double valueAsNumber;
           attribute unsigned long width;

  [Throws]
  void stepUp(optional long n = 1);
  [Throws]
  void stepDown(optional long n = 1);

  [Pure]
  readonly attribute boolean willValidate;
  [Pure]
  readonly attribute ValidityState validity;
  [GetterThrows]
  readonly attribute DOMString validationMessage;
  boolean checkValidity();
  boolean reportValidity();
  void setCustomValidity(DOMString error);

  // Bug 850365 readonly attribute NodeList labels;

  void select();

  [Throws]
           // TODO: unsigned vs signed
           attribute long selectionStart;
  [Throws]
           attribute long selectionEnd;
  [Throws]
           attribute DOMString selectionDirection;
  [Throws]
  void setRangeText(DOMString replacement);
  [Throws]
  void setRangeText(DOMString replacement, unsigned long start,
    unsigned long end, optional SelectionMode selectionMode = "preserve");

  // also has obsolete members
};

partial interface HTMLInputElement {
  [Pure, SetterThrows]
           attribute DOMString align;
  [Pure, SetterThrows]
           attribute DOMString useMap;
};

// Mozilla extensions

partial interface HTMLInputElement {
  [Throws]
  void setSelectionRange(long start, long end, optional DOMString direction);

  [GetterThrows, ChromeOnly]
  readonly attribute nsIControllers        controllers;
  [GetterThrows]
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

  // Number controls (<input type=number>) have an anonymous text control
  // (<input type=text>) in the anonymous shadow tree that they contain. On
  // such an anonymous text control this property provides access to the
  // number control that owns the text control. This is useful, for example,
  // in code that looks at the currently focused element to make decisions
  // about which IME to bring up. Such code needs to be able to check for any
  // owning number control since it probably wants to bring up a number pad
  // instead of the standard keyboard, even when the anonymous text control has
  // focus.
  [ChromeOnly]
  readonly attribute HTMLInputElement? ownerNumberControl;

  boolean mozIsTextField(boolean aExcludePassword);

  [ChromeOnly]
  // This function will return null if @autocomplete is not defined for the
  // current @type
  AutocompleteInfo? getAutocompleteInfo();
};

partial interface HTMLInputElement {
  // Mirrored chrome-only nsIDOMNSEditableElement methods.  Please make sure
  // to update this list if nsIDOMNSEditableElement changes.

  [Pure, ChromeOnly]
  readonly attribute nsIEditor? editor;

  // This is similar to set .value on nsIDOMInput/TextAreaElements, but handling
  // of the value change is closer to the normal user input, so 'change' event
  // for example will be dispatched when focusing out the element.
  [ChromeOnly]
  void setUserInput(DOMString input);
};

partial interface HTMLInputElement {
  [Pref="dom.input.dirpicker", SetterThrows]
  attribute boolean allowdirs;

  [Pref="dom.input.dirpicker"]
  readonly attribute boolean isFilesAndDirectoriesSupported;

  [Throws, Pref="dom.input.dirpicker"]
  Promise<sequence<(File or Directory)>> getFilesAndDirectories();

  [Throws, Pref="dom.input.dirpicker"]
  Promise<sequence<File>> getFiles(optional boolean recursiveFlag = false);

  [Throws, Pref="dom.input.dirpicker"]
  void chooseDirectory();
};

[NoInterfaceObject]
interface MozPhonetic {
  [Pure, ChromeOnly]
  readonly attribute DOMString phonetic;
};

HTMLInputElement implements MozImageLoadingContent;
HTMLInputElement implements MozPhonetic;

// Webkit/Blink
partial interface HTMLInputElement {
  [Pref="dom.webkitBlink.filesystem.enabled", Cached, Constant]
  readonly attribute sequence<Entry> webkitEntries;

  [Pref="dom.webkitBlink.dirPicker.enabled", BinaryName="WebkitDirectoryAttr", SetterThrows]
          attribute boolean webkitdirectory;
};
