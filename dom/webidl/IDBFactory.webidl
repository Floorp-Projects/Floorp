/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/IndexedDB/#factory-interface
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

interface Principal;

dictionary IDBOpenDBOptions
{
  [EnforceRange] unsigned long long version;
};

/**
 * Interface that defines the indexedDB property on a window.  See
 * https://w3c.github.io/IndexedDB/#idbfactory
 * for more information.
 */
[Exposed=(Window,Worker)]
interface IDBFactory {
  [NewObject, Throws, NeedsCallerType]
  IDBOpenDBRequest
  open(DOMString name,
       [EnforceRange] unsigned long long version);

  [NewObject, Throws, NeedsCallerType]
  IDBOpenDBRequest
  open(DOMString name,
       optional IDBOpenDBOptions options = {});

  [NewObject, Throws, NeedsCallerType]
  IDBOpenDBRequest
  deleteDatabase(DOMString name,
                 optional IDBOpenDBOptions options = {});

  [Throws]
  short
  cmp(any first,
      any second);

  [NewObject, Throws, ChromeOnly, NeedsCallerType]
  IDBOpenDBRequest
  openForPrincipal(Principal principal,
                   DOMString name,
                   [EnforceRange] unsigned long long version);

  [NewObject, Throws, ChromeOnly, NeedsCallerType]
  IDBOpenDBRequest
  openForPrincipal(Principal principal,
                   DOMString name,
                   optional IDBOpenDBOptions options = {});

  [NewObject, Throws, ChromeOnly, NeedsCallerType]
  IDBOpenDBRequest
  deleteForPrincipal(Principal principal,
                     DOMString name,
                     optional IDBOpenDBOptions options = {});
};
