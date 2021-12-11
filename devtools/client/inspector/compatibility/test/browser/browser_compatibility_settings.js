/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test whether settings page works.

const TEST_URI = `
  <style>
  body {
    border-block-color: lime;
  }
  div {
    border-block-width: 1px;
  }
  </style>
  <body><div></div></body>
`;

const {
  COMPATIBILITY_UPDATE_TARGET_BROWSERS_COMPLETE,
} = require("devtools/client/inspector/compatibility/actions/index");

add_task(async function() {
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref(
      "devtools.inspector.compatibility.target-browsers"
    );
  });

  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, panel } = await openCompatibilityView();
  const { store } = inspector;

  info("Check initial state");
  ok(
    panel.querySelector(`.compatibility-browser-icon__image[src*="firefox"]`),
    "Firefox browsers are the target"
  );

  info("Make Firefox browsers out of target");
  await updateTargetBrowsers(panel, store, id => !id.includes("firefox"));
  ok(
    !panel.querySelector(`.compatibility-browser-icon__image[src*="firefox"]`),
    "Firefox browsers are not the target"
  );

  info("Make all browsers out of target");
  await updateTargetBrowsers(panel, store, () => false);
  ok(
    !panel.querySelector(".compatibility-browser-icon__image"),
    "No browsers are the target"
  );

  info("Make Firefox browsers target");
  await updateTargetBrowsers(panel, store, id => id.includes("firefox"));
  ok(
    panel.querySelector(`.compatibility-browser-icon__image[src*="firefox"]`),
    "Firefox browsers are the target now"
  );
});

async function updateTargetBrowsers(panel, store, isTargetBrowserFunc) {
  info("Open settings pane");
  const settingsButton = panel.querySelector(".compatibility-footer__button");
  settingsButton.click();
  await waitUntil(() => panel.querySelector(".compatibility-settings"));

  info("Change target browsers");
  const settingsPane = panel.querySelector(".compatibility-settings");
  for (const checkbox of settingsPane.querySelectorAll(
    ".compatibility-settings__target-browsers-item input"
  )) {
    if (checkbox.checked !== isTargetBrowserFunc(checkbox.id)) {
      // Need to click to toggle since the input is designed as controlled component.
      checkbox.click();
    }
  }

  info("Close settings pane");
  const onUpdated = waitForDispatch(
    store,
    COMPATIBILITY_UPDATE_TARGET_BROWSERS_COMPLETE
  );
  const closeButton = settingsPane.querySelector(
    ".compatibility-settings__header-button"
  );
  closeButton.click();
  await onUpdated;
}
