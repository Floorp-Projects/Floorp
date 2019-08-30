/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

const LOCKWISE_URL = RPMGetStringPref(
  "browser.contentblocking.report.lockwise.url",
  ""
);
const MANAGE_DEVICES_URL = RPMGetStringPref(
  "browser.contentblocking.report.manage_devices.url",
  ""
);
const HOW_IT_WORKS_URL_PREF = RPMGetFormatURLPref(
  "browser.contentblocking.report.lockwise.how_it_works.url"
);

export default class LockwiseCard {
  constructor(document) {
    this.doc = document;
  }

  /**
   * Initializes message listeners/senders.
   */
  init() {
    const openAboutLoginsButton = this.doc.getElementById(
      "open-about-logins-button"
    );
    openAboutLoginsButton.addEventListener("click", () => {
      this.doc.sendTelemetryEvent("click", "lw_open_button");
      RPMSendAsyncMessage("OpenAboutLogins");
    });

    const syncLink = this.doc.getElementById("turn-on-sync");
    // Register a click handler for the anchor since it's not possible to navigate to about:preferences via href
    const eventHandler = evt => {
      if (evt.keyCode == evt.DOM_VK_RETURN || evt.type == "click") {
        this.doc.sendTelemetryEvent("click", "lw_app_link");
        RPMSendAsyncMessage("OpenSyncPreferences");
      }
    };
    syncLink.addEventListener("click", eventHandler);
    syncLink.addEventListener("keydown", eventHandler);

    // Attach link to Firefox Lockwise app page.
    const lockwiseAppLink = this.doc.getElementById("lockwise-inline-link");
    lockwiseAppLink.href = LOCKWISE_URL;
    lockwiseAppLink.addEventListener("click", () => {
      this.doc.sendTelemetryEvent("click", "lw_sync_link");
    });

    // Attack link to Firefox Lockwise "How it works" page.
    const lockwiseReportLink = this.doc.getElementById("lockwise-how-it-works");
    lockwiseReportLink.addEventListener("click", () => {
      this.doc.sendTelemetryEvent("click", "lw_about_link");
    });

    RPMAddMessageListener("SendUserLoginsData", ({ data }) => {
      // Once data for the user is retrieved, display the lockwise card.
      this.buildContent(data);

      const lockwiseUI = document.querySelector(".card.lockwise-card.loading");
      lockwiseUI.classList.remove("loading");
    });
  }

  buildContent(data) {
    const { hasFxa, numLogins, numSyncedDevices } = data;
    const isLoggedIn = numLogins > 0 || hasFxa;
    const title = this.doc.getElementById("lockwise-title");
    const headerContent = this.doc.getElementById("lockwise-header-content");
    const lockwiseBodyContent = this.doc.getElementById(
      "lockwise-body-content"
    );

    // Get the container for the content to display.
    const container = isLoggedIn
      ? lockwiseBodyContent.querySelector(".has-logins")
      : lockwiseBodyContent.querySelector(".no-logins");
    // Display the content
    container.classList.remove("hidden");

    if (isLoggedIn) {
      title.setAttribute("data-l10n-id", "lockwise-title-logged-in");
      headerContent.setAttribute(
        "data-l10n-id",
        "lockwise-header-content-logged-in"
      );
      this.renderContentForLoggedInUser(container, numLogins, numSyncedDevices);
    } else {
      title.setAttribute("data-l10n-id", "lockwise-title");
      headerContent.setAttribute("data-l10n-id", "lockwise-header-content");
    }
  }

  /**
   * Displays the number of stored logins and synced devices for a user.
   *
   * @param {Element} container
   *        The containing element for the content.
   * @param {Number}  storedLogins
   *        The number of browser-stored logins.
   * @param {Number}  syncedDevices
   *        The number of synced devices.
   */
  renderContentForLoggedInUser(container, storedLogins, syncedDevices) {
    const lockwiseCardBody = this.doc.querySelector(
      ".card.lockwise-card .card-body"
    );
    lockwiseCardBody.classList.remove("hidden");

    // Set the text for number of stored logins.
    const numberOfLoginsBlock = container.querySelector(
      ".number-of-logins.block"
    );
    numberOfLoginsBlock.textContent = storedLogins;

    const lockwisePasswordsStored = this.doc.getElementById(
      "lockwise-passwords-stored"
    );
    lockwisePasswordsStored.setAttribute(
      "data-l10n-args",
      JSON.stringify({ count: storedLogins })
    );
    lockwisePasswordsStored.setAttribute(
      "data-l10n-id",
      "lockwise-passwords-stored"
    );

    const howItWorksLink = this.doc.getElementById("lockwise-how-it-works");
    howItWorksLink.href = HOW_IT_WORKS_URL_PREF;

    // Set the text for the number of synced devices.
    const syncedDevicesBlock = container.querySelector(
      ".number-of-synced-devices.block"
    );
    syncedDevicesBlock.textContent = syncedDevices;

    const syncedDevicesText = container.querySelector(".synced-devices-text");
    const textEl = syncedDevicesText.querySelector("span");
    if (syncedDevices) {
      textEl.setAttribute(
        "data-l10n-args",
        JSON.stringify({ count: syncedDevices })
      );
      textEl.setAttribute("data-l10n-id", "lockwise-sync-status");
    } else {
      textEl.setAttribute("data-l10n-id", "lockwise-sync-not-syncing-devices");
    }
    // Display the link for enabling sync if no synced devices are detected.
    if (syncedDevices === 0) {
      const syncLink = this.doc.getElementById("turn-on-sync");
      syncLink.classList.remove("hidden");
    } else {
      const manageDevicesLink = this.doc.getElementById("manage-devices");
      manageDevicesLink.href = MANAGE_DEVICES_URL;
      manageDevicesLink.classList.remove("hidden");
    }
  }
}
