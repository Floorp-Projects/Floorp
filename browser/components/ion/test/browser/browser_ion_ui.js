/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "Ajv",
  "resource://testing-common/ajv-4.1.1.js"
);

const { TelemetryArchive } = ChromeUtils.import(
  "resource://gre/modules/TelemetryArchive.jsm"
);

const { TelemetryStorage } = ChromeUtils.import(
  "resource://gre/modules/TelemetryStorage.jsm"
);

const ORIG_AVAILABLE_LOCALES = Services.locale.availableLocales;
const ORIG_REQUESTED_LOCALES = Services.locale.requestedLocales;

const PREF_ION_ID = "toolkit.telemetry.pioneerId";
const PREF_ION_NEW_STUDIES_AVAILABLE =
  "toolkit.telemetry.pioneer-new-studies-available";
const PREF_ION_COMPLETED_STUDIES =
  "toolkit.telemetry.pioneer-completed-studies";

const PREF_TEST_CACHED_CONTENT = "toolkit.pioneer.testCachedContent";
const PREF_TEST_CACHED_ADDONS = "toolkit.pioneer.testCachedAddons";
const PREF_TEST_ADDONS = "toolkit.pioneer.testAddons";

const CACHED_CONTENT = [
  {
    title: "test title\ntest title line 2",
    summary: "test summary\ntest summary line 2",
    details: "1. test details\n2. test details line 2\n3. test details line 3",
    joinIonConsent: "test join consent\njoin consent line 2",
    leaveIonConsent: "test leave consent\ntest leave consent line 2",
    privacyPolicy: "http://localhost",
  },
];

const CACHED_ADDONS = [
  {
    addon_id: "ion-v2-example@mozilla.org",
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
    description: "Study purpose: Testing Ion.",
    privacyPolicy: {
      spec: "http://localhost",
    },
    studyType: "extension",
    authors: {
      name: "Ion Developers",
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
    addon_id: "ion-v2-default-example@mozilla.org",
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
    description: "Study purpose: Testing Ion.",
    privacyPolicy: {
      spec: "http://localhost",
    },
    studyType: "extension",
    authors: {
      name: "Ion Developers",
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
    description: "Study purpose: Testing Ion.",
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
    description: "Study purpose: Testing Ion.",
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

const CACHED_ADDONS_BAD_DEFAULT = [
  {
    addon_id: "ion-v2-bad-default-example@mozilla.org",
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
    description: "Study purpose: Testing Ion.",
    privacyPolicy: {
      spec: "http://localhost",
    },
    studyType: "extension",
    authors: {
      name: "Ion Developers",
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
    description: "Study purpose: Testing Ion.",
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
    description: "Study purpose: Testing Ion.",
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
  { id: "ion-v2-example@ion.mozilla.org" },
  { id: "ion-v2-default-example@mozilla.org" },
  { id: "study@partner" },
  { id: "second-study@parnter" },
];

const setupLocale = async locale => {
  Services.locale.availableLocales = [locale];
  Services.locale.requestedLocales = [locale];
};

const clearLocale = async () => {
  Services.locale.availableLocales = ORIG_AVAILABLE_LOCALES;
  Services.locale.requestedLocales = ORIG_REQUESTED_LOCALES;
};

add_task(async function testMockSchema() {
  for (const [schemaName, values] of [
    ["IonContentSchema", CACHED_CONTENT],
    ["IonStudyAddonsSchema", CACHED_ADDONS],
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

add_task(async function testBadDefaultAddon() {
  const cachedContent = JSON.stringify(CACHED_CONTENT);
  const cachedAddons = JSON.stringify(CACHED_ADDONS_BAD_DEFAULT);

  await SpecialPowers.pushPrefEnv({
    set: [
      [PREF_TEST_CACHED_ADDONS, cachedAddons],
      [PREF_TEST_CACHED_CONTENT, cachedContent],
      [PREF_TEST_ADDONS, "[]"],
    ],
    clear: [
      [PREF_ION_ID, ""],
      [PREF_ION_COMPLETED_STUDIES, "[]"],
    ],
  });

  await BrowserTestUtils.withNewTab(
    {
      url: "about:ion",
      gBrowser,
    },
    async function taskFn(browser) {
      const beforePref = Services.prefs.getStringPref(PREF_ION_ID, null);
      ok(beforePref === null, "before enrollment, Ion pref is null.");
      const enrollmentButton = content.document.getElementById(
        "enrollment-button"
      );
      enrollmentButton.click();

      const dialog = content.document.getElementById("join-ion-consent-dialog");
      ok(dialog.open, "after clicking enrollment, consent dialog is open.");

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
        "join-ion-accept-dialog-button"
      );
      // Wait for the enrollment button to change its label to "leave", meaning
      // that the policy was accepted.
      let promiseDialogAccepted = BrowserTestUtils.waitForAttribute(
        "data-l10n-id",
        enrollmentButton
      );
      acceptDialogButton.click();

      const ionEnrolled = Services.prefs.getStringPref(PREF_ION_ID, null);
      ok(ionEnrolled, "after enrollment, Ion pref is set.");

      await promiseDialogAccepted;
      ok(
        document.l10n.getAttributes(enrollmentButton).id ==
          "ion-unenrollment-button",
        "After Ion enrollment, join button is now leave button"
      );

      const availableStudies = content.document.getElementById(
        "available-studies"
      );
      ok(
        document.l10n.getAttributes(availableStudies).id ==
          "ion-no-current-studies",
        "No studies are available if default add-on install fails."
      );
    }
  );
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
    clear: [
      [PREF_ION_ID, ""],
      [PREF_ION_COMPLETED_STUDIES, "[]"],
    ],
  });

  await BrowserTestUtils.withNewTab(
    {
      url: "about:ion",
      gBrowser,
    },
    async function taskFn(browser) {
      const beforePref = Services.prefs.getStringPref(PREF_ION_ID, null);
      ok(beforePref === null, "before enrollment, Ion pref is null.");

      const beforeToolbarButton = document.getElementById("ion-button");
      ok(
        beforeToolbarButton.hidden,
        "before enrollment, Ion toolbar button is hidden."
      );

      const enrollmentButton = content.document.getElementById(
        "enrollment-button"
      );
      enrollmentButton.click();

      const dialog = content.document.getElementById("join-ion-consent-dialog");
      ok(dialog.open, "after clicking enrollment, consent dialog is open.");

      const cancelDialogButton = content.document.getElementById(
        "join-ion-cancel-dialog-button"
      );
      cancelDialogButton.click();

      ok(
        !dialog.open,
        "after cancelling enrollment, consent dialog is closed."
      );

      const canceledEnrollment = Services.prefs.getStringPref(
        PREF_ION_ID,
        null
      );

      ok(
        !canceledEnrollment,
        "after cancelling enrollment, Ion is not enrolled."
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
        "join-ion-accept-dialog-button"
      );
      // Wait for the enrollment button to change its label to "leave", meaning
      // that the policy was accepted.
      let promiseDialogAccepted = BrowserTestUtils.waitForAttribute(
        "data-l10n-id",
        enrollmentButton
      );
      acceptDialogButton.click();

      const ionEnrolled = Services.prefs.getStringPref(PREF_ION_ID, null);
      ok(ionEnrolled, "after enrollment, Ion pref is set.");

      await promiseDialogAccepted;
      ok(
        document.l10n.getAttributes(enrollmentButton).id ==
          "ion-unenrollment-button",
        "After Ion enrollment, join button is now leave button"
      );

      const enrolledToolbarButton = document.getElementById("ion-button");
      ok(
        !enrolledToolbarButton.hidden,
        "after enrollment, Ion toolbar button is not hidden."
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
          PREF_ION_COMPLETED_STUDIES,
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

        ok(
          !joinButton.disabled,
          "Before study enrollment, join button is enabled."
        );

        const studyCancelButton = content.document.getElementById(
          "join-study-cancel-dialog-button"
        );

        const joinDialogOpen = new Promise(resolve => {
          content.document
            .getElementById("join-study-consent-dialog")
            .addEventListener("open", () => {
              // Run resolve() on the next tick.
              setTimeout(() => resolve(), 0);
            });
        });

        // When a modal dialog is cancelled, the inertness for other elements
        // is reverted. However, in order to have the new state (non-inert)
        // effective, Firefox needs to do a frame flush. This flush is taken
        // place when it's really needed.
        // getBoundingClientRect forces a frame flush here to ensure the
        // following click is going to work properly.
        //
        // Note: this initial call is required because we're cycling through
        // addons. So while in the first iteration this would work, it could
        // fail on the second or third.
        joinButton.getBoundingClientRect();
        joinButton.click();

        await joinDialogOpen;

        ok(
          content.document.getElementById("join-study-consent").textContent ==
            cachedAddon.joinStudyConsent,
          "Join consent text matches remote settings data."
        );

        studyCancelButton.click();

        ok(
          !joinButton.disabled,
          "After canceling study enrollment, join button is enabled."
        );

        // When a modal dialog is cancelled, the inertness for other elements
        // is reverted. However, in order to have the new state (non-inert)
        // effective, Firefox needs to do a frame flush. This flush is taken
        // place when it's really needed.
        // getBoundingClientRect forces a frame flush here to ensure the
        // following click is going to work properly.
        joinButton.getBoundingClientRect();
        joinButton.click();

        const studyAcceptButton = content.document.getElementById(
          "join-study-accept-dialog-button"
        );

        // Wait for the "Join Button" to change to a "leave button".
        let promiseJoinTurnsToLeave = BrowserTestUtils.waitForAttribute(
          "data-l10n-id",
          joinButton
        );
        studyAcceptButton.click();
        await promiseJoinTurnsToLeave;

        ok(
          document.l10n.getAttributes(joinButton).id == "ion-leave-study",
          "After study enrollment, join button is now leave button"
        );

        ok(
          !joinButton.disabled,
          "After study enrollment, leave button is enabled."
        );

        const leaveStudyCancelButton = content.document.getElementById(
          "leave-study-cancel-dialog-button"
        );

        const leaveDialogOpen = new Promise(resolve => {
          content.document
            .getElementById("leave-study-consent-dialog")
            .addEventListener("open", () => {
              // Run resolve() on the next tick.
              setTimeout(() => resolve(), 0);
            });
        });

        // When a modal dialog is cancelled, the inertness for other elements
        // is reverted. However, in order to have the new state (non-inert)
        // effective, Firefox needs to do a frame flush. This flush is taken
        // place when it's really needed.
        // getBoundingClientRect forces a frame flush here to ensure the
        // following click is going to work properly.
        joinButton.getBoundingClientRect();
        joinButton.click();

        await leaveDialogOpen;

        ok(
          content.document.getElementById("leave-study-consent").textContent ==
            cachedAddon.leaveStudyConsent,
          "Leave consent text matches remote settings data."
        );

        leaveStudyCancelButton.click();

        ok(
          !joinButton.disabled,
          "After canceling study leave, leave/join button is enabled."
        );

        // When a modal dialog is cancelled, the inertness for other elements
        // is reverted. However, in order to have the new state (non-inert)
        // effective, Firefox needs to do a frame flush. This flush is taken
        // place when it's really needed.
        // getBoundingClientRect forces a frame flush here to ensure the
        // following click is going to work properly.
        joinButton.getBoundingClientRect();
        joinButton.click();

        const acceptStudyCancelButton = content.document.getElementById(
          "leave-study-accept-dialog-button"
        );

        let promiseJoinButtonDisabled = BrowserTestUtils.waitForAttribute(
          "disabled",
          joinButton
        );
        acceptStudyCancelButton.click();
        await promiseJoinButtonDisabled;

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

      const cancelUnenrollmentDialogButton = content.document.getElementById(
        "leave-ion-cancel-dialog-button"
      );
      cancelUnenrollmentDialogButton.click();

      const ionStillEnrolled = Services.prefs.getStringPref(PREF_ION_ID, null);

      ok(
        ionStillEnrolled,
        "after canceling unenrollment, Ion pref is still set."
      );

      enrollmentButton.click();

      const acceptUnenrollmentDialogButton = content.document.getElementById(
        "leave-ion-accept-dialog-button"
      );

      acceptUnenrollmentDialogButton.click();

      // Wait for deletion ping, uninstalls, and UI updates...
      const ionUnenrolled = await new Promise((resolve, reject) => {
        Services.prefs.addObserver(PREF_ION_ID, function observer(
          subject,
          topic,
          data
        ) {
          try {
            const prefValue = Services.prefs.getStringPref(PREF_ION_ID, null);
            Services.prefs.removeObserver(PREF_ION_ID, observer);
            resolve(prefValue);
          } catch (ex) {
            Services.prefs.removeObserver(PREF_ION_ID, observer);
            reject(ex);
          }
        });
      });

      ok(!ionUnenrolled, "after accepting unenrollment, Ion pref is null.");

      const unenrolledToolbarButton = document.getElementById("ion-button");
      ok(
        unenrolledToolbarButton.hidden,
        "after unenrollment, Ion toolbar button is hidden."
      );

      await TelemetryStorage.testClearPendingPings();
      let pings = await TelemetryArchive.promiseArchivedPingList();

      let pingDetails = [];
      for (const ping of pings) {
        ok(
          ping.type == "pioneer-study",
          "ping is of expected type pioneer-study"
        );
        const details = await TelemetryArchive.promiseArchivedPingById(ping.id);
        pingDetails.push(details.payload.studyName);
      }

      Services.prefs.setStringPref(PREF_TEST_ADDONS, "[]");

      for (const cachedAddon of CACHED_ADDONS) {
        const addonId = cachedAddon.addon_id;

        ok(
          pingDetails.includes(addonId),
          "each test add-on has sent a deletion ping"
        );

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
        }
      }
    }
  );
});

add_task(async function testEnrollmentPings() {
  const CACHED_TEST_ADDON = CACHED_ADDONS[2];
  const cachedContent = JSON.stringify(CACHED_CONTENT);
  const cachedAddons = JSON.stringify([CACHED_TEST_ADDON]);

  await SpecialPowers.pushPrefEnv({
    set: [
      [PREF_TEST_CACHED_ADDONS, cachedAddons],
      [PREF_TEST_CACHED_CONTENT, cachedContent],
      [PREF_TEST_ADDONS, "[]"],
    ],
    clear: [
      [PREF_ION_ID, ""],
      [PREF_ION_COMPLETED_STUDIES, "[]"],
    ],
  });

  // Clear any pending pings.
  await TelemetryStorage.testClearPendingPings();

  await BrowserTestUtils.withNewTab(
    {
      url: "about:ion",
      gBrowser,
    },
    async function taskFn(browser) {
      const beforePref = Services.prefs.getStringPref(PREF_ION_ID, null);
      ok(beforePref === null, "before enrollment, Ion pref is null.");

      // Enroll in ion.
      const enrollmentButton = content.document.getElementById(
        "enrollment-button"
      );

      let promiseDialogAccepted = BrowserTestUtils.waitForAttribute(
        "data-l10n-id",
        enrollmentButton
      );

      // When a modal dialog is cancelled, the inertness for other elements
      // is reverted. However, in order to have the new state (non-inert)
      // effective, Firefox needs to do a frame flush. This flush is taken
      // place when it's really needed.
      // getBoundingClientRect forces a frame flush here to ensure the
      // following click is going to work properly.
      enrollmentButton.getBoundingClientRect();
      enrollmentButton.click();

      const acceptDialogButton = content.document.getElementById(
        "join-ion-accept-dialog-button"
      );
      acceptDialogButton.click();

      const ionId = Services.prefs.getStringPref(PREF_ION_ID, null);
      ok(ionId, "after enrollment, Ion pref is set.");

      await promiseDialogAccepted;

      // Enroll in the CACHED_TEST_ADDON study.
      const joinButton = content.document.getElementById(
        `${CACHED_TEST_ADDON.addon_id}-join-button`
      );

      const joinDialogOpen = new Promise(resolve => {
        content.document
          .getElementById("join-study-consent-dialog")
          .addEventListener("open", () => {
            resolve();
          });
      });

      joinButton.click();

      await joinDialogOpen;

      // Accept consent for the study.
      const studyAcceptButton = content.document.getElementById(
        "join-study-accept-dialog-button"
      );

      studyAcceptButton.click();

      // Verify that the proper pings were generated.
      let pings;
      await TestUtils.waitForCondition(async function() {
        pings = await TelemetryArchive.promiseArchivedPingList();
        return pings.length >= 2;
      }, "Wait until we have at least 2 pings in the telemetry archive");

      let pingDetails = [];
      for (const ping of pings) {
        ok(
          ping.type == "pioneer-study",
          "ping is of expected type pioneer-study"
        );
        const details = await TelemetryArchive.promiseArchivedPingById(ping.id);
        pingDetails.push({
          schemaName: details.payload.schemaName,
          schemaNamespace: details.payload.schemaNamespace,
          studyName: details.payload.studyName,
          pioneerId: details.payload.pioneerId,
        });
      }

      // We expect 1 ping with just the ion id (ion consent) and another
      // with both the ion id and the study id (study consent).
      ok(
        pingDetails.find(
          p =>
            p.schemaName == "pioneer-enrollment" &&
            p.schemaNamespace == "pioneer-meta" &&
            p.pioneerId == ionId &&
            p.studyName == "pioneer-meta"
        ),
        "We expect the Ion program consent to be present"
      );

      ok(
        pingDetails.find(
          p =>
            p.schemaName == "pioneer-enrollment" &&
            p.schemaNamespace == CACHED_TEST_ADDON.addon_id &&
            p.pioneerId == ionId &&
            p.studyName == CACHED_TEST_ADDON.addon_id
        ),
        "We expect the study consent to be present"
      );
    }
  );
});

add_task(async function testIonBadge() {
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_ION_NEW_STUDIES_AVAILABLE, true]],
    clear: [
      [PREF_ION_NEW_STUDIES_AVAILABLE, false],
      [PREF_ION_ID, ""],
    ],
  });

  let ionTab = await BrowserTestUtils.openNewForegroundTab({
    url: "about:ion",
    gBrowser,
  });

  const enrollmentButton = content.document.getElementById("enrollment-button");
  enrollmentButton.click();

  let blankTab = await BrowserTestUtils.openNewForegroundTab({
    url: "about:home",
    gBrowser,
  });

  Services.prefs.setBoolPref(PREF_ION_NEW_STUDIES_AVAILABLE, true);

  const toolbarButton = document.getElementById("ion-button");
  const toolbarBadge = toolbarButton.querySelector(".toolbarbutton-badge");

  ok(
    toolbarBadge.classList.contains("feature-callout"),
    "When pref is true, Ion toolbar button is called out in the current window."
  );

  toolbarButton.click();

  await ionTab;

  ok(
    !toolbarBadge.classList.contains("feature-callout"),
    "When about:ion toolbar button is pressed, call-out is removed."
  );

  Services.prefs.setBoolPref(PREF_ION_NEW_STUDIES_AVAILABLE, true);

  const newWin = await BrowserTestUtils.openNewBrowserWindow();
  const newToolbarBadge = toolbarButton.querySelector(".toolbarbutton-badge");

  ok(
    newToolbarBadge.classList.contains("feature-callout"),
    "When pref is true, Ion toolbar button is called out in a new window."
  );

  await BrowserTestUtils.closeWindow(newWin);
  await BrowserTestUtils.removeTab(ionTab);
  await BrowserTestUtils.removeTab(blankTab);
});

add_task(async function testContentReplacement() {
  const cachedContent = JSON.stringify(CACHED_CONTENT);

  await SpecialPowers.pushPrefEnv({
    set: [
      [PREF_TEST_CACHED_CONTENT, cachedContent],
      [PREF_TEST_ADDONS, "[]"],
    ],
    clear: [[PREF_ION_ID, ""]],
  });

  await BrowserTestUtils.withNewTab(
    {
      url: "about:ion",
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

add_task(async function testLocaleGating() {
  const cachedContent = JSON.stringify(CACHED_CONTENT);
  const cachedAddons = JSON.stringify(CACHED_ADDONS);

  await SpecialPowers.pushPrefEnv({
    set: [
      [PREF_TEST_CACHED_ADDONS, cachedAddons],
      [PREF_TEST_CACHED_CONTENT, cachedContent],
      [PREF_TEST_ADDONS, "[]"],
    ],
    clear: [
      [PREF_ION_ID, ""],
      [PREF_ION_COMPLETED_STUDIES, "[]"],
    ],
  });

  await setupLocale("de");

  await BrowserTestUtils.withNewTab(
    {
      url: "about:ion",
      gBrowser,
    },
    async function taskFn(browser) {
      const localeNotificationBar = content.document.getElementById(
        "locale-notification"
      );

      is(
        Services.locale.requestedLocales[0],
        "de",
        "The requestedLocales has been set to German ('de')."
      );

      is(
        getComputedStyle(localeNotificationBar).display,
        "block",
        "Because the page locale is German, the notification bar is not hidden."
      );
    }
  );

  await clearLocale();

  await BrowserTestUtils.withNewTab(
    {
      url: "about:ion",
      gBrowser,
    },
    async function taskFn(browser) {
      const localeNotificationBar = content.document.getElementById(
        "locale-notification"
      );

      is(
        Services.locale.requestedLocales[0],
        "en-US",
        "The requestedLocales has been set to English ('en-US')."
      );

      is(
        getComputedStyle(localeNotificationBar).display,
        "none",
        "Because the page locale is en-US, the notification bar is hidden."
      );
    }
  );
});
