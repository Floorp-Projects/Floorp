/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");

const ONBOARDING_CSS_URL = "resource://onboarding/onboarding.css";
const ABOUT_HOME_URL = "about:home";
const ABOUT_NEWTAB_URL = "about:newtab";
const BUNDLE_URI = "chrome://onboarding/locale/onboarding.properties";
const UITOUR_JS_URI = "chrome://browser/content/UITour-lib.js";
const TOUR_AGENT_JS_URI = "resource://onboarding/onboarding-tour-agent.js";
const BRAND_SHORT_NAME = Services.strings
                     .createBundle("chrome://branding/locale/brand.properties")
                     .GetStringFromName("brandShortName");

/**
 * Add any number of tours, following the format
 * {
 *   // The unique tour id
 *   id: "onboarding-tour-addons",
 *   // The string id of tour name which would be displayed on the navigation bar
 *   tourNameId: "onboarding.tour-addon",
 *   // Return a div appended with elements for this tours.
 *   // Each tour should contain the following 3 sections in the div:
 *   // .onboarding-tour-description, .onboarding-tour-content, .onboarding-tour-button.
 *   // Add no-button css class in the div if this tour does not need a button.
 *   // The overlay layout will responsively position and distribute space for these 3 sections based on viewport size
 *   getPage() {},
 * },
 **/
var onboardingTours = [
  {
    id: "onboarding-tour-private-browsing",
    tourNameId: "onboarding.tour-private-browsing",
    getPage(win) {
      let div = win.document.createElement("div");
      div.innerHTML = `
        <section class="onboarding-tour-description">
          <h1 data-l10n-id="onboarding.tour-private-browsing.title"></h1>
          <p data-l10n-id="onboarding.tour-private-browsing.description"></p>
        </section>
        <section class="onboarding-tour-content">
          <img src="resource://onboarding/img/figure_private.svg" />
        </section>
        <aside class="onboarding-tour-button">
          <button id="onboarding-tour-private-browsing-button" data-l10n-id="onboarding.tour-private-browsing.button"></button>
        </aside>
      `;
      return div;
    },
  },
  {
    id: "onboarding-tour-addons",
    tourNameId: "onboarding.tour-addons",
    getPage(win) {
      let div = win.document.createElement("div");
      div.innerHTML = `
        <section class="onboarding-tour-description">
          <h1 data-l10n-id="onboarding.tour-addons.title"></h1>
          <p data-l10n-id="onboarding.tour-addons.description"></p>
        </section>
        <section class="onboarding-tour-content">
          <img src="resource://onboarding/img/figure_addons.svg" />
        </section>
        <aside class="onboarding-tour-button">
          <button id="onboarding-tour-addons-button" data-l10n-id="onboarding.tour-addons.button"></button>
        </aside>
      `;
      return div;
    },
  },
  {
    id: "onboarding-tour-customize",
    tourNameId: "onboarding.tour-customize",
    getPage(win) {
      let div = win.document.createElement("div");
      div.innerHTML = `
        <section class="onboarding-tour-description">
          <h1 data-l10n-id="onboarding.tour-customize.title"></h1>
          <p data-l10n-id="onboarding.tour-customize.description"></p>
        </section>
        <section class="onboarding-tour-content">
          <img src="resource://onboarding/img/figure_customize.svg" />
        </section>
        <aside class="onboarding-tour-button">
          <button id="onboarding-tour-customize-button" data-l10n-id="onboarding.tour-customize.button"></button>
        </aside>
      `;
      return div;
    },
  },
  {
    id: "onboarding-tour-search",
    tourNameId: "onboarding.tour-search",
    getPage(win) {
      let div = win.document.createElement("div");
      div.innerHTML = `
        <section class="onboarding-tour-description">
          <h1 data-l10n-id="onboarding.tour-search.title"></h1>
          <p data-l10n-id="onboarding.tour-search.description"></p>
        </section>
        <section class="onboarding-tour-content">
          <img src="resource://onboarding/img/figure_search.svg" />
        </section>
        <aside class="onboarding-tour-button">
          <button id="onboarding-tour-search-button" data-l10n-id="onboarding.tour-search.button"></button>
        </aside>
      `;
      return div;
    },
  },
  {
    id: "onboarding-tour-default-browser",
    tourNameId: "onboarding.tour-default-browser",
    getPage(win) {
      let div = win.document.createElement("div");
      div.innerHTML = `
        <section class="onboarding-tour-description">
          <h1 data-l10n-id="onboarding.tour-default-browser.title"></h1>
          <p data-l10n-id="onboarding.tour-default-browser.description"></p>
        </section>
        <section class="onboarding-tour-content">
          <img src="resource://onboarding/img/figure_default.svg" />
        </section>
      `;
      return div;
    },
  },
];

/**
 * The script won't be initialized if we turned off onboarding by
 * setting "browser.onboarding.enabled" to false.
 */
class Onboarding {
  constructor(contentWindow) {
    this.init(contentWindow);
    this._bundle = Services.strings.createBundle(BUNDLE_URI);
  }

  async init(contentWindow) {
    this._window = contentWindow;
    this._tourItems = [];
    this._tourPages = [];
    // We want to create and append elements after CSS is loaded so
    // no flash of style changes and no additional reflow.
    await this._loadCSS();
    this._overlayIcon = this._renderOverlayIcon();
    this._overlay = this._renderOverlay();
    this._window.document.body.appendChild(this._overlayIcon);
    this._window.document.body.appendChild(this._overlay);

    this._loadJS(UITOUR_JS_URI);
    this._loadJS(TOUR_AGENT_JS_URI);

    this._overlayIcon.addEventListener("click", this);
    this._overlay.addEventListener("click", this);
    // Destroy on unload. This is to ensure we remove all the stuff we left.
    // No any leak out there.
    this._window.addEventListener("unload", () => this.destroy());

    this._initPrefObserver();
  }

  _initPrefObserver() {
    if (this._prefsObserved) {
      return;
    }

    this._prefsObserved = new Map();
    this._prefsObserved.set("browser.onboarding.hidden", prefValue => {
      if (prefValue) {
        this.destroy();
      }
    });
    for (let [name, callback] of this._prefsObserved) {
      Preferences.observe(name, callback);
    }
  }

  _clearPrefObserver() {
    if (this._prefsObserved) {
      for (let [name, callback] of this._prefsObserved) {
        Preferences.ignore(name, callback);
      }
      this._prefsObserved = null;
    }
  }

  /**
   * @param {String} action the action to ask the chrome to do
   * @param {Array} params the parameters for the action
   */
  sendMessageToChrome(action, params) {
    sendAsyncMessage("Onboarding:OnContentMessage", {
      action, params
    });
  }

  handleEvent(evt) {
    switch (evt.target.id) {
      case "onboarding-overlay-icon":
      case "onboarding-overlay-close-btn":
      // If the clicking target is directly on the outer-most overlay,
      // that means clicking outside the tour content area.
      // Let's toggle the overlay.
      case "onboarding-overlay":
        this.toggleOverlay();
        break;
    }
    if (evt.target.classList.contains("onboarding-tour-item")) {
      this.gotoPage(evt.target.id);
    }
  }

  destroy() {
    this._clearPrefObserver();
    this._overlayIcon.remove();
    this._overlay.remove();
  }

  toggleOverlay() {
    if (this._tourItems.length == 0) {
      // Lazy loading until first toggle.
      this._loadTours(onboardingTours);
    }

    this._overlay.classList.toggle("opened");
    let hiddenCheckbox = this._window.document.getElementById("onboarding-tour-hidden-checkbox");
    if (hiddenCheckbox.checked) {
      this.hide();
      return;
    }

    this._overlay.classList.toggle("onboarding-opened");
  }

  gotoPage(tourId) {
    let targetPageId = `${tourId}-page`;
    for (let page of this._tourPages) {
      page.style.display = page.id != targetPageId ? "none" : "";
    }
    for (let li of this._tourItems) {
      if (li.id == tourId) {
        li.classList.add("onboarding-active");
      } else {
        li.classList.remove("onboarding-active");
      }
    }
  }

  hide() {
    this.sendMessageToChrome("set-prefs", [
      {
        name: "browser.onboarding.hidden",
        value: true
      },
      {
        name: "browser.onboarding.notification.finished",
        value: true
      }
    ]);
  }

  _renderOverlay() {
    let div = this._window.document.createElement("div");
    div.id = "onboarding-overlay";
    // Here we use `innerHTML` is for more friendly reading.
    // The security should be fine because this is not from an external input.
    // We're not shipping yet so l10n strings is going to be closed for now.
    div.innerHTML = `
      <div id="onboarding-overlay-dialog">
        <span id="onboarding-overlay-close-btn"></span>
        <header id="onboarding-header"></header>
        <nav>
          <ul id="onboarding-tour-list"></ul>
        </nav>
        <footer id="onboarding-footer">
          <input type="checkbox" id="onboarding-tour-hidden-checkbox" /><label for="onboarding-tour-hidden-checkbox"></label>
        </footer>
      </div>
    `;

    div.querySelector("label[for='onboarding-tour-hidden-checkbox']").textContent =
       this._bundle.GetStringFromName("onboarding.hidden-checkbox-label");
    div.querySelector("#onboarding-header").textContent =
       this._bundle.formatStringFromName("onboarding.overlay-title", [BRAND_SHORT_NAME], 1);
    return div;
  }

  _renderOverlayIcon() {
    let img = this._window.document.createElement("div");
    img.id = "onboarding-overlay-icon";
    return img;
  }

  _loadTours(tours) {
    let itemsFrag = this._window.document.createDocumentFragment();
    let pagesFrag = this._window.document.createDocumentFragment();
    for (let tour of tours) {
      // Create tour navigation items dynamically
      let li = this._window.document.createElement("li");
      li.textContent = this._bundle.GetStringFromName(tour.tourNameId);
      li.id = tour.id;
      li.className = "onboarding-tour-item";
      itemsFrag.appendChild(li);
      // Dynamically create tour pages
      let div = tour.getPage(this._window);

      // Do a traverse for elements in the page that need to be localized.
      let l10nElements = div.querySelectorAll("[data-l10n-id]");
      for (let i = 0; i < l10nElements.length; i++) {
        let element = l10nElements[i];
        // We always put brand short name as the first argument for it's the
        // only and frequently used arguments in our l10n case. Rewrite it if
        // other arguments appears.
        element.textContent = this._bundle.formatStringFromName(
                                element.dataset.l10nId, [BRAND_SHORT_NAME], 1);
      }

      div.id = `${tour.id}-page`;
      div.classList.add("onboarding-tour-page");
      div.style.display = "none";
      pagesFrag.appendChild(div);
      // Cache elements in arrays for later use to avoid cost of querying elements
      this._tourItems.push(li);
      this._tourPages.push(div);
    }

    let dialog = this._window.document.getElementById("onboarding-overlay-dialog");
    let ul = this._window.document.getElementById("onboarding-tour-list");
    ul.appendChild(itemsFrag);
    let footer = this._window.document.getElementById("onboarding-footer");
    dialog.insertBefore(pagesFrag, footer);
    this.gotoPage(tours[0].id);
  }

  _loadCSS() {
    // Returning a Promise so we can inform caller of loading complete
    // by resolving it.
    return new Promise(resolve => {
      let doc = this._window.document;
      let link = doc.createElement("link");
      link.rel = "stylesheet";
      link.type = "text/css";
      link.href = ONBOARDING_CSS_URL;
      link.addEventListener("load", resolve);
      doc.head.appendChild(link);
    });
  }

  _loadJS(uri) {
    let doc = this._window.document;
    let script = doc.createElement("script");
    script.type = "text/javascript";
    script.src = uri;
    doc.head.appendChild(script);
  }
}

// Load onboarding module only when we enable it.
if (Services.prefs.getBoolPref("browser.onboarding.enabled", false) &&
    !Services.prefs.getBoolPref("browser.onboarding.hidden", false)) {

  addEventListener("load", function onLoad(evt) {
    if (!content || evt.target != content.document) {
      return;
    }
    removeEventListener("load", onLoad);

    let window = evt.target.defaultView;
    let location = window.location.href;
    if (location == ABOUT_NEWTAB_URL || location == ABOUT_HOME_URL) {
      window.requestIdleCallback(() => {
        new Onboarding(window);
      });
    }
  }, true);
}
