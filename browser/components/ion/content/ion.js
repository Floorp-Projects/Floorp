/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Control panel for the Ion project, formerly known as Pioneer.
 * This lives in `about:ion` and provides a UI for users to un/enroll in the
 * overall program, and to un/enroll from individual studies.
 *
 * NOTE - prefs and Telemetry both still mention Pioneer for backwards-compatibility,
 *        this may change in the future.
 */

const { AddonManager } = ChromeUtils.import(
  "resource://gre/modules/AddonManager.jsm"
);

const { RemoteSettings } = ChromeUtils.import(
  "resource://services-settings/remote-settings.js"
);

const { TelemetryController } = ChromeUtils.import(
  "resource://gre/modules/TelemetryController.jsm"
);

let parserUtils = Cc["@mozilla.org/parserutils;1"].getService(
  Ci.nsIParserUtils
);

const PREF_ION_ID = "toolkit.telemetry.pioneerId";
const PREF_ION_NEW_STUDIES_AVAILABLE =
  "toolkit.telemetry.pioneer-new-studies-available";
const PREF_ION_COMPLETED_STUDIES =
  "toolkit.telemetry.pioneer-completed-studies";

/**
 * Remote Settings keys for general content, and available studies.
 */
const CONTENT_COLLECTION_KEY = "pioneer-content-v2";
const STUDY_ADDON_COLLECTION_KEY = "pioneer-study-addons-v2";

const STUDY_LEAVE_REASONS = {
  USER_ABANDONED: "user-abandoned",
  STUDY_ENDED: "study-ended",
};

const PREF_TEST_CACHED_CONTENT = "toolkit.pioneer.testCachedContent";
const PREF_TEST_CACHED_ADDONS = "toolkit.pioneer.testCachedAddons";
const PREF_TEST_ADDONS = "toolkit.pioneer.testAddons";

/**
 * Use the in-tree HTML Sanitizer to ensure that HTML from remote-settings is safe to use.
 * Note that RS does use content-signing, we're doing this extra step as an in-depth security measure.
 *
 * @param {string} htmlString - unsanitized HTML (content-signed by remote-settings)
 * @returns {DocumentFragment} - sanitized DocumentFragment
 */
function sanitizeHtml(htmlString) {
  const content = document.createElement("div");
  const contentFragment = parserUtils.parseFragment(
    htmlString,
    Ci.nsIParserUtils.SanitizerDropForms |
      Ci.nsIParserUtils.SanitizerAllowStyle |
      Ci.nsIParserUtils.SanitizerLogRemovals,
    false,
    Services.io.newURI("about:ion"),
    content
  );

  return contentFragment;
}

function showEnrollmentStatus() {
  const ionId = Services.prefs.getStringPref(PREF_ION_ID, null);

  const enrollmentButton = document.getElementById("enrollment-button");

  document.l10n.setAttributes(
    enrollmentButton,
    `ion-${ionId ? "un" : ""}enrollment-button`
  );
  enrollmentButton.classList.toggle("primary", !ionId);

  // collapse content above the fold if enrolled, otherwise open it.
  for (const section of ["details", "data"]) {
    const details = document.getElementById(section);
    if (ionId) {
      details.removeAttribute("open");
    } else {
      details.setAttribute("open", true);
    }
  }
}

function toggleContentBasedOnLocale() {
  const requestedLocale = Services.locale.requestedLocale;
  if (requestedLocale !== "en-US") {
    const localeNotificationBar = document.getElementById(
      "locale-notification"
    );
    localeNotificationBar.style.display = "block";

    const reportContent = document.getElementById("report-content");
    reportContent.style.display = "none";
  }
}

async function toggleEnrolled(studyAddonId, cachedAddons) {
  let addon;
  let install;

  const cachedAddon = cachedAddons.find(a => a.addon_id == studyAddonId);

  if (Cu.isInAutomation) {
    install = {
      install: async () => {
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
    PREF_ION_COMPLETED_STUDIES,
    "{}"
  );

  const study = document.querySelector(`.card[id="${cachedAddon.addon_id}"`);
  const joinBtn = study.querySelector(".join-button");

  if (addon) {
    joinBtn.disabled = true;
    await addon.uninstall();
    await sendDeletionPing(studyAddonId);

    document.l10n.setAttributes(joinBtn, "ion-join-study");
    joinBtn.disabled = false;

    // Record that the user abandoned this study, since it may not be re-join-able.
    if (completedStudies) {
      const studies = JSON.parse(completedStudies);
      studies[studyAddonId] = STUDY_LEAVE_REASONS.USER_ABANDONED;
      Services.prefs.setStringPref(
        PREF_ION_COMPLETED_STUDIES,
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
    document.l10n.setAttributes(joinBtn, "ion-leave-study");
    joinBtn.disabled = false;

    // Send an enrollment ping for this study. Note that this could be sent again
    // if we are re-joining.
    await sendEnrollmentPing(studyAddonId);
  }

  await updateStudy(cachedAddon.addon_id);
}

async function showAvailableStudies(cachedAddons) {
  const ionId = Services.prefs.getStringPref(PREF_ION_ID, null);
  const defaultAddons = cachedAddons.filter(a => a.isDefault);
  if (ionId) {
    for (const defaultAddon of defaultAddons) {
      let addon;
      let install;
      if (Cu.isInAutomation) {
        install = {
          install: async () => {
            if (
              defaultAddon.addon_id == "ion-v2-bad-default-example@mozilla.org"
            ) {
              throw new Error("Bad test default add-on");
            }
          },
        };
      } else {
        addon = await AddonManager.getAddonByID(defaultAddon.addon_id);
        install = await AddonManager.getInstallForURL(
          defaultAddon.sourceURI.spec
        );
      }

      if (!addon) {
        // Any default add-ons are required, try to reinstall.
        await install.install();
      }
    }
  }

  const studyAddons = cachedAddons.filter(a => !a.isDefault);
  for (const cachedAddon of studyAddons) {
    if (!cachedAddon) {
      console.error(
        `about:ion - Study addon ID not found in cache: ${studyAddonId}`
      );
      return;
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
    document.l10n.setAttributes(joinBtn, "ion-join-study");

    joinBtn.addEventListener("click", async () => {
      let addon;
      if (Cu.isInAutomation) {
        const testAddons = Services.prefs.getStringPref(PREF_TEST_ADDONS, "[]");
        for (const testAddon of JSON.parse(testAddons)) {
          if (testAddon == studyAddonId) {
            addon = {};
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

      // Clears out any existing children with a single #text node
      consentText.textContent = "";

      const contentFragment = sanitizeHtml(
        cachedAddon[`${joinOrLeave}StudyConsent`]
      );
      consentText.appendChild(contentFragment);

      dialog.showModal();
      dialog.scrollTop = 0;

      const openEvent = new Event("open");
      dialog.dispatchEvent(openEvent);
    });
    actions.appendChild(joinBtn);

    const studyDesc = document.createElement("div");
    studyDesc.setAttribute("class", "card-description");

    const contentFragment = sanitizeHtml(cachedAddon.description);
    studyDesc.appendChild(contentFragment);

    study.appendChild(studyDesc);

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
  document.l10n.setAttributes(availableStudies, "ion-current-studies");
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

  const ionId = Services.prefs.getStringPref(PREF_ION_ID, null);

  const completedStudies = Services.prefs.getStringPref(
    PREF_ION_COMPLETED_STUDIES,
    "{}"
  );

  const studies = JSON.parse(completedStudies);
  if (studyAddonId in studies) {
    study.style.opacity = 0.5;
    joinBtn.disabled = true;
    document.l10n.setAttributes(joinBtn, "ion-ended-study");
    return;
  }

  if (ionId) {
    study.style.opacity = 1;
    joinBtn.disabled = false;

    if (addon) {
      document.l10n.setAttributes(joinBtn, "ion-leave-study");
    } else {
      document.l10n.setAttributes(joinBtn, "ion-join-study");
    }
  } else {
    document.l10n.setAttributes(joinBtn, "ion-study-prompt");
    study.style.opacity = 0.5;
    joinBtn.disabled = true;
  }
}

// equivalent to what we use for Telemetry IDs
// https://searchfox.org/mozilla-central/rev/9193635dca8cfdcb68f114306194ffc860456044/toolkit/components/telemetry/app/TelemetryUtils.jsm#222
function generateUUID() {
  let str = Services.uuid.generateUUID().toString();
  return str.substring(1, str.length - 1);
}

async function setup(cachedAddons) {
  document
    .getElementById("enrollment-button")
    .addEventListener("click", async () => {
      const ionId = Services.prefs.getStringPref(PREF_ION_ID, null);

      if (ionId) {
        let dialog = document.getElementById("leave-ion-consent-dialog");
        dialog.showModal();
        dialog.scrollTop = 0;
      } else {
        let dialog = document.getElementById("join-ion-consent-dialog");
        dialog.showModal();
        dialog.scrollTop = 0;
      }
    });

  document
    .getElementById("join-ion-cancel-dialog-button")
    .addEventListener("click", () =>
      document.getElementById("join-ion-consent-dialog").close()
    );
  document
    .getElementById("leave-ion-cancel-dialog-button")
    .addEventListener("click", () =>
      document.getElementById("leave-ion-consent-dialog").close()
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
    .getElementById("join-ion-accept-dialog-button")
    .addEventListener("click", async event => {
      const ionId = Services.prefs.getStringPref(PREF_ION_ID, null);

      if (!ionId) {
        let uuid = generateUUID();
        Services.prefs.setStringPref(PREF_ION_ID, uuid);
        for (const cachedAddon of cachedAddons) {
          if (cachedAddon.isDefault) {
            let install;
            if (Cu.isInAutomation) {
              install = {
                install: async () => {
                  if (
                    cachedAddon.addon_id ==
                    "ion-v2-bad-default-example@mozilla.org"
                  ) {
                    throw new Error("Bad test default add-on");
                  }
                },
              };
            } else {
              install = await AddonManager.getInstallForURL(
                cachedAddon.sourceURI.spec
              );
            }

            try {
              await install.install();
            } catch (ex) {
              // No need to throw here, we'll try again before letting users enroll in any studies.
              console.error(
                `Could not install default add-on ${cachedAddon.addon_id}`
              );
              const availableStudies = document.getElementById(
                "available-studies"
              );
              document.l10n.setAttributes(
                availableStudies,
                "ion-no-current-studies"
              );
            }
          }
          const study = document.getElementById(cachedAddon.addon_id);
          if (study) {
            await updateStudy(cachedAddon.addon_id);
          }
        }
        document.querySelector("dialog").close();
      }
      // A this point we should have a valid ion id, so we should be able to send
      // the enrollment ping.
      await sendEnrollmentPing();

      showEnrollmentStatus();
    });

  document
    .getElementById("leave-ion-accept-dialog-button")
    .addEventListener("click", async event => {
      const completedStudies = Services.prefs.getStringPref(
        PREF_ION_COMPLETED_STUDIES,
        "{}"
      );
      const studies = JSON.parse(completedStudies);

      // Send a deletion ping for all completed studies the user has been a part of.
      for (const studyAddonId in studies) {
        await sendDeletionPing(studyAddonId);
      }

      Services.prefs.clearUserPref(PREF_ION_COMPLETED_STUDIES);

      for (const cachedAddon of cachedAddons) {
        // Record any studies that have been marked as concluded on the server, in case they re-enroll.
        if ("studyEnded" in cachedAddon && cachedAddon.studyEnded === true) {
          studies[cachedAddon.addon_id] = STUDY_LEAVE_REASONS.STUDY_ENDED;

          Services.prefs.setStringPref(
            PREF_ION_COMPLETED_STUDIES,
            JSON.stringify(studies)
          );
        }

        let addon;
        if (Cu.isInAutomation) {
          addon = {};
          addon.id = cachedAddon.addon_id;
          addon.uninstall = () => {
            let testAddons = Services.prefs.getStringPref(
              PREF_TEST_ADDONS,
              "[]"
            );
            testAddons = JSON.parse(testAddons);

            Services.prefs.setStringPref(
              PREF_TEST_ADDONS,
              JSON.stringify(
                testAddons.filter(a => a.id != cachedAddon.addon_id)
              )
            );
          };
        } else {
          addon = await AddonManager.getAddonByID(cachedAddon.addon_id);
        }
        if (addon) {
          await sendDeletionPing(addon.id);
          await addon.uninstall();
        }
      }

      Services.prefs.clearUserPref(PREF_ION_ID);
      for (const cachedAddon of cachedAddons) {
        const study = document.getElementById(cachedAddon.addon_id);
        if (study) {
          await updateStudy(cachedAddon.addon_id);
        }
      }

      document.getElementById("leave-ion-consent-dialog").close();
      showEnrollmentStatus();
    });

  document
    .getElementById("join-study-accept-dialog-button")
    .addEventListener("click", async event => {
      const dialog = document.getElementById("join-study-consent-dialog");
      const studyAddonId = dialog.getAttribute("addon-id");
      toggleEnrolled(studyAddonId, cachedAddons).then(dialog.close());
    });

  document
    .getElementById("leave-study-accept-dialog-button")
    .addEventListener("click", async event => {
      const dialog = document.getElementById("leave-study-consent-dialog");
      const studyAddonId = dialog.getAttribute("addon-id");
      await toggleEnrolled(studyAddonId, cachedAddons).then(dialog.close());
    });

  const onAddonEvent = async addon => {
    for (const cachedAddon of cachedAddons) {
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
  Services.prefs.setBoolPref(PREF_ION_NEW_STUDIES_AVAILABLE, false);

  for (let win of Services.wm.getEnumerator("navigator:browser")) {
    const badge = win.document
      .getElementById("ion-button")
      .querySelector(".toolbarbutton-badge");
    badge.classList.remove("feature-callout");
  }
}

// Updates Ion HTML page contents from RemoteSettings.
function updateContents(contents) {
  for (const section of [
    "title",
    "summary",
    "details",
    "data",
    "joinIonConsent",
    "leaveIonConsent",
  ]) {
    if (contents && section in contents) {
      // Generate a corresponding dom-id style ID for a camel-case domId style JS attribute.
      // Dynamically set the tag type based on which section is getting updated.
      const domId = section
        .split(/(?=[A-Z])/)
        .join("-")
        .toLowerCase();
      // Clears out any existing children with a single #text node.
      document.getElementById(domId).textContent = "";

      const contentFragment = sanitizeHtml(contents[section]);
      document.getElementById(domId).appendChild(contentFragment);
    }
  }
}

document.addEventListener("DOMContentLoaded", async domEvent => {
  toggleContentBasedOnLocale();

  showEnrollmentStatus();

  document.addEventListener("focus", removeBadge);
  removeBadge();

  const privacyPolicyLinks = document.querySelectorAll(
    ".privacy-policy,.privacy-notice"
  );
  for (const privacyPolicyLink of privacyPolicyLinks) {
    const privacyPolicyFormattedLink = Services.urlFormatter.formatURL(
      privacyPolicyLink.href
    );
    privacyPolicyLink.href = privacyPolicyFormattedLink;
  }

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
        PREF_ION_COMPLETED_STUDIES,
        "{}"
      );
      const studies = JSON.parse(completedStudies);
      studies[cachedAddon.addon_id] = STUDY_LEAVE_REASONS.STUDY_ENDED;

      Services.prefs.setStringPref(
        PREF_ION_COMPLETED_STUDIES,
        JSON.stringify(studies)
      );
    }
  }

  await setup(cachedAddons);

  try {
    await showAvailableStudies(cachedAddons);
  } catch (ex) {
    // No need to throw here, we'll try again before letting users enroll in any studies.
    console.error(`Could not show available studies`, ex);
  }
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
    encryptionKeyId: "discarded",
    publicKey: {
      crv: "P-256",
      kty: "EC",
      x: "XLkI3NaY3-AF2nRMspC63BT1u0Y3moXYSfss7VuQ0mk",
      y: "SB0KnIW-pqk85OIEYZenoNkEyOOp5GeWQhS1KeRtEUE",
    },
    schemaName: "deletion-request",
    schemaVersion: 1,
    // The schema namespace needs to be the study addon id, as we
    // want to route the ping to the specific study table.
    schemaNamespace: studyAddonId,
  };

  const payload = {
    encryptedData: "",
  };

  await TelemetryController.submitExternalPing(type, payload, options);
}

/**
 * Sends a Pioneer enrollment ping.
 *
 * The `creationDate` provided by the telemetry APIs will be used as the timestamp for
 * considering the user enrolled in pioneer and/or the study.
 *
 * @param [studyAddonid=undefined] - optional study id. It's sent in the ping, if present,
 * to signal that user enroled in the study.
 */
async function sendEnrollmentPing(studyAddonId) {
  let options = {
    studyName: "pioneer-meta",
    addPioneerId: true,
    useEncryption: true,
    // NOTE - while we're not actually sending useful data in this payload, the current Pioneer v2 Telemetry
    // pipeline requires that pings are shaped this way so they are routed to the correct environment.
    //
    // At the moment, the public key used here isn't important but we do need to use *something*.
    encryptionKeyId: "discarded",
    publicKey: {
      crv: "P-256",
      kty: "EC",
      x: "XLkI3NaY3-AF2nRMspC63BT1u0Y3moXYSfss7VuQ0mk",
      y: "SB0KnIW-pqk85OIEYZenoNkEyOOp5GeWQhS1KeRtEUE",
    },
    schemaName: "pioneer-enrollment",
    schemaVersion: 1,
    // Note that the schema namespace directly informs how data is segregated after ingestion.
    // If this is an enrollment ping for the pioneer program (in contrast to the enrollment to
    // a specific study), use a meta namespace.
    schemaNamespace: "pioneer-meta",
  };

  // If we were provided with a study id, then this is an enrollment to a study.
  // Send the id alongside with the data and change the schema namespace to simplify
  // the work on the ingestion pipeline.
  if (typeof studyAddonId != "undefined") {
    options.studyName = studyAddonId;
    // The schema namespace needs to be the study addon id, as we
    // want to route the ping to the specific study table.
    options.schemaNamespace = studyAddonId;
  }

  await TelemetryController.submitExternalPing("pioneer-study", {}, options);
}
