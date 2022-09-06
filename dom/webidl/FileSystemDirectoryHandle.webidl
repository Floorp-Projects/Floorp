/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

dictionary FileSystemGetFileOptions {
  boolean create = false;
};

dictionary FileSystemGetDirectoryOptions {
  boolean create = false;
};

dictionary FileSystemRemoveOptions {
  boolean recursive = false;
};

[Exposed=(Window,Worker), SecureContext, Serializable, Pref="dom.fs.enabled"]
interface FileSystemDirectoryHandle : FileSystemHandle {

  async iterable<USVString, FileSystemHandle>;

  [NewObject]
  Promise<FileSystemFileHandle> getFileHandle(USVString name, optional FileSystemGetFileOptions options = {});

  [NewObject]
  Promise<FileSystemDirectoryHandle> getDirectoryHandle(USVString name, optional FileSystemGetDirectoryOptions options = {});

  [NewObject]
  Promise<void> removeEntry(USVString name, optional FileSystemRemoveOptions options = {});

  [NewObject]
  Promise<sequence<USVString>?> resolve(FileSystemHandle possibleDescendant);
};
