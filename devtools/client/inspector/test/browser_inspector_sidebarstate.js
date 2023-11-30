/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const TEST_URI =
  "data:text/html;charset=UTF-8," +
  "<h1>browser_inspector_sidebarstate.js</h1>";
const ALL_CHANNELS = Ci.nsITelemetry.DATASET_ALL_CHANNELS;

const TELEMETRY_DATA = [
  {
    timestamp: null,
    category: "devtools.main",
    method: "tool_timer",
    object: "layoutview",
    value: null,
    extra: {
      time_open: "",
    },
  },
  {
    timestamp: null,
    category: "devtools.main",
    method: "tool_timer",
    object: "fontinspector",
    value: null,
    extra: {
      time_open: "",
    },
  },
  {
    timestamp: null,
    category: "devtools.main",
    method: "tool_timer",
    object: "compatibilityview",
    value: null,
    extra: {
      time_open: "",
    },
  },
  {
    timestamp: null,
    category: "devtools.main",
    method: "tool_timer",
    object: "computedview",
    value: null,
    extra: {
      time_open: "",
    },
  },
];

add_task(async function () {
  // Let's reset the counts.
  Services.telemetry.clearEvents();

  // Ensure no events have been logged
  const snapshot = Services.telemetry.snapshotEvents(ALL_CHANNELS, true);
  ok(!snapshot.parent, "No events have been logged for the main process");

  let { inspector, toolbox } = await openInspectorForURL(TEST_URI);

  info("Selecting font inspector.");
  inspector.sidebar.select("fontinspector");

  is(
    inspector.sidebar.getCurrentTabID(),
    "fontinspector",
    "Font Inspector is selected"
  );

  info("Selecting compatibility view.");
  const onCompatibilityViewInitialized = inspector.once(
    "compatibilityview-initialized"
  );
  inspector.sidebar.select("compatibilityview");
  await onCompatibilityViewInitialized;

  is(
    inspector.sidebar.getCurrentTabID(),
    "compatibilityview",
    "Compatibility View is selected"
  );

  info("Selecting computed view.");
  inspector.sidebar.select("computedview");

  is(
    inspector.sidebar.getCurrentTabID(),
    "computedview",
    "Computed View is selected"
  );

  info("Closing inspector.");
  await toolbox.destroy();

  info("Re-opening inspector.");
  inspector = (await openInspector()).inspector;

  if (!inspector.sidebar.getCurrentTabID()) {
    info("Default sidebar still to be selected, adding select listener.");
    await inspector.sidebar.once("select");
  }

  is(
    inspector.sidebar.getCurrentTabID(),
    "computedview",
    "Computed view is selected by default."
  );

  checkTelemetryResults();
});

function checkTelemetryResults() {
  const snapshot = Services.telemetry.snapshotEvents(ALL_CHANNELS, true);
  const events = snapshot.parent.filter(
    event => event[1] === "devtools.main" && event[2] === "tool_timer"
  );

  for (const i in TELEMETRY_DATA) {
    const [timestamp, category, method, object, value, extra] = events[i];
    const expected = TELEMETRY_DATA[i];

    // ignore timestamp
    ok(timestamp > 0, "timestamp is greater than 0");
    ok(extra.time_open > 0, "time_open is greater than 0");
    is(category, expected.category, "category is correct");
    is(method, expected.method, "method is correct");
    is(object, expected.object, "object is correct");
    is(value, expected.value, "value is correct");
  }
}
