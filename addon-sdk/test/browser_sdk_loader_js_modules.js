function test () {
  let loader = makeLoader();
  let module = Module("./main", gTestPath);
  let require = Require(loader, module);

  try {
    let Model = require("./cant-find-me");
    ok(false, "requiring a JS module that doesn't exist should throw");
  }
  catch (e) {
    ok(e, "requiring a JS module that doesn't exist should throw");
  }


  /*
   * Relative resource:// URI of JS
   */

  let { square } = require("./math");
  is(square(5), 25, "loads relative URI of JS");

  /*
   * Absolute resource:// URI of JS
   */

  let { has } = require("resource://gre/modules/commonjs/sdk/util/array");
  let testArray = ['rock', 'paper', 'scissors'];

  ok(has(testArray, 'rock'), "loads absolute resource:// URI of JS");
  ok(!has(testArray, 'dragon'), "loads absolute resource:// URI of JS");

  finish();
}
