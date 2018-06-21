/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,<p>browser_telemetry_sidebar.js</p>";
const OPTOUT = Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTOUT;

// Because we need to gather stats for the period of time that a tool has been
// opened we make use of setTimeout() to create tool active times.
const TOOL_DELAY = 200;

var animationPanelId;
if (Services.prefs.getBoolPref("devtools.new-animationinspector.enabled")) {
  animationPanelId = "newanimationinspector";
} else {
  animationPanelId = "animationinspector";
}

const DATA = [
  {
    timestamp: null,
    category: "devtools.main",
    method: "sidepanel_changed",
    object: "inspector",
    value: null,
    extra: {
      oldpanel: "computedview",
      newpanel: animationPanelId
    }
  },
  {
    timestamp: null,
    category: "devtools.main",
    method: "sidepanel_changed",
    object: "inspector",
    value: null,
    extra: {
      oldpanel: animationPanelId,
      newpanel: "fontinspector"
    }
  },
  {
    timestamp: null,
    category: "devtools.main",
    method: "sidepanel_changed",
    object: "inspector",
    value: null,
    extra: {
      oldpanel: "fontinspector",
      newpanel: "layoutview"
    }
  },
  {
    timestamp: null,
    category: "devtools.main",
    method: "sidepanel_changed",
    object: "inspector",
    value: null,
    extra: {
      oldpanel: "layoutview",
      newpanel: "computedview"
    }
  },
  {
    timestamp: null,
    category: "devtools.main",
    method: "sidepanel_changed",
    object: "inspector",
    value: null,
    extra: {
      oldpanel: "computedview",
      newpanel: animationPanelId
    }
  },
  {
    timestamp: null,
    category: "devtools.main",
    method: "sidepanel_changed",
    object: "inspector",
    value: null,
    extra: {
      oldpanel: animationPanelId,
      newpanel: "fontinspector"
    }
  },
  {
    timestamp: null,
    category: "devtools.main",
    method: "sidepanel_changed",
    object: "inspector",
    value: null,
    extra: {
      oldpanel: "fontinspector",
      newpanel: "layoutview"
    }
  },
  {
    timestamp: null,
    category: "devtools.main",
    method: "sidepanel_changed",
    object: "inspector",
    value: null,
    extra: {
      oldpanel: "layoutview",
      newpanel: "computedview"
    }
  }
];

add_task(async function() {
  // Let's reset the counts.
  Services.telemetry.clearEvents();

  // Ensure no events have been logged
  const snapshot = Services.telemetry.snapshotEvents(OPTOUT, true);
  ok(!snapshot.parent, "No events have been logged for the main process");

  await addTab(TEST_URI);
  startTelemetry();

  const target = TargetFactory.forTab(gBrowser.selectedTab);
  const toolbox = await gDevTools.showToolbox(target, "inspector");
  info("inspector opened");

  await testSidebar(toolbox);
  checkResults();
  checkEventTelemetry();

  await gDevTools.closeToolbox(target);
  gBrowser.removeCurrentTab();
});

function testSidebar(toolbox) {
  info("Testing sidebar");

  const inspector = toolbox.getCurrentPanel();
  let sidebarTools = ["computedview", "layoutview", "fontinspector",
                      animationPanelId];

  // Concatenate the array with itself so that we can open each tool twice.
  sidebarTools = [...sidebarTools, ...sidebarTools];

  return new Promise(resolve => {
    // See TOOL_DELAY for why we need setTimeout here
    setTimeout(function selectSidebarTab() {
      const tool = sidebarTools.pop();
      if (tool) {
        inspector.sidebar.select(tool);
        setTimeout(function() {
          setTimeout(selectSidebarTab, TOOL_DELAY);
        }, TOOL_DELAY);
      } else {
        resolve();
      }
    }, TOOL_DELAY);
  });
}

function checkResults() {
  // For help generating these tests use generateTelemetryTests("DEVTOOLS_")
  // here.
  checkTelemetry("DEVTOOLS_INSPECTOR_OPENED_COUNT", "", [1, 0, 0], "array");
  checkTelemetry("DEVTOOLS_RULEVIEW_OPENED_COUNT", "", [1, 0, 0], "array");
  checkTelemetry("DEVTOOLS_COMPUTEDVIEW_OPENED_COUNT", "", [3, 0, 0], "array");
  checkTelemetry("DEVTOOLS_LAYOUTVIEW_OPENED_COUNT", "", [2, 0, 0], "array");
  checkTelemetry("DEVTOOLS_FONTINSPECTOR_OPENED_COUNT", "", [2, 0, 0], "array");
  checkTelemetry("DEVTOOLS_COMPUTEDVIEW_TIME_ACTIVE_SECONDS", "", null, "hasentries");
  checkTelemetry("DEVTOOLS_LAYOUTVIEW_TIME_ACTIVE_SECONDS", "", null, "hasentries");
  checkTelemetry("DEVTOOLS_FONTINSPECTOR_TIME_ACTIVE_SECONDS", "", null, "hasentries");
}

function checkEventTelemetry() {
  const snapshot = Services.telemetry.snapshotEvents(OPTOUT, true);
  const events = snapshot.parent.filter(event => event[1] === "devtools.main" &&
                                                  event[2] === "sidepanel_changed" &&
                                                  event[3] === "inspector" &&
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

    is(extra.oldpanel, expected.extra.oldpanel, "oldpanel is correct");
    is(extra.newpanel, expected.extra.newpanel, "newpanel is correct");
  }
}
