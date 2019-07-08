/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

async function testSteps() {
  Assert.throws(
    () => {
      const principal = getPrincipal("http://example.com^123");
      getSimpleDatabase(principal);
    },
    /NS_ERROR_MALFORMED_URI/,
    "^ is not allowed in the hostname"
  );
}
