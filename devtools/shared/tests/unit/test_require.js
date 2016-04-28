/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test require

// Ensure that DevtoolsLoader.require doesn't spawn multiple
// loader/modules when early cached
function testBug1091706() {
  let loader = new DevToolsLoader();
  let require = loader.require;

  let indent1 = require("devtools/shared/indentation");
  let indent2 = require("devtools/shared/indentation");

  do_check_true(indent1 === indent2);
}

function run_test() {
  testBug1091706();
}
