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

  set supportURL(val) {
    this.shadowRoot.querySelector(".intro-help-link").setAttribute("href", val);
  }
}
customElements.define("login-intro", LoginIntro);
