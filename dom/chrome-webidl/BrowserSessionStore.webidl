/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// object contains either a CollectedFileListValue or a CollectedNonMultipleSelectValue or Sequence<DOMString>
typedef (DOMString or boolean or object) FormDataValue;

[ChromeOnly, Exposed=Window]
interface SessionStoreFormData {
  [Cached, Pure]
  readonly attribute ByteString? url;

  [Cached, Pure]
  readonly attribute record<DOMString, FormDataValue>? id;

  [Cached, Pure]
  readonly attribute record<DOMString, FormDataValue>? xpath;

  [Cached, Pure]
  readonly attribute DOMString? innerHTML;

  [Cached, Frozen, Pure]
  readonly attribute sequence<SessionStoreFormData?>? children;

  object toJSON();
};

[ChromeOnly, Exposed=Window]
interface SessionStoreScrollData {
  [Cached, Pure]
  readonly attribute ByteString? scroll;

  [Cached, Pure]
  readonly attribute sequence<SessionStoreScrollData?>? children;

  object toJSON();
};

[GenerateConversionToJS]
dictionary UpdateSessionStoreData {
  // This is docshell caps, but on-disk format uses the disallow property name.
  ByteString? disallow;
  boolean isPrivate;
  SessionStoreFormData? formdata;
  SessionStoreScrollData? scroll;
};
