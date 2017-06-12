/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

// Original source code for the inline source map test.
// The generated file was made with
//    webpack --devtool source-map code_no_race.js code_bundle_no_race.js

"use strict";

function f() {
  console.log("anything will do");
}

f();

// Avoid script GC.
window.f = f;
