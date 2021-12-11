/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import BasicCardOption from "../components/basic-card-option.js";
import CscInput from "../components/csc-input.js";
import HandleEventMixin from "../mixins/HandleEventMixin.js";
import RichPicker from "./rich-picker.js";
import paymentRequest from "../paymentRequest.js";

/* import-globals-from ../unprivileged-fallbacks.js */

/**
 * <payment-method-picker></payment-method-picker>
 * Container around add/edit links and <rich-select> with
 * <basic-card-option> listening to savedBasicCards.
 */

export default class PaymentMethodPicker extends HandleEventMixin(RichPicker) {
  constructor() {
    super();
    this.dropdown.setAttribute("option-type", "basic-card-option");
    this.securityCodeInput = new CscInput();
    this.securityCodeInput.className = "security-code-container";
    this.securityCodeInput.placeholder = this.dataset.cscPlaceholder;
    this.securityCodeInput.backTooltip = this.dataset.cscBackTooltip;
    this.securityCodeInput.frontTooltip = this.dataset.cscFrontTooltip;
    this.securityCodeInput.addEventListener("change", this);
    this.securityCodeInput.addEventListener("input", this);
  }

  connectedCallback() {
    super.connectedCallback();
    this.dropdown.after(this.securityCodeInput);
  }

  get fieldNames() {
    let fieldNames = [...BasicCardOption.recordAttributes];
    return fieldNames;
  }

  render(state) {
    let basicCards = paymentRequest.getBasicCards(state);
    let desiredOptions = [];
    for (let [guid, basicCard] of Object.entries(basicCards)) {
      let optionEl = this.dropdown.getOptionByValue(guid);
      if (!optionEl) {
        optionEl = document.createElement("option");
        optionEl.value = guid;
      }

      for (let key of BasicCardOption.recordAttributes) {
        let val = basicCard[key];
        if (val) {
          optionEl.setAttribute(key, val);
        } else {
          optionEl.removeAttribute(key);
        }
      }

      optionEl.textContent = BasicCardOption.formatSingleLineLabel(basicCard);
      desiredOptions.push(optionEl);
    }

    this.dropdown.popupBox.textContent = "";
    for (let option of desiredOptions) {
      this.dropdown.popupBox.appendChild(option);
    }

    // Update selectedness after the options are updated
    let selectedPaymentCardGUID = state[this.selectedStateKey];
    if (selectedPaymentCardGUID) {
      this.dropdown.value = selectedPaymentCardGUID;

      if (selectedPaymentCardGUID !== this.dropdown.value) {
        throw new Error(
          `The option ${selectedPaymentCardGUID} ` +
            `does not exist in the payment method picker`
        );
      }
    } else {
      this.dropdown.value = "";
    }

    let securityCodeState = state[this.selectedStateKey + "SecurityCode"];
    if (
      securityCodeState &&
      securityCodeState != this.securityCodeInput.value
    ) {
      this.securityCodeInput.defaultValue = securityCodeState;
    }

    let selectedCardType =
      (basicCards[selectedPaymentCardGUID] &&
        basicCards[selectedPaymentCardGUID]["cc-type"]) ||
      "";
    this.securityCodeInput.cardType = selectedCardType;

    super.render(state);
  }

  errorForSelectedOption(state) {
    let superError = super.errorForSelectedOption(state);
    if (superError) {
      return superError;
    }
    let selectedOption = this.selectedOption;
    if (!selectedOption) {
      return "";
    }

    let basicCardMethod = state.request.paymentMethods.find(
      method => method.supportedMethods == "basic-card"
    );
    let merchantNetworks =
      basicCardMethod &&
      basicCardMethod.data &&
      basicCardMethod.data.supportedNetworks;
    let acceptedNetworks =
      merchantNetworks || PaymentDialogUtils.getCreditCardNetworks();
    let selectedCard = paymentRequest.getBasicCards(state)[
      selectedOption.value
    ];
    let isSupported =
      selectedCard["cc-type"] &&
      acceptedNetworks.includes(selectedCard["cc-type"]);
    return isSupported ? "" : this.dataset.invalidLabel;
  }

  get selectedStateKey() {
    return this.getAttribute("selected-state-key");
  }

  onInput(event) {
    this.onInputOrChange(event);
  }

  onChange(event) {
    this.onInputOrChange(event);
  }

  onInputOrChange({ currentTarget }) {
    let selectedKey = this.selectedStateKey;
    let stateChange = {};

    if (!selectedKey) {
      return;
    }

    switch (currentTarget) {
      case this.dropdown: {
        stateChange[selectedKey] = this.dropdown.value;
        break;
      }
      case this.securityCodeInput: {
        stateChange[
          selectedKey + "SecurityCode"
        ] = this.securityCodeInput.value;
        break;
      }
      default: {
        return;
      }
    }

    this.requestStore.setState(stateChange);
  }

  onClick({ target }) {
    let nextState = {
      page: {
        id: "basic-card-page",
      },
      "basic-card-page": {
        selectedStateKey: this.selectedStateKey,
      },
    };

    switch (target) {
      case this.addLink: {
        nextState["basic-card-page"].guid = null;
        break;
      }
      case this.editLink: {
        let state = this.requestStore.getState();
        let selectedPaymentCardGUID = state[this.selectedStateKey];
        nextState["basic-card-page"].guid = selectedPaymentCardGUID;
        break;
      }
      default: {
        throw new Error("Unexpected onClick");
      }
    }

    this.requestStore.setState(nextState);
  }
}

customElements.define("payment-method-picker", PaymentMethodPicker);
