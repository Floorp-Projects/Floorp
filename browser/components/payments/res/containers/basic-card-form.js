/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from ../../../../../browser/extensions/formautofill/content/autofillEditForms.js*/
import LabelledCheckbox from "../components/labelled-checkbox.js";
import PaymentStateSubscriberMixin from "../mixins/PaymentStateSubscriberMixin.js";
import paymentRequest from "../paymentRequest.js";

/* import-globals-from ../unprivileged-fallbacks.js */

/**
 * <basic-card-form></basic-card-form>
 *
 * XXX: Bug 1446164 - This form isn't localized when used via this custom element
 * as it will be much easier to share the logic once we switch to Fluent.
 */

export default class BasicCardForm extends PaymentStateSubscriberMixin(HTMLElement) {
  constructor() {
    super();

    this.pageTitle = document.createElement("h1");
    this.genericErrorText = document.createElement("div");

    this.cancelButton = document.createElement("button");
    this.cancelButton.className = "cancel-button";
    this.cancelButton.addEventListener("click", this);

    this.addressAddLink = document.createElement("a");
    this.addressAddLink.className = "add-link";
    this.addressAddLink.href = "javascript:void(0)";
    this.addressAddLink.addEventListener("click", this);
    this.addressEditLink = document.createElement("a");
    this.addressEditLink.className = "edit-link";
    this.addressEditLink.href = "javascript:void(0)";
    this.addressEditLink.addEventListener("click", this);

    this.backButton = document.createElement("button");
    this.backButton.className = "back-button";
    this.backButton.addEventListener("click", this);

    this.saveButton = document.createElement("button");
    this.saveButton.className = "save-button";
    this.saveButton.addEventListener("click", this);

    this.persistCheckbox = new LabelledCheckbox();

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
      this.appendChild(this.pageTitle);
      this.appendChild(form);

      let record = {};
      let addresses = [];
      this.formHandler = new EditCreditCard({
        form,
      }, record, addresses, {
        isCCNumber: PaymentDialogUtils.isCCNumber,
        getAddressLabel: PaymentDialogUtils.getAddressLabel,
      });

      let fragment = document.createDocumentFragment();
      fragment.append(this.addressAddLink);
      fragment.append(" ");
      fragment.append(this.addressEditLink);
      let billingAddressRow = this.form.querySelector(".billingAddressRow");
      billingAddressRow.appendChild(fragment);

      this.appendChild(this.persistCheckbox);
      this.appendChild(this.genericErrorText);
      this.appendChild(this.cancelButton);
      this.appendChild(this.backButton);
      this.appendChild(this.saveButton);
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

    this.cancelButton.textContent = this.dataset.cancelButtonLabel;
    this.backButton.textContent = this.dataset.backButtonLabel;
    this.saveButton.textContent = this.dataset.saveButtonLabel;
    this.persistCheckbox.label = this.dataset.persistCheckboxLabel;
    this.addressAddLink.textContent = this.dataset.addressAddLinkLabel;
    this.addressEditLink.textContent = this.dataset.addressEditLinkLabel;

    // The back button is temporarily hidden(See Bug 1462461).
    this.backButton.hidden = !!page.onboardingWizard;
    this.cancelButton.hidden = !page.onboardingWizard;

    let record = {};
    let basicCards = paymentRequest.getBasicCards(state);
    let addresses = paymentRequest.getAddresses(state);

    this.genericErrorText.textContent = page.error;

    let editing = !!basicCardPage.guid;
    this.form.querySelector("#cc-number").disabled = editing;

    // If a card is selected we want to edit it.
    if (editing) {
      this.pageTitle.textContent = this.dataset.editBasicCardTitle;
      record = basicCards[basicCardPage.guid];
      if (!record) {
        throw new Error("Trying to edit a non-existing card: " + basicCardPage.guid);
      }
      // When editing an existing record, prevent changes to persistence
      this.persistCheckbox.hidden = true;
    } else {
      this.pageTitle.textContent = this.dataset.addBasicCardTitle;
      // Use a currently selected shipping address as the default billing address
      if (!record.billingAddressGUID && selectedShippingAddress) {
        record.billingAddressGUID = selectedShippingAddress;
      }
      // Adding a new record: default persistence to checked when in a not-private session
      this.persistCheckbox.hidden = false;
      this.persistCheckbox.checked = !state.isPrivate;
    }

    this.formHandler.loadRecord(record, addresses, basicCardPage.preserveFieldValues);

    this.form.querySelector(".billingAddressRow").hidden = false;

    if (basicCardPage.billingAddressGUID) {
      let addressGuid = basicCardPage.billingAddressGUID;
      let billingAddressSelect = this.form.querySelector("#billingAddressGUID");
      billingAddressSelect.value = addressGuid;
    }
  }

  handleEvent(event) {
    switch (event.type) {
      case "click": {
        this.onClick(event);
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
            selectedStateKey: ["basic-card-page", "billingAddressGUID"],
          },
          "address-page": {
            guid: null,
            title: this.dataset.billingAddressTitleAdd,
          },
          "basic-card-page": {
            preserveFieldValues: true,
            guid: basicCardPage.guid,
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
        this.requestStore.setState({
          page: {
            id: "payment-summary",
          },
        });
        break;
      }
      case this.saveButton: {
        this.saveRecord();
        break;
      }
      default: {
        throw new Error("Unexpected click target");
      }
    }
  }

  saveRecord() {
    let record = this.formHandler.buildFormObject();
    let currentState = this.requestStore.getState();
    let {
      page,
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

    let state = {
      errorStateChange: {
        page: {
          id: "basic-card-page",
          error: this.dataset.errorGenericSave,
        },
      },
      preserveOldProperties: true,
      selectedStateKey: ["selectedPaymentCard"],
      successStateChange: {
        page: {
          id: "payment-summary",
        },
      },
    };

    const previousId = page.previousId;
    if (previousId) {
      state.successStateChange[previousId] = Object.assign({}, currentState[previousId]);
    }

    paymentRequest.updateAutofillRecord("creditCards", record, basicCardPage.guid, state);
  }
}

customElements.define("basic-card-form", BasicCardForm);
