/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from preferences.js */

var gMoreFromMozillaPane = {
  initialized: false,
  option: null,

  getURL(url, option) {
    const URL_PARAMS = {
      utm_source: "about-prefs",
      utm_campaign: "morefrommozilla",
      utm_medium: "firefox-desktop",
    };
    // UTM content param used in analytics to record
    // UI template used to open URL
    const utm_content = {
      simple: "fxvt-113-a-na",
      advanced: "fxvt-113-b-na",
    };

    let pageUrl = new URL(url);
    for (let [key, val] of Object.entries(URL_PARAMS)) {
      pageUrl.searchParams.append(key, val);
    }
    if (option) {
      pageUrl.searchParams.set("utm_content", utm_content[option]);
    }
    return pageUrl.toString();
  },

  renderProducts() {
    let products = [
      {
        id: "firefox-mobile",
        title_string_id: "firefox-mobile-title",
        description_string_id: "firefox-mobile-description",
        qrcode: null,
        button: {
          id: "fxMobile",
          type: "link",
          label_string_id: "more-mozilla-learn-more-link",
          actionURL: "https://www.mozilla.org/firefox/browsers/mobile/",
        },
      },
      {
        id: "mozilla-vpn",
        title_string_id: "mozilla-vpn-title",
        description_string_id: "mozilla-vpn-description",
        button: {
          id: "mozillaVPN",
          label_string_id: "button-mozilla-vpn",
          actionURL: "https://www.mozilla.org/products/vpn/",
        },
      },
      {
        id: "mozilla-rally",
        title_string_id: "mozilla-rally-title",
        description_string_id: "mozilla-rally-description",
        button: {
          id: "mozillaRally",
          label_string_id: "button-mozilla-rally",
          actionURL: "https://rally.mozilla.org/",
        },
      },
    ];
    this._productsContainer = document.getElementById(
      "moreFromMozillaCategory"
    );
    let frag = document.createDocumentFragment();
    this._template = document.getElementById(this.option || "simple");

    // Exit when template is not found
    if (!this._template) {
      return;
    }

    for (let product of products) {
      let template = this._template.content.cloneNode(true);
      let title = template.querySelector(".product-title");
      let desc = template.querySelector(".description");

      title.setAttribute("data-l10n-id", product.title_string_id);
      title.id = product.id;

      // Handle advanced template display of product details
      if (this.option === "advanced") {
        template.querySelector(".product-img").id = `${product.id}-image`;
        desc.setAttribute(
          "data-l10n-id",
          `more-mozilla-advanced-${product.description_string_id}`
        );
      } else {
        desc.setAttribute("data-l10n-id", product.description_string_id);
      }

      let isLink = product.button.type === "link";
      let actionElement = template.querySelector(
        isLink ? ".text-link" : ".small-button"
      );

      if (actionElement) {
        actionElement.hidden = false;
        actionElement.id = `${this.option}-${product.button.id}`;
        actionElement.setAttribute(
          "data-l10n-id",
          product.button.label_string_id
        );

        if (isLink) {
          actionElement.setAttribute(
            "href",
            this.getURL(product.button.actionURL, this.option)
          );
          actionElement.setAttribute("target", "_blank");
        } else {
          actionElement.addEventListener("click", function() {
            let mainWindow = window.windowRoot.ownerGlobal;
            mainWindow.openTrustedLinkIn(
              gMoreFromMozillaPane.getURL(
                product.button.actionURL,
                gMoreFromMozillaPane.option
              ),
              "tab"
            );
          });
        }
      }

      if (product.qrcode) {
        template.querySelector(".qrcode-section").hidden = false;
      }
      frag.appendChild(template);
    }
    this._productsContainer.appendChild(frag);
  },

  async init() {
    if (this.initialized) {
      return;
    }
    this.initialized = true;
    document
      .getElementById("moreFromMozillaCategory")
      .removeAttribute("data-hidden-from-search");

    this.renderProducts();
  },
};
