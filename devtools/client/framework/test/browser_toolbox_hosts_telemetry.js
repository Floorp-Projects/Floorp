/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from head.js */

"use strict";

const {Toolbox} = require("devtools/client/framework/toolbox");
const {LEFT, RIGHT, BOTTOM, WINDOW} = Toolbox.HostType;

const URL = "data:text/html;charset=utf8,browser_toolbox_hosts_telemetry.js";

add_task(async function() {
  startTelemetry();

  info("Create a test tab and open the toolbox");
  const tab = await addTab(URL);
  const target = TargetFactory.forTab(tab);
  const toolbox = await gDevTools.showToolbox(target, "webconsole");

  await changeToolboxHost(toolbox);
  await checkResults();
});

async function changeToolboxHost(toolbox) {
  info("Switch toolbox host");
  await toolbox.switchHost(RIGHT);
  await toolbox.switchHost(WINDOW);
  await toolbox.switchHost(BOTTOM);
  await toolbox.switchHost(LEFT);
  await toolbox.switchHost(RIGHT);
  await toolbox.switchHost(WINDOW);
  await toolbox.switchHost(BOTTOM);
  await toolbox.switchHost(LEFT);
  await toolbox.switchHost(RIGHT);
}

function checkResults() {
  // Check for:
  //   - 3 "bottom" entries.
  //   - 2 "left" entries.
  //   - 3 "right" entries.
  //   - 2 "window" entries.
  checkTelemetry("DEVTOOLS_TOOLBOX_HOST", "", [3, 3, 2, 0, 2, 0, 0, 0, 0, 0], "array");
}
