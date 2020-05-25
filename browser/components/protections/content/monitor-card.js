/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

const MONITOR_URL = RPMGetStringPref(
  "browser.contentblocking.report.monitor.url",
  ""
);
const MONITOR_SIGN_IN_URL = RPMGetStringPref(
  "browser.contentblocking.report.monitor.sign_in_url",
  ""
);
const HOW_IT_WORKS_URL_PREF = RPMGetFormatURLPref(
  "browser.contentblocking.report.monitor.how_it_works.url"
);
const MONITOR_PREFERENCES_URL = RPMGetFormatURLPref(
  "browser.contentblocking.report.monitor.preferences_url"
);
const MONITOR_HOME_PAGE_URL = RPMGetFormatURLPref(
  "browser.contentblocking.report.monitor.home_page_url"
);

export default class MonitorClass {
  constructor(document) {
    this.doc = document;
  }

  init() {
    // Wait for monitor data and display the card.
    this.getMonitorData();

    let monitorAboutLink = this.doc.getElementById("monitor-link");
    monitorAboutLink.addEventListener("click", () => {
      this.doc.sendTelemetryEvent("click", "mtr_about_link");
    });

    const storedEmailLink = this.doc.querySelector(".monitor-block.email a");
    storedEmailLink.href = MONITOR_PREFERENCES_URL;

    const knownBreachesLink = this.doc.querySelector(
      ".monitor-block.breaches a"
    );
    knownBreachesLink.href = MONITOR_HOME_PAGE_URL;

    const exposedPasswordsLink = this.doc.querySelector(
      ".monitor-block.passwords a"
    );
    exposedPasswordsLink.href = MONITOR_HOME_PAGE_URL;
  }

  onClickMonitorInfoBlock() {
    RPMSendAsyncMessage("ClearMonitorCache");
  }

  /**
   * Retrieves the monitor data and displays this data in the card.
   **/
  getMonitorData() {
    RPMSendQuery("FetchMonitorData", {}).then(monitorData => {
      // Once data for the user is retrieved, display the monitor card.
      this.buildContent(monitorData);

      // Show the Monitor card.
      const monitorUI = this.doc.querySelector(".card.monitor-card.loading");
      monitorUI.classList.remove("loading");
    });
  }

  buildContent(monitorData) {
    const headerContent = this.doc.querySelector(
      "#monitor-header-content span"
    );
    const monitorCard = this.doc.querySelector(".card.monitor-card");
    if (!monitorData.error) {
      monitorCard.classList.add("has-logins");
      headerContent.setAttribute(
        "data-l10n-id",
        "monitor-header-content-signed-in"
      );
      this.renderContentForUserWithAccount(monitorData);
    } else {
      monitorCard.classList.add("no-logins");
      const signUpForMonitorLink = this.doc.getElementById(
        "sign-up-for-monitor-link"
      );
      signUpForMonitorLink.href = this.buildMonitorUrl(monitorData.userEmail);
      signUpForMonitorLink.setAttribute("data-l10n-id", "monitor-sign-up-link");
      headerContent.setAttribute(
        "data-l10n-id",
        "monitor-header-content-no-account"
      );
      signUpForMonitorLink.addEventListener("click", () => {
        this.doc.sendTelemetryEvent("click", "mtr_signup_button");
      });
    }
  }

  /**
   * Builds the appropriate URL that takes the user to the Monitor website's
   * sign-up/sign-in page.
   *
   * @param {String|null} email
   *        Optional. The email used to direct the user to the Monitor website's OAuth
   *        sign-in flow. If null, then direct user to just the Monitor website.
   *
   * @return URL to Monitor website.
   */
  buildMonitorUrl(email = null) {
    return email
      ? `${MONITOR_SIGN_IN_URL}${encodeURIComponent(email)}`
      : MONITOR_URL;
  }

  renderContentForUserWithAccount(monitorData) {
    const {
      numBreaches,
      numBreachesResolved,
      passwords,
      monitoredEmails,
    } = monitorData;
    const monitorCardBody = this.doc.querySelector(
      ".card.monitor-card .card-body"
    );
    monitorCardBody.classList.remove("hidden");

    const howItWorksLink = this.doc.getElementById("monitor-link");
    howItWorksLink.href = HOW_IT_WORKS_URL_PREF;

    const storedEmail = this.doc.querySelector(
      "span[data-type='stored-emails']"
    );
    const knownBreaches = this.doc.querySelector(
      "span[data-type='known-breaches']"
    );
    const exposedPasswords = this.doc.querySelector(
      "span[data-type='exposed-passwords']"
    );
    storedEmail.textContent = monitoredEmails;
    knownBreaches.textContent = numBreaches;
    exposedPasswords.textContent = passwords;

    const infoMonitoredAddresses = this.doc.getElementById(
      "info-monitored-addresses"
    );
    infoMonitoredAddresses.setAttribute(
      "data-l10n-args",
      JSON.stringify({ count: monitoredEmails })
    );
    infoMonitoredAddresses.setAttribute(
      "data-l10n-id",
      "info-monitored-emails"
    );

    const infoKnownBreaches = this.doc.getElementById("info-known-breaches");
    infoKnownBreaches.setAttribute(
      "data-l10n-args",
      JSON.stringify({ count: numBreaches })
    );
    infoKnownBreaches.setAttribute("data-l10n-id", "info-known-breaches-found");

    const infoExposedPasswords = this.doc.getElementById(
      "info-exposed-passwords"
    );
    infoExposedPasswords.setAttribute(
      "data-l10n-args",
      JSON.stringify({ count: passwords })
    );
    infoExposedPasswords.setAttribute(
      "data-l10n-id",
      "info-exposed-passwords-found"
    );

    const breachesWrapper = this.doc.querySelector(".monitor-breaches-wrapper");
    const partialBreachesWrapper = this.doc.querySelector(
      ".monitor-partial-breaches-wrapper"
    );
    const breachesTitle = this.doc.getElementById("monitor-breaches-title");
    const breachesIcon = this.doc.getElementById("monitor-breaches-icon");
    const breachesDesc = this.doc.getElementById(
      "monitor-breaches-description"
    );
    const breachesLink = this.doc.getElementById("monitor-breaches-link");
    if (numBreaches) {
      if (!numBreachesResolved) {
        partialBreachesWrapper.classList.add("hidden");
        breachesIcon.setAttribute(
          "src",
          "chrome://browser/skin/protections/new-feature.svg"
        );
        breachesTitle.setAttribute(
          "data-l10n-id",
          "monitor-breaches-unresolved-title"
        );
        breachesDesc.setAttribute(
          "data-l10n-id",
          "monitor-breaches-unresolved-description"
        );
        breachesLink.setAttribute(
          "data-l10n-id",
          "monitor-manage-breaches-link"
        );
      } else if (numBreaches == numBreachesResolved) {
        partialBreachesWrapper.classList.add("hidden");
        breachesIcon.setAttribute(
          "src",
          "chrome://browser/skin/protections/resolved-breach.svg"
        );
        breachesTitle.setAttribute(
          "data-l10n-id",
          "monitor-breaches-resolved-title"
        );
        breachesDesc.setAttribute(
          "data-l10n-id",
          "monitor-breaches-resolved-description"
        );
        breachesLink.setAttribute("data-l10n-id", "monitor-view-report-link");
      } else {
        breachesWrapper.classList.add("hidden");
        const partialBreachesTitle = document.getElementById(
          "monitor-partial-breaches-title"
        );
        partialBreachesTitle.setAttribute(
          "data-l10n-args",
          JSON.stringify({
            numBreaches,
            numBreachesResolved,
          })
        );
        partialBreachesTitle.setAttribute(
          "data-l10n-id",
          "monitor-partial-breaches-title"
        );

        const progressBar = this.doc.querySelector(".progress-bar");
        const partialBreachesMotivationTitle = document.getElementById(
          "monitor-partial-breaches-motivation-title"
        );

        let percentageResolved = Math.floor(
          (numBreachesResolved / numBreaches) * 100
        );
        progressBar.setAttribute("value", 100 - percentageResolved);
        switch (true) {
          case percentageResolved > 0 && percentageResolved < 25:
            partialBreachesMotivationTitle.setAttribute(
              "data-l10n-id",
              "monitor-partial-breaches-motivation-title-start"
            );
            break;

          case percentageResolved >= 25 && percentageResolved < 75:
            partialBreachesMotivationTitle.setAttribute(
              "data-l10n-id",
              "monitor-partial-breaches-motivation-title-middle"
            );
            break;

          case percentageResolved >= 75 && percentageResolved < 100:
            partialBreachesMotivationTitle.setAttribute(
              "data-l10n-id",
              "monitor-partial-breaches-motivation-title-end"
            );
            break;
        }

        const partialBreachesPercentage = document.getElementById(
          "monitor-partial-breaches-percentage"
        );
        partialBreachesPercentage.setAttribute(
          "data-l10n-args",
          JSON.stringify({
            percentageResolved,
          })
        );
        partialBreachesPercentage.setAttribute(
          "data-l10n-id",
          "monitor-partial-breaches-percentage"
        );

        const partialBreachesLink = document.getElementById(
          "monitor-partial-breaches-link"
        );
        partialBreachesLink.setAttribute("href", MONITOR_HOME_PAGE_URL);
        partialBreachesLink.addEventListener(
          "click",
          this.onClickMonitorInfoBlock
        );
      }
    } else {
      partialBreachesWrapper.classList.add("hidden");
      breachesIcon.setAttribute(
        "src",
        "chrome://browser/skin/protections/resolved-breach.svg"
      );
      breachesTitle.setAttribute("data-l10n-id", "monitor-no-breaches-title");
      breachesDesc.setAttribute(
        "data-l10n-id",
        "monitor-no-breaches-description"
      );
      breachesLink.setAttribute("data-l10n-id", "monitor-view-report-link");
    }

    breachesLink.setAttribute("href", MONITOR_HOME_PAGE_URL);
    breachesLink.addEventListener("click", this.onClickMonitorInfoBlock);
  }
}
