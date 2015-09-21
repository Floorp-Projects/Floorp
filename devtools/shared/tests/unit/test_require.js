/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test require

// Ensure that DevtoolsLoader.require doesn't spawn multiple
// loader/modules when early cached
function testBug1091706() {
  let loader = new DevToolsLoader();
  let require = loader.require;

  let color1 = require("devtools/shared/css-color");
  let color2 = require("devtools/shared/css-color");

  do_check_true(color1 === color2);
}

function run_test() {
  testBug1091706();
}
