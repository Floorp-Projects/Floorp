function test () {
  let loader = makeLoader();
  let module = Module("./main", gTestPath);
  let require = Require(loader, module);

  try {
    let Model = require("resource://gre/modules/BlinkTag.jsm");
    ok(false, "requiring a JS module that doesn't exist should throw");
  }
  catch (e) {
    ok(e, "requiring a JS module that doesn't exist should throw");
  }

  /*
   * Relative resource:// URI of JSM
   */

  let { square } = require("./Math.jsm").Math;
  is(square(5), 25, "loads relative URI of JSM");

  /*
   * Absolute resource:// URI of JSM
   */
  let { defer } = require("resource://gre/modules/Promise.jsm").Promise;
  let { resolve, promise } = defer();
  resolve(5);
  promise.then(val => {
    is(val, 5, "loads absolute resource:// URI of JSM");
  }).then(finish);

}
