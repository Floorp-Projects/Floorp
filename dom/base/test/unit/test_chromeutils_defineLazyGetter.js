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
