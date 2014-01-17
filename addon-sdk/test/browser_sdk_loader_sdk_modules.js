function test () {
  let loader = makeLoader();
  let module = Module("./main", "scratchpad://");
  let require = Require(loader, module);

  let { has } = require("sdk/util/array");
  let { extend } = require("sdk/util/object");
  let testArray = [1, 'hello', 2, 'hi'];

  ok(has(testArray, 1), 'loader loading simple array utils');
  ok(has(testArray, 'hello'), 'loader loading simple array utils');
  ok(!has(testArray, 4), 'loader loading simple array utils');

  let testObj1 = { strings: 6, prop: [] };
  let testObj2 = { name: 'Tosin Abasi', strings: 8 };
  let extended = extend(testObj1, testObj2);

  is(extended.name, 'Tosin Abasi', 'loader loading simple object utils');
  is(extended.strings, 8, 'loader loading simple object utils');
  is(extended.prop, testObj1.prop, 'loader loading simple object utils');
  finish();
}
