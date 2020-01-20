/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test DevToolsUtils.defineLazyPrototypeGetter

function Class() {}
DevToolsUtils.defineLazyPrototypeGetter(Class.prototype, "foo", () => []);

function run_test() {
  test_prototype_attributes();
  test_instance_attributes();
  test_multiple_instances();
  test_callback_receiver();
}

function test_prototype_attributes() {
  // Check that the prototype has a getter property with expected attributes.
  const descriptor = Object.getOwnPropertyDescriptor(Class.prototype, "foo");
  Assert.equal(typeof descriptor.get, "function");
  Assert.equal(descriptor.set, undefined);
  Assert.equal(descriptor.enumerable, false);
  Assert.equal(descriptor.configurable, true);
}

function test_instance_attributes() {
  // Instances should not have an own property until the lazy getter has been
  // activated.
  const instance = new Class();
  Assert.ok(!instance.hasOwnProperty("foo"));
  instance.foo;
  Assert.ok(instance.hasOwnProperty("foo"));

  // Check that the instance has an own property with the expecred value and
  // attributes after the lazy getter is activated.
  const descriptor = Object.getOwnPropertyDescriptor(instance, "foo");
  Assert.ok(descriptor.value instanceof Array);
  Assert.equal(descriptor.writable, true);
  Assert.equal(descriptor.enumerable, false);
  Assert.equal(descriptor.configurable, true);
}

function test_multiple_instances() {
  const instance1 = new Class();
  const instance2 = new Class();
  const foo1 = instance1.foo;
  const foo2 = instance2.foo;
  // Check that the lazy getter returns the expected type of value.
  Assert.ok(foo1 instanceof Array);
  Assert.ok(foo2 instanceof Array);
  // Make sure the lazy getter runs once and only once per instance.
  Assert.equal(instance1.foo, foo1);
  Assert.equal(instance2.foo, foo2);
  // Make sure each instance gets its own unique value.
  Assert.notEqual(foo1, foo2);
}

function test_callback_receiver() {
  function Foo() {}
  DevToolsUtils.defineLazyPrototypeGetter(Foo.prototype, "foo", function() {
    return this;
  });

  // Check that the |this| value in the callback is the instance itself.
  const instance = new Foo();
  Assert.equal(instance.foo, instance);
}
