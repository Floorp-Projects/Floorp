/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PREF_PIONEER_ID = "toolkit.telemetry.pioneerId";
const PREF_PIONEER_NEW_STUDIES_AVAILABLE =
  "toolkit.telemetry.pioneer-new-studies-available";

const PREF_CACHED_ADDONS = "toolkit.pioneer.testCachedAddons";
const PREF_TEST_ADDON_INSTALLED = "toolkit.pioneer.testAddonInstalled";

const CACHED_ADDONS = [
  {
    addon_id: "pioneer-v2-example@mozilla.org",
    icons: {
      "32":
        "https://localhost/user-media/addon_icons/2644/2644632-32.png?modified=4a64e2bc",
      "64":
        "https://localhost/user-media/addon_icons/2644/2644632-64.png?modified=4a64e2bc",
      "128":
        "https://localhost/user-media/addon_icons/2644/2644632-128.png?modified=4a64e2bc",
    },
    _unsupportedProperties: {},
    name: "Demo Study",
    version: "1.0",
    sourceURI: {
      spec: "https://localhost",
    },
    homepageURL: "https://github.com/rhelmer/pioneer-v2-example",
    supportURL: null,
    description: "Study purpose: Testing Pioneer.",
    fullDescription:
      "Data collected: An encrypted ping containing the current date and time is sent to Mozilla's servers.",
    weeklyDownloads: 0,
    type: "extension",
    creator: {
      name: "Pioneer Developers",
      url: "https://addons.mozilla.org/en-US/firefox/user/6510522/",
    },
    developers: [],
    screenshots: [],
    contributionURL: "",
    averageRating: 0,
    reviewCount: 0,
    reviewURL:
      "https://addons.mozilla.org/en-US/firefox/addon/pioneer-v2-example/reviews/",
    updateDate: "2020-05-27T20:47:35.000Z",
  },
  {
    addon_id: "pioneer-v2-default-example@mozilla.org",
    icons: {
      "32":
        "https://localhost/user-media/addon_icons/2644/2644632-32.png?modified=4a64e2bc",
      "64":
        "https://localhost/user-media/addon_icons/2644/2644632-64.png?modified=4a64e2bc",
      "128":
        "https://localhost/user-media/addon_icons/2644/2644632-128.png?modified=4a64e2bc",
    },
    _unsupportedProperties: {},
    name: "Demo Default Study",
    version: "1.0",
    sourceURI: {
      spec: "https://localhost",
    },
    homepageURL: "https://github.com/rhelmer/pioneer-v2-example",
    supportURL: null,
    description: "Study purpose: Testing Pioneer.",
    fullDescription:
      "Data collected: An encrypted ping containing the current date and time is sent to Mozilla's servers.",
    weeklyDownloads: 0,
    type: "extension",
    creator: {
      name: "Pioneer Developers",
      url: "https://addons.mozilla.org/en-US/firefox/user/6510522/",
    },
    developers: [],
    screenshots: [],
    contributionURL: "",
    averageRating: 0,
    reviewCount: 0,
    reviewURL:
      "https://addons.mozilla.org/en-US/firefox/addon/pioneer-v2-example/reviews/",
    updateDate: "2020-05-27T20:47:35.000Z",
    isDefault: true,
  },
];

const waitForAnimationFrame = () =>
  new Promise(resolve => {
    content.window.requestAnimationFrame(resolve);
  });

add_task(async function testAboutPage() {
  const cachedAddons = JSON.stringify(CACHED_ADDONS);
  await SpecialPowers.pushPrefEnv({
    set: [
      [PREF_CACHED_ADDONS, cachedAddons],
      [PREF_TEST_ADDON_INSTALLED, false],
    ],
    clear: [[PREF_PIONEER_ID, ""]],
  });

  await BrowserTestUtils.withNewTab(
    {
      url: "about:pioneer",
      gBrowser,
    },
    async function taskFn(browser) {
      const beforePref = Services.prefs.getStringPref(PREF_PIONEER_ID, null);
      ok(beforePref === null, "before enrollment, Pioneer pref is null.");

      const beforeToolbarButton = document.getElementById("pioneer-button");
      ok(
        beforeToolbarButton.hidden,
        "before enrollment, Pioneer toolbar button is hidden."
      );

      const enrollmentButton = content.document.getElementById(
        "enrollment-button"
      );
      enrollmentButton.click();

      const dialog = content.document.getElementById("consent-dialog");
      ok(dialog.open, "after clicking enrollment, consent dialog is open.");

      const cancleDialogButton = content.document.getElementById(
        "cancel-dialog-button"
      );
      cancleDialogButton.click();

      ok(
        !dialog.open,
        "after cancelling enrollment, consent dialog is closed."
      );

      const canceledEnrollment = Services.prefs.getStringPref(
        PREF_PIONEER_ID,
        null
      );
      ok(
        !canceledEnrollment,
        "after cancelling enrollment, Pioneer is not enrolled."
      );

      enrollmentButton.click();
      ok(dialog.open, "after retrying enrollment, consent dialog is open.");

      const acceptDialogButton = content.document.getElementById(
        "accept-dialog-button"
      );
      acceptDialogButton.click();

      const pioneerEnrolled = Services.prefs.getStringPref(
        PREF_PIONEER_ID,
        null
      );
      ok(pioneerEnrolled, "after enrollment, Pioneer pref is set.");

      const enrolledToolbarButton = document.getElementById("pioneer-button");
      ok(
        !enrolledToolbarButton.hidden,
        "after enrollment, Pioneer toolbar button is not hidden."
      );

      for (const cachedAddon of CACHED_ADDONS) {
        const addonId = cachedAddon.addon_id;
        const joinButton = content.document.getElementById(
          `${addonId}-join-button`
        );

        if (cachedAddon.isDefault) {
          ok(!joinButton, "There is no join button for default study.");
          continue;
        }

        ok(!joinButton.hidden, "Join button is not hidden.");

        Services.prefs.setBoolPref(PREF_TEST_ADDON_INSTALLED, true);

        joinButton.click();
        await waitForAnimationFrame();

        ok(joinButton.hidden, "Join button is hidden.");
      }

      enrollmentButton.click();

      await waitForAnimationFrame();

      const pioneerUnenrolled = Services.prefs.getStringPref(
        PREF_PIONEER_ID,
        null
      );
      ok(!pioneerUnenrolled, "after unenrollment, Pioneer pref is null.");

      const unenrolledToolbarButton = document.getElementById("pioneer-button");
      ok(
        unenrolledToolbarButton.hidden,
        "after unenrollment, Pioneer toolbar button is hidden."
      );

      for (const cachedAddon of CACHED_ADDONS) {
        const addonId = cachedAddon.addon_id;
        const joinButton = content.document.getElementById(
          `${addonId}-join-button`
        );

        Services.prefs.setBoolPref(PREF_TEST_ADDON_INSTALLED, false);
        if (cachedAddon.isDefault) {
          ok(!joinButton, "There is no join button for default study.");
        } else {
          ok(
            joinButton.disabled,
            "After unenrollment, join button is disabled."
          );
          ok(
            !joinButton.hidden,
            "After unenrollment, join button is not hidden."
          );
        }
      }
    }
  );
});

add_task(async function testPioneerBadge() {
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_PIONEER_NEW_STUDIES_AVAILABLE, true]],
    clear: [
      [PREF_PIONEER_NEW_STUDIES_AVAILABLE, false],
      [PREF_PIONEER_ID, ""],
    ],
  });

  let pioneerTab = await BrowserTestUtils.openNewForegroundTab({
    url: "about:pioneer",
    gBrowser,
  });

  const enrollmentButton = content.document.getElementById("enrollment-button");
  enrollmentButton.click();

  let blankTab = await BrowserTestUtils.openNewForegroundTab({
    url: "about:home",
    gBrowser,
  });

  Services.prefs.setBoolPref(PREF_PIONEER_NEW_STUDIES_AVAILABLE, true);

  const toolbarButton = document.getElementById("pioneer-button");
  const toolbarBadge = toolbarButton.querySelector(".toolbarbutton-badge");

  ok(
    toolbarBadge.classList.contains("feature-callout"),
    "When pref is true, Pioneer toolbar button is called out in the current window."
  );

  toolbarButton.click();

  ok(
    !toolbarBadge.classList.contains("feature-callout"),
    "When about:pioneer toolbar button is pressed, call-out is removed."
  );

  Services.prefs.setBoolPref(PREF_PIONEER_NEW_STUDIES_AVAILABLE, true);

  const newWin = await BrowserTestUtils.openNewBrowserWindow();
  const newToolbarBadge = toolbarButton.querySelector(".toolbarbutton-badge");

  ok(
    newToolbarBadge.classList.contains("feature-callout"),
    "When pref is true, Pioneer toolbar button is called out in a new window."
  );

  await BrowserTestUtils.closeWindow(newWin);
  await BrowserTestUtils.removeTab(pioneerTab);
  await BrowserTestUtils.removeTab(blankTab);
});
