/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is:
 * https://html.spec.whatwg.org/multipage/interaction.html#the-datatransferitem-interface
 * https://wicg.github.io/entries-api/#idl-index
 */

[InstrumentedProps=(webkitGetAsEntry),Exposed=Window]
interface DataTransferItem {
  readonly attribute DOMString kind;
  readonly attribute DOMString type;
  [Throws, NeedsSubjectPrincipal]
  undefined getAsString(FunctionStringCallback? callback);
  [Throws, NeedsSubjectPrincipal]
  File? getAsFile();
};

callback FunctionStringCallback = undefined (DOMString data);

// https://wicg.github.io/entries-api/#idl-index
partial interface DataTransferItem {
  [Pref="dom.webkitBlink.filesystem.enabled", BinaryName="getAsEntry", Throws,
   NeedsSubjectPrincipal]
  FileSystemEntry? webkitGetAsEntry();
};
