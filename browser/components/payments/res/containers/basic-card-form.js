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

    this.backButton = document.createElement("button");
    this.backButton.addEventListener("click", this);

    this.saveButton = document.createElement("button");
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

      this.appendChild(this.persistCheckbox);
      this.appendChild(this.genericErrorText);
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
      savedAddresses,
      selectedShippingAddress,
    } = state;

    this.pageTitle.textContent = page.title;
    this.backButton.textContent = this.dataset.backButtonLabel;
    this.saveButton.textContent = this.dataset.saveButtonLabel;
    this.persistCheckbox.label = this.dataset.persistCheckboxLabel;

    let record = {};
    let basicCards = paymentRequest.getBasicCards(state);

    this.genericErrorText.textContent = page.error;

    let editing = !!page.guid;
    this.form.querySelector("#cc-number").disabled = editing;

    // If a card is selected we want to edit it.
    if (editing) {
      record = basicCards[page.guid];
      if (!record) {
        throw new Error("Trying to edit a non-existing card: " + page.guid);
      }
      // When editing an existing record, prevent changes to persistence
      this.persistCheckbox.hidden = true;
    } else {
      if (selectedShippingAddress) {
        record.billingAddressGUID = selectedShippingAddress;
      }
      // Adding a new record: default persistence to checked when in a not-private session
      this.persistCheckbox.hidden = false;
      this.persistCheckbox.checked = !state.isPrivate;
    }

    this.formHandler.loadRecord(record, savedAddresses);
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
    let {
      page,
      tempBasicCards,
    } = this.requestStore.getState();
    let editing = !!page.guid;
    let tempRecord = editing && tempBasicCards[page.guid];

    for (let editableFieldName of ["cc-name", "cc-exp-month", "cc-exp-year"]) {
      record[editableFieldName] = record[editableFieldName] || "";
    }

    // Only save the card number if we're saving a new record, otherwise we'd
    // overwrite the unmasked card number with the masked one.
    if (!editing) {
      record["cc-number"] = record["cc-number"] || "";
    }

    if (!tempRecord && this.persistCheckbox.checked) {
      log.debug(`BasicCardForm: persisting creditCard record: ${page.guid || "(new)"}`);
      paymentRequest.updateAutofillRecord("creditCards", record, page.guid, {
        errorStateChange: {
          page: {
            id: "basic-card-page",
            error: this.dataset.errorGenericSave,
          },
        },
        preserveOldProperties: true,
        selectedStateKey: "selectedPaymentCard",
        successStateChange: {
          page: {
            id: "payment-summary",
          },
        },
      });
    } else {
      // This record will never get inserted into the store
      // so we generate a faux-guid for a new record
      record.guid = page.guid || "temp-" + Math.abs(Math.random() * 0xffffffff|0);

      log.debug(`BasicCardForm: saving temporary record: ${record.guid}`);
      this.requestStore.setState({
        page: {
          id: "payment-summary",
        },
        selectedPaymentCard: record.guid,
        tempBasicCards: Object.assign({}, tempBasicCards, {
        // Mix-in any previous values - equivalent to the store's preserveOldProperties: true,
          [record.guid]: Object.assign({}, tempRecord, record),
        }),
      });
    }
  }
}

customElements.define("basic-card-form", BasicCardForm);
