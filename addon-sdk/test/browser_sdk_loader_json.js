function test () {
  let loader = makeLoader();
  let module = Module("./main", gTestPath);
  let require = Require(loader, module);

  /*
   * Relative resource:// URI of json
   */

  let data = require("./data.json");
  is(data.title, "jetpack mochitests", "loads relative JSON");
  is(data.dependencies.underscore, "1.0.0", "loads relative JSON");

  try {
    let data = require("./invalid.json");
    ok(false, "parsing an invalid JSON should throw");
  }
  catch (e) {
    ok(e, "parsing an invalid JSON should throw");
  }

  finish();
}
