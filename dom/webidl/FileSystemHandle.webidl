/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

enum FileSystemHandleKind {
  "file",
  "directory",
};

[Exposed=(Window,Worker), SecureContext, Serializable, Pref="dom.fs.enabled"]
interface FileSystemHandle {
  readonly attribute FileSystemHandleKind kind;
  readonly attribute USVString name;

  /* https://whatpr.org/fs/10.html#api-filesystemhandle */
  [NewObject]
  Promise<void> move(USVString name);
  [NewObject]
  Promise<void> move(FileSystemDirectoryHandle parent);
  [NewObject]
  Promise<void> move(FileSystemDirectoryHandle parent, USVString name);
  
  [NewObject]
  Promise<boolean> isSameEntry(FileSystemHandle other);
};
