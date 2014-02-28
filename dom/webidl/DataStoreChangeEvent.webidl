/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

dictionary DataStoreChangeEventInit : EventInit {
  DOMString revisionId = "";
  DataStoreKey id = 0;
  DOMString operation = "";
  DOMString owner = "";
};

[Func="Navigator::HasDataStoreSupport",
 Constructor(DOMString type, optional DataStoreChangeEventInit eventInitDict)]
interface DataStoreChangeEvent : Event {
  readonly attribute DOMString revisionId;
  readonly attribute DataStoreKey id;
  readonly attribute DOMString operation;
  readonly attribute DOMString owner;
};
