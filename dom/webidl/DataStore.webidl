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
  Promise update(unsigned long id, any obj);

  // Promise<unsigned long>
  Promise add(any obj);

  // Promise<boolean>
  Promise remove(unsigned long id);

  // Promise<void>
  Promise clear();

  readonly attribute DOMString revisionId;

  attribute EventHandler onchange;

  // Promise<DataStoreChanges>
  Promise getChanges(DOMString revisionId);

  // Promise<unsigned long>
  Promise getLength();
};

dictionary DataStoreChanges {
  DOMString revisionId;
  sequence<unsigned long> addedIds;
  sequence<unsigned long> updatedIds;
  sequence<unsigned long> removedIds;
};
