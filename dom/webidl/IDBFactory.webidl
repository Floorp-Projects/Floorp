/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

interface IDBOpenDBRequest;
interface Principal;

/**
 * Interface that defines the indexedDB property on a window.  See
 * http://dvcs.w3.org/hg/IndexedDB/raw-file/tip/Overview.html#idl-def-IDBFactory
 * for more information.
 */
interface IDBFactory {
  [Throws]
  IDBOpenDBRequest
  open(DOMString name,
       [EnforceRange] optional unsigned long long version);

  [Throws]
  IDBOpenDBRequest
  deleteDatabase(DOMString name);

  [Throws]
  short
  cmp(any first,
      any second);

  [Throws, ChromeOnly]
  IDBOpenDBRequest
  openForPrincipal(Principal principal,
                   DOMString name,
                   [EnforceRange] optional unsigned long long version);

  [Throws, ChromeOnly]
  IDBOpenDBRequest
  deleteForPrincipal(Principal principal,
                     DOMString name);
};
