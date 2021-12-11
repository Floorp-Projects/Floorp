/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const URL =
  "data:text/html;charset=utf8,browser_toolbox_telemetry_activate_splitconsole.js";
const ALL_CHANNELS = Ci.nsITelemetry.DATASET_ALL_CHANNELS;
const DATA = [
  {
    timestamp: null,
    category: "devtools.main",
    method: "activate",
    object: "split_console",
    value: null,
    extra: {
      host: "bottom",
      width: "1300",
    },
  },
  {
    timestamp: null,
    category: "devtools.main",
    method: "deactivate",
    object: "split_console",
    value: null,
    extra: {
      host: "bottom",
      width: "1300",
    },
  },
  {
    timestamp: null,
    category: "devtools.main",
    method: "activate",
    object: "split_console",
    value: null,
    extra: {
      host: "bottom",
      width: "1300",
    },
  },
  {
    timestamp: null,
    category: "devtools.main",
    method: "deactivate",
    object: "split_console",
    value: null,
    extra: {
      host: "bottom",
      width: "1300",
    },
  },
];

add_task(async function() {
  // See Bug 1500141: this test frequently fails on beta because some highlighter
  // requests made by the BoxModel component in the layout view come back when the
  // connection between the client and the server has been destroyed. We are forcing
  // the computed view here to avoid the failures but ideally we should have an event
  // or a promise on the inspector we can wait for to be sure the initialization is over.
  // Logged Bug 1500918 to investigate this.
  await pushPref("devtools.inspector.activeSidebar", "computedview");

  // Let's reset the counts.
  Services.telemetry.clearEvents();

  // Ensure no events have been logged
  const snapshot = Services.telemetry.snapshotEvents(ALL_CHANNELS, true);
  ok(!snapshot.parent, "No events have been logged for the main process");

  const tab = await addTab(URL);
  const toolbox = await gDevTools.showToolboxForTab(tab, {
    toolId: "inspector",
  });

  await toolbox.openSplitConsole();
  await toolbox.closeSplitConsole();
  await toolbox.openSplitConsole();
  await toolbox.closeSplitConsole();

  await checkResults();
});

async function checkResults() {
  const snapshot = Services.telemetry.snapshotEvents(ALL_CHANNELS, true);
  const events = snapshot.parent.filter(
    event =>
      event[1] === "devtools.main" &&
      (event[2] === "activate" || event[2] === "deactivate")
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
  }
}
