/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { Toolbox } = require("devtools/client/framework/toolbox");

const URL = "data:text/html;charset=utf8,browser_toolbox_telemetry_close.js";
const OPTOUT = Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTOUT;
const { SIDE, BOTTOM } = Toolbox.HostType;
const DATA = [
  {
    timestamp: null,
    category: "devtools.main",
    method: "close",
    object: "tools",
    value: null,
    extra: {
      host: "side",
      width: "1440"
    }
  },
  {
    timestamp: null,
    category: "devtools.main",
    method: "close",
    object: "tools",
    value: null,
    extra: {
      host: "bottom",
      width: "1440"
    }
  }
];

add_task(async function() {
  // Let's reset the counts.
  Services.telemetry.clearEvents();

  // Ensure no events have been logged
  const snapshot = Services.telemetry.snapshotEvents(OPTOUT, true);
  ok(!snapshot.parent, "No events have been logged for the main process");

  await openAndCloseToolbox("webconsole", SIDE);
  await openAndCloseToolbox("webconsole", BOTTOM);

  checkResults();
});

async function openAndCloseToolbox(toolId, host) {
  const tab = await addTab(URL);
  const target = TargetFactory.forTab(tab);
  const toolbox = await gDevTools.showToolbox(target, toolId);

  await toolbox.switchHost(host);
  await toolbox.destroy();
}

function checkResults() {
  const snapshot = Services.telemetry.snapshotEvents(OPTOUT, true);
  const events = snapshot.parent.filter(event => event[1] === "devtools.main" &&
                                                 event[2] === "close" &&
                                                 event[3] === "tools" &&
                                                 event[4] === null
  );

  for (const i in DATA) {
    const [ timestamp, category, method, object, value, extra ] = events[i];
    const expected = DATA[i];

    // ignore timestamp
    ok(timestamp > 0, "timestamp is greater than 0");
    is(category, expected.category, "category is correct");
    is(method, expected.method, "method is correct");
    is(object, expected.object, "object is correct");
    is(value, expected.value, "value is correct");

    is(extra.host, expected.extra.host, "host is correct");
    ok(extra.width > 0, "width is greater than 0");
  }
}
