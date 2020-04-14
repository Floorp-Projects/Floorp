/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the console records the reverse search telemetry event with expected data
// on open, navigate forward, navigate back and evaluate expression.

"use strict";

const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

const TEST_URI = `data:text/html,<meta charset=utf8>Test reverse_search telemetry event`;
const ALL_CHANNELS = Ci.nsITelemetry.DATASET_ALL_CHANNELS;
const isMacOS = AppConstants.platform === "macosx";

add_task(async function() {
  // Let's reset the counts.
  Services.telemetry.clearEvents();

  // Ensure no events have been logged
  TelemetryTestUtils.assertNumberOfEvents(0);

  const hud = await openNewTabAndConsole(TEST_URI);

  info("Evaluate single line expressions");
  await keyboardExecuteAndWaitForMessage(hud, `"single line 1"`, "", ".result");
  await keyboardExecuteAndWaitForMessage(hud, `"single line 2"`, "", ".result");
  await keyboardExecuteAndWaitForMessage(hud, `"single line 3"`, "", ".result");

  info("Open editor mode");
  await toggleLayout(hud);

  info("Open reverse search from editor mode");
  hud.ui.outputNode
    .querySelector(".webconsole-editor-toolbar-reverseSearchButton")
    .click();

  info("Close reverse search");
  EventUtils.synthesizeKey("KEY_Escape");

  info("Open reverse search using keyboard shortcut");
  await openReverseSearch(hud);

  info("Send keys to reverse search");
  EventUtils.sendString("sin");

  info("Reverse search navigate next - keyboard");
  navigateReverseSearch("keyboard", "next", hud);

  info("Reverse search navigate previous - keyboard");
  navigateReverseSearch("keyboard", "previous", hud);

  info("Reverse search navigate next - mouse");
  navigateReverseSearch("mouse", "next", hud);

  info("Reverse search navigate previous - mouse");
  navigateReverseSearch("mouse", "previous", hud);

  info("Reverse search evaluate expression");
  EventUtils.synthesizeKey("KEY_Enter");

  info("Check reverse search telemetry");
  checkEventTelemetry([
    getTelemetryEventData("editor-toolbar-icon", { functionality: "open" }),
    getTelemetryEventData("keyboard", { functionality: "open" }),
    getTelemetryEventData("keyboard", { functionality: "navigate next" }),
    getTelemetryEventData("keyboard", { functionality: "navigate previous" }),
    getTelemetryEventData("click", { functionality: "navigate next" }),
    getTelemetryEventData("click", { functionality: "navigate previous" }),
    getTelemetryEventData(null, { functionality: "evaluate expression" }),
  ]);

  info("Revert to inline layout");
  await toggleLayout(hud);
});

function triggerPreviousResultShortcut() {
  if (isMacOS) {
    EventUtils.synthesizeKey("r", { ctrlKey: true });
  } else {
    EventUtils.synthesizeKey("VK_F9");
  }
}

function triggerNextResultShortcut() {
  if (isMacOS) {
    EventUtils.synthesizeKey("s", { ctrlKey: true });
  } else {
    EventUtils.synthesizeKey("VK_F9", { shiftKey: true });
  }
}

function clickPreviousButton(hud) {
  const reverseSearchElement = getReverseSearchElement(hud);
  if (!reverseSearchElement) {
    return;
  }
  const button = reverseSearchElement.querySelector(
    ".search-result-button-prev"
  );
  if (!button) {
    return;
  }

  button.click();
}

function clickNextButton(hud) {
  const reverseSearchElement = getReverseSearchElement(hud);
  if (!reverseSearchElement) {
    return;
  }
  const button = reverseSearchElement.querySelector(
    ".search-result-button-next"
  );
  if (!button) {
    return;
  }
  button.click();
}

function navigateReverseSearch(access, direction, hud) {
  if (access == "keyboard") {
    if (direction === "previous") {
      triggerPreviousResultShortcut();
    } else {
      triggerNextResultShortcut();
    }
  } else if (access === "mouse") {
    if (direction === "previous") {
      clickPreviousButton(hud);
    } else {
      clickNextButton(hud);
    }
  }
}

function getTelemetryEventData(value, extra) {
  return {
    timestamp: null,
    category: "devtools.main",
    method: "reverse_search",
    object: "webconsole",
    value,
    extra,
  };
}

function checkEventTelemetry(expectedData) {
  const snapshot = Services.telemetry.snapshotEvents(ALL_CHANNELS, true);
  const events = snapshot.parent.filter(event => event[2] === "reverse_search");

  for (const [i, expected] of expectedData.entries()) {
    const [timestamp, category, method, object, value, extra] = events[i];

    ok(timestamp > 0, "timestamp is greater than 0");
    is(category, expected.category, "'category' is correct");
    is(method, expected.method, "'method' is correct");
    is(object, expected.object, "'object' is correct");
    is(value, expected.value, "'value' is correct");
    is(
      extra.functionality,
      expected.extra.functionality,
      "'functionality' is correct"
    );
    ok(extra.session_id > 0, "'session_id' is correct");
  }
}
