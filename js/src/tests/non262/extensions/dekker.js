// |reftest| skip
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Jason Orendorff
 */

//-----------------------------------------------------------------------------

var summary = "Dekker's algorithm for mutual exclusion";
// Adapted from pseudocode in Wikipedia:
// http://en.wikipedia.org/wiki/Dekker%27s_algorithm

printStatus (summary);

var N = 500;  // number of iterations

// the mutex mechanism
var f = [false, false];
var turn = 0;

// resource being protected
var counter = 0;

function worker(me) {
  let him = 1 - me;

  for (let i = 0; i < N; i++) {
    // enter the mutex
    f[me] = true;
    while (f[him]) {
      if (turn != me) {
        f[me] = false;
        while (turn != me)
          ;  // busy wait
        f[me] = true;
      }
    }

    // critical section
    let x = counter;
    sleep(0.003);
    counter = x + 1;

    // leave the mutex
    turn = him;
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
  var results = scatter([function () { return worker(0); },
                         function () { return worker(1); }]);

  expect = "Thread status: [ok,ok], counter: " + (2 * N);
  actual = "Thread status: [" + results + "], counter: " + counter;
}

reportCompare(expect, actual, summary);
