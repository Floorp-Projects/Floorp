/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test require

// Ensure that DevtoolsLoader.require doesn't spawn multiple
// loader/modules when early cached
function testBug1091706() {
  const loader = new DevToolsLoader();
  const require = loader.require;

  const indent1 = require("devtools/shared/indentation");
  const indent2 = require("devtools/shared/indentation");

  Assert.ok(indent1 === indent2);
}

function run_test() {
  testBug1091706();
}
