/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Constructor(boolean serializable, boolean deserializable),
 Exposed=(Window,Worker),
 Pref="dom.testing.structuredclonetester.enabled",
 Serializable]
interface StructuredCloneTester {
  readonly attribute boolean serializable;
  readonly attribute boolean deserializable;
};
