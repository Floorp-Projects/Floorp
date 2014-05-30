/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

typedef (DOMString or unsigned long) DataStoreKey;

// TODO Bug 957086 - The constructor and the setDataStoreImpl(...) will be
//                   removed once the DataStore API is fully rewritten in C++,
//                   which currently plays a role of C++ proxy directing to the
//                   JS codes implemented by the DataStoreImpl WebIDL.

[Func="Navigator::HasDataStoreSupport",
 ChromeConstructor]
interface DataStore : EventTarget {
  // Returns the label of the DataSource.
  [GetterThrows]
  readonly attribute DOMString name;

  // Returns the origin of the DataSource (e.g., 'facebook.com').
  // This value is the manifest URL of the owner app.
  [GetterThrows]
  readonly attribute DOMString owner;

  // is readOnly a F(current_app, datastore) function? yes
  [GetterThrows]
  readonly attribute boolean readOnly;

  // Promise<any>
  [Throws]
  Promise get(DataStoreKey... id);

  // Promise<void>
  [Throws]
  Promise put(any obj, DataStoreKey id, optional DOMString revisionId = "");

  // Promise<DataStoreKey>
  [Throws]
  Promise add(any obj, optional DataStoreKey id,
              optional DOMString revisionId = "");

  // Promise<boolean>
  [Throws]
  Promise remove(DataStoreKey id, optional DOMString revisionId = "");

  // Promise<void>
  [Throws]
  Promise clear(optional DOMString revisionId = "");

  [GetterThrows]
  readonly attribute DOMString revisionId;

  attribute EventHandler onchange;

  // Promise<unsigned long>
  [Throws]
  Promise getLength();

  [NewObject, Throws]
  DataStoreCursor sync(optional DOMString revisionId = "");
};

partial interface DataStore {
  [ChromeOnly, Throws]
  void setDataStoreImpl(DataStoreImpl store);
};

// TODO Bug 957086 - The constructor and the setDataStoreCursorImpl(...) will be
//                   removed once the DataStore API is fully rewritten in C++,
//                   which currently plays a role of C++ proxy directing to the
//                   JS codes implemented by the DataStoreCursorImpl WebIDL.

[Pref="dom.datastore.enabled",
 ChromeConstructor]
interface DataStoreCursor {
  // the DataStore
  [GetterThrows]
  readonly attribute DataStore store;

  // Promise<DataStoreTask>
  [Throws]
  Promise next();

  [Throws]
  void close();
};

partial interface DataStoreCursor {
  [ChromeOnly]
  void setDataStoreCursorImpl(DataStoreCursorImpl cursor);
};

enum DataStoreOperation {
  "add",
  "update",
  "remove",
  "clear",
  "done"
};

dictionary DataStoreTask {
  DOMString revisionId;

  DataStoreOperation operation;

  // When |operation| is "clear" or "done", this must return null.
  DataStoreKey? id;
  any data;
};

// For internal use.
dictionary DataStoreRevisionData {
  DOMString revisionId = "";
  unsigned long objectId = 0;
  DOMString operation = "";
};
