/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "Ajv",
  "resource://testing-common/ajv-4.1.1.js"
);

const PREF_PIONEER_ID = "toolkit.telemetry.pioneerId";
const PREF_PIONEER_NEW_STUDIES_AVAILABLE =
  "toolkit.telemetry.pioneer-new-studies-available";
const PREF_PIONEER_COMPLETED_STUDIES =
  "toolkit.telemetry.pioneer-completed-studies";

const PREF_TEST_CACHED_CONTENT = "toolkit.pioneer.testCachedContent";
const PREF_TEST_CACHED_ADDONS = "toolkit.pioneer.testCachedAddons";
const PREF_TEST_ADDONS = "toolkit.pioneer.testAddons";

const CACHED_CONTENT = [
  {
    title: "test title\ntest title line 2",
    summary: "test summary\ntest summary line 2",
    details: "1. test details\n2. test details line 2\n3. test details line 3",
    joinPioneerConsent: "test join consent\njoin consent line 2",
    leavePioneerConsent: "test leave consent\ntest leave consent line 2",
    privacyPolicy: "http://localhost",
  },
];

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
    name: "Demo Study",
    version: "1.0",
    sourceURI: {
      spec: "https://localhost",
    },
    description: "Study purpose: Testing Pioneer.",
    privacyPolicy: {
      spec: "http://localhost",
    },
    studyType: "extension",
    authors: {
      name: "Pioneer Developers",
      url: "https://addons.mozilla.org/en-US/firefox/user/6510522/",
    },
    dataCollectionDetails: ["test123", "test345"],
    moreInfo: {
      spec: "http://localhost",
    },
    isDefault: false,
    studyEnded: true,
    joinStudyConsent: "test123",
    leaveStudyConsent: "test345",
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
    name: "Demo Default Study",
    version: "1.0",
    sourceURI: {
      spec: "https://localhost",
    },
    description: "Study purpose: Testing Pioneer.",
    privacyPolicy: {
      spec: "http://localhost",
    },
    studyType: "extension",
    authors: {
      name: "Pioneer Developers",
      url: "https://addons.mozilla.org/en-US/firefox/user/6510522/",
    },
    dataCollectionDetails: ["test123", "test345"],
    moreInfo: {
      spec: "http://localhost",
    },
    isDefault: true,
    studyEnded: false,
    joinStudyConsent: "test456",
    leaveStudyConsent: "test789",
  },
  {
    addon_id: "study@partner",
    icons: {
      "32":
        "https://localhost/user-media/addon_icons/2644/2644632-32.png?modified=4a64e2bc",
      "64":
        "https://localhost/user-media/addon_icons/2644/2644632-64.png?modified=4a64e2bc",
      "128":
        "https://localhost/user-media/addon_icons/2644/2644632-128.png?modified=4a64e2bc",
    },
    name: "Example Partner Study",
    version: "1.0",
    sourceURI: {
      spec: "https://localhost",
    },
    description: "Study purpose: Testing Pioneer.",
    privacyPolicy: {
      spec: "http://localhost",
    },
    studyType: "extension",
    authors: {
      name: "Study Partners",
      url: "http://localhost",
    },
    dataCollectionDetails: ["test123", "test345"],
    moreInfo: {
      spec: "http://localhost",
    },
    isDefault: false,
    studyEnded: false,
    joinStudyConsent: "test012",
    leaveStudyConsent: "test345",
  },
  {
    addon_id: "second-study@partner",
    icons: {
      "32":
        "https://localhost/user-media/addon_icons/2644/2644632-32.png?modified=4a64e2bc",
      "64":
        "https://localhost/user-media/addon_icons/2644/2644632-64.png?modified=4a64e2bc",
      "128":
        "https://localhost/user-media/addon_icons/2644/2644632-128.png?modified=4a64e2bc",
    },
    name: "Example Second Partner Study",
    version: "1.0",
    sourceURI: {
      spec: "https://localhost",
    },
    description: "Study purpose: Testing Pioneer.",
    privacyPolicy: {
      spec: "http://localhost",
    },
    studyType: "extension",
    authors: {
      name: "Second Study Partners",
      url: "https://localhost",
    },
    dataCollectionDetails: ["test123", "test345"],
    moreInfo: {
      spec: "http://localhost",
    },
    isDefault: false,
    studyEnded: false,
    joinStudyConsent: "test678",
    leaveStudyConsent: "test901",
  },
];

const TEST_ADDONS = [
  { id: "pioneer-v2-example@pioneer.mozilla.org" },
  { id: "pioneer-v2-default-example@mozilla.org" },
  { id: "study@partner" },
  { id: "second-study@parnter" },
];

const waitForAnimationFrame = () =>
  new Promise(resolve => {
    content.window.requestAnimationFrame(resolve);
  });

add_task(async function testMockSchema() {
  for (const [schemaName, values] of [
    ["PioneerContentSchema", CACHED_CONTENT],
    ["PioneerStudyAddonsSchema", CACHED_ADDONS],
  ]) {
    const response = await fetch(
      `resource://testing-common/${schemaName}.json`
    );

    const schema = await response.json();
    if (!schema) {
      throw new Error(`Failed to load ${schemaName}`);
    }

    const ajv = new Ajv({ allErrors: true });
    const validate = ajv.compile(schema);

    for (const entry of values) {
      const valid = validate(entry);
      if (!valid) {
        throw new Error(JSON.stringify(validate.errors));
      }
    }
  }
});

add_task(async function testAboutPage() {
  const cachedContent = JSON.stringify(CACHED_CONTENT);
  const cachedAddons = JSON.stringify(CACHED_ADDONS);

  await SpecialPowers.pushPrefEnv({
    set: [
      [PREF_TEST_CACHED_ADDONS, cachedAddons],
      [PREF_TEST_CACHED_CONTENT, cachedContent],
      [PREF_TEST_ADDONS, "[]"],
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

      const dialog = content.document.getElementById(
        "join-pioneer-consent-dialog"
      );
      ok(dialog.open, "after clicking enrollment, consent dialog is open.");

      const cancelDialogButton = content.document.getElementById(
        "join-pioneer-cancel-dialog-button"
      );
      cancelDialogButton.click();

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

      // When a modal dialog is cancelled, the inertness for other elements
      // is reverted. However, in order to have the new state (non-inert)
      // effective, Firefox needs to do a frame flush. This flush is taken
      // place when it's really needed.
      // getBoundingClientRect forces a frame flush here to ensure the
      // following click is going to work properly.
      enrollmentButton.getBoundingClientRect();
      enrollmentButton.click();
      ok(dialog.open, "after retrying enrollment, consent dialog is open.");

      const acceptDialogButton = content.document.getElementById(
        "join-pioneer-accept-dialog-button"
      );
      acceptDialogButton.click();

      const pioneerEnrolled = Services.prefs.getStringPref(
        PREF_PIONEER_ID,
        null
      );
      ok(pioneerEnrolled, "after enrollment, Pioneer pref is set.");

      await waitForAnimationFrame();
      ok(
        document.l10n.getAttributes(enrollmentButton).id ==
          "pioneer-unenrollment-button",
        "After Pioneer enrollment, join button is now leave button"
      );

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

        const completedStudies = Services.prefs.getStringPref(
          PREF_PIONEER_COMPLETED_STUDIES,
          "{}"
        );

        const studies = JSON.parse(completedStudies);

        if (cachedAddon.studyEnded || Object.keys(studies).includes(addonId)) {
          ok(
            joinButton.disabled,
            "Join button is disabled, study has already ended."
          );
          continue;
        }

        await waitForAnimationFrame();

        ok(
          !joinButton.disabled,
          "Before study enrollment, join button is enabled."
        );

        joinButton.click();
        await waitForAnimationFrame();

        ok(
          content.document.getElementById("join-study-consent").textContent ==
            cachedAddon.joinStudyConsent,
          "Join consent text matches remote settings data."
        );

        const studyCancelButton = content.document.getElementById(
          "join-study-cancel-dialog-button"
        );

        studyCancelButton.click();

        ok(
          !joinButton.disabled,
          "After canceling study enrollment, join button is enabled."
        );

        // Similar to the getBoundingClientRect call we did for
        // enrollmentButton, we are forcing a frame flush here.
        joinButton.getBoundingClientRect();
        joinButton.click();
        await waitForAnimationFrame();

        const studyAcceptButton = content.document.getElementById(
          "join-study-accept-dialog-button"
        );

        studyAcceptButton.click();
        await waitForAnimationFrame();

        ok(
          document.l10n.getAttributes(joinButton).id == "pioneer-leave-study",
          "After study enrollment, join button is now leave button"
        );

        ok(
          !joinButton.disabled,
          "After study enrollment, leave button is enabled."
        );

        joinButton.click();
        await waitForAnimationFrame();

        const leaveStudyCancelButton = content.document.getElementById(
          "leave-study-cancel-dialog-button"
        );

        leaveStudyCancelButton.click();
        await waitForAnimationFrame();

        ok(
          content.document.getElementById("leave-study-consent").textContent ==
            cachedAddon.leaveStudyConsent,
          "Leave consent text matches remote settings data."
        );

        ok(
          !joinButton.disabled,
          "After canceling study leave, leave/join button is enabled."
        );

        joinButton.click();
        await waitForAnimationFrame();

        const acceptStudyCancelButton = content.document.getElementById(
          "leave-study-accept-dialog-button"
        );

        acceptStudyCancelButton.click();
        await waitForAnimationFrame();

        ok(
          joinButton.disabled,
          "After leaving study, join button is disabled."
        );

        ok(
          Services.prefs.getStringPref(PREF_TEST_ADDONS, null) == "[]",
          "Correct add-on was uninstalled"
        );
      }

      enrollmentButton.click();
      await waitForAnimationFrame();

      const cancelUnenrollmentDialogButton = content.document.getElementById(
        "leave-pioneer-cancel-dialog-button"
      );
      cancelUnenrollmentDialogButton.click();

      const pioneerStillEnrolled = Services.prefs.getStringPref(
        PREF_PIONEER_ID,
        null
      );

      ok(
        pioneerStillEnrolled,
        "after canceling unenrollment, Pioneer pref is still set."
      );

      enrollmentButton.click();
      await waitForAnimationFrame();

      const acceptUnenrollmentDialogButton = content.document.getElementById(
        "leave-pioneer-accept-dialog-button"
      );
      acceptUnenrollmentDialogButton.click();

      const pioneerUnenrolled = Services.prefs.getStringPref(
        PREF_PIONEER_ID,
        null
      );

      ok(
        !pioneerUnenrolled,
        "after accepting unenrollment, Pioneer pref is null."
      );

      const unenrolledToolbarButton = document.getElementById("pioneer-button");
      ok(
        unenrolledToolbarButton.hidden,
        "after unenrollment, Pioneer toolbar button is hidden."
      );

      Services.prefs.setStringPref(PREF_TEST_ADDONS, "[]");
      for (const cachedAddon of CACHED_ADDONS) {
        const addonId = cachedAddon.addon_id;
        const joinButton = content.document.getElementById(
          `${addonId}-join-button`
        );

        if (cachedAddon.isDefault) {
          ok(!joinButton, "There is no join button for default study.");
        } else {
          ok(
            joinButton.disabled,
            "After unenrollment, join button is disabled."
          );

          joinButton.click();
          await waitForAnimationFrame();
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

add_task(async function testAboutPage() {
  const cachedContent = JSON.stringify(CACHED_CONTENT);

  await SpecialPowers.pushPrefEnv({
    set: [
      [PREF_TEST_CACHED_CONTENT, cachedContent],
      [PREF_TEST_ADDONS, "[]"],
    ],
    clear: [[PREF_PIONEER_ID, ""]],
  });

  await BrowserTestUtils.withNewTab(
    {
      url: "about:pioneer",
      gBrowser,
    },
    async function taskFn(browser) {
      // Check that text was updated from Remote Settings.
      ok(
        content.document.getElementById("title").textContent ==
          "test titletest title line 2",
        "Title was replaced correctly."
      );
    }
  );
});
