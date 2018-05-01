/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "data:text/html;charset=utf-8," +
  "<p>browser_telemetry_button_eyedropper.js</p><div>test</div>";

add_task(async function() {
  await addTab(TEST_URI);
  startTelemetry();

  let target = TargetFactory.forTab(gBrowser.selectedTab);
  let toolbox = await gDevTools.showToolbox(target, "inspector");

  info("testing the eyedropper button");
  await testButton(toolbox);

  await gDevTools.closeToolbox(target);
  gBrowser.removeCurrentTab();
});

async function testButton(toolbox, Telemetry) {
  info("Calling the eyedropper button's callback");
  // We call the button callback directly because we don't need to test the UI here, we're
  // only concerned about testing the telemetry probe.
  await toolbox.getPanel("inspector").showEyeDropper();

  checkResults();
}

function checkResults() {
  // For help generating these tests use generateTelemetryTests("devtools.")
  // here.
  checkTelemetry("devtools.toolbar.eyedropper.opened", "", 1, "scalar");
}
