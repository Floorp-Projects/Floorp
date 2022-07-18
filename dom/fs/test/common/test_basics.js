/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

async function test1() {
  const { nsresult } = await require_module("dom/fs/test/common/nsresult.js");

  try {
    await navigator.storage.getDirectory();

    Assert.ok(false, "Should have thrown");
  } catch (e) {
    Assert.ok(true, "Should have thrown");
    Assert.ok(
      e.result === nsresult.NS_ERROR_NOT_IMPLEMENTED,
      "Threw right result code"
    );
  }
}
exported_symbols.test1 = test1;
