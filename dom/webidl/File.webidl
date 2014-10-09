/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

interface nsIFile;

[Constructor(sequence<(ArrayBuffer or ArrayBufferView or Blob or DOMString)> fileBits,
             ScalarValueString fileName, optional FilePropertyBag options),

 // These constructors are just for chrome callers:
 Constructor(Blob fileBits, optional ChromeFilePropertyBag options),
 Constructor(nsIFile fileBits, optional ChromeFilePropertyBag options),
 Constructor(ScalarValueString fileBits, optional ChromeFilePropertyBag options),

 Exposed=(Window,Worker)]
interface File : Blob {

  readonly attribute DOMString name;

  [GetterThrows]
  readonly attribute long long lastModified;

};


dictionary FilePropertyBag {

      DOMString type = "";
      long long lastModified;

};

dictionary ChromeFilePropertyBag : FilePropertyBag {

      DOMString name = "";

};

// Mozilla extensions
partial interface File {

  [GetterThrows]
  readonly attribute Date lastModifiedDate;

  [GetterThrows]
  readonly attribute DOMString mozFullPath;

};
