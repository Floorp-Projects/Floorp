/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

dictionary FileSystemReadWriteOptions {
  [EnforceRange] unsigned long long at;
};

[Exposed=(DedicatedWorker), SecureContext, Pref="dom.fs.enabled"]
interface FileSystemSyncAccessHandle {
  // TODO: Use `[AllowShared] BufferSource data` once it works (bug 1696216)
  [Throws] unsigned long long read(([AllowShared] ArrayBufferView or [AllowShared] ArrayBuffer) buffer, optional FileSystemReadWriteOptions options = {});
  [Throws] unsigned long long write(([AllowShared] ArrayBufferView or [AllowShared] ArrayBuffer) buffer, optional FileSystemReadWriteOptions options = {});

  [NewObject]
  Promise<void> truncate([EnforceRange] unsigned long long size);
  [NewObject]
  Promise<unsigned long long> getSize();
  [NewObject]
  Promise<void> flush();
  [NewObject]
  Promise<void> close();
};
