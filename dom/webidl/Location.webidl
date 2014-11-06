/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://www.whatwg.org/specs/web-apps/current-work/#the-location-interface
 *
 * © Copyright 2004-2011 Apple Computer, Inc., Mozilla Foundation, and
 * Opera Software ASA. You are granted a license to use, reproduce
 * and create derivative works of this document.
 */

[Unforgeable]
interface Location {
  [Throws]
  void assign(DOMString url);
  [Throws, CrossOriginCallable]
  void replace(DOMString url);
  // XXXbz there is no forceget argument in the spec!  See bug 1037721.
  [Throws]
  void reload(optional boolean forceget = false);
};
// No support for .searchParams on Location yet.  See bug 1082734.

Location implements URLUtils;
