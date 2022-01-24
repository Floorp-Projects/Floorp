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

// TODO: Add Serializzable
[Exposed=(Window,Worker), SecureContext, Pref="dom.fs.enabled"]
interface FileSystemDirectoryHandle : FileSystemHandle {
  // This interface defines an async iterable, however that isn't supported yet
  // by the bindings. So for now just explicitly define what an async iterable
  // definition implies.
  //async iterable<USVString, FileSystemHandle>;
  FileSystemDirectoryIterator entries();
  FileSystemDirectoryIterator keys();
  FileSystemDirectoryIterator values();

  Promise<FileSystemFileHandle> getFileHandle(USVString name, optional FileSystemGetFileOptions options = {});
  Promise<FileSystemDirectoryHandle> getDirectoryHandle(USVString name, optional FileSystemGetDirectoryOptions options = {});

  Promise<void> removeEntry(USVString name, optional FileSystemRemoveOptions options = {});

  Promise<sequence<USVString>?> resolve(FileSystemHandle possibleDescendant);
};
