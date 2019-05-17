/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals ReflectedFluentElement */

class CopyToClipboardButton extends ReflectedFluentElement {
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
    if (event.type != "click") {
      return;
    }

    this.setAttribute("copied", "");
  }
}
customElements.define("copy-to-clipboard-button", CopyToClipboardButton);
