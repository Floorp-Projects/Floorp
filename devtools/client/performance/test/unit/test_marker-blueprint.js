/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";
/**
 * Tests if the timeline blueprint has a correct structure.
 */

add_task(function() {
  const { TIMELINE_BLUEPRINT } = require("devtools/client/performance/modules/markers");

  ok(TIMELINE_BLUEPRINT,
    "A timeline blueprint should be available.");

  ok(Object.keys(TIMELINE_BLUEPRINT).length,
    "The timeline blueprint has at least one entry.");

  for (const value of Object.values(TIMELINE_BLUEPRINT)) {
    ok("group" in value,
      "Each entry in the timeline blueprint contains a `group` key.");
    ok("colorName" in value,
      "Each entry in the timeline blueprint contains a `colorName` key.");
    ok("label" in value,
      "Each entry in the timeline blueprint contains a `label` key.");
  }
});
