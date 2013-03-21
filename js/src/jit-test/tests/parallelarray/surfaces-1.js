load(libdir + "eqArrayHelper.js");

// ParallelArray surfaces

function test() {
  var desc = Object.getOwnPropertyDescriptor(this, "ParallelArray");
  assertEq(desc.enumerable, false);
  assertEq(desc.configurable, true);
  assertEq(desc.writable, true);

  assertEq(typeof ParallelArray, 'function');
  assertEq(Object.keys(ParallelArray).length, 0);
  assertEq(ParallelArray.length, 0);
  assertEq(ParallelArray.name, "ParallelArray");

  assertEq(Object.getPrototypeOf(ParallelArray.prototype), Object.prototype);
  assertEq(Object.prototype.toString.call(ParallelArray.prototype), "[object ParallelArray]");
  assertEq(Object.prototype.toString.call(new ParallelArray), "[object ParallelArray]");
  assertEq(Object.prototype.toString.call(ParallelArray()), "[object ParallelArray]");
  assertEq(Object.keys(ParallelArray.prototype).join(), "");
  assertEq(ParallelArray.prototype.constructor, ParallelArray);

  function checkMethod(name, arity) { 
    var desc = Object.getOwnPropertyDescriptor(ParallelArray.prototype, name);
    assertEq(desc.enumerable, false);
    assertEq(desc.configurable, true);
    assertEq(desc.writable, true);
    assertEq(typeof desc.value, 'function');
    assertEq(desc.value.name, name);
    assertEq(desc.value.length, arity);
  }

  checkMethod("map", 1);
  checkMethod("reduce", 1);
  checkMethod("scan", 1);
  checkMethod("scatter", 1);
  checkMethod("filter", 1);
  checkMethod("flatten", 0);
  checkMethod("partition", 1);
  checkMethod("get", 1);

  function checkAccessor(name) {
    var desc = Object.getOwnPropertyDescriptor(ParallelArray.prototype, name);
    assertEq(desc.enumerable, false);
    assertEq(desc.configurable, false);
    assertEq(typeof desc.get, 'function');
    assertEq(desc.set, undefined);
  }

  checkAccessor("length");
  checkAccessor("shape");
}

// FIXME(bug 844882) self-hosted object not array-like, exposes internal properties
// test();
