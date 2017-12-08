// |reftest| skip
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------

var summary = "Peterson's algorithm for mutual exclusion";

printStatus(summary);

var N = 500;  // number of iterations

// the mutex mechanism
var f = [false, false];
var turn = 0;

// the resource being protected
var counter = 0;

function worker(me) {
  let him = 1 - me;

  for (let i = 0; i < N; i++) {
    // enter the mutex
    f[me] = true;
    turn = him;
    while (f[him] && turn == him)
      ;  // busy wait

    // critical section
    let x = counter;
    sleep(0.003);
    counter = x+1;

    // leave the mutex
    f[me] = false;
  }

  return 'ok';
}

var expect;
var actual;

if (typeof scatter == 'undefined' || typeof sleep == 'undefined') {
  print('Test skipped. scatter or sleep not defined.');
  expect = actual = 'Test skipped.';
} else {
  var results = scatter ([function() { return worker(0); },
                          function() { return worker(1); }]);
  expect = "Thread status: [ok,ok], counter: " + (2 * N);
  actual = "Thread status: [" + results + "], counter: " + counter;
}

reportCompare(expect, actual, summary);
