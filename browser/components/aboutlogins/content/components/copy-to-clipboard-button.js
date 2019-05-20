/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals ReflectedFluentElement */

class CopyToClipboardButton extends ReflectedFluentElement {
  static get BUTTON_RESET_TIMEOUT() {
    return 5000;
  }

  constructor() {
    super();

    this._relatedInput = null;
  }

  connectedCallback() {
    if (this.children.length) {
      return;
    }

    let CopyToClipboardButtonTemplate = document.querySelector("#copy-to-clipboard-button-template");
    this.attachShadow({mode: "open"})
        .appendChild(CopyToClipboardButtonTemplate.content.cloneNode(true));

    this.shadowRoot.querySelector(".copy-button").addEventListener("click", this);
  }

  static get reflectedFluentIDs() {
    return ["copy-button-text", "copied-button-text"];
  }

  static get observedAttributes() {
    return CopyToClipboardButton.reflectedFluentIDs;
  }

  handleSpecialCaseFluentString(attrName) {
    if (attrName != "copied-button-text" &&
        attrName != "copy-button-text") {
      return false;
    }

    let span = this.shadowRoot.querySelector("." + attrName);
    span.textContent = this.getAttribute(attrName);
    return true;
  }

  handleEvent(event) {
    let copyButton = this.shadowRoot.querySelector(".copy-button");
    if (event.type != "click" || event.currentTarget != copyButton) {
      return;
    }

    copyButton.disabled = true;
    navigator.clipboard.writeText(this._relatedInput.value).then(() => {
      this.setAttribute("copied", "");
      setTimeout(() => {
        copyButton.disabled = false;
        this.removeAttribute("copied");
      }, CopyToClipboardButton.BUTTON_RESET_TIMEOUT);
    }, () => copyButton.disabled = false);
  }

  set relatedInput(val) {
    this._relatedInput = val;
  }
}
customElements.define("copy-to-clipboard-button", CopyToClipboardButton);
