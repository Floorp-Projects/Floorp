/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

assertEq(  function () { g = (for (d of [0]) d); g.next(); }.toSource(),
         '(function () { g = (for (d of [0]) d); g.next(); })');
         

assertEq(  function () { return [for (d of [0]) d]; }.toSource(),
         '(function () { return [for (d of [0]) d]; })');

if (typeof reportCompare === "function")
  reportCompare(true, true);
