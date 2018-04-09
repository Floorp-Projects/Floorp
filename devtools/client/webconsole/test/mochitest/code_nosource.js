/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

// Original source code for the cross-domain source map test.
// The generated file was made with
//    webpack --devtool nosources-source-map code_nosource.js code_bundle_nosource.js
// ... and then the bundle was edited to change the source name.

"use strict";

function f() {
  console.log("here");
}

f();

// Avoid script GC.
window.f = f;
