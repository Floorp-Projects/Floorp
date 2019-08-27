/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export default class FxAccountsButton extends HTMLElement {
  connectedCallback() {
    if (this.shadowRoot) {
      return;
    }
    let template = document.querySelector("#fxaccounts-button-template");
    let shadowRoot = this.attachShadow({ mode: "open" });
    document.l10n.connectRoot(shadowRoot);
    shadowRoot.appendChild(template.content.cloneNode(true));

    this._avatarButton = shadowRoot.querySelector(".fxaccounts-avatar-button");
    this._extraText = shadowRoot.querySelector(".fxaccounts-extra-text");
    this._enableButton = shadowRoot.querySelector(".fxaccounts-enable-button");
    this._loggedOutView = shadowRoot.querySelector(".logged-out-view");
    this._loggedInView = shadowRoot.querySelector(".logged-in-view");
    this._emailText = shadowRoot.querySelector(".fxaccount-email");

    this._avatarButton.addEventListener("click", this);
    this._enableButton.addEventListener("click", this);

    this.render();
  }

  handleEvent(event) {
    if (event.currentTarget == this._avatarButton) {
      document.dispatchEvent(
        new CustomEvent("AboutLoginsSyncOptions", {
          bubbles: true,
        })
      );
      return;
    }
    if (event.target == this._enableButton) {
      document.dispatchEvent(
        new CustomEvent("AboutLoginsSyncEnable", {
          bubbles: true,
        })
      );
    }
  }

  render() {
    this._loggedOutView.hidden = !!this._loggedIn;
    this._loggedInView.hidden = !this._loggedIn;
    this._emailText.textContent = this._email;
    this._avatarButton.style.setProperty(
      "--avatar-url",
      `url(${this._avatarURL})`
    );
  }

  /**
   *
   * @param {object} state
   *                   loggedIn: {Boolean} FxAccount authentication
   *                             status.
   *                   email: {String} Email address used with FxAccount. Must
   *                          be empty if `loggedIn` is false.
   *                   avatarURL: {String} URL of account avatar. Must
   *                              be empty if `loggedIn` is false.
   */
  updateState(state) {
    this.hidden = false;
    this._loggedIn = state.loggedIn;
    this._email = state.email;
    this._avatarURL = state.avatarURL;

    this.render();
  }
}
customElements.define("fxaccounts-button", FxAccountsButton);
