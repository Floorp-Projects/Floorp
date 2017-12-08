// |reftest| skip -- slow, obsoleted by 98409 fix
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 324278;
var summary = 'GC without recursion';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);

// Number to push native stack size beyond 10MB if GC recurses generating
// segfault on Fedora Core / Ubuntu Linuxes where the stack size by default
// is 10MB/8MB.
var N = 100*1000;

function build(N) {
  // Exploit the fact that (in ES3), regexp literals are shared between
  // function invocations. Thus we build the following chain:
  // chainTop: function->regexp->function->regexp....->null
  // to check how GC would deal with this chain.

  var chainTop = null;
  for (var i = 0; i != N; ++i) {
    var f = Function('some_arg'+i, ' return /test/;');
    var re = f();
    re.previous = chainTop;
    chainTop = f;
  }
  return chainTop;
}

function check(chainTop, N) {
  for (var i = 0; i != N; ++i) {
    var re = chainTop();
    chainTop = re.previous;
  }
  if (chainTop !== null)
    throw "Bad chainTop";

}

if (typeof gc != "function") {
  gc = function() {
    for (var i = 0; i != 50*1000; ++i) {
      var tmp = new Object();
    }
  }
}

var chainTop = build(N);
printStatus("BUILT");
gc();
check(chainTop, N);
printStatus("CHECKED");
chainTop = null;
gc();
 
reportCompare(expect, actual, summary);
