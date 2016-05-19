/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

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
  let descriptor = Object.getOwnPropertyDescriptor(Class.prototype, "foo");
  do_check_eq(typeof descriptor.get, "function");
  do_check_eq(descriptor.set, undefined);
  do_check_eq(descriptor.enumerable, false);
  do_check_eq(descriptor.configurable, true);
}

function test_instance_attributes() {
  // Instances should not have an own property until the lazy getter has been
  // activated.
  let instance = new Class();
  do_check_false(instance.hasOwnProperty("foo"));
  instance.foo;
  do_check_true(instance.hasOwnProperty("foo"));

  // Check that the instance has an own property with the expecred value and
  // attributes after the lazy getter is activated.
  let descriptor = Object.getOwnPropertyDescriptor(instance, "foo");
  do_check_true(descriptor.value instanceof Array);
  do_check_eq(descriptor.writable, true);
  do_check_eq(descriptor.enumerable, false);
  do_check_eq(descriptor.configurable, true);
}

function test_multiple_instances() {
  let instance1 = new Class();
  let instance2 = new Class();
  let foo1 = instance1.foo;
  let foo2 = instance2.foo;
  // Check that the lazy getter returns the expected type of value.
  do_check_true(foo1 instanceof Array);
  do_check_true(foo2 instanceof Array);
  // Make sure the lazy getter runs once and only once per instance.
  do_check_eq(instance1.foo, foo1);
  do_check_eq(instance2.foo, foo2);
  // Make sure each instance gets its own unique value.
  do_check_neq(foo1, foo2);
}

function test_callback_receiver() {
  function Foo() {}
  DevToolsUtils.defineLazyPrototypeGetter(Foo.prototype, "foo", function () {
    return this;
  });

  // Check that the |this| value in the callback is the instance itself.
  let instance = new Foo();
  do_check_eq(instance.foo, instance);
}
