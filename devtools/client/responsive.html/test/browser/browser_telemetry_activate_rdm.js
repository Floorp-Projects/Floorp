/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";
const URL = "data:text/html;charset=utf8,browser_telemetry_activate_rdm.js";
const OPTOUT = Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTOUT;
const DATA = [
  {
    timestamp: null,
    category: "devtools.main",
    method: "activate",
    object: "responsive_design",
    value: null,
    extra: {
      host: "none",
      width: "1300"
    }
  }, {
    timestamp: null,
    category: "devtools.main",
    method: "deactivate",
    object: "responsive_design",
    value: null,
    extra: {
      host: "bottom",
      width: "1300"
    }
  }, {
    timestamp: null,
    category: "devtools.main",
    method: "activate",
    object: "responsive_design",
    value: null,
    extra: {
      host: "bottom",
      width: "1300"
    }
  }, {
    timestamp: null,
    category: "devtools.main",
    method: "deactivate",
    object: "responsive_design",
    value: null,
    extra: {
      host: "bottom",
      width: "1300"
    }
  }
];

add_task(async function() {
  // Let's reset the counts.
  Services.telemetry.clearEvents();

  // Ensure no events have been logged
  const snapshot = Services.telemetry.snapshotEvents(OPTOUT, true);
  ok(!snapshot.parent, "No events have been logged for the main process");

  const tab = await addTab(URL);
  const target = TargetFactory.forTab(tab);

  await openCloseRDM(tab);
  await gDevTools.showToolbox(target, "inspector");
  await openCloseRDM(tab);
  await checkResults();
});

async function openCloseRDM(tab) {
  const { ui } = await openRDM(tab);
  const clientClosed = waitForClientClose(ui);

  closeRDM(tab, {
    reason: "TabClose",
  });

  // This flag is set at the end of `ResponsiveUI.destroy`.  If it is true
  // without waiting for `closeRDM` above, then we must have closed
  // synchronously.
  is(ui.destroyed, true, "RDM closed synchronously");

  await clientClosed;
}

async function checkResults() {
  const snapshot = Services.telemetry.snapshotEvents(OPTOUT, true);
  const events = snapshot.parent.filter(event => event[1] === "devtools.main" &&
                                                 (event[2] === "activate" ||
                                                 event[2] === "deactivate")
  );

  for (const i in events) {
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
  }
}
