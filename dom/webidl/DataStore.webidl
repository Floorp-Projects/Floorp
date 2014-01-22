/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Pref="dom.datastore.enabled",
 JSImplementation="@mozilla.org/dom/datastore;1"]
interface DataStore : EventTarget {
  // Returns the label of the DataSource.
  readonly attribute DOMString name;

  // Returns the origin of the DataSource (e.g., 'facebook.com').
  // This value is the manifest URL of the owner app.
  readonly attribute DOMString owner;

  // is readOnly a F(current_app, datastore) function? yes
  readonly attribute boolean readOnly;

  // Promise<any>
  Promise get(unsigned long id);

  // Promise<any>
  Promise get(sequence<unsigned long> id);

  // Promise<void>
  Promise put(any obj, unsigned long id);

  // Promise<unsigned long>
  Promise add(any obj, optional unsigned long id);

  // Promise<boolean>
  Promise remove(unsigned long id);

  // Promise<void>
  Promise clear();

  readonly attribute DOMString revisionId;

  attribute EventHandler onchange;

  // Promise<unsigned long>
  Promise getLength();

  DataStoreCursor sync(optional DOMString revisionId = "");
};

[Pref="dom.datastore.enabled",
 JSImplementation="@mozilla.org/dom/datastore-cursor;1"]
interface DataStoreCursor {

  // the DataStore
  readonly attribute DataStore store;

  // Promise<DataStoreTask>
  Promise next();

  void close();
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
  unsigned long id;
  any data;
};
