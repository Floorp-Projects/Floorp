/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// [LegacyUnenumerableNamedProperties]
// Named properties enumerable for now; see
// https://bugzilla.mozilla.org/show_bug.cgi?id=1270364
interface MimeTypeArray {
  readonly attribute unsigned long length;

  getter MimeType? item(unsigned long index);
  getter MimeType? namedItem(DOMString name);
};
