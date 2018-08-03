/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from ../../../../../browser/extensions/formautofill/content/autofillEditForms.js*/
import LabelledCheckbox from "../components/labelled-checkbox.js";
import PaymentRequestPage from "../components/payment-request-page.js";
import PaymentStateSubscriberMixin from "../mixins/PaymentStateSubscriberMixin.js";
import paymentRequest from "../paymentRequest.js";

/* import-globals-from ../unprivileged-fallbacks.js */

/**
 * <basic-card-form></basic-card-form>
 *
 * XXX: Bug 1446164 - This form isn't localized when used via this custom element
 * as it will be much easier to share the logic once we switch to Fluent.
 */

export default class BasicCardForm extends PaymentStateSubscriberMixin(PaymentRequestPage) {
  constructor() {
    super();

    this.genericErrorText = document.createElement("div");
    this.genericErrorText.setAttribute("aria-live", "polite");
    this.genericErrorText.classList.add("page-error");

    this.addressAddLink = document.createElement("a");
    this.addressAddLink.className = "add-link";
    this.addressAddLink.href = "javascript:void(0)";
    this.addressAddLink.addEventListener("click", this);
    this.addressEditLink = document.createElement("a");
    this.addressEditLink.className = "edit-link";
    this.addressEditLink.href = "javascript:void(0)";
    this.addressEditLink.addEventListener("click", this);

    this.persistCheckbox = new LabelledCheckbox();
    this.persistCheckbox.className = "persist-checkbox";

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

  connectedCallback() {
    this.promiseReady.then(form => {
      this.body.appendChild(form);

      let record = {};
      let addresses = [];
      this.formHandler = new EditCreditCard({
        form,
      }, record, addresses, {
        isCCNumber: PaymentDialogUtils.isCCNumber,
        getAddressLabel: PaymentDialogUtils.getAddressLabel,
      });

      // The EditCreditCard constructor adds input event listeners on the same element,
      // which update field validity. By adding our event listeners after this constructor,
      // validity will be updated before our handlers get the event
      form.addEventListener("input", this);
      form.addEventListener("invalid", this);

      let fragment = document.createDocumentFragment();
      fragment.append(this.addressAddLink);
      fragment.append(" ");
      fragment.append(this.addressEditLink);
      let billingAddressRow = this.form.querySelector(".billingAddressRow");
      billingAddressRow.appendChild(fragment);

      this.body.appendChild(this.persistCheckbox);
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
      log.debug(`BasicCardForm: no need to further render inactive page: ${page.id}`);
      return;
    }

    let editing = !!basicCardPage.guid;
    this.cancelButton.textContent = this.dataset.cancelButtonLabel;
    this.backButton.textContent = this.dataset.backButtonLabel;
    this.saveButton.textContent = editing ? this.dataset.updateButtonLabel :
                                            this.dataset.addButtonLabel;
    this.persistCheckbox.label = this.dataset.persistCheckboxLabel;
    this.addressAddLink.textContent = this.dataset.addressAddLinkLabel;
    this.addressEditLink.textContent = this.dataset.addressEditLinkLabel;

    // The next line needs an onboarding check since we don't set previousId
    // when navigating to add/edit directly from the summary page.
    this.backButton.hidden = !page.previousId && page.onboardingWizard;
    this.cancelButton.hidden = !page.onboardingWizard;

    let record = {};
    let basicCards = paymentRequest.getBasicCards(state);
    let addresses = paymentRequest.getAddresses(state);

    this.genericErrorText.textContent = page.error;

    this.form.querySelector("#cc-number").disabled = editing;

    // If a card is selected we want to edit it.
    if (editing) {
      this.pageTitleHeading.textContent = this.dataset.editBasicCardTitle;
      record = basicCards[basicCardPage.guid];
      if (!record) {
        throw new Error("Trying to edit a non-existing card: " + basicCardPage.guid);
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

      let defaults = PaymentDialogUtils.getDefaultPreferences();
      // Adding a new record: default persistence to pref value when in a not-private session
      this.persistCheckbox.hidden = false;
      if (basicCardPage.hasOwnProperty("persistCheckboxValue")) {
        // returning to this page, use previous checked state
        this.persistCheckbox.checked = basicCardPage.persistCheckboxValue;
      } else {
        this.persistCheckbox.checked = state.isPrivate ? false :
                                                         defaults.saveCreditCardDefaultChecked;
      }
    }

    this.formHandler.loadRecord(record, addresses, basicCardPage.preserveFieldValues);

    this.form.querySelector(".billingAddressRow").hidden = false;

    let billingAddressSelect = this.form.querySelector("#billingAddressGUID");
    if (basicCardPage.billingAddressGUID) {
      billingAddressSelect.value = basicCardPage.billingAddressGUID;
    } else if (!editing) {
      if (paymentRequest.getAddresses(state)[selectedShippingAddress]) {
        billingAddressSelect.value = selectedShippingAddress;
      } else {
        billingAddressSelect.value = Object.keys(addresses)[0];
      }
    }

    this.updateRequiredState();
    this.updateSaveButtonState();
  }

  handleEvent(event) {
    switch (event.type) {
      case "click": {
        this.onClick(event);
        break;
      }
      case "input": {
        this.onInput(event);
        break;
      }
      case "invalid": {
        this.onInvalid(event);
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
      case this.addressAddLink:
      case this.addressEditLink: {
        let {
          "basic-card-page": basicCardPage,
        } = this.requestStore.getState();
        let nextState = {
          page: {
            id: "address-page",
            previousId: "basic-card-page",
          },
          "address-page": {
            guid: null,
            selectedStateKey: ["basic-card-page", "billingAddressGUID"],
            title: this.dataset.billingAddressTitleAdd,
          },
          "basic-card-page": {
            preserveFieldValues: true,
            guid: basicCardPage.guid,
            persistCheckboxValue: this.persistCheckbox.checked,
          },
        };
        let billingAddressGUID = this.form.querySelector("#billingAddressGUID");
        let selectedOption = billingAddressGUID.selectedOptions.length &&
                             billingAddressGUID.selectedOptions[0];
        if (evt.target == this.addressEditLink && selectedOption && selectedOption.value) {
          nextState["address-page"].title = this.dataset.billingAddressTitleEdit;
          nextState["address-page"].guid = selectedOption.value;
        }
        this.requestStore.setState(nextState);
        break;
      }
      case this.backButton: {
        let {
          page,
          request,
          "address-page": addressPage,
          "basic-card-page": basicCardPage,
          selectedShippingAddress,
        } = this.requestStore.getState();

        let nextState = {
          page: {
            id: page.previousId || "payment-summary",
            onboardingWizard: page.onboardingWizard,
          },
        };

        let addressPageState;
        if (page.onboardingWizard) {
          if (request.paymentOptions.requestShipping) {
            addressPageState = Object.assign({}, addressPage, {guid: selectedShippingAddress});
          } else {
            addressPageState =
              Object.assign({}, addressPage, {guid: basicCardPage.billingAddressGUID});
          }

          let basicCardPageState = Object.assign({}, basicCardPage, {preserveFieldValues: true});
          delete basicCardPageState.persistCheckboxValue;

          Object.assign(nextState, {
            "address-page": addressPageState,
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
    this.updateSaveButtonState();
  }

  onInvalid(event) {
    this.saveButton.disabled = true;
  }

  updateSaveButtonState() {
    this.saveButton.disabled = !this.form.checkValidity();
  }

  updateRequiredState() {
    for (let formElement of this.form.elements) {
      let container = formElement.closest("label") || formElement.closest("div");
      let span = container.querySelector("span");
      span.setAttribute("fieldRequiredSymbol", this.dataset.fieldRequiredSymbol);
      let required = formElement.required && !formElement.disabled;
      if (required) {
        container.setAttribute("required", "true");
      } else {
        container.removeAttribute("required");
      }
    }
  }

  async saveRecord() {
    let record = this.formHandler.buildFormObject();
    let currentState = this.requestStore.getState();
    let {
      tempBasicCards,
      "basic-card-page": basicCardPage,
    } = currentState;
    let editing = !!basicCardPage.guid;

    if (editing ? (basicCardPage.guid in tempBasicCards) : !this.persistCheckbox.checked) {
      record.isTemporary = true;
    }

    for (let editableFieldName of ["cc-name", "cc-exp-month", "cc-exp-year"]) {
      record[editableFieldName] = record[editableFieldName] || "";
    }

    // Only save the card number if we're saving a new record, otherwise we'd
    // overwrite the unmasked card number with the masked one.
    if (!editing) {
      record["cc-number"] = record["cc-number"] || "";
    }

    try {
      let {guid} = await paymentRequest.updateAutofillRecord("creditCards", record,
                                                             basicCardPage.guid);
      this.requestStore.setState({
        page: {
          id: "payment-summary",
        },
        selectedPaymentCard: guid,
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
