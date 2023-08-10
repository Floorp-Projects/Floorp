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

  didDestroy() {
    this._destroyed = true;
    super.didDestroy?.();
    gAllActors.delete(this);
    this.#product?.uninit();
  }

  #productURI = null;
  #product = null;

  receiveMessage(message) {
    switch (message.name) {
      case "ShoppingSidebar:UpdateProductURL":
        let { url } = message.data;
        let uri = url ? Services.io.newURI(url) : null;
        // If we're going from null to null, bail out:
        if (!this.#productURI && !uri) {
          return;
        }
        // Otherwise, check if we now have a product:
        if (uri && this.#productURI?.equalsExceptRef(uri)) {
          return;
        }
        this.#productURI = uri;
        this.updateContent({ haveUpdatedURI: true });
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

  get canFetchAndShowAd() {
    // TODO: Bug 1840520 will add the pref that toggles showing ads
    return true;
  }

  optedInStateChanged() {
    // Force re-fetching things if needed by clearing the last product URI:
    this.#productURI = null;
    // Then let content know.
    this.updateContent();
  }

  getProductURI() {
    return this.#productURI;
  }

  /**
   * This callback is invoked whenever something changes that requires
   * re-rendering content. The expected cases for this are:
   * - page navigations (both to new products and away from a product once
   *   the sidebar has been created)
   * - opt in state changes.
   *
   * @param {object?} options
   *        Optional parameter object.
   * @param {bool} options.haveUpdatedURI = false
   *        Whether we've got an up-to-date URI already. If true, we avoid
   *        fetching the URI from the parent, and assume `this.#productURI`
   *        is current. Defaults to false.
   *
   */
  async updateContent({ haveUpdatedURI = false } = {}) {
    // updateContent is an async function, and when we're off making requests or doing
    // other things asynchronously, the actor can be destroyed, the user
    // might navigate to a new page, the user might disable the feature ... -
    // all kinds of things can change. So we need to repeatedly check
    // whether we can keep going with our async processes. This helper takes
    // care of these checks.
    let canContinue = (currentURI, checkURI = true) => {
      if (this._destroyed || !this.canFetchAndShowData) {
        return false;
      }
      if (!checkURI) {
        return true;
      }
      return currentURI && currentURI == this.#productURI;
    };
    this.#product?.uninit();
    // We are called either because the URL has changed or because the opt-in
    // state has changed. In both cases, we want to clear out content
    // immediately, without waiting for potentially async operations like
    // obtaining product information.
    this.sendToContent("Update", {
      showOnboarding: !this.canFetchAndShowData,
      data: null,
      recommendationData: null,
    });
    if (this.canFetchAndShowData) {
      if (!this.#productURI) {
        // If we already have a URI and it's just null, bail immediately.
        if (haveUpdatedURI) {
          return;
        }
        let url = await this.sendQuery("GetProductURL");

        // Bail out if we opted out in the meantime, or don't have a URI.
        if (!canContinue(null, false)) {
          return;
        }

        this.#productURI = Services.io.newURI(url);
      }

      let uri = this.#productURI;
      this.#product = new ShoppingProduct(uri);
      let data = await this.#product.requestAnalysis().catch(err => {
        console.error("Failed to fetch product analysis data", err);
        return { error: err };
      });
      // Check if we got nuked from orbit, or the product URI or opt in changed while we waited.
      if (!canContinue(uri)) {
        return;
      }
      this.sendToContent("Update", {
        showOnboarding: false,
        data,
        productUrl: this.#productURI.spec,
      });

      if (!this.canFetchAndShowAd || data.error) {
        return;
      }
      this.#product.requestRecommendations().then(recommendationData => {
        // Check if the product URI or opt in changed while we waited.
        if (
          uri != this.#productURI ||
          !this.canFetchAndShowData ||
          !this.canFetchAndShowAd
        ) {
          return;
        }

        this.sendToContent("Update", {
          showOnboarding: false,
          data,
          productUrl: this.#productURI.spec,
          recommendationData,
        });
      });
    }
  }

  sendToContent(eventName, detail) {
    if (this._destroyed) {
      return;
    }
    let win = this.contentWindow;
    let evt = new win.CustomEvent(eventName, {
      bubbles: true,
      detail: Cu.cloneInto(detail, win),
    });
    win.document.dispatchEvent(evt);
  }
}
