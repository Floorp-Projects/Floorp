/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 446026;
var summary = 'brian loves eval(s, o)';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

expect = 'locallocal';

var x = "global";
(function() {
  var x = "local";
  (function() {
    actual = x;
    eval("", {});
    actual += x;
  })();
})();
 
reportCompare(expect, actual, summary);
