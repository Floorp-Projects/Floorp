/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/web-locks/
 */

[SecureContext, Exposed=(Window,Worker)]
interface LockManager {
  [NewObject]
  Promise<any> request(DOMString name,
                       LockGrantedCallback callback);
  [NewObject]
  Promise<any> request(DOMString name,
                       LockOptions options,
                       LockGrantedCallback callback);

  [NewObject]
  Promise<LockManagerSnapshot> query();
};

callback LockGrantedCallback = Promise<any> (Lock? lock);

enum LockMode { "shared", "exclusive" };

dictionary LockOptions {
  LockMode mode = "exclusive";
  boolean ifAvailable = false;
  boolean steal = false;
  AbortSignal signal;
};

dictionary LockManagerSnapshot {
  sequence<LockInfo> held;
  sequence<LockInfo> pending;
};

dictionary LockInfo {
  DOMString name;
  LockMode mode;
  DOMString clientId;
};
