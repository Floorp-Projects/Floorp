/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://dvcs.w3.org/hg/IndexedDB/raw-file/tip/Overview.html#idl-def-IDBFactory
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

interface Principal;

dictionary IDBOpenDBOptions
{
  [EnforceRange] unsigned long long version;
  StorageType storage;
};

/**
 * Interface that defines the indexedDB property on a window.  See
 * http://dvcs.w3.org/hg/IndexedDB/raw-file/tip/Overview.html#idl-def-IDBFactory
 * for more information.
 */
[Exposed=(Window,Worker,System)]
interface IDBFactory {
  [Throws]
  IDBOpenDBRequest
  open(DOMString name,
       [EnforceRange] unsigned long long version);

  [Throws]
  IDBOpenDBRequest
  open(DOMString name,
       optional IDBOpenDBOptions options);

  [Throws]
  IDBOpenDBRequest
  deleteDatabase(DOMString name,
                 optional IDBOpenDBOptions options);

  [Throws]
  short
  cmp(any first,
      any second);

  [Throws, ChromeOnly]
  IDBOpenDBRequest
  openForPrincipal(Principal principal,
                   DOMString name,
                   [EnforceRange] unsigned long long version);

  [Throws, ChromeOnly]
  IDBOpenDBRequest
  openForPrincipal(Principal principal,
                   DOMString name,
                   optional IDBOpenDBOptions options);

  [Throws, ChromeOnly]
  IDBOpenDBRequest
  deleteForPrincipal(Principal principal,
                     DOMString name,
                     optional IDBOpenDBOptions options);
};
