/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from ../../../../../browser/extensions/formautofill/content/autofillEditForms.js*/
import AcceptedCards from "../components/accepted-cards.js";
import BillingAddressPicker from "./billing-address-picker.js";
import CscInput from "../components/csc-input.js";
import LabelledCheckbox from "../components/labelled-checkbox.js";
import PaymentRequestPage from "../components/payment-request-page.js";
import PaymentStateSubscriberMixin from "../mixins/PaymentStateSubscriberMixin.js";
import paymentRequest from "../paymentRequest.js";
import HandleEventMixin from "../mixins/HandleEventMixin.js";

/* import-globals-from ../unprivileged-fallbacks.js */

/**
 * <basic-card-form></basic-card-form>
 *
 * XXX: Bug 1446164 - This form isn't localized when used via this custom element
 * as it will be much easier to share the logic once we switch to Fluent.
 */

export default class BasicCardForm extends HandleEventMixin(
  PaymentStateSubscriberMixin(PaymentRequestPage)
) {
  constructor() {
    super();

    this.genericErrorText = document.createElement("div");
    this.genericErrorText.setAttribute("aria-live", "polite");
    this.genericErrorText.classList.add("page-error");

    this.cscInput = new CscInput({
      useAlwaysVisiblePlaceholder: true,
      inputId: "cc-csc",
    });

    this.persistCheckbox = new LabelledCheckbox();
    // The persist checkbox shouldn't be part of the record which gets saved so
    // exclude it from the form.
    this.persistCheckbox.form = "";
    this.persistCheckbox.className = "persist-checkbox";

    this.acceptedCardsList = new AcceptedCards();

    // page footer
    this.cancelButton = document.createElement("button");
    this.cancelButton.className = "cancel-button";
    this.cancelButton.addEventListener("click", this);

    this.backButton = document.createElement("button");
    this.backButton.className = "back-button";
    this.backButton.addEventListener("click", this);

    this.saveButton = document.createElement("button");
    this.saveButton.className = "save-button primary";
    this.saveButton.addEventListener("click", this);

    this.footer.append(this.cancelButton, this.backButton, this.saveButton);

    // The markup is shared with form autofill preferences.
    let url = "formautofill/editCreditCard.xhtml";
    this.promiseReady = this._fetchMarkup(url).then(doc => {
      this.form = doc.getElementById("form");
      return this.form;
    });
  }

  _fetchMarkup(url) {
    return new Promise((resolve, reject) => {
      let xhr = new XMLHttpRequest();
      xhr.responseType = "document";
      xhr.addEventListener("error", reject);
      xhr.addEventListener("load", evt => {
        resolve(xhr.response);
      });
      xhr.open("GET", url);
      xhr.send();
    });
  }

  _upgradeBillingAddressPicker() {
    let addressRow = this.form.querySelector(".billingAddressRow");
    let addressPicker = (this.billingAddressPicker = new BillingAddressPicker());

    // Wrap the existing <select> that the formHandler manages
    if (addressPicker.dropdown.popupBox) {
      addressPicker.dropdown.popupBox.remove();
    }
    addressPicker.dropdown.popupBox = this.form.querySelector(
      "#billingAddressGUID"
    );

    // Hide the original label as the address picker provide its own,
    // but we'll copy the localized textContent from it when rendering
    addressRow.querySelector(".label-text").hidden = true;

    addressPicker.dataset.addLinkLabel = this.dataset.addressAddLinkLabel;
    addressPicker.dataset.editLinkLabel = this.dataset.addressEditLinkLabel;
    addressPicker.dataset.fieldSeparator = this.dataset.addressFieldSeparator;
    addressPicker.dataset.addAddressTitle = this.dataset.billingAddressTitleAdd;
    addressPicker.dataset.editAddressTitle = this.dataset.billingAddressTitleEdit;
    addressPicker.dataset.invalidLabel = this.dataset.invalidAddressLabel;
    // break-after-nth-field, address-fields not needed here

    // this state is only used to carry the selected guid between pages;
    // the select#billingAddressGUID is the source of truth for the current value
    addressPicker.setAttribute(
      "selected-state-key",
      "basic-card-page|billingAddressGUID"
    );

    addressPicker.addLink.addEventListener("click", this);
    addressPicker.editLink.addEventListener("click", this);

    addressRow.appendChild(addressPicker);
  }

  connectedCallback() {
    this.promiseReady.then(form => {
      this.body.appendChild(form);

      let record = {};
      let addresses = [];
      this.formHandler = new EditCreditCard(
        {
          form,
        },
        record,
        addresses,
        {
          isCCNumber: PaymentDialogUtils.isCCNumber,
          getAddressLabel: PaymentDialogUtils.getAddressLabel,
          getSupportedNetworks: PaymentDialogUtils.getCreditCardNetworks,
        }
      );

      // The EditCreditCard constructor adds `change` and `input` event listeners on the same
      // element, which update field validity. By adding our event listeners after this
      // constructor, validity will be updated before our handlers get the event
      form.addEventListener("change", this);
      form.addEventListener("input", this);
      form.addEventListener("invalid", this);

      this._upgradeBillingAddressPicker();

      // The "invalid" event does not bubble and needs to be listened for on each
      // form element.
      for (let field of this.form.elements) {
        field.addEventListener("invalid", this);
      }

      // Replace the form-autofill cc-csc fields with our csc-input.
      let cscContainer = this.form.querySelector("#cc-csc-container");
      cscContainer.textContent = "";
      cscContainer.appendChild(this.cscInput);

      let billingAddressRow = this.form.querySelector(".billingAddressRow");
      form.insertBefore(this.persistCheckbox, billingAddressRow);
      form.insertBefore(this.acceptedCardsList, billingAddressRow);
      this.body.appendChild(this.genericErrorText);
      // Only call the connected super callback(s) once our markup is fully
      // connected, including the shared form fetched asynchronously.
      super.connectedCallback();
    });
  }

  render(state) {
    let {
      page,
      selectedShippingAddress,
      "basic-card-page": basicCardPage,
    } = state;

    if (this.id && page && page.id !== this.id) {
      log.debug(
        `BasicCardForm: no need to further render inactive page: ${page.id}`
      );
      return;
    }

    if (!basicCardPage.selectedStateKey) {
      throw new Error("A `selectedStateKey` is required");
    }

    let editing = !!basicCardPage.guid;
    this.cancelButton.textContent = this.dataset.cancelButtonLabel;
    this.backButton.textContent = this.dataset.backButtonLabel;
    if (editing) {
      this.saveButton.textContent = this.dataset.updateButtonLabel;
    } else {
      this.saveButton.textContent = this.dataset.nextButtonLabel;
    }

    this.cscInput.placeholder = this.dataset.cscPlaceholder;
    this.cscInput.frontTooltip = this.dataset.cscFrontInfoTooltip;
    this.cscInput.backTooltip = this.dataset.cscBackInfoTooltip;

    // The label text from the form isn't available until render() time.
    let labelText = this.form.querySelector(".billingAddressRow .label-text")
      .textContent;
    this.billingAddressPicker.setAttribute("label", labelText);

    this.persistCheckbox.label = this.dataset.persistCheckboxLabel;
    this.persistCheckbox.infoTooltip = this.dataset.persistCheckboxInfoTooltip;

    this.acceptedCardsList.label = this.dataset.acceptedCardsLabel;

    // The next line needs an onboarding check since we don't set previousId
    // when navigating to add/edit directly from the summary page.
    this.backButton.hidden = !page.previousId && page.onboardingWizard;
    this.cancelButton.hidden = !page.onboardingWizard;

    let record = {};
    let basicCards = paymentRequest.getBasicCards(state);
    let addresses = paymentRequest.getAddresses(state);

    this.genericErrorText.textContent = page.error;

    this.form.querySelector("#cc-number").disabled = editing;

    // The CVV fields should be hidden and disabled when editing.
    this.form.querySelector("#cc-csc-container").hidden = editing;
    this.cscInput.disabled = editing;

    // If a card is selected we want to edit it.
    if (editing) {
      this.pageTitleHeading.textContent = this.dataset.editBasicCardTitle;
      record = basicCards[basicCardPage.guid];
      if (!record) {
        throw new Error(
          "Trying to edit a non-existing card: " + basicCardPage.guid
        );
      }
      // When editing an existing record, prevent changes to persistence
      this.persistCheckbox.hidden = true;
    } else {
      this.pageTitleHeading.textContent = this.dataset.addBasicCardTitle;
      // Use a currently selected shipping address as the default billing address
      record.billingAddressGUID = basicCardPage.billingAddressGUID;
      if (!record.billingAddressGUID && selectedShippingAddress) {
        record.billingAddressGUID = selectedShippingAddress;
      }

      let {
        saveCreditCardDefaultChecked,
      } = PaymentDialogUtils.getDefaultPreferences();
      if (typeof saveCreditCardDefaultChecked != "boolean") {
        throw new Error(`Unexpected non-boolean value for saveCreditCardDefaultChecked from
          PaymentDialogUtils.getDefaultPreferences(): ${typeof saveCreditCardDefaultChecked}`);
      }
      // Adding a new record: default persistence to pref value when in a not-private session
      this.persistCheckbox.hidden = false;
      if (basicCardPage.hasOwnProperty("persistCheckboxValue")) {
        // returning to this page, use previous checked state
        this.persistCheckbox.checked = basicCardPage.persistCheckboxValue;
      } else {
        this.persistCheckbox.checked = state.isPrivate
          ? false
          : saveCreditCardDefaultChecked;
      }
    }

    this.formHandler.loadRecord(
      record,
      addresses,
      basicCardPage.preserveFieldValues
    );

    this.form.querySelector(".billingAddressRow").hidden = false;

    let billingAddressSelect = this.billingAddressPicker.dropdown;
    if (basicCardPage.billingAddressGUID) {
      billingAddressSelect.value = basicCardPage.billingAddressGUID;
    } else if (!editing) {
      if (paymentRequest.getAddresses(state)[selectedShippingAddress]) {
        billingAddressSelect.value = selectedShippingAddress;
      } else {
        let firstAddressGUID = Object.keys(addresses)[0];
        if (firstAddressGUID) {
          // Only set the value if we have a saved address to not mark the field
          // dirty and invalid on an add form with no saved addresses.
          billingAddressSelect.value = firstAddressGUID;
        }
      }
    }
    // Need to recalculate the populated state since
    // billingAddressSelect is updated after loadRecord.
    this.formHandler.updatePopulatedState(billingAddressSelect.popupBox);

    this.updateRequiredState();
    this.updateSaveButtonState();
  }

  onChange(evt) {
    let ccType = this.form.querySelector("#cc-type");
    this.cscInput.setAttribute("card-type", ccType.value);

    this.updateSaveButtonState();
  }

  onClick(evt) {
    switch (evt.target) {
      case this.cancelButton: {
        paymentRequest.cancel();
        break;
      }
      case this.billingAddressPicker.addLink:
      case this.billingAddressPicker.editLink: {
        // The address-picker has set state for the page to advance to, now set up the
        // necessary state for returning to and re-rendering this page
        let {
          "basic-card-page": basicCardPage,
          page,
        } = this.requestStore.getState();
        let nextState = {
          page: Object.assign({}, page, {
            previousId: "basic-card-page",
          }),
          "basic-card-page": {
            preserveFieldValues: true,
            guid: basicCardPage.guid,
            persistCheckboxValue: this.persistCheckbox.checked,
            selectedStateKey: basicCardPage.selectedStateKey,
          },
        };
        this.requestStore.setState(nextState);
        break;
      }
      case this.backButton: {
        let currentState = this.requestStore.getState();
        let {
          page,
          request,
          "shipping-address-page": shippingAddressPage,
          "billing-address-page": billingAddressPage,
          "basic-card-page": basicCardPage,
          selectedShippingAddress,
        } = currentState;

        let nextState = {
          page: {
            id: page.previousId || "payment-summary",
            onboardingWizard: page.onboardingWizard,
          },
        };

        if (page.onboardingWizard) {
          if (request.paymentOptions.requestShipping) {
            shippingAddressPage = Object.assign({}, shippingAddressPage, {
              guid: selectedShippingAddress,
            });
            Object.assign(nextState, {
              "shipping-address-page": shippingAddressPage,
            });
          } else {
            billingAddressPage = Object.assign({}, billingAddressPage, {
              guid: basicCardPage.billingAddressGUID,
            });
            Object.assign(nextState, {
              "billing-address-page": billingAddressPage,
            });
          }

          let basicCardPageState = Object.assign({}, basicCardPage, {
            preserveFieldValues: true,
          });
          delete basicCardPageState.persistCheckboxValue;

          Object.assign(nextState, {
            "basic-card-page": basicCardPageState,
          });
        }

        this.requestStore.setState(nextState);
        break;
      }
      case this.saveButton: {
        if (this.form.checkValidity()) {
          this.saveRecord();
        }
        break;
      }
      default: {
        throw new Error("Unexpected click target");
      }
    }
  }

  onInput(event) {
    event.target.setCustomValidity("");
    this.updateSaveButtonState();
  }

  onInvalid(event) {
    if (event.target instanceof HTMLFormElement) {
      this.onInvalidForm(event);
    } else {
      this.onInvalidField(event);
    }
  }

  /**
   * @param {Event} event - "invalid" event
   * Note: Keep this in-sync with the equivalent version in address-form.js
   */
  onInvalidField(event) {
    let field = event.target;
    let container = field.closest(`#${field.id}-container`);
    let errorTextSpan = paymentRequest.maybeCreateFieldErrorElement(container);
    errorTextSpan.textContent = field.validationMessage;
  }

  onInvalidForm() {
    this.saveButton.disabled = true;
  }

  updateSaveButtonState() {
    const INVALID_CLASS_NAME = "invalid-selected-option";
    let isValid =
      this.form.checkValidity() &&
      !this.billingAddressPicker.classList.contains(INVALID_CLASS_NAME);
    this.saveButton.disabled = !isValid;
  }

  updateRequiredState() {
    for (let field of this.form.elements) {
      let container = field.closest(".container");
      let span = container.querySelector(".label-text");
      if (!span) {
        // The billing address field doesn't use a label inside the field.
        continue;
      }
      span.setAttribute(
        "fieldRequiredSymbol",
        this.dataset.fieldRequiredSymbol
      );
      container.toggleAttribute("required", field.required && !field.disabled);
    }
  }

  async saveRecord() {
    let record = this.formHandler.buildFormObject();
    let currentState = this.requestStore.getState();
    let { tempBasicCards, "basic-card-page": basicCardPage } = currentState;
    let editing = !!basicCardPage.guid;

    if (
      editing
        ? basicCardPage.guid in tempBasicCards
        : !this.persistCheckbox.checked
    ) {
      record.isTemporary = true;
    }

    for (let editableFieldName of [
      "cc-name",
      "cc-exp-month",
      "cc-exp-year",
      "cc-type",
    ]) {
      record[editableFieldName] = record[editableFieldName] || "";
    }

    // Only save the card number if we're saving a new record, otherwise we'd
    // overwrite the unmasked card number with the masked one.
    if (!editing) {
      record["cc-number"] = record["cc-number"] || "";
    }

    // Never save the CSC in storage. Storage will throw and not save the record
    // if it is passed.
    delete record["cc-csc"];

    try {
      let { guid } = await paymentRequest.updateAutofillRecord(
        "creditCards",
        record,
        basicCardPage.guid
      );
      let { selectedStateKey } = currentState["basic-card-page"];
      if (!selectedStateKey) {
        throw new Error(
          `state["basic-card-page"].selectedStateKey is required`
        );
      }
      this.requestStore.setState({
        page: {
          id: "payment-summary",
        },
        [selectedStateKey]: guid,
        [selectedStateKey + "SecurityCode"]: this.cscInput.value,
      });
    } catch (ex) {
      log.warn("saveRecord: error:", ex);
      this.requestStore.setState({
        page: {
          id: "basic-card-page",
          error: this.dataset.errorGenericSave,
        },
      });
    }
  }
}

customElements.define("basic-card-form", BasicCardForm);
