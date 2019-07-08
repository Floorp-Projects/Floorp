/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { Toolbox } = require("devtools/client/framework/toolbox");
const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

const URL = "data:text/html;charset=utf8,browser_toolbox_telemetry_close.js";
const { RIGHT, BOTTOM } = Toolbox.HostType;
const DATA = [
  {
    category: "devtools.main",
    method: "close",
    object: "tools",
    value: null,
    extra: {
      host: "right",
      width: w => w > 0,
    },
  },
  {
    category: "devtools.main",
    method: "close",
    object: "tools",
    value: null,
    extra: {
      host: "bottom",
      width: w => w > 0,
    },
  },
];

add_task(async function() {
  // Let's reset the counts.
  Services.telemetry.clearEvents();

  // Ensure no events have been logged
  TelemetryTestUtils.assertNumberOfEvents(0);

  await openAndCloseToolbox("webconsole", RIGHT);
  await openAndCloseToolbox("webconsole", BOTTOM);

  checkResults();
});

async function openAndCloseToolbox(toolId, host) {
  const tab = await addTab(URL);
  const target = await TargetFactory.forTab(tab);
  const toolbox = await gDevTools.showToolbox(target, toolId);

  await toolbox.switchHost(host);
  await toolbox.destroy();
}

function checkResults() {
  TelemetryTestUtils.assertEvents(DATA, {
    category: "devtools.main",
    method: "close",
    object: "tools",
  });
}
