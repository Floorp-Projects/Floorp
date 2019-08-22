/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export default class LoginFooter extends HTMLElement {
  connectedCallback() {
    if (this.shadowRoot) {
      return;
    }

    let LoginFooterTemplate = document.querySelector("#login-footer-template");
    let shadowRoot = this.attachShadow({ mode: "open" });
    document.l10n.connectRoot(shadowRoot);
    shadowRoot.appendChild(LoginFooterTemplate.content.cloneNode(true));

    this.shadowRoot.querySelector(".app-store").addEventListener("click", this);
    this.shadowRoot
      .querySelector(".play-store")
      .addEventListener("click", this);
    this.shadowRoot.querySelector(".close").addEventListener("click", this);
    this._imageAppStore = shadowRoot.querySelector(".image-app-store");
    this._imagePlayStore = shadowRoot.querySelector(".image-play-store");
  }

  handleEvent(event) {
    switch (event.type) {
      case "click": {
        let target = event.currentTarget;
        let classList = target.classList;

        if (
          classList.contains("play-store") ||
          classList.contains("app-store") ||
          classList.contains("close")
        ) {
          let eventName = target.dataset.eventName;
          const linkTrackingSource = "Footer_Menu";
          document.dispatchEvent(
            new CustomEvent(eventName, {
              bubbles: true,
              detail: linkTrackingSource,
            })
          );

          if (classList.contains("close")) {
            this.shadowRoot.host.hidden = true;
          }
        }
        break;
      }
    }
  }

  _setAppStoreImage(lang) {
    const currLang = lang.toLowerCase();
    let appStoreLink =
      "chrome://browser/content/aboutlogins/third-party/app-store/app_" +
      currLang +
      ".png";
    this._imageAppStore.setAttribute("src", appStoreLink);
  }

  _setPlayStoreImage(lang) {
    const currLang = lang.toLowerCase();
    let playStoreLink =
      "chrome://browser/content/aboutlogins/third-party/play-store/play_" +
      currLang +
      ".png";
    this._imagePlayStore.setAttribute("src", playStoreLink);
  }

  showStoreIconsForLocales(appLocales) {
    this._setAppStoreImage(appLocales.appStoreBadge[0]);
    this._setPlayStoreImage(appLocales.playStoreBadge[0]);
  }
}
customElements.define("login-footer", LoginFooter);
