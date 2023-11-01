/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/remote-page */

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
  constructor(doc) {
    this.doc = doc;
  }

  init() {
    // Wait for monitor data and display the card.
    this.getMonitorData();

    let monitorAboutLink = this.doc.getElementById("monitor-link");
    monitorAboutLink.addEventListener("click", () => {
      this.doc.sendTelemetryEvent("click", "mtr_about_link");
    });

    const storedEmailLink = this.doc.getElementById(
      "monitor-stored-emails-link"
    );
    storedEmailLink.href = MONITOR_PREFERENCES_URL;
    storedEmailLink.addEventListener(
      "click",
      this.onClickMonitorButton.bind(this)
    );

    const knownBreachesLink = this.doc.getElementById(
      "monitor-known-breaches-link"
    );
    knownBreachesLink.href = MONITOR_HOME_PAGE_URL;
    knownBreachesLink.addEventListener(
      "click",
      this.onClickMonitorButton.bind(this)
    );

    const exposedPasswordsLink = this.doc.getElementById(
      "monitor-exposed-passwords-link"
    );
    exposedPasswordsLink.href = MONITOR_HOME_PAGE_URL;
    exposedPasswordsLink.addEventListener(
      "click",
      this.onClickMonitorButton.bind(this)
    );
  }

  onClickMonitorButton(evt) {
    RPMSendAsyncMessage("ClearMonitorCache");
    switch (evt.currentTarget.id) {
      case "monitor-partial-breaches-link":
        this.doc.sendTelemetryEvent(
          "click",
          "mtr_report_link",
          "resolve_breaches"
        );
        break;
      case "monitor-breaches-link":
        if (evt.currentTarget.classList.contains("no-breaches-resolved")) {
          this.doc.sendTelemetryEvent(
            "click",
            "mtr_report_link",
            "manage_breaches"
          );
        } else {
          this.doc.sendTelemetryEvent(
            "click",
            "mtr_report_link",
            "view_report"
          );
        }
        break;
      case "monitor-stored-emails-link":
        this.doc.sendTelemetryEvent(
          "click",
          "mtr_report_link",
          "stored_emails"
        );
        break;
      case "monitor-known-breaches-link":
        const knownBreaches = this.doc.querySelector(
          "span[data-type='known-breaches']"
        );
        if (knownBreaches.classList.contains("known-resolved-breaches")) {
          this.doc.sendTelemetryEvent(
            "click",
            "mtr_report_link",
            "known_resolved_breaches"
          );
        } else if (
          knownBreaches.classList.contains("known-unresolved-breaches")
        ) {
          this.doc.sendTelemetryEvent(
            "click",
            "mtr_report_link",
            "known_unresolved_breaches"
          );
        }
        break;
      case "monitor-exposed-passwords-link":
        const exposedPasswords = this.doc.querySelector(
          "span[data-type='exposed-passwords']"
        );
        if (
          exposedPasswords.classList.contains("passwords-exposed-all-breaches")
        ) {
          this.doc.sendTelemetryEvent(
            "click",
            "mtr_report_link",
            "exposed_passwords_all_breaches"
          );
        } else if (
          exposedPasswords.classList.contains(
            "passwords-exposed-unresolved-breaches"
          )
        ) {
          this.doc.sendTelemetryEvent(
            "click",
            "mtr_report_link",
            "exposed_passwords_unresolved_breaches"
          );
        }
        break;
    }
  }

  /**
   * Retrieves the monitor data and displays this data in the card.
   */
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
      this.doc.l10n.setAttributes(
        headerContent,
        "monitor-header-content-signed-in"
      );
      this.renderContentForUserWithAccount(monitorData);
    } else {
      monitorCard.classList.add("no-logins");
      const signUpForMonitorLink = this.doc.getElementById(
        "sign-up-for-monitor-link"
      );
      signUpForMonitorLink.href = this.buildMonitorUrl(monitorData.userEmail);
      this.doc.l10n.setAttributes(signUpForMonitorLink, "monitor-sign-up-link");
      this.doc.l10n.setAttributes(
        headerContent,
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
   * @param {string | null} email
   *        Optional. The email used to direct the user to the Monitor website's OAuth
   *        sign-in flow. If null, then direct user to just the Monitor website.
   *
   * @returns {string} URL to Monitor website.
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
      passwordsResolved,
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
    storedEmail.textContent = monitoredEmails;
    const infoMonitoredAddresses = this.doc.getElementById(
      "info-monitored-addresses"
    );
    this.doc.l10n.setAttributes(
      infoMonitoredAddresses,
      "info-monitored-emails",
      { count: monitoredEmails }
    );

    const knownBreaches = this.doc.querySelector(
      "span[data-type='known-breaches']"
    );
    const exposedPasswords = this.doc.querySelector(
      "span[data-type='exposed-passwords']"
    );

    const infoKnownBreaches = this.doc.getElementById("info-known-breaches");
    const infoExposedPasswords = this.doc.getElementById(
      "info-exposed-passwords"
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
        knownBreaches.textContent = numBreaches;
        knownBreaches.classList.add("known-unresolved-breaches");
        knownBreaches.classList.remove("known-resolved-breaches");
        this.doc.l10n.setAttributes(
          infoKnownBreaches,
          "info-known-breaches-found",
          { count: numBreaches }
        );
        exposedPasswords.textContent = passwords;
        exposedPasswords.classList.add("passwords-exposed-all-breaches");
        exposedPasswords.classList.remove(
          "passwords-exposed-unresolved-breaches"
        );
        this.doc.l10n.setAttributes(
          infoExposedPasswords,
          "info-exposed-passwords-found",
          { count: passwords }
        );

        breachesIcon.setAttribute(
          "src",
          "chrome://browser/skin/protections/new-feature.svg"
        );
        this.doc.l10n.setAttributes(
          breachesTitle,
          "monitor-breaches-unresolved-title"
        );
        this.doc.l10n.setAttributes(
          breachesDesc,
          "monitor-breaches-unresolved-description"
        );
        this.doc.l10n.setAttributes(
          breachesLink,
          "monitor-manage-breaches-link"
        );
        breachesLink.classList.add("no-breaches-resolved");
      } else if (numBreaches == numBreachesResolved) {
        partialBreachesWrapper.classList.add("hidden");
        knownBreaches.textContent = numBreachesResolved;
        knownBreaches.classList.remove("known-unresolved-breaches");
        knownBreaches.classList.add("known-resolved-breaches");
        this.doc.l10n.setAttributes(
          infoKnownBreaches,
          "info-known-breaches-resolved",
          { count: numBreachesResolved }
        );
        let unresolvedPasswords = passwords - passwordsResolved;
        exposedPasswords.textContent = unresolvedPasswords;
        exposedPasswords.classList.remove("passwords-exposed-all-breaches");
        exposedPasswords.classList.add("passwords-exposed-unresolved-breaches");
        this.doc.l10n.setAttributes(
          infoExposedPasswords,
          "info-exposed-passwords-resolved",
          { count: unresolvedPasswords }
        );

        breachesIcon.setAttribute(
          "src",
          "chrome://browser/skin/protections/resolved-breach.svg"
        );
        this.doc.l10n.setAttributes(
          breachesTitle,
          "monitor-breaches-resolved-title"
        );
        this.doc.l10n.setAttributes(
          breachesDesc,
          "monitor-breaches-resolved-description"
        );
        this.doc.l10n.setAttributes(breachesLink, "monitor-view-report-link");
      } else {
        breachesWrapper.classList.add("hidden");
        knownBreaches.textContent = numBreachesResolved;
        knownBreaches.classList.remove("known-unresolved-breaches");
        knownBreaches.classList.add("known-resolved-breaches");
        this.doc.l10n.setAttributes(
          infoKnownBreaches,
          "info-known-breaches-resolved",
          { count: numBreachesResolved }
        );
        let unresolvedPasswords = passwords - passwordsResolved;
        exposedPasswords.textContent = unresolvedPasswords;
        exposedPasswords.classList.remove("passwords-exposed-all-breaches");
        exposedPasswords.classList.add("passwords-exposed-unresolved-breaches");
        this.doc.l10n.setAttributes(
          infoExposedPasswords,
          "info-exposed-passwords-resolved",
          { count: unresolvedPasswords }
        );

        const partialBreachesTitle = document.getElementById(
          "monitor-partial-breaches-title"
        );
        this.doc.l10n.setAttributes(
          partialBreachesTitle,
          "monitor-partial-breaches-title",
          {
            numBreaches,
            numBreachesResolved,
          }
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
            this.doc.l10n.setAttributes(
              partialBreachesMotivationTitle,
              "monitor-partial-breaches-motivation-title-start"
            );
            break;

          case percentageResolved >= 25 && percentageResolved < 75:
            this.doc.l10n.setAttributes(
              partialBreachesMotivationTitle,
              "monitor-partial-breaches-motivation-title-middle"
            );
            break;

          case percentageResolved >= 75 && percentageResolved < 100:
            this.doc.l10n.setAttributes(
              partialBreachesMotivationTitle,
              "monitor-partial-breaches-motivation-title-end"
            );
            break;
        }

        const partialBreachesPercentage = document.getElementById(
          "monitor-partial-breaches-percentage"
        );
        this.doc.l10n.setAttributes(
          partialBreachesPercentage,
          "monitor-partial-breaches-percentage",
          { percentageResolved }
        );

        const partialBreachesLink = document.getElementById(
          "monitor-partial-breaches-link"
        );
        partialBreachesLink.setAttribute("href", MONITOR_HOME_PAGE_URL);
        partialBreachesLink.addEventListener(
          "click",
          this.onClickMonitorButton.bind(this)
        );
      }
    } else {
      partialBreachesWrapper.classList.add("hidden");
      knownBreaches.textContent = numBreaches;
      knownBreaches.classList.add("known-unresolved-breaches");
      knownBreaches.classList.remove("known-resolved-breaches");
      this.doc.l10n.setAttributes(
        infoKnownBreaches,
        "info-known-breaches-found",
        { count: numBreaches }
      );
      exposedPasswords.textContent = passwords;
      exposedPasswords.classList.add("passwords-exposed-all-breaches");
      exposedPasswords.classList.remove(
        "passwords-exposed-unresolved-breaches"
      );
      this.doc.l10n.setAttributes(
        infoExposedPasswords,
        "info-exposed-passwords-found",
        { count: passwords }
      );

      breachesIcon.setAttribute(
        "src",
        "chrome://browser/skin/protections/resolved-breach.svg"
      );
      this.doc.l10n.setAttributes(breachesTitle, "monitor-no-breaches-title");
      this.doc.l10n.setAttributes(
        breachesDesc,
        "monitor-no-breaches-description"
      );
      this.doc.l10n.setAttributes(breachesLink, "monitor-view-report-link");
    }

    breachesLink.setAttribute("href", MONITOR_HOME_PAGE_URL);
    breachesLink.addEventListener(
      "click",
      this.onClickMonitorButton.bind(this)
    );
  }
}
