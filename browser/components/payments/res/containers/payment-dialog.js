/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import HandleEventMixin from "../mixins/HandleEventMixin.js";
import PaymentStateSubscriberMixin from "../mixins/PaymentStateSubscriberMixin.js";
import paymentRequest from "../paymentRequest.js";

import "../components/currency-amount.js";
import "../components/payment-request-page.js";
import "../components/accepted-cards.js";
import "./address-picker.js";
import "./address-form.js";
import "./basic-card-form.js";
import "./completion-error-page.js";
import "./order-details.js";
import "./payment-method-picker.js";
import "./shipping-option-picker.js";

/* import-globals-from ../unprivileged-fallbacks.js */

/**
 * <payment-dialog></payment-dialog>
 *
 * Warning: Do not import this module from any other module as it will import
 * everything else (see above) and ruin element independence. This can stop
 * being exported once tests stop depending on it.
 */

export default class PaymentDialog extends HandleEventMixin(
  PaymentStateSubscriberMixin(HTMLElement)
) {
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
    this._orderDetailsOverlay = contents.querySelector(
      "#order-details-overlay"
    );

    this._shippingAddressPicker = contents.querySelector(
      "address-picker.shipping-related"
    );
    this._shippingOptionPicker = contents.querySelector(
      "shipping-option-picker"
    );
    this._shippingRelatedEls = contents.querySelectorAll(".shipping-related");
    this._payerRelatedEls = contents.querySelectorAll(".payer-related");
    this._payerAddressPicker = contents.querySelector(
      "address-picker.payer-related"
    );
    this._paymentMethodPicker = contents.querySelector("payment-method-picker");
    this._acceptedCardsList = contents.querySelector("accepted-cards");
    this._manageText = contents.querySelector(".manage-text");
    this._manageText.addEventListener("click", this);

    this._header = contents.querySelector("header");

    this._errorText = contents.querySelector("header > .page-error");

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

  onClick(event) {
    switch (event.currentTarget) {
      case this._viewAllButton:
        let orderDetailsShowing = !this.requestStore.getState()
          .orderDetailsShowing;
        this.requestStore.setState({ orderDetailsShowing });
        break;
      case this._payButton:
        this.pay();
        break;
      case this._manageText:
        if (event.target instanceof HTMLAnchorElement) {
          this.openPreferences(event);
        }
        break;
    }
  }

  openPreferences(event) {
    paymentRequest.openPreferences();
    event.preventDefault();
  }

  cancelRequest() {
    paymentRequest.cancel();
  }

  pay() {
    let state = this.requestStore.getState();
    let {
      selectedPayerAddress,
      selectedPaymentCard,
      selectedPaymentCardSecurityCode,
      selectedShippingAddress,
    } = state;

    let data = {
      selectedPaymentCardGUID: selectedPaymentCard,
      selectedPaymentCardSecurityCode,
    };

    data.selectedShippingAddressGUID = state.request.paymentOptions
      .requestShipping
      ? selectedShippingAddress
      : null;

    data.selectedPayerAddressGUID = this._isPayerRequested(
      state.request.paymentOptions
    )
      ? selectedPayerAddress
      : null;

    paymentRequest.pay(data);
  }

  /**
   * Called when the selectedShippingAddress or its properties are changed.
   * @param {string} shippingAddressGUID
   */
  changeShippingAddress(shippingAddressGUID) {
    // Clear shipping address merchant errors when the shipping address changes.
    let request = Object.assign({}, this.requestStore.getState().request);
    request.paymentDetails = Object.assign({}, request.paymentDetails);
    request.paymentDetails.shippingAddressErrors = {};
    this.requestStore.setState({ request });

    paymentRequest.changeShippingAddress({
      shippingAddressGUID,
    });
  }

  changeShippingOption(optionID) {
    paymentRequest.changeShippingOption({
      optionID,
    });
  }

  /**
   * Called when the selectedPaymentCard or its relevant properties or billingAddress are changed.
   * @param {string} selectedPaymentCardBillingAddressGUID
   */
  changePaymentMethod(selectedPaymentCardBillingAddressGUID) {
    // Clear paymentMethod merchant errors when the paymentMethod or billingAddress changes.
    let request = Object.assign({}, this.requestStore.getState().request);
    request.paymentDetails = Object.assign({}, request.paymentDetails);
    request.paymentDetails.paymentMethodErrors = null;
    this.requestStore.setState({ request });

    paymentRequest.changePaymentMethod({
      selectedPaymentCardBillingAddressGUID,
    });
  }

  /**
   * Called when the selectedPayerAddress or its relevant properties are changed.
   * @param {string} payerAddressGUID
   */
  changePayerAddress(payerAddressGUID) {
    // Clear payer address merchant errors when the payer address changes.
    let request = Object.assign({}, this.requestStore.getState().request);
    request.paymentDetails = Object.assign({}, request.paymentDetails);
    request.paymentDetails.payerErrors = {};
    this.requestStore.setState({ request });

    paymentRequest.changePayerAddress({
      payerAddressGUID,
    });
  }

  _isPayerRequested(paymentOptions) {
    return (
      paymentOptions.requestPayerName ||
      paymentOptions.requestPayerEmail ||
      paymentOptions.requestPayerPhone
    );
  }

  _getAdditionalDisplayItems(state) {
    let methodId = state.selectedPaymentCard;
    let modifier = paymentRequest.getModifierForPaymentMethod(state, methodId);
    if (modifier && modifier.additionalDisplayItems) {
      return modifier.additionalDisplayItems;
    }
    return [];
  }

  _updateCompleteStatus(state) {
    let { completeStatus } = state.request;
    switch (completeStatus) {
      case "fail":
      case "timeout":
      case "unknown":
        state.page = {
          id: `completion-${completeStatus}-error`,
        };
        state.changesPrevented = false;
        break;
      case "": {
        // When we get a DOM update for an updateWith() or retry() the completeStatus
        // is "" when we need to show non-final screens. Don't set the page as we
        // may be on a form instead of payment-summary
        state.changesPrevented = false;
        break;
      }
    }
    return state;
  }

  /**
   * Set some state from the privileged parent process.
   * Other elements that need to set state should use their own `this.requestStore.setState`
   * method provided by the `PaymentStateSubscriberMixin`.
   *
   * @param {object} state - See `PaymentsStore.setState`
   */
  // eslint-disable-next-line complexity
  async setStateFromParent(state) {
    let oldAddresses = paymentRequest.getAddresses(
      this.requestStore.getState()
    );
    let oldBasicCards = paymentRequest.getBasicCards(
      this.requestStore.getState()
    );
    if (state.request) {
      state = this._updateCompleteStatus(state);
    }
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
    let { paymentOptions } = state.request;

    if (paymentOptions.requestShipping) {
      let shippingOptions = state.request.paymentDetails.shippingOptions;
      let shippingAddress =
        selectedShippingAddress && addresses[selectedShippingAddress];
      let oldShippingAddress =
        selectedShippingAddress && oldAddresses[selectedShippingAddress];

      // Ensure `selectedShippingAddress` never refers to a deleted address.
      // We also compare address timestamps to notify about changes
      // made outside the payments UI.
      if (shippingAddress) {
        // invalidate the cached value if the address was modified
        if (
          oldShippingAddress &&
          shippingAddress.guid == oldShippingAddress.guid &&
          shippingAddress.timeLastModified !=
            oldShippingAddress.timeLastModified
        ) {
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

      // Ensure `selectedShippingOption` never refers to a deleted shipping option and
      // matches the merchant's selected option if the user hasn't made a choice.
      if (
        shippingOptions &&
        (!selectedShippingOption ||
          !shippingOptions.find(opt => opt.id == selectedShippingOption))
      ) {
        this._cachedState.selectedShippingOption = selectedShippingOption;
        this.requestStore.setState({
          // Use the DOM's computed selected shipping option:
          selectedShippingOption: state.request.shippingOption,
        });
      }
    }

    let basicCards = paymentRequest.getBasicCards(state);
    let oldPaymentMethod =
      selectedPaymentCard && oldBasicCards[selectedPaymentCard];
    let paymentMethod = selectedPaymentCard && basicCards[selectedPaymentCard];
    if (
      oldPaymentMethod &&
      paymentMethod.guid == oldPaymentMethod.guid &&
      paymentMethod.timeLastModified != oldPaymentMethod.timeLastModified
    ) {
      delete this._cachedState.selectedPaymentCard;
    } else {
      // Changes to the billing address record don't change the `timeLastModified`
      // on the card record so we have to check for changes to the address separately.

      let billingAddressGUID =
        paymentMethod && paymentMethod.billingAddressGUID;
      let billingAddress = billingAddressGUID && addresses[billingAddressGUID];
      let oldBillingAddress =
        billingAddressGUID && oldAddresses[billingAddressGUID];

      if (
        oldBillingAddress &&
        billingAddress &&
        billingAddress.timeLastModified != oldBillingAddress.timeLastModified
      ) {
        delete this._cachedState.selectedPaymentCard;
      }
    }

    // Ensure `selectedPaymentCard` never refers to a deleted payment card.
    if (selectedPaymentCard && !basicCards[selectedPaymentCard]) {
      this.requestStore.setState({
        selectedPaymentCard: null,
        selectedPaymentCardSecurityCode: null,
      });
    }

    if (this._isPayerRequested(state.request.paymentOptions)) {
      let payerAddress =
        selectedPayerAddress && addresses[selectedPayerAddress];
      let oldPayerAddress =
        selectedPayerAddress && oldAddresses[selectedPayerAddress];

      if (
        oldPayerAddress &&
        payerAddress &&
        ((paymentOptions.requestPayerName &&
          payerAddress.name != oldPayerAddress.name) ||
          (paymentOptions.requestPayerEmail &&
            payerAddress.email != oldPayerAddress.email) ||
          (paymentOptions.requestPayerPhone &&
            payerAddress.tel != oldPayerAddress.tel))
      ) {
        // invalidate the cached value if the payer address fields were modified
        delete this._cachedState.selectedPayerAddress;
      }

      // Ensure `selectedPayerAddress` never refers to a deleted address and refers
      // to an address if one exists.
      if (!addresses[selectedPayerAddress]) {
        this.requestStore.setState({
          selectedPayerAddress: Object.keys(addresses)[0] || null,
        });
      }
    }
  }

  _renderPayButton(state) {
    let completeStatus = state.request.completeStatus;
    switch (completeStatus) {
      case "processing":
      case "success":
      case "unknown": {
        this._payButton.disabled = true;
        this._payButton.textContent = this._payButton.dataset[
          completeStatus + "Label"
        ];
        break;
      }
      case "": {
        // initial/default state
        this._payButton.textContent = this._payButton.dataset.label;
        const INVALID_CLASS_NAME = "invalid-selected-option";
        this._payButton.disabled =
          (state.request.paymentOptions.requestShipping &&
            (!this._shippingAddressPicker.selectedOption ||
              this._shippingAddressPicker.classList.contains(
                INVALID_CLASS_NAME
              ) ||
              !this._shippingOptionPicker.selectedOption)) ||
          (this._isPayerRequested(state.request.paymentOptions) &&
            (!this._payerAddressPicker.selectedOption ||
              this._payerAddressPicker.classList.contains(
                INVALID_CLASS_NAME
              ))) ||
          !this._paymentMethodPicker.securityCodeInput.isValid ||
          !this._paymentMethodPicker.selectedOption ||
          this._paymentMethodPicker.classList.contains(INVALID_CLASS_NAME) ||
          state.changesPrevented;
        break;
      }
      case "fail":
      case "timeout": {
        // pay button is hidden in fail/timeout states.
        this._payButton.textContent = this._payButton.dataset.label;
        this._payButton.disabled = true;
        break;
      }
      default: {
        throw new Error(`Invalid completeStatus: ${completeStatus}`);
      }
    }
  }

  _renderPayerFields(state) {
    let paymentOptions = state.request.paymentOptions;
    let payerRequested = this._isPayerRequested(paymentOptions);
    let payerAddressForm = this.querySelector(
      "address-form[selected-state-key='selectedPayerAddress']"
    );

    for (let element of this._payerRelatedEls) {
      element.hidden = !payerRequested;
    }

    if (payerRequested) {
      let fieldNames = new Set();
      if (paymentOptions.requestPayerName) {
        fieldNames.add("name");
      }
      if (paymentOptions.requestPayerEmail) {
        fieldNames.add("email");
      }
      if (paymentOptions.requestPayerPhone) {
        fieldNames.add("tel");
      }
      let addressFields = [...fieldNames].join(" ");
      this._payerAddressPicker.setAttribute("address-fields", addressFields);
      if (payerAddressForm.form) {
        payerAddressForm.form.dataset.extraRequiredFields = addressFields;
      }

      // For the payer picker we want to have a line break after the name field (#1)
      // if all three fields are requested.
      if (fieldNames.size == 3) {
        this._payerAddressPicker.setAttribute("break-after-nth-field", 1);
      } else {
        this._payerAddressPicker.removeAttribute("break-after-nth-field");
      }
    } else {
      this._payerAddressPicker.removeAttribute("address-fields");
    }
  }

  stateChangeCallback(state) {
    super.stateChangeCallback(state);

    // Don't dispatch change events for initial selectedShipping* changes at initialization
    // if requestShipping is false.
    if (state.request.paymentOptions.requestShipping) {
      if (
        state.selectedShippingAddress !=
        this._cachedState.selectedShippingAddress
      ) {
        this.changeShippingAddress(state.selectedShippingAddress);
      }

      if (
        state.selectedShippingOption != this._cachedState.selectedShippingOption
      ) {
        this.changeShippingOption(state.selectedShippingOption);
      }
    }

    let selectedPaymentCard = state.selectedPaymentCard;
    let basicCards = paymentRequest.getBasicCards(state);
    let billingAddressGUID = (basicCards[selectedPaymentCard] || {})
      .billingAddressGUID;
    if (
      selectedPaymentCard != this._cachedState.selectedPaymentCard &&
      billingAddressGUID
    ) {
      // Update _cachedState to prevent an infinite loop when changePaymentMethod updates state.
      this._cachedState.selectedPaymentCard = state.selectedPaymentCard;
      this.changePaymentMethod(billingAddressGUID);
    }

    if (this._isPayerRequested(state.request.paymentOptions)) {
      if (
        state.selectedPayerAddress != this._cachedState.selectedPayerAddress
      ) {
        this.changePayerAddress(state.selectedPayerAddress);
      }
    }

    this._cachedState.selectedShippingAddress = state.selectedShippingAddress;
    this._cachedState.selectedShippingOption = state.selectedShippingOption;
    this._cachedState.selectedPayerAddress = state.selectedPayerAddress;
  }

  render(state) {
    let request = state.request;
    let paymentDetails = request.paymentDetails;
    this._hostNameEl.textContent = request.topLevelPrincipal.URI.displayHost;

    let displayItems = request.paymentDetails.displayItems || [];
    let additionalItems = this._getAdditionalDisplayItems(state);
    this._viewAllButton.hidden =
      !displayItems.length && !additionalItems.length;

    let shippingType = state.request.paymentOptions.shippingType || "shipping";
    let addressPickerLabel = this._shippingAddressPicker.dataset[
      shippingType + "AddressLabel"
    ];
    this._shippingAddressPicker.setAttribute("label", addressPickerLabel);
    let optionPickerLabel = this._shippingOptionPicker.dataset[
      shippingType + "OptionsLabel"
    ];
    this._shippingOptionPicker.setAttribute("label", optionPickerLabel);

    let shippingAddressForm = this.querySelector(
      "address-form[selected-state-key='selectedShippingAddress']"
    );
    shippingAddressForm.dataset.titleAdd = this.dataset[
      shippingType + "AddressTitleAdd"
    ];
    shippingAddressForm.dataset.titleEdit = this.dataset[
      shippingType + "AddressTitleEdit"
    ];

    let totalItem = paymentRequest.getTotalItem(state);
    let totalAmountEl = this.querySelector("#total > currency-amount");
    totalAmountEl.value = totalItem.amount.value;
    totalAmountEl.currency = totalItem.amount.currency;

    // Show the total header on the address and basic card pages only during
    // on-boarding(FTU) and on the payment summary page.
    this._header.hidden =
      !state.page.onboardingWizard && state.page.id != "payment-summary";

    this._orderDetailsOverlay.hidden = !state.orderDetailsShowing;
    let genericError = "";
    if (
      this._shippingAddressPicker.selectedOption &&
      (!request.paymentDetails.shippingOptions ||
        !request.paymentDetails.shippingOptions.length)
    ) {
      genericError = this._errorText.dataset[shippingType + "GenericError"];
    }
    this._errorText.textContent = paymentDetails.error || genericError;

    let paymentOptions = request.paymentOptions;
    for (let element of this._shippingRelatedEls) {
      element.hidden = !paymentOptions.requestShipping;
    }

    this._renderPayerFields(state);

    let isMac = /mac/i.test(navigator.platform);
    for (let manageTextEl of this._manageText.children) {
      manageTextEl.hidden = manageTextEl.dataset.os == "mac" ? !isMac : isMac;
      let link = manageTextEl.querySelector("a");
      // The href is only set to be exposed to accessibility tools so users know what will open.
      // The actual opening happens from the click event listener.
      link.href = "about:preferences#privacy-form-autofill";
    }

    this._renderPayButton(state);

    for (let page of this._mainContainer.querySelectorAll(":scope > .page")) {
      page.hidden = state.page.id != page.id;
    }

    this.toggleAttribute("changes-prevented", state.changesPrevented);
    this.setAttribute("complete-status", request.completeStatus);
    this._disabledOverlay.hidden = !state.changesPrevented;
  }
}

customElements.define("payment-dialog", PaymentDialog);
