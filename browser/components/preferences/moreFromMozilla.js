/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from preferences.js */

var gMoreFromMozillaPane = {
  initialized: false,

  /**
   * "default" is whatever template is the default, as defined by the code
   * in this file (currently in `getTemplateName`).  Setting option to an
   * invalid value will leave it unchanged.
   */
  _option: "default",
  set option(value) {
    if (!value) {
      this._option = "default";
      return;
    }

    if (value === "default" || value === "simple" || value === "advanced") {
      this._option = value;
    }
  },

  get option() {
    return this._option;
  },

  getTemplateName() {
    if (!this._option || this._option == "default") {
      return "simple";
    }
    return this._option;
  },

  getURL(url, region, option, hasEmail) {
    const URL_PARAMS = {
      utm_source: "about-prefs",
      utm_campaign: "morefrommozilla",
      utm_medium: "firefox-desktop",
    };
    // UTM content param used in analytics to record
    // UI template used to open URL
    const utm_content = {
      default: "default",
      simple: "fxvt-113-a",
      advanced: "fxvt-113-b",
    };

    const experiment_params = {
      entrypoint_experiment: "morefrommozilla-experiment-1846",
    };

    let pageUrl = new URL(url);
    for (let [key, val] of Object.entries(URL_PARAMS)) {
      pageUrl.searchParams.append(key, val);
    }

    // Append region by product to utm_cotent and also
    // append '-email' when URL is opened
    // from send email link in QRCode box
    if (option) {
      pageUrl.searchParams.set(
        "utm_content",
        `${utm_content[option]}-${region}${hasEmail ? "-email" : ""}`
      );
    }

    // Add experiments params when user is shown an experimental UI
    // with template value as 'simple' or 'advanced' set via Nimbus
    if (option !== "default") {
      pageUrl.searchParams.set(
        "entrypoint_experiment",
        experiment_params.entrypoint_experiment
      );
      pageUrl.searchParams.set("entrypoint_variation", `treatment-${option}`);
    }
    return pageUrl.toString();
  },

  renderProducts() {
    let products = [
      {
        id: "firefox-mobile",
        title_string_id: "more-from-moz-firefox-mobile-title",
        description_string_id: "more-from-moz-firefox-mobile-description",
        region: "global",
        button: {
          id: "fxMobile",
          type: "link",
          label_string_id: "more-from-moz-learn-more-link",
          actionURL: AppConstants.isChinaRepack()
            ? "https://www.firefox.com.cn/browsers/mobile/"
            : "https://www.mozilla.org/firefox/browsers/mobile/",
        },
        qrcode: {
          title: {
            string_id: "more-from-moz-qr-code-box-firefox-mobile-title",
          },
          image_src_prefix:
            "chrome://browser/content/preferences/more-from-mozilla-qr-code",
          button: {
            id: "qr-code-send-email",
            label: {
              string_id: "more-from-moz-qr-code-box-firefox-mobile-button",
            },
            actionURL: AppConstants.isChinaRepack()
              ? "https://www.firefox.com.cn/mobile/get-app/"
              : "https://www.mozilla.org/firefox/mobile/get-app/?v=mfm",
          },
        },
      },
    ];

    if (BrowserUtils.shouldShowVPNPromo()) {
      const vpn = {
        id: "mozilla-vpn",
        title_string_id: "more-from-moz-mozilla-vpn-title",
        description_string_id: "more-from-moz-mozilla-vpn-description",
        region: "global",
        button: {
          id: "mozillaVPN",
          label_string_id: "more-from-moz-button-mozilla-vpn-2",
          actionURL: "https://www.mozilla.org/products/vpn/",
        },
      };
      products.push(vpn);
    }

    if (BrowserUtils.shouldShowRallyPromo()) {
      const rally = {
        id: "mozilla-rally",
        title_string_id: "more-from-moz-mozilla-rally-title",
        description_string_id: "more-from-moz-mozilla-rally-description",
        region: "na",
        button: {
          id: "mozillaRally",
          label_string_id: "more-from-moz-button-mozilla-rally-2",
          actionURL: "https://rally.mozilla.org/",
        },
      };
      products.push(rally);
    }

    this._productsContainer = document.getElementById(
      "moreFromMozillaCategory"
    );
    let frag = document.createDocumentFragment();
    this._template = document.getElementById(this.getTemplateName());

    // Exit when internal data is incomplete
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
        // So that we can build a selector that applies to .product-info differently
        // for different products.
        template.querySelector(
          ".mozilla-product-item.advanced"
        ).id = `${product.id}-div`;

        template.querySelector(".product-img").id = `${product.id}-image`;
        desc.setAttribute(
          "data-l10n-id",
          `${product.description_string_id}-advanced`
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
        document.l10n.setAttributes(
          actionElement,
          product.button.label_string_id
        );

        if (isLink) {
          actionElement.setAttribute(
            "href",
            this.getURL(product.button.actionURL, product.region, this.option)
          );
          actionElement.setAttribute("target", "_blank");
        } else {
          actionElement.addEventListener("click", function() {
            let mainWindow = window.windowRoot.ownerGlobal;
            mainWindow.openTrustedLinkIn(
              gMoreFromMozillaPane.getURL(
                product.button.actionURL,
                product.region,
                gMoreFromMozillaPane.option
              ),
              "tab"
            );
          });
        }
      }

      if (product.qrcode) {
        let qrcode = template.querySelector(".qr-code-box");
        qrcode.setAttribute("hidden", "false");

        let qrcode_title = template.querySelector(".qr-code-box-title");
        qrcode_title.setAttribute(
          "data-l10n-id",
          product.qrcode.title.string_id
        );

        let img = template.querySelector(".qr-code-box-image");
        // Append QRCode image source by template. For CN region
        // simple template, we want a CN specific QRCode
        img.src =
          product.qrcode.image_src_prefix +
          "-" +
          this.getTemplateName() +
          `${
            AppConstants.isChinaRepack() &&
            this.getTemplateName().includes("simple")
              ? "-cn"
              : ""
          }` +
          ".svg";
        // Add image a11y attributes
        img.setAttribute(
          "data-l10n-id",
          "more-from-moz-qr-code-firefox-mobile-img"
        );

        let qrc_link = template.querySelector(".qr-code-link");

        // So the telemetry includes info about which option is being used
        qrc_link.id = `${this.option}-${product.qrcode.button.id}`;

        // For supported locales, this link allows users to send themselves a download link by email. It should be hidden for unsupported locales.
        if (!BrowserUtils.sendToDeviceEmailsSupported()) {
          qrc_link.classList.add("hidden");
        } else {
          qrc_link.setAttribute(
            "data-l10n-id",
            product.qrcode.button.label.string_id
          );
          qrc_link.href = this.getURL(
            product.qrcode.button.actionURL,
            product.region,
            this.option,
            true
          );
        }
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
    document
      .getElementById("moreFromMozillaCategory-header")
      .removeAttribute("data-hidden-from-search");

    this.renderProducts();
  },
};
