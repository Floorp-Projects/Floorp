/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the basic functionality of async-storage.
// Adapted from https://github.com/mozilla-b2g/gaia/blob/f09993563fb5fec4393eb71816ce76cb00463190/apps/sharedtest/test/unit/async_storage_test.js.

const asyncStorage = require("devtools/shared/async-storage");
add_task(async function() {
  is(typeof asyncStorage.length, "function", "API exists.");
  is(typeof asyncStorage.key, "function", "API exists.");
  is(typeof asyncStorage.getItem, "function", "API exists.");
  is(typeof asyncStorage.setItem, "function", "API exists.");
  is(typeof asyncStorage.removeItem, "function", "API exists.");
  is(typeof asyncStorage.clear, "function", "API exists.");
});

add_task(async function() {
  await asyncStorage.setItem("foo", "bar");
  let value = await asyncStorage.getItem("foo");
  is(value, "bar", "value is correct");
  await asyncStorage.setItem("foo", "overwritten");
  value = await asyncStorage.getItem("foo");
  is(value, "overwritten", "value is correct");
  await asyncStorage.removeItem("foo");
  value = await asyncStorage.getItem("foo");
  is(value, null, "value is correct");
});

add_task(async function() {
  const object = {
    x: 1,
    y: "foo",
    z: true
  };

  await asyncStorage.setItem("myobj", object);
  let value = await asyncStorage.getItem("myobj");
  is(object.x, value.x, "value is correct");
  is(object.y, value.y, "value is correct");
  is(object.z, value.z, "value is correct");
  await asyncStorage.removeItem("myobj");
  value = await asyncStorage.getItem("myobj");
  is(value, null, "value is correct");
});

add_task(async function() {
  await asyncStorage.clear();
  let len = await asyncStorage.length();
  is(len, 0, "length is correct");
  await asyncStorage.setItem("key1", "value1");
  len = await asyncStorage.length();
  is(len, 1, "length is correct");
  await asyncStorage.setItem("key2", "value2");
  len = await asyncStorage.length();
  is(len, 2, "length is correct");
  await asyncStorage.setItem("key3", "value3");
  len = await asyncStorage.length();
  is(len, 3, "length is correct");

  let key = await asyncStorage.key(0);
  is(key, "key1", "key is correct");
  key = await asyncStorage.key(1);
  is(key, "key2", "key is correct");
  key = await asyncStorage.key(2);
  is(key, "key3", "key is correct");
  key = await asyncStorage.key(3);
  is(key, null, "key is correct");
  await asyncStorage.clear();
  key = await asyncStorage.key(0);
  is(key, null, "key is correct");

  len = await asyncStorage.length();
  is(len, 0, "length is correct");
});
