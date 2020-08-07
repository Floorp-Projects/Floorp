/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * MVP UI for Pioneer v2
 *
 * This is an experimental MVP for the Pioneer v2 project. It supports a
 * single study add-on, and the only entry point is the `about:pioneer` page.
 *
 * The purpose of this page is to demonstrate a fully-working Pioneer study
 * data collection pipeline, and also to potentially advertise to real users
 * in order to observe enrollment behavior.
 *
 * Results from the above will inform the next version of Pioneer UI.
 */

const { AddonManager } = ChromeUtils.import(
  "resource://gre/modules/AddonManager.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const { RemoteSettings } = ChromeUtils.import(
  "resource://services-settings/remote-settings.js"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  FileUtils: "resource://gre/modules/FileUtils.jsm",
});

const PREF_PIONEER_ID = "toolkit.telemetry.pioneerId";
const PREF_PIONEER_NEW_STUDIES_AVAILABLE =
  "toolkit.telemetry.pioneer-new-studies-available";
const PREF_PIONEER_COMPLETED_STUDIES =
  "toolkit.telemetry.pioneer-completed-studies";

/**
 * This is the Remote Settings key that we use to get the list of available studies.
 */
const STUDY_ADDON_COLLECTION_KEY = "pioneer-study-addons-v1";

const STUDY_LEAVE_REASONS = {
  USER_ABANDONED: "user-abandoned",
  STUDY_ENDED: "study-ended",
};

const PREF_TEST_CACHED_ADDONS = "toolkit.pioneer.testCachedAddons";
const PREF_TEST_ADDONS = "toolkit.pioneer.testAddons";

function showEnrollmentStatus() {
  const pioneerId = Services.prefs.getStringPref(PREF_PIONEER_ID, null);

  const enrollmentButton = document.getElementById("enrollment-button");

  document.l10n.setAttributes(
    enrollmentButton,
    `pioneer-${pioneerId ? "un" : ""}enrollment-button`
  );
  enrollmentButton.classList.toggle("primary", !pioneerId);
}

async function showAvailableStudies(cachedAddons) {
  for (const cachedAddon of cachedAddons) {
    if (!cachedAddon) {
      console.error(
        `about:pioneer - Study addon ID not found in cache: ${studyAddonId}`
      );
      return;
    }

    if (cachedAddon.isDefault) {
      continue;
    }

    const studyAddonId = cachedAddon.addon_id;

    const study = document.createElement("div");
    study.setAttribute("id", studyAddonId);
    study.setAttribute("class", "card card-no-hover");

    if (cachedAddon.icons && 32 in cachedAddon.icons) {
      const iconName = document.createElement("img");
      iconName.setAttribute("class", "card-icon");
      iconName.setAttribute("src", cachedAddon.icons[32]);
      study.appendChild(iconName);
    }

    const studyBody = document.createElement("div");
    studyBody.classList.add("card-body");
    study.appendChild(studyBody);

    const studyName = document.createElement("h3");
    studyName.setAttribute("class", "card-name");
    studyName.textContent = cachedAddon.name;
    studyBody.appendChild(studyName);

    const studyAuthor = document.createElement("span");
    studyAuthor.setAttribute("class", "card-author");
    studyAuthor.textContent = cachedAddon.authors.name;
    studyBody.appendChild(studyAuthor);

    const actions = document.createElement("div");
    actions.classList.add("card-actions");
    study.appendChild(actions);

    const joinBtn = document.createElement("button");
    joinBtn.setAttribute("id", `${studyAddonId}-join-button`);
    joinBtn.classList.add("primary");
    joinBtn.classList.add("join-button");
    document.l10n.setAttributes(joinBtn, "pioneer-join-study");
    actions.appendChild(joinBtn);

    const studyDesc = document.createElement("div");
    studyDesc.setAttribute("class", "card-description");
    study.appendChild(studyDesc);

    const shortDesc = document.createElement("p");
    shortDesc.textContent = cachedAddon.description;
    studyDesc.appendChild(shortDesc);

    const privacyPolicyLink = document.createElement("a");
    privacyPolicyLink.href = cachedAddon.privacyPolicy.spec;
    privacyPolicyLink.textContent = "privacy policy";

    const privacyPolicy = document.createElement("p");
    const privacyPolicyStart = document.createElement("span");
    privacyPolicyStart.textContent = "You can always find the ";
    privacyPolicy.append(privacyPolicyStart);
    privacyPolicy.append(privacyPolicyLink);
    const privacyPolicyEnd = document.createElement("span");
    privacyPolicyEnd.textContent = " at our website.";
    privacyPolicy.append(privacyPolicyEnd);
    studyDesc.appendChild(privacyPolicy);

    const studyDataCollected = document.createElement("div");
    studyDataCollected.setAttribute("class", "card-data-collected");
    study.appendChild(studyDataCollected);

    const dataCollectionDetailsHeader = document.createElement("p");
    dataCollectionDetailsHeader.textContent = "This study will collect:";
    studyDataCollected.appendChild(dataCollectionDetailsHeader);

    const dataCollectionDetails = document.createElement("ul");
    for (const dataCollectionDetail of cachedAddon.dataCollectionDetails) {
      const detailsBullet = document.createElement("li");
      detailsBullet.textContent = dataCollectionDetail;
      dataCollectionDetails.append(detailsBullet);
    }
    studyDataCollected.appendChild(dataCollectionDetails);

    async function toggleEnrolled() {
      let addon;
      let install;

      if (Cu.isInAutomation) {
        let testAddons = Services.prefs.getStringPref(PREF_TEST_ADDONS, null);
        testAddons = JSON.parse(testAddons);
        install = {
          install: () => {},
        };
        for (const testAddon of testAddons) {
          if (testAddon.id == studyAddonId) {
            addon = testAddon;
            addon.install = () => {};
            addon.uninstall = () => {
              Services.prefs.setStringPref(PREF_TEST_ADDONS, "[]");
            };
          }
        }
      } else {
        addon = await AddonManager.getAddonByID(studyAddonId);
        install = await AddonManager.getInstallForURL(
          cachedAddon.sourceURI.spec
        );
      }

      const completedStudies = Services.prefs.getStringPref(
        PREF_PIONEER_COMPLETED_STUDIES,
        "{}"
      );

      if (addon) {
        joinBtn.disabled = true;
        await addon.uninstall();
        document.l10n.setAttributes(joinBtn, "pioneer-join-study");
        joinBtn.disabled = false;

        // Record that the user abandoned this study, since it may not be re-join-able.
        if (completedStudies) {
          const studies = JSON.parse(completedStudies);
          studies[studyAddonId] = STUDY_LEAVE_REASONS.USER_ABANDONED;
          Services.prefs.setStringPref(
            PREF_PIONEER_COMPLETED_STUDIES,
            JSON.stringify(studies)
          );
        }
      } else {
        // Check if this study is re-join-able before enrollment.
        const studies = JSON.parse(completedStudies);
        if (studyAddonId in studies) {
          if (
            "canRejoin" in cachedAddons[studyAddonId] &&
            cachedAddons[studyAddonId].canRejoin === false
          ) {
            console.error(
              `Cannot rejoin ended study ${studyAddonId}, reason: ${studies[studyAddonId]}`
            );
            return;
          }
        }
        joinBtn.disabled = true;
        await install.install();
        document.l10n.setAttributes(joinBtn, "pioneer-leave-study");
        joinBtn.disabled = false;
      }
      await updateStudy(studyAddonId);
    }

    joinBtn.addEventListener("click", toggleEnrolled);

    const availableStudies = document.getElementById("available-studies");
    availableStudies.appendChild(study);

    await updateStudy(studyAddonId);
  }

  const availableStudies = document.getElementById("header-available-studies");
  document.l10n.setAttributes(availableStudies, "pioneer-available-studies");
}

async function updateStudy(studyAddonId) {
  let addon;
  if (Cu.isInAutomation) {
    if (Services.prefs.getBoolPref(PREF_TEST_ADDONS, false)) {
      addon = { uninstall() {} };
    }
  } else {
    addon = await AddonManager.getAddonByID(studyAddonId);
  }

  const study = document.querySelector(`.card[id="${studyAddonId}"`);

  const joinBtn = study.querySelector(".join-button");

  const pioneerId = Services.prefs.getStringPref(PREF_PIONEER_ID, null);

  if (pioneerId) {
    const completedStudies = Services.prefs.getStringPref(
      PREF_PIONEER_COMPLETED_STUDIES,
      "{}"
    );

    const studies = JSON.parse(completedStudies);
    if (studyAddonId in studies) {
      joinBtn.disabled = true;
      document.l10n.setAttributes(joinBtn, "pioneer-ended-study");
    } else {
      study.style.opacity = 1;
      joinBtn.disabled = false;

      if (addon) {
        document.l10n.setAttributes(joinBtn, "pioneer-leave-study");
      } else {
        document.l10n.setAttributes(joinBtn, "pioneer-join-study");
      }
    }
  } else {
    document.l10n.setAttributes(joinBtn, "pioneer-join-study");
    study.style.opacity = 0.5;
    joinBtn.disabled = true;
  }
}

// equivalent to what we use for Telemetry IDs
// https://searchfox.org/mozilla-central/rev/9193635dca8cfdcb68f114306194ffc860456044/toolkit/components/telemetry/app/TelemetryUtils.jsm#222
function generateUUID() {
  let str = Cc["@mozilla.org/uuid-generator;1"]
    .getService(Ci.nsIUUIDGenerator)
    .generateUUID()
    .toString();
  return str.substring(1, str.length - 1);
}

async function setup(cachedAddons) {
  document
    .getElementById("enrollment-button")
    .addEventListener("click", async () => {
      const pioneerId = Services.prefs.getStringPref(PREF_PIONEER_ID, null);

      if (pioneerId) {
        Services.prefs.clearUserPref(PREF_PIONEER_ID);
        Services.prefs.clearUserPref(PREF_PIONEER_COMPLETED_STUDIES);
        for (const cachedAddon of cachedAddons) {
          const addon = await AddonManager.getAddonByID(cachedAddon.addon_id);
          if (addon) {
            await addon.uninstall();
          }

          const study = document.getElementById(cachedAddon.addon_id);
          if (study) {
            await updateStudy(cachedAddon.addon_id);
          }
        }
        showEnrollmentStatus();
      } else {
        let dialog = document.querySelector("dialog");
        dialog.showModal();
        dialog.scrollTop = 0;
      }
    });

  document
    .getElementById("cancel-dialog-button")
    .addEventListener("click", () => document.querySelector("dialog").close());

  document
    .getElementById("accept-dialog-button")
    .addEventListener("click", async event => {
      const pioneerId = Services.prefs.getStringPref(PREF_PIONEER_ID, null);

      if (!pioneerId) {
        let uuid = generateUUID();
        Services.prefs.setStringPref(PREF_PIONEER_ID, uuid);
        for (const cachedAddon of cachedAddons) {
          if (cachedAddon.isDefault) {
            const install = await AddonManager.getInstallForURL(
              cachedAddon.sourceURI.spec
            );
            install.install();
          }
          const study = document.getElementById(cachedAddon.addon_id);
          if (study) {
            await updateStudy(cachedAddon.addon_id);
          }
        }
        document.querySelector("dialog").close();
      }
      showEnrollmentStatus();
    });

  const onAddonEvent = async addon => {
    for (const cachedAddon in cachedAddons) {
      if (cachedAddon.addon_id == addon.id) {
        await updateStudy(addon.id);
      }
    }
  };
  const addonsListener = {
    onEnabled: onAddonEvent,
    onDisabled: onAddonEvent,
    onInstalled: onAddonEvent,
    onUninstalled: onAddonEvent,
  };
  AddonManager.addAddonListener(addonsListener);

  window.addEventListener("unload", event => {
    AddonManager.removeAddonListener(addonsListener);
  });
}

function removeBadge() {
  Services.prefs.setBoolPref(PREF_PIONEER_NEW_STUDIES_AVAILABLE, false);

  for (let win of Services.wm.getEnumerator("navigator:browser")) {
    const badge = win.document
      .getElementById("pioneer-button")
      .querySelector(".toolbarbutton-badge");
    badge.classList.remove("feature-callout");
  }
}

document.addEventListener("DOMContentLoaded", async domEvent => {
  showEnrollmentStatus();

  document.addEventListener("focus", removeBadge);
  removeBadge();

  let cachedAddons;
  if (Cu.isInAutomation) {
    let testCachedAddons = Services.prefs.getStringPref(
      PREF_TEST_CACHED_ADDONS,
      null
    );
    if (testCachedAddons) {
      cachedAddons = JSON.parse(testCachedAddons);
    }
  } else {
    cachedAddons = await RemoteSettings(STUDY_ADDON_COLLECTION_KEY).get();
  }

  for (const cachedAddon of cachedAddons) {
    // Record any studies that have been marked as concluded on the server.
    if ("studyEnded" in cachedAddon && cachedAddon.studyEnded === true) {
      const completedStudies = Services.prefs.getStringPref(
        PREF_PIONEER_COMPLETED_STUDIES,
        "{}"
      );
      const studies = JSON.parse(completedStudies);
      studies[cachedAddon.addon_id] = STUDY_LEAVE_REASONS.STUDY_ENDED;

      Services.prefs.setStringPref(
        PREF_PIONEER_COMPLETED_STUDIES,
        JSON.stringify(studies)
      );
    }
  }

  await setup(cachedAddons);
  await showAvailableStudies(cachedAddons);
});
