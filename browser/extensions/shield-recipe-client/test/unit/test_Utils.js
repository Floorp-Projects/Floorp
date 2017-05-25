"use strict";

Cu.import("resource://shield-recipe-client/lib/Utils.jsm");

add_task(async function testKeyBy() {
  const list = [];
  deepEqual(Utils.keyBy(list, "foo"), {});

  const foo = {name: "foo", value: 1};
  list.push(foo);
  deepEqual(Utils.keyBy(list, "name"), {foo});

  const bar = {name: "bar", value: 2};
  list.push(bar);
  deepEqual(Utils.keyBy(list, "name"), {foo, bar});

  const missingKey = {value: 7};
  list.push(missingKey);
  deepEqual(Utils.keyBy(list, "name"), {foo, bar}, "keyBy skips items that are missing the key");
});
