/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const URL = "data:text/html;charset=utf8,browser_toolbox_telemetry_enter.js";
const ALL_CHANNELS = Ci.nsITelemetry.DATASET_ALL_CHANNELS;
const DATA = [
  {
    timestamp: null,
    category: "devtools.main",
    method: "exit",
    object: "inspector",
    value: null,
    extra: {
      host: "bottom",
      width: 1300,
      panel_name: "inspector",
      next_panel: "jsdebugger",
      reason: "toolbox_show",
    },
  }, {
    timestamp: null,
    category: "devtools.main",
    method: "exit",
    object: "jsdebugger",
    value: null,
    extra: {
      host: "bottom",
      width: 1300,
      panel_name: "jsdebugger",
      next_panel: "styleeditor",
      reason: "toolbox_show",
    },
  }, {
    timestamp: null,
    category: "devtools.main",
    method: "exit",
    object: "styleeditor",
    value: null,
    extra: {
      host: "bottom",
      width: 1300,
      panel_name: "styleeditor",
      next_panel: "netmonitor",
      reason: "toolbox_show",
    },
  }, {
    timestamp: null,
    category: "devtools.main",
    method: "exit",
    object: "netmonitor",
    value: null,
    extra: {
      host: "bottom",
      width: 1300,
      panel_name: "netmonitor",
      next_panel: "storage",
      reason: "toolbox_show",
    },
  }, {
    timestamp: null,
    category: "devtools.main",
    method: "exit",
    object: "storage",
    value: null,
    extra: {
      host: "bottom",
      width: 1300,
      panel_name: "storage",
      next_panel: "netmonitor",
      reason: "toolbox_show",
    },
  },
];

add_task(async function() {
  // Let's reset the counts.
  Services.telemetry.clearEvents();

  // Ensure no events have been logged
  const snapshot = Services.telemetry.snapshotEvents(ALL_CHANNELS, true);
  ok(!snapshot.parent, "No events have been logged for the main process");

  const tab = await addTab(URL);
  const target = await TargetFactory.forTab(tab);

  // Open the toolbox
  await gDevTools.showToolbox(target, "inspector");

  // Switch between a few tools
  await gDevTools.showToolbox(target, "jsdebugger");
  await gDevTools.showToolbox(target, "styleeditor");
  await gDevTools.showToolbox(target, "netmonitor");
  await gDevTools.showToolbox(target, "storage");
  await gDevTools.showToolbox(target, "netmonitor");

  await checkResults();
});

async function checkResults() {
  const snapshot = Services.telemetry.snapshotEvents(ALL_CHANNELS, true);
  const events = snapshot.parent.filter(event => event[1] === "devtools.main" &&
                                                 event[2] === "exit" &&
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

    // extras
    is(extra.host, expected.extra.host, "host is correct");
    ok(extra.width > 0, "width is greater than 0");
    is(extra.panel_name, expected.extra.panel_name, "panel_name is correct");
    is(extra.next_panel, expected.extra.next_panel, "next_panel is correct");
    is(extra.reason, expected.extra.reason, "reason is correct");
  }
}
