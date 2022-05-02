/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

enum WriteCommandType {
  "write",
  "seek",
  "truncate",
};

dictionary WriteParams {
  required WriteCommandType type;
  unsigned long long? size;
  unsigned long long? position;
  (BufferSource or Blob or USVString)? data;
};

typedef (BufferSource or Blob or USVString or WriteParams) FileSystemWriteChunkType;

[Exposed=(Window,Worker), SecureContext, Pref="dom.fs.enabled"]
interface FileSystemWritableFileStream : WritableStream {
  [NewObject]
  Promise<void> write(FileSystemWriteChunkType data);
  [NewObject]
  Promise<void> seek(unsigned long long position);
  [NewObject]
  Promise<void> truncate(unsigned long long size);
};
