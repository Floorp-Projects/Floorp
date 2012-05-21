// |reftest| skip
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------

var summary = "Lamport Bakery's algorithm for mutual exclusion";
// Adapted from pseudocode in Wikipedia:
// http://en.wikipedia.org/wiki/Lamport%27s_bakery_algorithm

printStatus(summary);

var N = 15; // Number of threads.
var LOOP_COUNT = 10; // Number of times each thread should loop

function range(n) {
  for (let i = 0; i < n; i++)
    yield i;
}

function max(a) {
  let x = Number.NEGATIVE_INFINITY;
  for each (let i in a)
    if (i > x)
      x = i;
  return x;
}

// the mutex mechanism
var entering = [false for (i in range(N))];
var ticket = [0 for (i in range(N))];

// the object being protected
var counter = 0;

function lock(i)
{
  entering[i] = true;
  ticket[i] = 1 + max(ticket);
  entering[i] = false;

  for (let j = 0; j < N; j++) {
    // If thread j is in the middle of getting a ticket, wait for that to
    // finish.
    while (entering[j])
      ;

    // Wait until all threads with smaller ticket numbers or with the same
    // ticket number, but with higher priority, finish their work.
    while ((ticket[j] != 0) && ((ticket[j] < ticket[i]) ||
                                ((ticket[j] == ticket[i]) && (i < j))))
      ;
  }
}

function unlock(i) {
  ticket[i] = 0;
}

function worker(i) {
  for (let k = 0; k < LOOP_COUNT; k++) {
    lock(i);

    // The critical section
    let x = counter;
    sleep(0.003);
    counter = x + 1;

    unlock(i);
  }
  return 'ok';
}

function makeWorker(id) {
  return function () { return worker(id); };
}

var expect;
var actual;

if (typeof scatter == 'undefined' || typeof sleep == 'undefined') {
  print('Test skipped. scatter or sleep not defined.');
  expect = actual = 'Test skipped.';
} else {
  scatter([makeWorker(i) for (i in range(N))]);

  expect = "counter: " + (N * LOOP_COUNT);
  actual = "counter: " + counter;
}

reportCompare(expect, actual, summary);
