/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";
import { RemotePageChild } from "resource://gre/actors/RemotePageChild.sys.mjs";

import { ShoppingProduct } from "chrome://global/content/shopping/ShoppingProduct.mjs";

let lazy = {};

let gAllActors = new Set();

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "optedIn",
  "browser.shopping.experience2023.optedIn",
  null,
  function optedInStateChanged() {
    for (let actor of gAllActors) {
      actor.optedInStateChanged();
    }
  }
);

export class ShoppingSidebarChild extends RemotePageChild {
  constructor() {
    super();
  }

  actorCreated() {
    super.actorCreated();
    gAllActors.add(this);
  }

  actorDestroyed() {
    super.actorDestroyed();
    gAllActors.delete(this);
    this.#product?.uninit();
  }

  #productURI = null;
  #product = null;

  receiveMessage(message) {
    switch (message.name) {
      case "ShoppingSidebar:UpdateProductURL":
        let { url } = message.data;
        let uri = Services.io.newURI(url);
        if (this.#productURI?.equalsExceptRef(uri)) {
          return;
        }
        this.#productURI = uri;
        this.updateContent();
        break;
    }
  }

  handleEvent(event) {
    switch (event.type) {
      case "ContentReady":
        this.updateContent();
        break;
    }
  }

  get canFetchAndShowData() {
    return lazy.optedIn === 1;
  }

  optedInStateChanged() {
    // Force re-fetching things if needed by clearing the last product URI:
    this.#productURI = null;
    // Then let content know.
    this.updateContent();
  }

  async updateContent() {
    this.#product?.uninit();
    // We are called either because the URL has changed or because the opt-in
    // state has changed. In both cases, we want to clear out content
    // immediately, without waiting for potentially async operations like
    // obtaining product information.
    this.sendToContent("Update", {
      showOnboarding: !this.canFetchAndShowData,
      data: null,
    });
    if (this.canFetchAndShowData) {
      if (!this.#productURI) {
        let url = await this.sendQuery("GetProductURL");

        // Bail out if we opted out in the meantime, or don't have a URI.
        if (lazy.optedIn !== 1 || !url) {
          return;
        }

        this.#productURI = Services.io.newURI(url);
      }

      let uri = this.#productURI;
      this.#product = new ShoppingProduct(uri);
      let data = await this.#product.requestAnalysis().catch(err => {
        console.error("Failed to fetch product analysis data", err);
      });
      // Check if the product URI or opt in changed while we waited.
      if (uri != this.#productURI || !this.canFetchAndShowData) {
        return;
      }
      this.sendToContent("Update", {
        showOnboarding: false,
        data,
      });
    }
  }

  sendToContent(eventName, detail) {
    let win = this.contentWindow;
    let evt = new win.CustomEvent(eventName, {
      bubbles: true,
      detail: Cu.cloneInto(detail, win),
    });
    win.document.dispatchEvent(evt);
  }
}
