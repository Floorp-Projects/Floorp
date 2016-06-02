/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/FileAPI/#file
 */

interface nsIFile;

[Constructor(sequence<BlobPart> fileBits,
             USVString fileName, optional FilePropertyBag options),

 // These constructors are just for chrome callers:
 Constructor(Blob fileBits, optional ChromeFilePropertyBag options),
 Constructor(nsIFile fileBits, optional ChromeFilePropertyBag options),
 Constructor(USVString fileBits, optional ChromeFilePropertyBag options),

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
  boolean temporary = false;
};

// Mozilla extensions
partial interface File {
  [GetterThrows, Deprecated="FileLastModifiedDate"]
  readonly attribute Date lastModifiedDate;

  [BinaryName="path", Func="mozilla::dom::Directory::WebkitBlinkDirectoryPickerEnabled"]
  readonly attribute DOMString webkitRelativePath;

  [GetterThrows, ChromeOnly]
  readonly attribute DOMString mozFullPath;
};
