/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// A regression test for bug 665880 to make sure elements inside <object> can
// be inspected without exceptions.

const TEST_URI =
  "data:text/html;charset=utf-8," +
  "<object><p>browser_inspector_inspect-object-element.js</p></object>";

add_task(async function() {
  const { inspector } = await openInspectorForURL(TEST_URI);

  await selectNode("object", inspector);

  ok(true, "Selected <object> without throwing");
});
