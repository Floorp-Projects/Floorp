/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

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
    const savePasswordsButton = this.doc.getElementById(
      "save-passwords-button"
    );
    savePasswordsButton.addEventListener(
      "click",
      this.openAboutLogins.bind(this)
    );

    const managePasswordsButton = this.doc.getElementById(
      "manage-passwords-button"
    );
    managePasswordsButton.addEventListener(
      "click",
      this.openAboutLogins.bind(this)
    );

    // Attack link to Firefox Lockwise "How it works" page.
    const lockwiseReportLink = this.doc.getElementById("lockwise-how-it-works");
    lockwiseReportLink.addEventListener("click", () => {
      this.doc.sendTelemetryEvent("click", "lw_about_link");
    });
  }

  openAboutLogins() {
    const lockwiseCard = this.doc.querySelector(".lockwise-card");
    if (lockwiseCard.classList.contains("has-logins")) {
      if (lockwiseCard.classList.contains("breached-logins")) {
        this.doc.sendTelemetryEvent(
          "click",
          "lw_open_button",
          "manage_breached_passwords"
        );
      } else if (lockwiseCard.classList.contains("no-breached-logins")) {
        this.doc.sendTelemetryEvent(
          "click",
          "lw_open_button",
          "manage_passwords"
        );
      }
    } else if (lockwiseCard.classList.contains("no-logins")) {
      this.doc.sendTelemetryEvent("click", "lw_open_button", "save_passwords");
    }
    RPMSendAsyncMessage("OpenAboutLogins");
  }

  buildContent(data) {
    const { numLogins, potentiallyBreachedLogins } = data;
    const hasLogins = numLogins > 0;
    const title = this.doc.getElementById("lockwise-title");
    const headerContent = this.doc.querySelector(
      "#lockwise-header-content span"
    );
    const lockwiseCard = this.doc.querySelector(".card.lockwise-card");

    if (hasLogins) {
      lockwiseCard.classList.remove("no-logins");
      lockwiseCard.classList.add("has-logins");
      title.setAttribute("data-l10n-id", "passwords-title-logged-in");
      headerContent.setAttribute(
        "data-l10n-id",
        "lockwise-header-content-logged-in"
      );
      this.renderContentForLoggedInUser(numLogins, potentiallyBreachedLogins);
    } else {
      lockwiseCard.classList.remove("has-logins");
      lockwiseCard.classList.add("no-logins");
      title.setAttribute("data-l10n-id", "lockwise-title");
      headerContent.setAttribute("data-l10n-id", "passwords-header-content");
    }

    const lockwiseUI = document.querySelector(".card.lockwise-card.loading");
    lockwiseUI.classList.remove("loading");
  }

  /**
   * Displays strings indicating stored logins for a user.
   *
   * @param {Number}  storedLogins
   *        The number of browser-stored logins.
   * @param {Number}  potentiallyBreachedLogins
   *        The number of potentially breached logins.
   */
  renderContentForLoggedInUser(storedLogins, potentiallyBreachedLogins) {
    const lockwiseScannedText = this.doc.getElementById(
      "lockwise-scanned-text"
    );
    const lockwiseScannedIcon = this.doc.getElementById(
      "lockwise-scanned-icon"
    );
    const lockwiseCard = this.doc.querySelector(".card.lockwise-card");

    if (potentiallyBreachedLogins) {
      document.l10n.setAttributes(
        lockwiseScannedText,
        "lockwise-scanned-text-breached-logins",
        {
          count: potentiallyBreachedLogins,
        }
      );
      lockwiseScannedIcon.setAttribute(
        "src",
        "chrome://browser/skin/protections/breached-password.svg"
      );
      lockwiseCard.classList.add("breached-logins");
    } else {
      document.l10n.setAttributes(
        lockwiseScannedText,
        "lockwise-scanned-text-no-breached-logins",
        {
          count: storedLogins,
        }
      );
      lockwiseScannedIcon.setAttribute(
        "src",
        "chrome://browser/skin/protections/resolved-breach.svg"
      );
      lockwiseCard.classList.add("no-breached-logins");
    }

    const howItWorksLink = this.doc.getElementById("lockwise-how-it-works");
    howItWorksLink.href = HOW_IT_WORKS_URL_PREF;
  }
}
