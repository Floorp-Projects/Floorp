/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import PaymentStateSubscriberMixin from "../mixins/PaymentStateSubscriberMixin.js";
import paymentRequest from "../paymentRequest.js";
/* import-globals-from ../unprivileged-fallbacks.js */

/**
 * <accepted-cards></accepted-cards>
 */

export default class AcceptedCards extends PaymentStateSubscriberMixin(HTMLElement) {
  constructor() {
    super();

    this._listEl = document.createElement("ul");
    this._listEl.classList.add("accepted-cards-list");
    this._labelEl = document.createElement("span");
    this._labelEl.classList.add("accepted-cards-label");
  }

  connectedCallback() {
    this.label = this.getAttribute("label");
    this.appendChild(this._labelEl);

    this._listEl.textContent = "";
    let allNetworks = PaymentDialogUtils.getCreditCardNetworks();
    for (let network of allNetworks) {
      let item = document.createElement("li");
      item.classList.add("accepted-cards-item");
      item.dataset.networkId = network;
      this._listEl.appendChild(item);
    }
    this.appendChild(this._listEl);
    // Only call the connected super callback(s) once our markup is fully
    // connected
    super.connectedCallback();
  }

  render(state) {
    let acceptedNetworks = paymentRequest.getAcceptedNetworks(state.request);
    for (let item of this._listEl.children) {
      let network = item.dataset.networkId;
      item.hidden = !(network && acceptedNetworks.includes(network));
    }
  }

  set label(value) {
    this._labelEl.textContent = value;
  }

  get acceptedItems() {
    return Array.from(this._listEl.children).filter(item => !item.hidden);
  }
}

customElements.define("accepted-cards", AcceptedCards);
