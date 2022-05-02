/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

enum FileSystemHandleKind {
  "file",
  "directory",
};

// TODO: Add Serializable
[Exposed=(Window,Worker), SecureContext, Pref="dom.fs.enabled"]
interface FileSystemHandle {
  readonly attribute FileSystemHandleKind kind;
  readonly attribute USVString name;

  [NewObject]
  Promise<boolean> isSameEntry(FileSystemHandle other);
};
