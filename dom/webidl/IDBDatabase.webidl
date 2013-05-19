/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

dictionary IDBObjectStoreParameters {
  // XXXbz this should be "(DOMString or sequence<DOMString>)?", but
  // we don't support unions in dictionaries yet.  See bug 767926.
  any keyPath = null;
  boolean autoIncrement = false;
};

// If we start using IDBObjectStoreParameters here, remove it from DummyBinding.
