/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

dictionary FileSystemCreateWritableOptions {
  boolean keepExistingData = false;
};

[Exposed=(Window,Worker), SecureContext, Serializable, Pref="dom.fs.enabled"]
interface FileSystemFileHandle : FileSystemHandle {
  [NewObject]
  Promise<File> getFile();

  [NewObject]
  Promise<FileSystemWritableFileStream> createWritable(optional FileSystemCreateWritableOptions options = {});

  [Exposed=DedicatedWorker, NewObject]
  Promise<FileSystemSyncAccessHandle> createSyncAccessHandle();
};
