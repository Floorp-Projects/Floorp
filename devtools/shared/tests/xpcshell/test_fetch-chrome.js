/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Tests for DevToolsUtils.fetch on chrome:// URI's.

const URL_FOUND = "chrome://devtools-shared/locale/debugger.properties";
const URL_NOT_FOUND = "chrome://this/is/not/here.js";

/**
 * Test that non-existent files are handled correctly.
 */
add_task(async function test_missing() {
  await DevToolsUtils.fetch(URL_NOT_FOUND).then(
    result => {
      info(result);
      ok(false, "fetch resolved unexpectedly for non-existent chrome:// URI");
    },
    () => {
      ok(true, "fetch rejected as the chrome:// URI was non-existent.");
    }
  );
});

/**
 * Tests that existing files are handled correctly.
 */
add_task(async function test_normal() {
  await DevToolsUtils.fetch(URL_FOUND).then(result => {
    notDeepEqual(
      result.content,
      "",
      "chrome:// URI seems to be read correctly."
    );
  });
});
