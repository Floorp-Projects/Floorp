/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

export default class MonitorClass {
  constructor(document) {
    this.doc = document;
  }

  init() {
    const signUpForMonitorButton = this.doc.getElementById(
      "sign-up-for-monitor-button"
    );
    signUpForMonitorButton.addEventListener("click", () => {
      console.log("TODO: Where is this link supposed to go.");
    });

    RPMAddMessageListener("SendUserLoginsData", ({ data }) => {
      // Wait for monitor data and display the card.
      this.getMonitorData(data);
      RPMSendAsyncMessage("FetchMonitorData");
    });
  }

  /**
   * Adds a listener for receiving the monitor data. Once received then display this data
   * in the card.
   *
   * @param {Object}  loginData
   *        Login data received from the Logins service.
   */
  getMonitorData(loginData) {
    RPMAddMessageListener("SendMonitorData", ({ data: monitorData }) => {
      // Once data for the user is retrieved, display the monitor card.
      this.buildContent(loginData, monitorData);

      // Show the Monitor card.
      const monitorCard = this.doc.querySelector(
        ".report-card.monitor-card.hidden"
      );
      monitorCard.classList.remove("hidden");
    });
  }

  buildContent(loginData, monitorData) {
    const { hasFxa, numLogins } = loginData;
    const isLoggedIn = numLogins > 0 || hasFxa;
    const headerContent = this.doc.querySelector(
      "#monitor-header-content span"
    );
    const monitorCard = this.doc.querySelector(".report-card.monitor-card");
    if (isLoggedIn && !monitorData.error) {
      monitorCard.classList.add("has-logins");
      headerContent.textContent =
        "Firefox Monitor warns you if your info has appeared in a known data breach";
      this.renderContentForUserWithLogins(monitorData);
    } else {
      monitorCard.classList.add("no-logins");
      const signUpForMonitorButton = this.doc.getElementById(
        "sign-up-for-monitor-button"
      );
      signUpForMonitorButton.textContent = hasFxa
        ? "Turn on Monitor"
        : "Sign up for Monitor";
      headerContent.textContent =
        "Check Firefox Monitor to see if you've been part of a data breach and get alerts about new breaches.";
    }
  }

  renderContentForUserWithLogins(monitorData) {
    const storedEmail = this.doc.querySelector(
      "span[data-type='stored-emails']"
    );
    const knownBreaches = this.doc.querySelector(
      "span[data-type='known-breaches']"
    );
    const exposedPasswords = this.doc.querySelector(
      "span[data-type='exposed-passwords']"
    );
    const exposedLockwisePasswords = this.doc.querySelector(
      ".number-of-breaches.block"
    );

    storedEmail.textContent = monitorData.monitoredEmails;
    knownBreaches.textContent = monitorData.numBreaches;
    exposedPasswords.textContent = monitorData.passwords;
    exposedLockwisePasswords.textContent = monitorData.lockwisePasswords;
  }
}
