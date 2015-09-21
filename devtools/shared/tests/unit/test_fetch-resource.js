/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Tests for DevToolsUtils.fetch on resource:// URI's.

const URL_FOUND = "resource://gre/modules/devtools/shared/DevToolsUtils.js";
const URL_NOT_FOUND = "resource://gre/modules/devtools/this/is/not/here.js";

/**
 * Test that non-existent files are handled correctly.
 */
add_task(function* test_missing() {
  yield DevToolsUtils.fetch(URL_NOT_FOUND).then(result => {
    do_print(result);
    ok(false, "fetch resolved unexpectedly for non-existent resource:// URI");
  }, () => {
    ok(true, "fetch rejected as the resource:// URI was non-existent.");
  });
});

/**
 * Tests that existing files are handled correctly.
 */
add_task(function* test_normal() {
  yield DevToolsUtils.fetch(URL_FOUND).then(result => {
    notDeepEqual(result.content, "",
      "resource:// URI seems to be read correctly.");
  });
});
