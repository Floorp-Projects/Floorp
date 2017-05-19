/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

// Original source code for the inline source map test.
// The generated file was made with
//    webpack --devtool source-map code_reload_2.js code_bundle_reload_2.js
//    perl -pi -e 's/code_bundle_reload_2.js.map/sjs_code_bundle_reload_map.sjs/' \
//         code_bundle_reload_2.js

"use strict";

function f() {
  console.log("The second version of the script");
}

f();
