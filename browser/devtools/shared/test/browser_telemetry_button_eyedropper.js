/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URI = "data:text/html;charset=utf-8," +
  "<p>browser_telemetry_button_eyedropper.js</p><div>test</div>";

let {EyedropperManager} = require("devtools/eyedropper/eyedropper");

add_task(function*() {
  yield promiseTab(TEST_URI);

  startTelemetry();

  let target = TargetFactory.forTab(gBrowser.selectedTab);
  let toolbox = yield gDevTools.showToolbox(target, "inspector");
  info("inspector opened");

  info("testing the eyedropper button");
  testButton(toolbox);

  yield gDevTools.closeToolbox(target);
  gBrowser.removeCurrentTab();
});

function testButton(toolbox) {
  let button = toolbox.doc.querySelector("#command-button-eyedropper");
  ok(button, "Captain, we have the eyedropper button");

  info("clicking the button to open the eyedropper");
  button.click();

  checkResults();
}

function checkResults() {
  // For help generating these tests use generateTelemetryTests("DEVTOOLS_")
  // here.

  checkTelemetry("DEVTOOLS_EYEDROPPER_OPENED_BOOLEAN", [0,1,0]);
  checkTelemetry("DEVTOOLS_EYEDROPPER_OPENED_PER_USER_FLAG", [0,1,0]);
}
