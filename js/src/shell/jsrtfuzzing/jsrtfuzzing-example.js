/* -*- Mode: javascript; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This global will hold the current fuzzing buffer for each iteration.
var fuzzBuf;

function JSFuzzIterate() {
  // This function is called per iteration. You must ensure that:
  //
  //   1) Each of your actions/decisions is only based on fuzzBuf,
  //      in particular not on Math.random(), Date/Time or other
  //      external inputs.
  //
  //   2) Your actions should be deterministic. The same fuzzBuf
  //      should always lead to the same set of actions/decisions.
  //
  //   3) You can modify the global where needed, but ensure that
  //      each iteration is isolated from one another by cleaning
  //      any modifications to the global after each iteration.
  //      In particular, iterations must not depend on or influence
  //      each other in any way (see also 1)).
  //
  //   4) You must catch all exceptions.

  try {
    // This is a very simple UTF-16 string conversion for example purposes only.
    let input = String.fromCharCode.apply(
      null,
      new Uint16Array(fuzzBuf.buffer)
    );

    // Pass the input through the JSON code as an example. Note that this
    // particular example could probably be implemented more efficiently
    // directly in fuzz-tests on a C++ level. This is purely for demonstration
    // purposes.
    print(JSON.stringify(JSON.parse(input)));
  } catch (exc) {
    print(exc);
  }
}
