/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

// Original source code for the cross-domain source map test.
// The generated file was made with
//    webpack --devtool source-map code_cross_domain.js code_bundle_cross_domain.js
// ... and then the bundle was edited to replace the generated
// sourceMappingURL.

"use strict";

function f() {
  console.log("anything will do");
}

f();

// Avoid script GC.
window.f = f;
