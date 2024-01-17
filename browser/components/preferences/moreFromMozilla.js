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

    if (value === "default" || value === "simple") {
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
    };

    const experiment_params = {
      entrypoint_experiment: "morefrommozilla-experiment-1846",
    };

    let pageUrl = new URL(url);
    for (let [key, val] of Object.entries(URL_PARAMS)) {
      pageUrl.searchParams.append(key, val);
    }

    // Append region by product to utm_content and also
    // append '-email' when URL is opened
    // from send email link in QRCode box
    if (option) {
      pageUrl.searchParams.set(
        "utm_content",
        `${utm_content[option]}-${region}${hasEmail ? "-email" : ""}`
      );
    }

    // Add experiments params when user is shown an experimental UI
    // with template value as 'simple' set via Nimbus
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
    const isRegionUS = Region.home.toLowerCase() === "us";
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
      {
        id: "mozilla-monitor",
        title_string_id: "more-from-moz-mozilla-monitor-title",
        description_string_id: isRegionUS
          ? "more-from-moz-mozilla-monitor-us-description"
          : "more-from-moz-mozilla-monitor-global-description",
        region: isRegionUS ? "us" : "global",
        button: {
          id: "mozillaMonitor",
          label_string_id: "more-from-moz-mozilla-monitor-button",
          actionURL: "https://monitor.mozilla.org/",
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

    if (BrowserUtils.shouldShowPromo(BrowserUtils.PromoType.RELAY)) {
      const relay = {
        id: "firefox-relay",
        title_string_id: "more-from-moz-firefox-relay-title",
        description_string_id: "more-from-moz-firefox-relay-description",
        region: "global",
        button: {
          id: "firefoxRelay",
          label_string_id: "more-from-moz-firefox-relay-button",
          actionURL: "https://relay.firefox.com/",
        },
      };
      products.push(relay);
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

      document.l10n.setAttributes(title, product.title_string_id);
      title.id = product.id;

      document.l10n.setAttributes(desc, product.description_string_id);

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
        } else {
          actionElement.addEventListener("click", function () {
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
        document.l10n.setAttributes(
          qrcode_title,
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
        document.l10n.setAttributes(
          img,
          "more-from-moz-qr-code-firefox-mobile-img"
        );

        let qrc_link = template.querySelector(".qr-code-link");

        // So the telemetry includes info about which option is being used
        qrc_link.id = `${this.option}-${product.qrcode.button.id}`;

        // For supported locales, this link allows users to send themselves a
        // download link by email. It should be hidden for unsupported locales.
        if (BrowserUtils.sendToDeviceEmailsSupported()) {
          document.l10n.setAttributes(
            qrc_link,
            product.qrcode.button.label.string_id
          );
          qrc_link.href = this.getURL(
            product.qrcode.button.actionURL,
            product.region,
            this.option,
            true
          );
          qrc_link.hidden = false;
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
