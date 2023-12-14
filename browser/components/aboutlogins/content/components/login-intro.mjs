/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export default class LoginIntro extends HTMLElement {
  connectedCallback() {
    if (this.shadowRoot) {
      return;
    }

    let loginIntroTemplate = document.querySelector("#login-intro-template");
    let shadowRoot = this.attachShadow({ mode: "open" });
    document.l10n.connectRoot(shadowRoot);
    shadowRoot.appendChild(loginIntroTemplate.content.cloneNode(true));
  }

  focus() {
    let helpLink = this.shadowRoot.querySelector(".intro-help-link");
    helpLink.focus();
  }

  handleEvent(event) {
    if (
      event.currentTarget.classList.contains("intro-import-text") &&
      event.target.localName == "a"
    ) {
      let eventName =
        event.target.dataset.l10nName == "import-file-link"
          ? "AboutLoginsImportFromFile"
          : "AboutLoginsImportFromBrowser";
      document.dispatchEvent(
        new CustomEvent(eventName, {
          bubbles: true,
        })
      );
    }
    event.preventDefault();
  }

  updateState(syncState) {
    let l10nId = "about-logins-login-intro-heading-message";
    document.l10n.setAttributes(
      this.shadowRoot.querySelector(".heading"),
      l10nId
    );

    this.shadowRoot
      .querySelector(".illustration")
      .classList.toggle("logged-in", syncState.loggedIn);
    let supportURL =
      window.AboutLoginsUtils.supportBaseURL +
      "password-manager-remember-delete-edit-logins";
    this.shadowRoot
      .querySelector(".intro-help-link")
      .setAttribute("href", supportURL);

    let importClass = window.AboutLoginsUtils.fileImportEnabled
      ? ".intro-import-text.file-import"
      : ".intro-import-text.no-file-import";
    let importText = this.shadowRoot.querySelector(importClass);
    importText.addEventListener("click", this);
    importText.hidden = !window.AboutLoginsUtils.importVisible;
  }
}
customElements.define("login-intro", LoginIntro);
