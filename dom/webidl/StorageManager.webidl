/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://storage.spec.whatwg.org/#storagemanager
 *
 */

[SecureContext, Exposed=(Window,Worker)]
interface StorageManager {
  [NewObject]
  Promise<boolean> persisted();

  [Exposed=Window, NewObject]
  Promise<boolean> persist();

  [NewObject]
  Promise<StorageEstimate> estimate();
};

dictionary StorageEstimate {
  unsigned long long usage;
  unsigned long long quota;
};

[SecureContext]
partial interface StorageManager {
  [Pref="dom.fs.enabled", NewObject]
  Promise<FileSystemDirectoryHandle> getDirectory();
};

/**
 * Testing methods that exist only for the benefit of automated glass-box
 * testing.  Will never be exposed to content at large and unlikely to be useful
 * in a WebDriver context.
 */
[SecureContext]
partial interface StorageManager {
  [ChromeOnly]
  undefined shutdown();
};
