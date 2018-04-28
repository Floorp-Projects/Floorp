/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import ObservedPropertiesMixin from "../mixins/ObservedPropertiesMixin.js";
import RichOption from "./rich-option.js";

/**
 * <rich-select>
 *  <rich-option></rich-option>
 * </rich-select>
 *
 * Note: The only supported way to change the selected option is via the
 *       `selectedOption` setter.
 */
export default class RichSelect extends ObservedPropertiesMixin(HTMLElement) {
  static get observedAttributes() {
    return [
      "open",
      "disabled",
      "hidden",
    ];
  }

  constructor() {
    super();

    this.addEventListener("blur", this);
    this.addEventListener("click", this);
    this.addEventListener("keydown", this);

    this.popupBox = document.createElement("div");
    this.popupBox.classList.add("rich-select-popup-box");

    this._mutationObserver = new MutationObserver((mutations) => {
      for (let mutation of mutations) {
        for (let addedNode of mutation.addedNodes) {
          if (addedNode.nodeType != Node.ELEMENT_NODE ||
              !addedNode.matches(".rich-option:not(.rich-select-selected-clone)")) {
            continue;
          }
          // Move the added rich option to the popup.
          this.popupBox.appendChild(addedNode);
        }
      }
    });
    this._mutationObserver.observe(this, {
      childList: true,
    });
  }

  connectedCallback() {
    this.tabIndex = 0;
    this.appendChild(this.popupBox);

    // Move options initially placed inside the select to the popup box.
    let options = this.querySelectorAll(":scope > .rich-option:not(.rich-select-selected-clone)");
    for (let option of options) {
      this.popupBox.appendChild(option);
    }

    this.render();
  }

  get selectedOption() {
    return this.popupBox.querySelector(":scope > [selected]");
  }

  /**
   * This is the only supported method of changing the selected option. Do not
   * manipulate the `selected` property or attribute on options directly.
   * @param {HTMLOptionElement} option
   */
  set selectedOption(option) {
    for (let child of this.popupBox.children) {
      child.selected = child == option;
    }

    this.render();
  }

  getOptionByValue(value) {
    return this.popupBox.querySelector(`:scope > [value="${CSS.escape(value)}"]`);
  }

  handleEvent(event) {
    switch (event.type) {
      case "blur": {
        this.onBlur(event);
        break;
      }
      case "click": {
        this.onClick(event);
        break;
      }
      case "keydown": {
        this.onKeyDown(event);
        break;
      }
    }
  }

  onBlur(event) {
    if (event.target == this) {
      this.open = false;
    }
  }

  onClick(event) {
    if (event.button != 0) {
      return;
    }
    // Cache the state of .open since the payment-method-picker change handler
    // may cause onBlur to change .open to false and cause !this.open to change.
    let isOpen = this.open;

    let option = event.target.closest(".rich-option");
    if (isOpen && option && !option.matches(".rich-select-selected-clone") && !option.selected) {
      this.selectedOption = option;
      this._dispatchChangeEvent();
    }
    this.open = !isOpen;
  }

  onKeyDown(event) {
    if (event.key == " ") {
      this.open = !this.open;
    } else if (event.key == "ArrowDown") {
      let selectedOption = this.selectedOption;
      let next = selectedOption.nextElementSibling;
      if (next) {
        next.selected = true;
        selectedOption.selected = false;
        this._dispatchChangeEvent();
      }
    } else if (event.key == "ArrowUp") {
      let selectedOption = this.selectedOption;
      let next = selectedOption.previousElementSibling;
      if (next) {
        next.selected = true;
        selectedOption.selected = false;
        this._dispatchChangeEvent();
      }
    } else if (event.key == "Enter" ||
               event.key == "Escape") {
      this.open = false;
    }
  }

  /**
   * Only dispatched upon a user-initiated change.
   */
  _dispatchChangeEvent() {
    let changeEvent = document.createEvent("UIEvent");
    changeEvent.initEvent("change", true, true);
    this.dispatchEvent(changeEvent);
  }

  _optionsAreEquivalent(a, b) {
    if (!a || !b) {
      return false;
    }

    let aAttrs = a.constructor.observedAttributes;
    let bAttrs = b.constructor.observedAttributes;
    if (aAttrs.length != bAttrs.length) {
      return false;
    }

    for (let aAttr of aAttrs) {
      if (aAttr == "selected") {
        continue;
      }
      if (a.getAttribute(aAttr) != b.getAttribute(aAttr)) {
        return false;
      }
    }

    return true;
  }

  render() {
    let selectedChild;
    for (let child of this.popupBox.children) {
      if (child.selected) {
        selectedChild = child;
        break;
      }
    }

    let selectedClone = this.querySelector(":scope > .rich-select-selected-clone");
    if (this._optionsAreEquivalent(selectedClone, selectedChild)) {
      return;
    }

    if (selectedClone) {
      selectedClone.remove();
    }

    if (selectedChild) {
      selectedClone = selectedChild.cloneNode(false);
      selectedClone.removeAttribute("id");
      selectedClone.removeAttribute("selected");
    } else {
      selectedClone = new RichOption();
      selectedClone.textContent = "(None selected)";
    }
    selectedClone.classList.add("rich-select-selected-clone");
    selectedClone = this.appendChild(selectedClone);
  }
}

customElements.define("rich-select", RichSelect);
