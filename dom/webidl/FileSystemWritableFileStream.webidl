/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

enum WriteCommandType {
  "write",
  "seek",
  "truncate"
};

[GenerateConversionToJS]
dictionary WriteParams {
  required WriteCommandType type;
  unsigned long long? size;
  unsigned long long? position;
  (BufferSource or Blob or UTF8String)? data;
};

typedef (BufferSource or Blob or UTF8String or WriteParams) FileSystemWriteChunkType;

[Exposed=(Window,Worker), SecureContext, Pref="dom.fs.enabled"]
interface FileSystemWritableFileStream : WritableStream {
  [NewObject, Throws]
  Promise<undefined> write(FileSystemWriteChunkType data);
  [NewObject]
  Promise<undefined> seek(unsigned long long position);
  [NewObject]
  Promise<undefined> truncate(unsigned long long size);
};
