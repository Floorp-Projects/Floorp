/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Tests for DevToolsUtils.fetch on resource:// URI's.

const URL_FOUND = "resource://devtools/shared/DevToolsUtils.js";
const URL_NOT_FOUND = "resource://devtools/this/is/not/here.js";

// Disable `xpc::IsInAutomation()` so we don't crash when accessing a
// nonexistent resource URI.
Services.prefs.setBoolPref(
  "security.turn_off_all_security_so_that_viruses_can_take_over_this_computer",
  false
);

/**
 * Test that non-existent files are handled correctly.
 */
add_task(async function test_missing() {
  await DevToolsUtils.fetch(URL_NOT_FOUND).then(
    result => {
      info(result);
      ok(false, "fetch resolved unexpectedly for non-existent resource:// URI");
    },
    () => {
      ok(true, "fetch rejected as the resource:// URI was non-existent.");
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
      "resource:// URI seems to be read correctly."
    );
  });
});
