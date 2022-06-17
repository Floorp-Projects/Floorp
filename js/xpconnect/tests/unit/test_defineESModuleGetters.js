function assertAccessor(lazy, name) {
  let desc = Object.getOwnPropertyDescriptor(lazy, name);
  Assert.equal(typeof desc.get, "function");
  Assert.equal(desc.get.name, name);
  Assert.equal(typeof desc.set, "function");
  Assert.equal(desc.set.name, name);
  Assert.equal(desc.enumerable, true);
  Assert.equal(desc.configurable, true);
}

function assertDataProperty(lazy, name, value) {
  let desc = Object.getOwnPropertyDescriptor(lazy, name);
  Assert.equal(desc.value, value);
  Assert.equal(desc.writable, true);
  Assert.equal(desc.enumerable, true);
  Assert.equal(desc.configurable, true);
}

add_task(function test_getter() {
  // The property should be defined as getter, and getting it should make it
  // a data property.

  const lazy = {};
  ChromeUtils.defineESModuleGetters(lazy, {
    X: "resource://test/esm_lazy-1.sys.mjs",
  });

  assertAccessor(lazy, "X");

  Assert.equal(lazy.X, 10);
  assertDataProperty(lazy, "X", 10);
});

add_task(function test_setter() {
  // Setting the value before the first get should result in a data property.
  const lazy = {};
  ChromeUtils.defineESModuleGetters(lazy, {
    X: "resource://test/esm_lazy-1.sys.mjs",
  });

  assertAccessor(lazy, "X");
  lazy.X = 20;
  Assert.equal(lazy.X, 20);
  assertDataProperty(lazy, "X", 20);

  // The above set shouldn't affect the module's value.
  const lazy2 = {};
  ChromeUtils.defineESModuleGetters(lazy2, {
    X: "resource://test/esm_lazy-1.sys.mjs",
  });

  Assert.equal(lazy2.X, 10);
});

add_task(function test_order() {
  // The change to the exported value should be reflected until it's accessed.

  const lazy = {};
  ChromeUtils.defineESModuleGetters(lazy, {
    Y: "resource://test/esm_lazy-2.sys.mjs",
    AddY: "resource://test/esm_lazy-2.sys.mjs",
  });

  assertAccessor(lazy, "Y");
  assertAccessor(lazy, "AddY");

  // The change before getting the value should be reflected.
  lazy.AddY(2);
  Assert.equal(lazy.Y, 22);
  assertDataProperty(lazy, "Y", 22);

  // Change after getting the value shouldn't be reflected.
  lazy.AddY(2);
  Assert.equal(lazy.Y, 22);
  assertDataProperty(lazy, "Y", 22);
});
