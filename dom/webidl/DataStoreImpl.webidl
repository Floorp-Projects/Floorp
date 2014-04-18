/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// TODO Bug 957086 - The DataStoreImpl WebIDL will be removed once the
//                   DataStore API is fully rewritten in C++ (i.e. should be
//                   directly implemented by the DataStore WebIDL).

[HeaderFile="mozilla/dom/DataStore.h",
 Func="mozilla::dom::DataStore::EnabledForScope",
 JSImplementation="@mozilla.org/dom/datastore-impl;1"]
interface DataStoreImpl {
  void setEventTarget(EventTarget eventTarget);

  // Returns the label of the DataSource.
  readonly attribute DOMString name;

  // Returns the origin of the DataSource (e.g., 'facebook.com').
  // This value is the manifest URL of the owner app.
  readonly attribute DOMString owner;

  // is readOnly a F(current_app, datastore) function? yes
  readonly attribute boolean readOnly;

  // Promise<any>
  Promise get(DataStoreKey... id);

  // Promise<void>
  Promise put(any obj, DataStoreKey id, optional DOMString revisionId = "");

  // Promise<DataStoreKey>
  Promise add(any obj, optional DataStoreKey id,
              optional DOMString revisionId = "");

  // Promise<boolean>
  Promise remove(DataStoreKey id, optional DOMString revisionId = "");

  // Promise<void>
  Promise clear(optional DOMString revisionId = "");

  readonly attribute DOMString revisionId;

  // Promise<unsigned long>
  Promise getLength();

  [NewObject]
  DataStoreCursor sync(optional DOMString revisionId = "");
};


// TODO Bug 957086 - The DataStoreCursorImpl WebIDL will be removed once the
//                   DataStore API is fully rewritten in C++ (i.e. should be
//                   directly implemented by the DataStoreCursor WebIDL).

[HeaderFile="mozilla/dom/DataStore.h",
 Func="mozilla::dom::DataStore::EnabledForScope",
 JSImplementation="@mozilla.org/dom/datastore-cursor-impl;1"]
interface DataStoreCursorImpl {
  // the DataStore
  readonly attribute DataStore store;

  // Promise<DataStoreTask>
  Promise next();

  void close();
};
