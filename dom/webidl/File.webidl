/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/FileAPI/#file
 * https://wicg.github.io/entries-api
 */

interface nsIFile;

[Exposed=(Window,Worker)]
interface File : Blob {
  [Throws]
  constructor(sequence<BlobPart> fileBits,
              USVString fileName, optional FilePropertyBag options = {});

  readonly attribute DOMString name;

  [GetterThrows]
  readonly attribute long long lastModified;
};

dictionary FilePropertyBag : BlobPropertyBag {
  long long lastModified;
};

dictionary ChromeFilePropertyBag : FilePropertyBag {
  DOMString name = "";
  boolean existenceCheck = true;
};

// https://wicg.github.io/entries-api
partial interface File {
  [BinaryName="relativePath", Pref="dom.webkitBlink.dirPicker.enabled"]
  readonly attribute USVString webkitRelativePath;
};

// Mozilla extensions
partial interface File {
  [GetterThrows, ChromeOnly, NeedsCallerType]
  readonly attribute DOMString mozFullPath;
};

// Mozilla extensions
// These 2 methods can be used only in these conditions:
// - the main-thread
// - parent process OR file process OR, only for testing, with pref
//   `dom.file.createInChild' set to true.
[Exposed=(Window)]
partial interface File {
  [ChromeOnly, Throws, NeedsCallerType]
  static Promise<File> createFromNsIFile(nsIFile file,
                                         optional ChromeFilePropertyBag options = {});

  [ChromeOnly, Throws, NeedsCallerType]
  static Promise<File> createFromFileName(USVString fileName,
                                          optional ChromeFilePropertyBag options = {});
};
