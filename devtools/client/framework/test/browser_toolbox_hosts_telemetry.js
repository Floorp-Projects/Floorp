/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {Toolbox} = require("devtools/client/framework/toolbox");
const {SIDE, BOTTOM, WINDOW} = Toolbox.HostType;

const URL = "data:text/html;charset=utf8,browser_toolbox_hosts_telemetry.js";

function getHostHistogram() {
  return Services.telemetry.getHistogramById("DEVTOOLS_TOOLBOX_HOST");
}

add_task(async function() {
  // Reset it to make counting easier
  getHostHistogram().clear();

  info("Create a test tab and open the toolbox");
  let tab = await addTab(URL);
  let target = TargetFactory.forTab(tab);
  let toolbox = await gDevTools.showToolbox(target, "webconsole");

  await changeToolboxHost(toolbox);
  await checkResults();
  await toolbox.destroy();

  toolbox = target = null;
  gBrowser.removeCurrentTab();

  // Cleanup
  getHostHistogram().clear();
});

async function changeToolboxHost(toolbox) {
  info("Switch toolbox host");
  await toolbox.switchHost(SIDE);
  await toolbox.switchHost(WINDOW);
  await toolbox.switchHost(BOTTOM);
  await toolbox.switchHost(SIDE);
  await toolbox.switchHost(WINDOW);
  await toolbox.switchHost(BOTTOM);
}

function checkResults() {
  let counts = getHostHistogram().snapshot().counts;
  is(counts[0], 3, "Toolbox HostType bottom has 3 successful entries");
  is(counts[1], 2, "Toolbox HostType side has 2 successful entries");
  is(counts[2], 2, "Toolbox HostType window has 2 successful entries");
}
