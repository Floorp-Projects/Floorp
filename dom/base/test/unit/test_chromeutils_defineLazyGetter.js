"use strict";

add_task(function test_defineLazyGetter() {
  let accessCount = 0;
  let obj = {
    inScope: false,
  };
  const TEST_VALUE = "test value";
  ChromeUtils.defineLazyGetter(obj, "foo", function () {
    accessCount++;
    this.inScope = true;
    return TEST_VALUE;
  });
  Assert.equal(accessCount, 0);

  // Get the property, making sure the access count has increased.
  Assert.equal(obj.foo, TEST_VALUE);
  Assert.equal(accessCount, 1);
  Assert.ok(obj.inScope);

  // Get the property once more, making sure the access count has not
  // increased.
  Assert.equal(obj.foo, TEST_VALUE);
  Assert.equal(accessCount, 1);
});

add_task(function test_defineLazyGetter_name() {
  // Properties that are convertible to NonIntAtom should be reflected to
  // the getter name.
  for (const name of [
    "foo",
    "\u3042",
    true,
    false,
    -1,
    1.1,
    0.1,
    null,
    undefined,
    {},
    [],
    /a/,
  ]) {
    const obj = {};
    const v = {};
    ChromeUtils.defineLazyGetter(obj, name, () => v);
    Assert.equal(
      Object.getOwnPropertyDescriptor(obj, name).get.name,
      String(name)
    );
    Assert.equal(obj[name], v);
    Assert.equal(obj[name], v);
  }

  // Int and Symbol properties are not reflected to the getter name.
  for (const name of [
    0,
    10,
    Symbol.iterator,
    Symbol("foo"),
    Symbol.for("foo"),
  ]) {
    const obj = {};
    const v = {};
    ChromeUtils.defineLazyGetter(obj, name, () => v);
    Assert.equal(Object.getOwnPropertyDescriptor(obj, name).get.name, "");
    Assert.equal(obj[name], v);
    Assert.equal(obj[name], v);
  }
});
