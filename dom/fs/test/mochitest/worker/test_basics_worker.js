/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

add_task(async function test_1() {
  const { test1 } = await require_module("dom/fs/test/common/test_basics.js");

  await test1();
});
