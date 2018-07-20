/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import PaymentRequestPage from "../components/payment-request-page.js";
import PaymentStateSubscriberMixin from "../mixins/PaymentStateSubscriberMixin.js";
import paymentRequest from "../paymentRequest.js";

/* import-globals-from ../unprivileged-fallbacks.js */

/**
 * <completion-error-page></completion-error-page>
 *
 * XXX: Bug 1473772 - This page isn't fully localized when used via this custom element
 * as it will be much easier to implement and share the logic once we switch to Fluent.
 */

export default class CompletionErrorPage extends PaymentStateSubscriberMixin(PaymentRequestPage) {
  constructor() {
    super();

    this.classList.add("error-page");
    this.suggestionsList = document.createElement("ul");
    this.suggestions = [];
    this.body.append(this.suggestionsList);

    this.doneButton = document.createElement("button");
    this.doneButton.classList.add("done-button", "primary");
    this.doneButton.addEventListener("click", this);

    this.footer.appendChild(this.doneButton);
  }

  render(state) {
    let { page } = state;

    if (this.id && page && page.id !== this.id) {
      log.debug(`CompletionErrorPage: no need to further render inactive page: ${page.id}`);
      return;
    }

    this.pageTitleHeading.textContent = this.dataset.pageTitle;
    this.doneButton.textContent = this.dataset.doneButtonLabel;

    this.suggestionsList.textContent = "";

     // FIXME: should come from this.dataset.suggestionN when those strings are created
    this.suggestions[0] = "First suggestion";

    let suggestionsFragment = document.createDocumentFragment();
    for (let suggestionText of this.suggestions) {
      let listNode = document.createElement("li");
      listNode.textContent = suggestionText;
      suggestionsFragment.appendChild(listNode);
    }
    this.suggestionsList.appendChild(suggestionsFragment);
  }

  handleEvent(event) {
    if (event.type == "click") {
      switch (event.target) {
        case this.doneButton: {
          this.onDoneButtonClick(event);
          break;
        }
        default: {
          throw new Error("Unexpected click target");
        }
      }
    }
  }

  onDoneButtonClick(event) {
    paymentRequest.closeDialog();
  }
}

customElements.define("completion-error-page", CompletionErrorPage);
