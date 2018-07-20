/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import "../vendor/custom-elements.min.js";

import PaymentStateSubscriberMixin from "../mixins/PaymentStateSubscriberMixin.js";
import paymentRequest from "../paymentRequest.js";

import "../components/currency-amount.js";
import "../components/payment-request-page.js";
import "./address-picker.js";
import "./address-form.js";
import "./basic-card-form.js";
import "./order-details.js";
import "./payment-method-picker.js";
import "./shipping-option-picker.js";

/* import-globals-from ../unprivileged-fallbacks.js */

/**
 * <payment-dialog></payment-dialog>
 */

export default class PaymentDialog extends PaymentStateSubscriberMixin(HTMLElement) {
  constructor() {
    super();
    this._template = document.getElementById("payment-dialog-template");
    this._cachedState = {};
  }

  connectedCallback() {
    let contents = document.importNode(this._template.content, true);
    this._hostNameEl = contents.querySelector("#host-name");

    this._cancelButton = contents.querySelector("#cancel");
    this._cancelButton.addEventListener("click", this.cancelRequest);

    this._payButton = contents.querySelector("#pay");
    this._payButton.addEventListener("click", this);

    this._viewAllButton = contents.querySelector("#view-all");
    this._viewAllButton.addEventListener("click", this);

    this._mainContainer = contents.getElementById("main-container");
    this._orderDetailsOverlay = contents.querySelector("#order-details-overlay");

    this._shippingAddressPicker = contents.querySelector("address-picker.shipping-related");
    this._shippingRelatedEls = contents.querySelectorAll(".shipping-related");
    this._payerRelatedEls = contents.querySelectorAll(".payer-related");
    this._payerAddressPicker = contents.querySelector("address-picker.payer-related");

    this._header = contents.querySelector("header");

    this._errorText = contents.querySelector("#error-text");

    this._disabledOverlay = contents.getElementById("disabled-overlay");

    this.appendChild(contents);

    super.connectedCallback();
  }

  disconnectedCallback() {
    this._cancelButton.removeEventListener("click", this.cancelRequest);
    this._payButton.removeEventListener("click", this.pay);
    this._viewAllButton.removeEventListener("click", this);
    super.disconnectedCallback();
  }

  handleEvent(event) {
    if (event.type == "click") {
      switch (event.target) {
        case this._viewAllButton:
          let orderDetailsShowing = !this.requestStore.getState().orderDetailsShowing;
          this.requestStore.setState({ orderDetailsShowing });
          break;
        case this._payButton:
          this.pay();
          break;
      }
    }
  }

  cancelRequest() {
    paymentRequest.cancel();
  }

  pay() {
    let {
      selectedPayerAddress,
      selectedPaymentCard,
      selectedPaymentCardSecurityCode,
    } = this.requestStore.getState();

    paymentRequest.pay({
      selectedPayerAddressGUID: selectedPayerAddress,
      selectedPaymentCardGUID: selectedPaymentCard,
      selectedPaymentCardSecurityCode,
    });
  }

  changeShippingAddress(shippingAddressGUID) {
    paymentRequest.changeShippingAddress({
      shippingAddressGUID,
    });
  }

  changeShippingOption(optionID) {
    paymentRequest.changeShippingOption({
      optionID,
    });
  }

  _getAdditionalDisplayItems(state) {
    let methodId = state.selectedPaymentCard;
    let modifier = paymentRequest.getModifierForPaymentMethod(state, methodId);
    if (modifier && modifier.additionalDisplayItems) {
      return modifier.additionalDisplayItems;
    }
    return [];
  }

  /**
   * Set some state from the privileged parent process.
   * Other elements that need to set state should use their own `this.requestStore.setState`
   * method provided by the `PaymentStateSubscriberMixin`.
   *
   * @param {object} state - See `PaymentsStore.setState`
   */
  setStateFromParent(state) {
    let oldAddresses = paymentRequest.getAddresses(this.requestStore.getState());
    this.requestStore.setState(state);

    // Check if any foreign-key constraints were invalidated.
    state = this.requestStore.getState();
    let {
      selectedPayerAddress,
      selectedPaymentCard,
      selectedShippingAddress,
      selectedShippingOption,
    } = state;
    let addresses = paymentRequest.getAddresses(state);
    let shippingOptions = state.request.paymentDetails.shippingOptions;
    let shippingAddress = selectedShippingAddress && addresses[selectedShippingAddress];
    let oldShippingAddress = selectedShippingAddress &&
                             oldAddresses[selectedShippingAddress];

    // Ensure `selectedShippingAddress` never refers to a deleted address.
    // We also compare address timestamps to notify about changes
    // made outside the payments UI.
    if (shippingAddress) {
      // invalidate the cached value if the address was modified
      if (oldShippingAddress &&
          shippingAddress.guid == oldShippingAddress.guid &&
          shippingAddress.timeLastModified != oldShippingAddress.timeLastModified) {
        delete this._cachedState.selectedShippingAddress;
      }
    } else if (selectedShippingAddress !== null) {
      // null out the `selectedShippingAddress` property if it is undefined,
      // or if the address it pointed to was removed from storage.
      log.debug("resetting invalid/deleted shipping address");
      this.requestStore.setState({
        selectedShippingAddress: null,
      });
    }

    // Ensure `selectedPaymentCard` never refers to a deleted payment card and refers
    // to a payment card if one exists.
    let basicCards = paymentRequest.getBasicCards(state);
    if (!basicCards[selectedPaymentCard]) {
      // Determining the initial selection is tracked in bug 1455789
      this.requestStore.setState({
        selectedPaymentCard: Object.keys(basicCards)[0] || null,
        selectedPaymentCardSecurityCode: null,
      });
    }

    // Ensure `selectedShippingOption` never refers to a deleted shipping option and
    // refers to a shipping option if one exists.
    if (shippingOptions && (!selectedShippingOption ||
                            !shippingOptions.find(option => option.id == selectedShippingOption))) {
      // Use the DOM's computed selected shipping option:
      selectedShippingOption = state.request.shippingOption;

      // Otherwise, default to selecting the first option:
      if (!selectedShippingOption && shippingOptions.length) {
        selectedShippingOption = shippingOptions[0].id;
      }
      this._cachedState.selectedShippingOption = selectedShippingOption;
      this.requestStore.setState({
        selectedShippingOption,
      });
    }

    // Ensure `selectedPayerAddress` never refers to a deleted address and refers
    // to an address if one exists.
    if (!addresses[selectedPayerAddress]) {
      this.requestStore.setState({
        selectedPayerAddress: Object.keys(addresses)[0] || null,
      });
    }
  }

  _renderPayButton(state) {
    this._payButton.disabled = state.changesPrevented;
    switch (state.completionState) {
      case "initial":
      case "processing":
      case "success":
      case "fail":
      case "unknown":
        break;
      default:
        throw new Error("Invalid completionState");
    }

    this._payButton.textContent = this._payButton.dataset[state.completionState + "Label"];
  }

  stateChangeCallback(state) {
    super.stateChangeCallback(state);

    // Don't dispatch change events for initial selectedShipping* changes at initialization
    // if requestShipping is false.
    if (state.request.paymentOptions.requestShipping) {
      if (state.selectedShippingAddress != this._cachedState.selectedShippingAddress) {
        this.changeShippingAddress(state.selectedShippingAddress);
      }

      if (state.selectedShippingOption != this._cachedState.selectedShippingOption) {
        this.changeShippingOption(state.selectedShippingOption);
      }
    }

    this._cachedState.selectedShippingAddress = state.selectedShippingAddress;
    this._cachedState.selectedShippingOption = state.selectedShippingOption;
  }

  render(state) {
    let request = state.request;
    let paymentDetails = request.paymentDetails;
    this._hostNameEl.textContent = request.topLevelPrincipal.URI.displayHost;

    let displayItems = request.paymentDetails.displayItems || [];
    let additionalItems = this._getAdditionalDisplayItems(state);
    this._viewAllButton.hidden = !displayItems.length && !additionalItems.length;

    let shippingType = state.request.paymentOptions.shippingType || "shipping";
    this._shippingAddressPicker.dataset.addAddressTitle =
      this.dataset[shippingType + "AddressTitleAdd"];
    this._shippingAddressPicker.dataset.editAddressTitle =
      this.dataset[shippingType + "AddressTitleEdit"];
    let addressPickerLabel = this._shippingAddressPicker.dataset[shippingType + "AddressLabel"];
    this._shippingAddressPicker.setAttribute("label", addressPickerLabel);

    let totalItem = paymentRequest.getTotalItem(state);
    let totalAmountEl = this.querySelector("#total > currency-amount");
    totalAmountEl.value = totalItem.amount.value;
    totalAmountEl.currency = totalItem.amount.currency;

    // Show the total header on the address and basic card pages only during
    // on-boarding(FTU) and on the payment summary page.
    this._header.hidden = !state.page.onboardingWizard && state.page.id != "payment-summary";

    this._orderDetailsOverlay.hidden = !state.orderDetailsShowing;
    this._errorText.textContent = paymentDetails.error;

    let paymentOptions = request.paymentOptions;
    for (let element of this._shippingRelatedEls) {
      element.hidden = !paymentOptions.requestShipping;
    }
    let payerRequested = paymentOptions.requestPayerName ||
                         paymentOptions.requestPayerEmail ||
                         paymentOptions.requestPayerPhone;
    for (let element of this._payerRelatedEls) {
      element.hidden = !payerRequested;
    }

    if (payerRequested) {
      let fieldNames = new Set(); // default: ["name", "tel", "email"]
      if (paymentOptions.requestPayerName) {
        fieldNames.add("name");
      }
      if (paymentOptions.requestPayerEmail) {
        fieldNames.add("email");
      }
      if (paymentOptions.requestPayerPhone) {
        fieldNames.add("tel");
      }
      this._payerAddressPicker.setAttribute("address-fields", [...fieldNames].join(" "));
    } else {
      this._payerAddressPicker.removeAttribute("address-fields");
    }
    this._payerAddressPicker.dataset.addAddressTitle = this.dataset.payerTitleAdd;
    this._payerAddressPicker.dataset.editAddressTitle = this.dataset.payerTitleEdit;

    this._renderPayButton(state);

    for (let page of this._mainContainer.querySelectorAll(":scope > .page")) {
      page.hidden = state.page.id != page.id;
    }

    let {
      changesPrevented,
      completionState,
    } = state;
    if (changesPrevented) {
      this.setAttribute("changes-prevented", "");
    } else {
      this.removeAttribute("changes-prevented");
    }
    this.setAttribute("completion-state", completionState);
    this._disabledOverlay.hidden = !changesPrevented;
  }
}

customElements.define("payment-dialog", PaymentDialog);
