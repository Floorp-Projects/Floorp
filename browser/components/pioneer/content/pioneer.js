/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { AddonManager } = ChromeUtils.import(
  "resource://gre/modules/AddonManager.jsm"
);

const { RemoteSettings } = ChromeUtils.import(
  "resource://services-settings/remote-settings.js"
);

const { TelemetryController } = ChromeUtils.import(
  "resource://gre/modules/TelemetryController.jsm"
);

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const PREF_PIONEER_ID = "toolkit.telemetry.pioneerId";
const PREF_PIONEER_NEW_STUDIES_AVAILABLE =
  "toolkit.telemetry.pioneer-new-studies-available";
const PREF_PIONEER_COMPLETED_STUDIES =
  "toolkit.telemetry.pioneer-completed-studies";

/**
 * Remote Settings keys for general content, and available studies.
 */
const CONTENT_COLLECTION_KEY = "pioneer-content-v1";
const STUDY_ADDON_COLLECTION_KEY = "pioneer-study-addons-v1";

const STUDY_LEAVE_REASONS = {
  USER_ABANDONED: "user-abandoned",
  STUDY_ENDED: "study-ended",
};

const PREF_TEST_CACHED_CONTENT = "toolkit.pioneer.testCachedContent";
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

async function toggleEnrolled(studyAddonId, cachedAddons) {
  let addon;
  let install;

  const cachedAddon = cachedAddons.find(a => a.addon_id == studyAddonId);

  if (Cu.isInAutomation) {
    install = {
      install: () => {
        let testAddons = Services.prefs.getStringPref(PREF_TEST_ADDONS, "[]");
        testAddons = JSON.parse(testAddons);

        testAddons.push(studyAddonId);
        Services.prefs.setStringPref(
          PREF_TEST_ADDONS,
          JSON.stringify(testAddons)
        );
      },
    };

    let testAddons = Services.prefs.getStringPref(PREF_TEST_ADDONS, "[]");
    testAddons = JSON.parse(testAddons);

    for (const testAddon of testAddons) {
      if (testAddon == studyAddonId) {
        addon = {};
        addon.install = () => {};
        addon.uninstall = () => {
          Services.prefs.setStringPref(
            PREF_TEST_ADDONS,
            JSON.stringify(testAddons.filter(a => a.id != testAddon.id))
          );
        };
      }
    }
  } else {
    addon = await AddonManager.getAddonByID(studyAddonId);
    install = await AddonManager.getInstallForURL(cachedAddon.sourceURI.spec);
  }

  const completedStudies = Services.prefs.getStringPref(
    PREF_PIONEER_COMPLETED_STUDIES,
    "{}"
  );

  const study = document.querySelector(`.card[id="${cachedAddon.addon_id}"`);
  const joinBtn = study.querySelector(".join-button");

  // Check if this study is re-join-able before enrollment.
  if (addon) {
    joinBtn.disabled = true;
    await addon.uninstall();
    await sendDeletionPing(studyAddonId);

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

  await updateStudy(cachedAddon.addon_id);
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

    joinBtn.addEventListener("click", async () => {
      let addon;
      if (Cu.isInAutomation) {
        const testAddons = Services.prefs.getStringPref(PREF_TEST_ADDONS, "[]");
        for (const testAddon of JSON.parse(testAddons)) {
          if (testAddon == studyAddonId) {
            addon = {};
            addon.install = () => {};
            addon.uninstall = () => {
              Services.prefs.setStringPref(PREF_TEST_ADDONS, "[]");
            };
          }
        }
      } else {
        addon = await AddonManager.getAddonByID(studyAddonId);
      }
      let joinOrLeave = addon ? "leave" : "join";
      let dialog = document.getElementById(
        `${joinOrLeave}-study-consent-dialog`
      );
      dialog.setAttribute("addon-id", cachedAddon.addon_id);
      const consentText = dialog.querySelector(
        `[id=${joinOrLeave}-study-consent]`
      );

      // Clears out any existing children with a single #text node.
      consentText.textContent = "";
      for (const line of cachedAddon[`${joinOrLeave}StudyConsent`].split(
        "\n"
      )) {
        const li = document.createElement("li");
        li.textContent = line;
        consentText.appendChild(li);
      }

      dialog.showModal();
      dialog.scrollTop = 0;
    });

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

    const availableStudies = document.getElementById("available-studies");
    availableStudies.appendChild(study);

    await updateStudy(studyAddonId);
  }

  const availableStudies = document.getElementById("header-available-studies");
  document.l10n.setAttributes(availableStudies, "pioneer-current-studies");
}

async function updateStudy(studyAddonId) {
  let addon;
  if (Cu.isInAutomation) {
    const testAddons = Services.prefs.getStringPref(PREF_TEST_ADDONS, "[]");
    for (const testAddon of JSON.parse(testAddons)) {
      if (testAddon == studyAddonId) {
        addon = {
          uninstall() {},
        };
      }
    }
  } else {
    addon = await AddonManager.getAddonByID(studyAddonId);
  }

  const study = document.querySelector(`.card[id="${studyAddonId}"`);

  const joinBtn = study.querySelector(".join-button");

  const pioneerId = Services.prefs.getStringPref(PREF_PIONEER_ID, null);

  const completedStudies = Services.prefs.getStringPref(
    PREF_PIONEER_COMPLETED_STUDIES,
    "{}"
  );

  const studies = JSON.parse(completedStudies);
  if (studyAddonId in studies) {
    study.style.opacity = 0.5;
    joinBtn.disabled = true;
    document.l10n.setAttributes(joinBtn, "pioneer-ended-study");
    return;
  }

  if (pioneerId) {
    study.style.opacity = 1;
    joinBtn.disabled = false;

    if (addon) {
      document.l10n.setAttributes(joinBtn, "pioneer-leave-study");
    } else {
      document.l10n.setAttributes(joinBtn, "pioneer-join-study");
    }
  } else {
    document.l10n.setAttributes(joinBtn, "pioneer-study-prompt");
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
        let dialog = document.getElementById("leave-pioneer-consent-dialog");
        dialog.showModal();
        dialog.scrollTop = 0;
      } else {
        let dialog = document.getElementById("join-pioneer-consent-dialog");
        dialog.showModal();
        dialog.scrollTop = 0;
      }
    });

  document
    .getElementById("join-pioneer-cancel-dialog-button")
    .addEventListener("click", () =>
      document.getElementById("join-pioneer-consent-dialog").close()
    );
  document
    .getElementById("leave-pioneer-cancel-dialog-button")
    .addEventListener("click", () =>
      document.getElementById("leave-pioneer-consent-dialog").close()
    );
  document
    .getElementById("join-study-cancel-dialog-button")
    .addEventListener("click", () =>
      document.getElementById("join-study-consent-dialog").close()
    );
  document
    .getElementById("leave-study-cancel-dialog-button")
    .addEventListener("click", () =>
      document.getElementById("leave-study-consent-dialog").close()
    );

  document
    .getElementById("join-pioneer-accept-dialog-button")
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

  document
    .getElementById("leave-pioneer-accept-dialog-button")
    .addEventListener("click", async event => {
      const completedStudies = Services.prefs.getStringPref(
        PREF_PIONEER_COMPLETED_STUDIES,
        "{}"
      );
      const studies = JSON.parse(completedStudies);

      // Send a deletion ping for all studies the user has been a part of.
      for (const studyAddonId in studies) {
        await sendDeletionPing(studyAddonId);
      }

      for (const cachedAddon of cachedAddons) {
        const addon = await AddonManager.getAddonByID(cachedAddon.addon_id);
        if (addon) {
          // The user has ended all studies by unenrolling from Pioneer, so send deletion ping for any active studies.
          await sendDeletionPing(addon.id);
          await addon.uninstall();
        }

        const study = document.getElementById(cachedAddon.addon_id);
        if (study) {
          await updateStudy(cachedAddon.addon_id);
        }
      }

      Services.prefs.clearUserPref(PREF_PIONEER_ID);
      Services.prefs.clearUserPref(PREF_PIONEER_COMPLETED_STUDIES);

      for (const cachedAddon of cachedAddons) {
        // Record any studies that have been marked as concluded on the server, in case they re-enroll.
        if ("studyEnded" in cachedAddon && cachedAddon.studyEnded === true) {
          studies[cachedAddon.addon_id] = STUDY_LEAVE_REASONS.STUDY_ENDED;

          Services.prefs.setStringPref(
            PREF_PIONEER_COMPLETED_STUDIES,
            JSON.stringify(studies)
          );
        }
        const addon = await AddonManager.getAddonByID(cachedAddon.addon_id);
        if (addon) {
          await addon.uninstall();
        }

        const study = document.getElementById(cachedAddon.addon_id);
        if (study) {
          await updateStudy(cachedAddon.addon_id);
        }
      }
      document.getElementById("leave-pioneer-consent-dialog").close();
      showEnrollmentStatus();
    });

  document
    .getElementById("join-study-accept-dialog-button")
    .addEventListener("click", async event => {
      const dialog = document.getElementById("join-study-consent-dialog");
      const studyAddonId = dialog.getAttribute("addon-id");
      toggleEnrolled(studyAddonId, cachedAddons);

      dialog.close();
    });

  document
    .getElementById("leave-study-accept-dialog-button")
    .addEventListener("click", async event => {
      const dialog = document.getElementById("leave-study-consent-dialog");
      const studyAddonId = dialog.getAttribute("addon-id");
      toggleEnrolled(studyAddonId, cachedAddons);

      dialog.close();
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

// Updates Pioneer HTML page contents from RemoteSettings.
function updateContents(contents) {
  for (const section of [
    "title",
    "joinPioneerConsent",
    "leavePioneerConsent",
  ]) {
    if (contents && section in contents) {
      // Generate a corresponding dom-id style ID for a camel-case domId style JS attribute.
      // Dynamically set the tag type based on which section is getting updated.
      let tagType = "li";
      if (section === "title") {
        tagType = "p";
      }

      const domId = section
        .split(/(?=[A-Z])/)
        .join("-")
        .toLowerCase();
      // Clears out any existing children with a single #text node.
      document.getElementById(domId).textContent = "";
      for (const line of contents[section].split("\n")) {
        const entry = document.createElement(tagType);
        entry.textContent = line;
        document.getElementById(domId).appendChild(entry);
      }
    }
  }
  if ("privacyPolicy" in contents) {
    document.getElementById("privacy-policy").href = contents.privacyPolicy;
  }
}

document.addEventListener("DOMContentLoaded", async domEvent => {
  showEnrollmentStatus();

  document.addEventListener("focus", removeBadge);
  removeBadge();

  const privacyPolicyLink = document.getElementById("privacy-policy");
  const privacyPolicyFormattedLink = Services.urlFormatter.formatURL(
    privacyPolicyLink.href
  );
  privacyPolicyLink.href = privacyPolicyFormattedLink;

  let cachedContent;
  let cachedAddons;
  if (Cu.isInAutomation) {
    let testCachedAddons = Services.prefs.getStringPref(
      PREF_TEST_CACHED_ADDONS,
      null
    );
    if (testCachedAddons) {
      cachedAddons = JSON.parse(testCachedAddons);
    }

    let testCachedContent = Services.prefs.getStringPref(
      PREF_TEST_CACHED_CONTENT,
      null
    );
    if (testCachedContent) {
      cachedContent = JSON.parse(testCachedContent);
    }
  } else {
    cachedContent = await RemoteSettings(CONTENT_COLLECTION_KEY).get();
    cachedAddons = await RemoteSettings(STUDY_ADDON_COLLECTION_KEY).get();
  }

  // Replace existing contents immediately on page load.
  for (const contents of cachedContent) {
    updateContents(contents);
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

async function sendDeletionPing(studyAddonId) {
  const type = "pioneer-study";

  const options = {
    studyName: studyAddonId,
    addPioneerId: true,
    useEncryption: true,
    // NOTE - while we're not actually sending useful data in this payload, the current Pioneer v2 Telemetry
    // pipeline requires that pings are shaped this way so they are routed to the correct environment.
    //
    // At the moment, the public key used here isn't important but we do need to use *something*.
    encryptionKeyId: "debug",
    publicKey: {
      crv: "P-256",
      kty: "EC",
      x: "XLkI3NaY3-AF2nRMspC63BT1u0Y3moXYSfss7VuQ0mk",
      y: "SB0KnIW-pqk85OIEYZenoNkEyOOp5GeWQhS1KeRtEUE",
    },
    schemaName: "deletion-request",
    schemaVersion: 1,
    schemaNamespace: "pioneer-debug",
  };

  const payload = {
    encryptedData: "",
  };

  await TelemetryController.submitExternalPing(type, payload, options);
}
