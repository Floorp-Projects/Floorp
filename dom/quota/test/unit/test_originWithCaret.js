/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

async function testSteps() {
  const principal = getPrincipal("http://example.com^123");

  try {
    getSimpleDatabase(principal);
    ok(false, "Should have thrown");
  } catch (ex) {
    ok(true, "Did throw");
  }
}
