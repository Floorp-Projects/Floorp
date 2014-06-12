/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtaone at http://mozilla.org/MPL/2.0/. */

dictionary DOMFileMetadataParameters
{
  boolean size = true;
  boolean lastModified = true;
};

interface FileHandle : EventTarget
{
  readonly attribute MutableFile? mutableFile;
  // this is deprecated due to renaming in the spec
  readonly attribute MutableFile? fileHandle; // now mutableFile
  readonly attribute FileMode mode;
  readonly attribute boolean active;
  attribute unsigned long long? location;

  [Throws]
  FileRequest? getMetadata(optional DOMFileMetadataParameters parameters);
  [Throws]
  FileRequest? readAsArrayBuffer(unsigned long long size);
  [Throws]
  FileRequest? readAsText(unsigned long long size,
                         optional DOMString? encoding = null);

  [Throws]
  FileRequest? write(ArrayBuffer value);
  [Throws]
  FileRequest? write(Blob value);
  [Throws]
  FileRequest? write(DOMString value);
  [Throws]
  FileRequest? append(ArrayBuffer value);
  [Throws]
  FileRequest? append(Blob value);
  [Throws]
  FileRequest? append(DOMString value);
  [Throws]
  FileRequest? truncate(optional unsigned long long size);
  [Throws]
  FileRequest? flush();
  [Throws]
  void abort();

  attribute EventHandler oncomplete;
  attribute EventHandler onabort;
  attribute EventHandler onerror;
};
