/* -*- Mode: IDL; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
 * The ParentSHistory interface represents the parent side of a browsing
 * context's session history. Theoretically this object manages all session
 * history state for the browsing context.
 */
[ChromeOnly]
interface ParentSHistory {
  [Pure]
  readonly attribute long count;
  [Pure]
  readonly attribute long index;
};
