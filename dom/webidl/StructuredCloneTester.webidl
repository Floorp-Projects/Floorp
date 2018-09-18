/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// The WebIDL compiler does not accept a Pref-ed interface exposed to any scopes
// other than *only* `Window`, so the Func is Pref-ed instead.
[Constructor(boolean serializable, boolean deserializable),
 Exposed=(Window,Worker),
 Func="mozilla::dom::DOMPrefs::dom_structuredclonetester_enabled"]
interface StructuredCloneTester {
  readonly attribute boolean serializable;
  readonly attribute boolean deserializable;
};
