/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from ../../../../../browser/extensions/formautofill/content/autofillEditForms.js*/
import LabelledCheckbox from "../components/labelled-checkbox.js";
import PaymentDialog from "./payment-dialog.js";
import PaymentRequestPage from "../components/payment-request-page.js";
import PaymentStateSubscriberMixin from "../mixins/PaymentStateSubscriberMixin.js";
import paymentRequest from "../paymentRequest.js";
/* import-globals-from ../unprivileged-fallbacks.js */

/**
 * <address-form></address-form>
 *
 * Don't use document.getElementById or document.querySelector* to access form
 * elements, use querySelector on `this` or `this.form` instead so that elements
 * can be found before the element is connected.
 *
 * XXX: Bug 1446164 - This form isn't localized when used via this custom element
 * as it will be much easier to share the logic once we switch to Fluent.
 */

export default class AddressForm extends PaymentStateSubscriberMixin(PaymentRequestPage) {
  constructor() {
    super();

    this.genericErrorText = document.createElement("div");
    this.genericErrorText.setAttribute("aria-live", "polite");
    this.genericErrorText.classList.add("page-error");

    this.cancelButton = document.createElement("button");
    this.cancelButton.className = "cancel-button";
    this.cancelButton.addEventListener("click", this);

    this.backButton = document.createElement("button");
    this.backButton.className = "back-button";
    this.backButton.addEventListener("click", this);

    this.saveButton = document.createElement("button");
    this.saveButton.className = "save-button primary";
    this.saveButton.addEventListener("click", this);

    this.persistCheckbox = new LabelledCheckbox();
    this.persistCheckbox.className = "persist-checkbox";

    // Combination of AddressErrors and PayerErrorFields as keys
    this._errorFieldMap = {
      addressLine: "#street-address",
      city: "#address-level2",
      country: "#country",
      email: "#email",
      // Bug 1472283 is on file to support
      // additional-name and family-name.
      // XXX: For now payer name errors go on the family-name and address-errors
      //      go on the given-name so they don't overwrite each other.
      name: "#family-name",
      organization: "#organization",
      phone: "#tel",
      postalCode: "#postal-code",
      // Bug 1472283 is on file to support
      // additional-name and family-name.
      recipient: "#given-name",
      region: "#address-level1",
    };

    // The markup is shared with form autofill preferences.
    let url = "formautofill/editAddress.xhtml";
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

  connectedCallback() {
    this.promiseReady.then(form => {
      this.body.appendChild(form);

      let record = undefined;
      this.formHandler = new EditAddress({
        form,
      }, record, {
        DEFAULT_REGION: PaymentDialogUtils.DEFAULT_REGION,
        getFormFormat: PaymentDialogUtils.getFormFormat,
        countries: PaymentDialogUtils.countries,
      });

      // The EditAddress constructor adds `input` event listeners on the same element,
      // which update field validity. By adding our event listeners after this constructor,
      // validity will be updated before our handlers get the event
      this.form.addEventListener("input", this);
      this.form.addEventListener("invalid", this);
      this.form.addEventListener("change", this);

      // The "invalid" event does not bubble and needs to be listened for on each
      // form element.
      for (let field of this.form.elements) {
        field.addEventListener("invalid", this);
      }

      this.body.appendChild(this.persistCheckbox);
      this.body.appendChild(this.genericErrorText);

      this.footer.appendChild(this.cancelButton);
      this.footer.appendChild(this.backButton);
      this.footer.appendChild(this.saveButton);
      // Only call the connected super callback(s) once our markup is fully
      // connected, including the shared form fetched asynchronously.
      super.connectedCallback();
    });
  }

  render(state) {
    let record;
    let {
      page,
      "address-page": addressPage,
    } = state;

    if (this.id && page && page.id !== this.id) {
      log.debug(`AddressForm: no need to further render inactive page: ${page.id}`);
      return;
    }

    let editing = !!addressPage.guid;
    this.cancelButton.textContent = this.dataset.cancelButtonLabel;
    this.backButton.textContent = this.dataset.backButtonLabel;
    if (editing) {
      this.saveButton.textContent = this.dataset.updateButtonLabel;
    } else {
      this.saveButton.textContent = this.dataset.nextButtonLabel;
    }

    this.persistCheckbox.label = this.dataset.persistCheckboxLabel;
    this.persistCheckbox.infoTooltip = this.dataset.persistCheckboxInfoTooltip;

    this.backButton.hidden = page.onboardingWizard;
    this.cancelButton.hidden = !page.onboardingWizard;

    this.pageTitleHeading.textContent = addressPage.title;
    this.genericErrorText.textContent = page.error;

    let addresses = paymentRequest.getAddresses(state);

    // If an address is selected we want to edit it.
    if (editing) {
      record = addresses[addressPage.guid];
      if (!record) {
        throw new Error("Trying to edit a non-existing address: " + addressPage.guid);
      }
      // When editing an existing record, prevent changes to persistence
      this.persistCheckbox.hidden = true;
    } else {
      let {saveAddressDefaultChecked} = PaymentDialogUtils.getDefaultPreferences();
      if (typeof saveAddressDefaultChecked != "boolean") {
        throw new Error(`Unexpected non-boolean value for saveAddressDefaultChecked from
          PaymentDialogUtils.getDefaultPreferences(): ${typeof saveAddressDefaultChecked}`);
      }
      // Adding a new record: default persistence to the pref value when in a not-private session
      this.persistCheckbox.hidden = false;
      this.persistCheckbox.checked = state.isPrivate ? false :
                                                       saveAddressDefaultChecked;
    }

    if (addressPage.addressFields) {
      this.form.dataset.addressFields = addressPage.addressFields;
    } else {
      this.form.dataset.addressFields = "mailing-address tel";
    }
    this.formHandler.loadRecord(record);

    // Add validation to some address fields
    this.updateRequiredState();

    // Show merchant errors for the appropriate address form.
    let merchantFieldErrors = AddressForm.merchantFieldErrorsForForm(state,
                                                                     addressPage.selectedStateKey);
    for (let [errorName, errorSelector] of Object.entries(this._errorFieldMap)) {
      let container = this.form.querySelector(errorSelector + "-container");
      let field = this.form.querySelector(errorSelector);
      // Never show errors on an 'add' screen as they would be for a different address.
      let errorText = (editing && merchantFieldErrors && merchantFieldErrors[errorName]) || "";
      field.setCustomValidity(errorText);
      let span = PaymentDialog.maybeCreateFieldErrorElement(container);
      span.textContent = errorText;
    }

    this.updateSaveButtonState();
  }

  handleEvent(event) {
    switch (event.type) {
      case "change": {
        this.updateSaveButtonState();
        break;
      }
      case "click": {
        this.onClick(event);
        break;
      }
      case "input": {
        this.onInput(event);
        break;
      }
      case "invalid": {
        if (event.target instanceof HTMLFormElement) {
          this.onInvalidForm(event);
          break;
        }

        this.onInvalidField(event);
        break;
      }
    }
  }

  onClick(evt) {
    switch (evt.target) {
      case this.cancelButton: {
        paymentRequest.cancel();
        break;
      }
      case this.backButton: {
        let currentState = this.requestStore.getState();
        const previousId = currentState.page.previousId;
        let state = {
          page: {
            id: previousId || "payment-summary",
          },
        };
        if (previousId) {
          state[previousId] = Object.assign({}, currentState[previousId], {
            preserveFieldValues: true,
          });
        }
        this.requestStore.setState(state);
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

  /**
   * @param {Event} event - "invalid" event
   * Note: Keep this in-sync with the equivalent version in basic-card-form.js
   */
  onInvalidField(event) {
    let field = event.target;
    let container = field.closest(`#${field.id}-container`);
    let errorTextSpan = PaymentDialog.maybeCreateFieldErrorElement(container);
    errorTextSpan.textContent = field.validationMessage;
  }

  onInvalidForm() {
    this.saveButton.disabled = true;
  }

  updateRequiredState() {
    for (let field of this.form.elements) {
      let container = field.closest(`#${field.id}-container`);
      if (field.localName == "button" || !container) {
        continue;
      }
      let span = container.querySelector(".label-text");
      span.setAttribute("fieldRequiredSymbol", this.dataset.fieldRequiredSymbol);
      let required = field.required && !field.disabled;
      if (required) {
        container.setAttribute("required", "true");
      } else {
        container.removeAttribute("required");
      }
    }
  }

  updateSaveButtonState() {
    this.saveButton.disabled = !this.form.checkValidity();
  }

  async saveRecord() {
    let record = this.formHandler.buildFormObject();
    let currentState = this.requestStore.getState();
    let {
      page,
      tempAddresses,
      savedBasicCards,
      "address-page": addressPage,
    } = currentState;
    let editing = !!addressPage.guid;

    if (editing ? (addressPage.guid in tempAddresses) : !this.persistCheckbox.checked) {
      record.isTemporary = true;
    }

    let successStateChange;
    const previousId = page.previousId;
    if (page.onboardingWizard && !Object.keys(savedBasicCards).length) {
      successStateChange = {
        "basic-card-page": {
          selectedStateKey: "selectedPaymentCard",
          // Preserve field values as the user may have already edited the card
          // page and went back to the address page to make a correction.
          preserveFieldValues: true,
        },
        page: {
          id: "basic-card-page",
          previousId: "address-page",
          onboardingWizard: page.onboardingWizard,
        },
      };
    } else {
      successStateChange = {
        page: {
          id: previousId || "payment-summary",
          onboardingWizard: page.onboardingWizard,
        },
      };
    }

    if (previousId) {
      successStateChange[previousId] = Object.assign({}, currentState[previousId]);
      successStateChange[previousId].preserveFieldValues = true;
    }

    try {
      let {guid} = await paymentRequest.updateAutofillRecord("addresses", record, addressPage.guid);
      let selectedStateKey = addressPage.selectedStateKey;

      if (selectedStateKey.length == 1) {
        Object.assign(successStateChange, {
          [selectedStateKey[0]]: guid,
        });
      } else if (selectedStateKey.length == 2) {
        // Need to keep properties like preserveFieldValues from getting removed.
        let subObj = Object.assign({}, successStateChange[selectedStateKey[0]]);
        subObj[selectedStateKey[1]] = guid;
        Object.assign(successStateChange, {
          [selectedStateKey[0]]: subObj,
        });
      } else {
        throw new Error(`selectedStateKey not supported: '${selectedStateKey}'`);
      }

      this.requestStore.setState(successStateChange);
    } catch (ex) {
      log.warn("saveRecord: error:", ex);
      this.requestStore.setState({
        page: {
          id: "address-page",
          onboardingWizard: page.onboardingWizard,
          error: this.dataset.errorGenericSave,
        },
      });
    }
  }

  /**
   * Get the dictionary of field-specific merchant errors relevant to the
   * specific form identified by the state key.
   * @param {object} state The application state
   * @param {string[]} stateKey The key in state to return address errors for.
   * @returns {object} with keys as PaymentRequest field names and values of
   *                   merchant-provided error strings.
   */
  static merchantFieldErrorsForForm(state, stateKey) {
    let {paymentDetails} = state.request;
    switch (stateKey.join("|")) {
      case "selectedShippingAddress": {
        return paymentDetails.shippingAddressErrors;
      }
      case "selectedPayerAddress": {
        return paymentDetails.payer;
      }
      case "basic-card-page|billingAddressGUID": {
        // `paymentMethod` can be null.
        return (paymentDetails.paymentMethod
                && paymentDetails.paymentMethod.billingAddress) || {};
      }
      default: {
        throw new Error("Unknown selectedStateKey");
      }
    }
  }
}

customElements.define("address-form", AddressForm);
