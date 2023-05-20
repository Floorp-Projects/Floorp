/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test whether the compatibility tool is enabled or not according to the preference.

add_task(async function () {
  info("Check the compatibility tool is enabled if the pref is on");
  await addTab("data:text/html;charset=utf-8,test");
  await pushPref("devtools.inspector.compatibility.enabled", true);
  const { inspector } = await openRuleView();
  const compatibilityTab = inspector.panelDoc.getElementById(
    "compatibilityview-tab"
  );
  ok(compatibilityTab, "The compatibility tool is enabled");
});

add_task(async function () {
  info("Check the compatibility tool is disabled if the pref is off");
  await addTab("data:text/html;charset=utf-8,test");
  await pushPref("devtools.inspector.compatibility.enabled", false);
  const { inspector } = await openRuleView();
  const compatibilityTab = inspector.panelDoc.getElementById(
    "compatibilityview-tab"
  );
  ok(!compatibilityTab, "The compatibility tool is disabled");
});
