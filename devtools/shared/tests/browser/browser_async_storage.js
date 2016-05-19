/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the basic functionality of async-storage.
// Adapted from https://github.com/mozilla-b2g/gaia/blob/f09993563fb5fec4393eb71816ce76cb00463190/apps/sharedtest/test/unit/async_storage_test.js.

const asyncStorage = require("devtools/shared/async-storage");
add_task(function* () {
  is(typeof asyncStorage.length, "function", "API exists.");
  is(typeof asyncStorage.key, "function", "API exists.");
  is(typeof asyncStorage.getItem, "function", "API exists.");
  is(typeof asyncStorage.setItem, "function", "API exists.");
  is(typeof asyncStorage.removeItem, "function", "API exists.");
  is(typeof asyncStorage.clear, "function", "API exists.");
});

add_task(function* () {
  yield asyncStorage.setItem("foo", "bar");
  let value = yield asyncStorage.getItem("foo");
  is(value, "bar", "value is correct");
  yield asyncStorage.setItem("foo", "overwritten");
  value = yield asyncStorage.getItem("foo");
  is(value, "overwritten", "value is correct");
  yield asyncStorage.removeItem("foo");
  value = yield asyncStorage.getItem("foo");
  is(value, null, "value is correct");
});

add_task(function* () {
  var object = {
    x: 1,
    y: "foo",
    z: true
  };

  yield asyncStorage.setItem("myobj", object);
  let value = yield asyncStorage.getItem("myobj");
  is(object.x, value.x, "value is correct");
  is(object.y, value.y, "value is correct");
  is(object.z, value.z, "value is correct");
  yield asyncStorage.removeItem("myobj");
  value = yield asyncStorage.getItem("myobj");
  is(value, null, "value is correct");
});

add_task(function* () {
  yield asyncStorage.clear();
  let len = yield asyncStorage.length();
  is(len, 0, "length is correct");
  yield asyncStorage.setItem("key1", "value1");
  len = yield asyncStorage.length();
  is(len, 1, "length is correct");
  yield asyncStorage.setItem("key2", "value2");
  len = yield asyncStorage.length();
  is(len, 2, "length is correct");
  yield asyncStorage.setItem("key3", "value3");
  len = yield asyncStorage.length();
  is(len, 3, "length is correct");

  let key = yield asyncStorage.key(0);
  is(key, "key1", "key is correct");
  key = yield asyncStorage.key(1);
  is(key, "key2", "key is correct");
  key = yield asyncStorage.key(2);
  is(key, "key3", "key is correct");
  key = yield asyncStorage.key(3);
  is(key, null, "key is correct");
  yield asyncStorage.clear();
  key = yield asyncStorage.key(0);
  is(key, null, "key is correct");

  len = yield asyncStorage.length();
  is(len, 0, "length is correct");
});
