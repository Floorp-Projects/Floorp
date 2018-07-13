/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * <payment-request-page></payment-request-page>
 */

export default class PaymentRequestPage extends HTMLElement {
  constructor() {
    super();

    this.classList.add("page");

    this.pageTitleHeading = document.createElement("h2");

    // The body and footer may be pre-defined in the template so re-use them if they exist.
    this.body = this.querySelector(":scope > .page-body") || document.createElement("div");
    this.body.classList.add("page-body");

    this.footer = this.querySelector(":scope > footer") || document.createElement("footer");
  }

  connectedCallback() {
    // The heading goes inside the body so it scrolls.
    this.body.prepend(this.pageTitleHeading);
    this.appendChild(this.body);

    this.appendChild(this.footer);
  }
}

customElements.define("payment-request-page", PaymentRequestPage);
