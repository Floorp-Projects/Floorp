/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from ../../../../../browser/extensions/formautofill/content/autofillEditForms.js*/
import PaymentStateSubscriberMixin from "../mixins/PaymentStateSubscriberMixin.js";
import paymentRequest from "../paymentRequest.js";
/* import-globals-from ../unprivileged-fallbacks.js */

/**
 * <address-form></address-form>
 *
 * XXX: Bug 1446164 - This form isn't localized when used via this custom element
 * as it will be much easier to share the logic once we switch to Fluent.
 */

export default class AddressForm extends PaymentStateSubscriberMixin(HTMLElement) {
  constructor() {
    super();

    this.pageTitle = document.createElement("h1");
    this.genericErrorText = document.createElement("div");

    this.cancelButton = document.createElement("button");
    this.cancelButton.id = "address-page-cancel-button";
    this.cancelButton.addEventListener("click", this);

    this.backButton = document.createElement("button");
    this.backButton.addEventListener("click", this);

    this.saveButton = document.createElement("button");
    this.saveButton.addEventListener("click", this);

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
      this.appendChild(this.pageTitle);
      this.appendChild(form);

      let record = {};
      this.formHandler = new EditAddress({
        form,
      }, record, {
        DEFAULT_REGION: PaymentDialogUtils.DEFAULT_REGION,
        getFormFormat: PaymentDialogUtils.getFormFormat,
        supportedCountries: PaymentDialogUtils.supportedCountries,
      });

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
    this.cancelButton.textContent = this.dataset.cancelButtonLabel;
    this.backButton.textContent = this.dataset.backButtonLabel;
    this.saveButton.textContent = this.dataset.saveButtonLabel;

    let record = {};
    let {
      page,
      savedAddresses,
    } = state;

    this.backButton.hidden = page.onboardingWizard;

    if (page.addressFields) {
      this.setAttribute("address-fields", page.addressFields);
    } else {
      this.removeAttribute("address-fields");
    }

    this.pageTitle.textContent = page.title;
    this.genericErrorText.textContent = page.error;

    let editing = !!page.guid;

    // If an address is selected we want to edit it.
    if (editing) {
      record = savedAddresses[page.guid];
      if (!record) {
        throw new Error("Trying to edit a non-existing address: " + page.guid);
      }
    }

    this.formHandler.loadRecord(record);
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
    } = this.requestStore.getState();

    paymentRequest.updateAutofillRecord("addresses", record, page.guid, {
      errorStateChange: {
        page: {
          id: "address-page",
          onboardingWizard: page.onboardingWizard,
          error: this.dataset.errorGenericSave,
        },
      },
      preserveOldProperties: true,
      selectedStateKey: page.selectedStateKey,
      successStateChange: {
        page: {
          id: "payment-summary",
        },
      },
    });
  }
}

customElements.define("address-form", AddressForm);
