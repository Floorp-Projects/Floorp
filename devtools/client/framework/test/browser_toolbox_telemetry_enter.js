/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const URL = "data:text/html;charset=utf8,browser_toolbox_telemetry_enter.js";
const ALL_CHANNELS = Ci.nsITelemetry.DATASET_ALL_CHANNELS;
const DATA = [
  {
    timestamp: null,
    category: "devtools.main",
    method: "enter",
    object: "inspector",
    value: null,
    extra: {
      host: "bottom",
      width: "1300",
      start_state: "initial_panel",
      panel_name: "inspector",
      cold: "true",
    },
  },
  {
    timestamp: null,
    category: "devtools.main",
    method: "enter",
    object: "jsdebugger",
    value: null,
    extra: {
      host: "bottom",
      width: "1300",
      start_state: "toolbox_show",
      panel_name: "jsdebugger",
      cold: "true",
    },
  },
  {
    timestamp: null,
    category: "devtools.main",
    method: "enter",
    object: "styleeditor",
    value: null,
    extra: {
      host: "bottom",
      width: "1300",
      start_state: "toolbox_show",
      panel_name: "styleeditor",
      cold: "true",
    },
  },
  {
    timestamp: null,
    category: "devtools.main",
    method: "enter",
    object: "netmonitor",
    value: null,
    extra: {
      host: "bottom",
      width: "1300",
      start_state: "toolbox_show",
      panel_name: "netmonitor",
      cold: "true",
    },
  },
  {
    timestamp: null,
    category: "devtools.main",
    method: "enter",
    object: "storage",
    value: null,
    extra: {
      host: "bottom",
      width: "1300",
      start_state: "toolbox_show",
      panel_name: "storage",
      cold: "true",
    },
  },
  {
    timestamp: null,
    category: "devtools.main",
    method: "enter",
    object: "netmonitor",
    value: null,
    extra: {
      host: "bottom",
      width: "1300",
      start_state: "toolbox_show",
      panel_name: "netmonitor",
      cold: "false",
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

  // Set up some cached messages for the web console.
  await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    content.console.log("test 1");
    content.console.log("test 2");
    content.console.log("test 3");
    content.console.log("test 4");
    content.console.log("test 5");
  });

  // Open the toolbox
  await gDevTools.showToolboxForTab(tab, { toolId: "inspector" });

  // Switch between a few tools
  await gDevTools.showToolboxForTab(tab, { toolId: "jsdebugger" });
  await gDevTools.showToolboxForTab(tab, { toolId: "styleeditor" });
  await gDevTools.showToolboxForTab(tab, { toolId: "netmonitor" });
  await gDevTools.showToolboxForTab(tab, { toolId: "storage" });
  await gDevTools.showToolboxForTab(tab, { toolId: "netmonitor" });

  await checkResults();
});

async function checkResults() {
  const snapshot = Services.telemetry.snapshotEvents(ALL_CHANNELS, true);
  const events = snapshot.parent.filter(
    event =>
      event[1] === "devtools.main" && event[2] === "enter" && event[4] === null
  );

  for (const i in DATA) {
    const [timestamp, category, method, object, value, extra] = events[i];
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
    is(extra.start_state, expected.extra.start_state, "start_state is correct");
    is(extra.panel_name, expected.extra.panel_name, "panel_name is correct");
    is(extra.cold, expected.extra.cold, "cold is correct");
  }
}
