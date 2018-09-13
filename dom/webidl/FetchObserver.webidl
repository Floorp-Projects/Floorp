/* -*- Mode: IDL; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

callback interface ObserverCallback {
  void handleEvent(FetchObserver observer);
};

enum FetchState {
  // Pending states
  "requesting", "responding",
  // Final states
  "aborted", "errored", "complete"
};

[Exposed=(Window,Worker),
 Func="mozilla::dom::DOMPrefs::dom_fetchObserver_enabled"]
interface FetchObserver : EventTarget {
  readonly attribute FetchState state;

  // Events
  attribute EventHandler onstatechange;
  attribute EventHandler onrequestprogress;
  attribute EventHandler onresponseprogress;
};
