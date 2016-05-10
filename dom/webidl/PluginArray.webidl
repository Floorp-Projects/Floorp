/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// [LegacyUnenumerableNamedProperties]
// Named properties enumerable for now; see
// https://bugzilla.mozilla.org/show_bug.cgi?id=1270366
interface PluginArray {
  readonly attribute unsigned long length;

  getter Plugin? item(unsigned long index);
  getter Plugin? namedItem(DOMString name);

  void refresh(optional boolean reloadDocuments = false);
};
