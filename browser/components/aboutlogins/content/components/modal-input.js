/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import ReflectedFluentElement from "chrome://browser/content/aboutlogins/components/reflected-fluent-element.js";

export default class ModalInput extends ReflectedFluentElement {
  static get LOCKED_PASSWORD_DISPLAY() {
    return "••••••••";
  }

  connectedCallback() {
    if (this.children.length) {
      return;
    }

    let modalInputTemplate = document.querySelector("#modal-input-template");
    this.attachShadow({mode: "open"})
        .appendChild(modalInputTemplate.content.cloneNode(true));

    if (this.hasAttribute("value")) {
      this.value = this.getAttribute("value");
    }

    let unlockedValue = this.shadowRoot.querySelector(".unlocked-value");
    if (this.getAttribute("type") == "password") {
      unlockedValue.setAttribute("type", "password");
    }

    if (this.hasAttribute("required")) {
      unlockedValue.setAttribute("required", "");
    }

    this.shadowRoot.querySelector(".reveal-checkbox").addEventListener("click", this);
  }

  static get reflectedFluentIDs() {
    return ["reveal-checkbox-hide", "reveal-checkbox-show"];
  }

  static get observedAttributes() {
    return ["editing", "required", "type", "value"].concat(ModalInput.reflectedFluentIDs);
  }

  handleSpecialCaseFluentString(attrName) {
    if (attrName != "reveal-checkbox-hide" &&
        attrName != "reveal-checkbox-show") {
      return false;
    }

    this.updateRevealCheckboxTitle();
    return true;
  }

  attributeChangedCallback(attr, oldValue, newValue) {
    super.attributeChangedCallback(attr, oldValue, newValue);

    if (!this.shadowRoot) {
      return;
    }

    let lockedValue = this.shadowRoot.querySelector(".locked-value");
    let unlockedValue = this.shadowRoot.querySelector(".unlocked-value");

    switch (attr) {
      case "editing": {
        let isEditing = newValue !== null;
        if (!isEditing) {
          this.setAttribute("value", unlockedValue.value);
        }
        break;
      }
      case "required": {
        unlockedValue.toggleAttribute("required", this.hasAttribute("required"));
        break;
      }
      case "type": {
        if (newValue == "password") {
          lockedValue.textContent = this.constructor.LOCKED_PASSWORD_DISPLAY;
          unlockedValue.setAttribute("type", "password");
        } else {
          lockedValue.textContent = this.getAttribute("value");
          unlockedValue.setAttribute("type", "text");
        }
        break;
      }
      case "value": {
        this.value = newValue;
        break;
      }
    }
  }

  handleEvent(event) {
    if (event.type != "click" ||
        !event.target.classList.contains("reveal-checkbox")) {
      return;
    }

    let revealCheckbox = event.target;
    let lockedValue = this.shadowRoot.querySelector(".locked-value");
    let unlockedValue = this.shadowRoot.querySelector(".unlocked-value");
    if (revealCheckbox.checked) {
      lockedValue.textContent = this.value;
      unlockedValue.setAttribute("type", "text");
    } else {
      lockedValue.textContent = this.constructor.LOCKED_PASSWORD_DISPLAY;
      unlockedValue.setAttribute("type", "password");
    }
    this.updateRevealCheckboxTitle();
  }

  get value() {
    return this.hasAttribute("editing") ? this.shadowRoot.querySelector(".unlocked-value").value.trim()
                                        : this.getAttribute("value") || "";
  }

  set value(val) {
    if (this.getAttribute("value") != val) {
      this.setAttribute("value", val);
      return;
    }
    this.shadowRoot.querySelector(".unlocked-value").value = val;
    let lockedValue = this.shadowRoot.querySelector(".locked-value");
    if (this.getAttribute("type") == "password" && val && val.length) {
      lockedValue.textContent = this.constructor.LOCKED_PASSWORD_DISPLAY;
    } else {
      lockedValue.textContent = val;
    }
    let revealCheckbox = this.shadowRoot.querySelector(".reveal-checkbox");
    revealCheckbox.checked = false;
  }

  checkValidity() {
    if (!this.hasAttribute("required")) {
      return true;
    }
    let unlockedValue = this.shadowRoot.querySelector(".unlocked-value");
    return unlockedValue.checkValidity();
  }

  reportValidity() {
    if (!this.hasAttribute("required")) {
      return true;
    }
    let unlockedValue = this.shadowRoot.querySelector(".unlocked-value");
    return unlockedValue.reportValidity();
  }

  updateRevealCheckboxTitle() {
    let revealCheckbox = this.shadowRoot.querySelector(".reveal-checkbox");
    let labelAttr = revealCheckbox.checked ? "reveal-checkbox-hide"
                                           : "reveal-checkbox-show";
    revealCheckbox.setAttribute("aria-label", this.getAttribute(labelAttr));
    revealCheckbox.setAttribute("title", this.getAttribute(labelAttr));
  }
}
customElements.define("modal-input", ModalInput);
