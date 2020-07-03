/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PREF_PIONEER_ID = "toolkit.telemetry.pioneerId";

const PREF_CACHED_ADDONS = "toolkit.pioneer.testCachedAddons";
const PREF_TEST_ADDON_INSTALLED = "toolkit.pioneer.testAddonInstalled";

const CACHED_ADDONS = [
  {
    addon_id: "pioneer-v2-example@pioneer.mozilla.org",
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
];

add_task(async function() {
  const cachedAddons = JSON.stringify(CACHED_ADDONS);
  await SpecialPowers.pushPrefEnv({
    set: [
      [PREF_CACHED_ADDONS, cachedAddons],
      [PREF_TEST_ADDON_INSTALLED, false],
    ],
    clear: [[PREF_PIONEER_ID, ""]],
  });

  let tab = await BrowserTestUtils.openNewForegroundTab({
    url: "about:pioneer",
    gBrowser,
  });

  const waitForAnimationFrame = () =>
    new Promise(resolve => {
      content.window.requestAnimationFrame(resolve);
    });

  const beforePref = Services.prefs.getStringPref(PREF_PIONEER_ID, null);
  ok(beforePref === null, "before enrollment, Pioneer pref is null.");

  const beforeToolbarButton = document.getElementById("pioneer-button");
  ok(
    beforeToolbarButton.hidden,
    "before enrollment, Pioneer toolbar button is hidden."
  );

  const enrollmentButton = content.document.getElementById("enrollment-button");
  enrollmentButton.click();

  await waitForAnimationFrame();

  const pioneerEnrolled = Services.prefs.getStringPref(PREF_PIONEER_ID, null);
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

    ok(!joinButton.hidden, "Join button is not hidden.");

    Services.prefs.setBoolPref(PREF_TEST_ADDON_INSTALLED, true);

    joinButton.click();
    await waitForAnimationFrame();

    ok(joinButton.hidden, "Join button is hidden.");
  }

  enrollmentButton.click();

  await waitForAnimationFrame();

  const pioneerUnenrolled = Services.prefs.getStringPref(PREF_PIONEER_ID, null);
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
    ok(joinButton.disabled, "After unenrollment, join button is disabled.");
    ok(!joinButton.hidden, "After unenrollment, join button is not hidden.");
  }

  await BrowserTestUtils.removeTab(tab);
});
