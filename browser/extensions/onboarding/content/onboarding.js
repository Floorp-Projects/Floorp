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
const UITOUR_JS_URI = "resource://onboarding/lib/UITour-lib.js";
const TOUR_AGENT_JS_URI = "resource://onboarding/onboarding-tour-agent.js";
const BRAND_SHORT_NAME = Services.strings
                     .createBundle("chrome://branding/locale/brand.properties")
                     .GetStringFromName("brandShortName");

/**
 * Add any number of tours, following the format
 * "tourId": { // The short tour id which could be saved in pref
 *   // The unique tour id
 *   id: "onboarding-tour-addons",
 *   // The string id of tour name which would be displayed on the navigation bar
 *   tourNameId: "onboarding.tour-addon",
 *   // The method returing strings used on tour notification
 *   getNotificationStrings(bundle):
 *     - title: // The string of tour notification title
 *     - message: // The string of tour notification message
 *     - button: // The string of tour notification action button title
 *   // Return a div appended with elements for this tours.
 *   // Each tour should contain the following 3 sections in the div:
 *   // .onboarding-tour-description, .onboarding-tour-content, .onboarding-tour-button-container.
 *   // Add onboarding-no-button css class in the div if this tour does not need a button container.
 *   // If there was a .onboarding-tour-action-button present and was clicked, tour would be marked as completed.
 *   getPage() {},
 * },
 **/
var onboardingTourset = {
  "private": {
    id: "onboarding-tour-private-browsing",
    tourNameId: "onboarding.tour-private-browsing",
    getNotificationStrings(bundle) {
      return {
        title: bundle.GetStringFromName("onboarding.notification.onboarding-tour-private-browsing.title"),
        message: bundle.GetStringFromName("onboarding.notification.onboarding-tour-private-browsing.message"),
        button: bundle.GetStringFromName("onboarding.button.learnMore"),
      };
    },
    getPage(win) {
      let div = win.document.createElement("div");
      div.innerHTML = `
        <section class="onboarding-tour-description">
          <h1 data-l10n-id="onboarding.tour-private-browsing.title2"></h1>
          <p data-l10n-id="onboarding.tour-private-browsing.description2"></p>
        </section>
        <section class="onboarding-tour-content">
          <img src="resource://onboarding/img/figure_private.svg" />
        </section>
        <aside class="onboarding-tour-button-container">
          <button id="onboarding-tour-private-browsing-button" class="onboarding-tour-action-button" data-l10n-id="onboarding.tour-private-browsing.button"></button>
        </aside>
      `;
      return div;
    },
  },
  "addons": {
    id: "onboarding-tour-addons",
    tourNameId: "onboarding.tour-addons",
    getNotificationStrings(bundle) {
      return {
        title: bundle.GetStringFromName("onboarding.notification.onboarding-tour-addons.title"),
        message: bundle.formatStringFromName("onboarding.notification.onboarding-tour-addons.message", [BRAND_SHORT_NAME], 1),
        button: bundle.GetStringFromName("onboarding.button.learnMore"),
      };
    },
    getPage(win) {
      let div = win.document.createElement("div");
      div.innerHTML = `
        <section class="onboarding-tour-description">
          <h1 data-l10n-id="onboarding.tour-addons.title2"></h1>
          <p data-l10n-id="onboarding.tour-addons.description2"></p>
        </section>
        <section class="onboarding-tour-content">
          <img src="resource://onboarding/img/figure_addons.svg" />
        </section>
        <aside class="onboarding-tour-button-container">
          <button id="onboarding-tour-addons-button" class="onboarding-tour-action-button" data-l10n-id="onboarding.tour-addons.button"></button>
        </aside>
      `;
      return div;
    },
  },
  "customize": {
    id: "onboarding-tour-customize",
    tourNameId: "onboarding.tour-customize",
    getNotificationStrings(bundle) {
      return {
        title: bundle.GetStringFromName("onboarding.notification.onboarding-tour-customize.title"),
        message: bundle.formatStringFromName("onboarding.notification.onboarding-tour-customize.message", [BRAND_SHORT_NAME], 1),
        button: bundle.GetStringFromName("onboarding.button.learnMore"),
      };
    },
    getPage(win) {
      let div = win.document.createElement("div");
      div.innerHTML = `
        <section class="onboarding-tour-description">
          <h1 data-l10n-id="onboarding.tour-customize.title2"></h1>
          <p data-l10n-id="onboarding.tour-customize.description2"></p>
        </section>
        <section class="onboarding-tour-content">
          <img src="resource://onboarding/img/figure_customize.svg" />
        </section>
        <aside class="onboarding-tour-button-container">
          <button id="onboarding-tour-customize-button" class="onboarding-tour-action-button" data-l10n-id="onboarding.tour-customize.button"></button>
        </aside>
      `;
      return div;
    },
  },
  "search": {
    id: "onboarding-tour-search",
    tourNameId: "onboarding.tour-search2",
    getNotificationStrings(bundle) {
      return {
        title: bundle.GetStringFromName("onboarding.notification.onboarding-tour-search.title"),
        message: bundle.GetStringFromName("onboarding.notification.onboarding-tour-search.message"),
        button: bundle.GetStringFromName("onboarding.button.learnMore"),
      };
    },
    getPage(win) {
      let div = win.document.createElement("div");
      div.innerHTML = `
        <section class="onboarding-tour-description">
          <h1 data-l10n-id="onboarding.tour-search.title2"></h1>
          <p data-l10n-id="onboarding.tour-search.description2"></p>
        </section>
        <section class="onboarding-tour-content">
          <img src="resource://onboarding/img/figure_search.svg" />
        </section>
        <aside class="onboarding-tour-button-container">
          <button id="onboarding-tour-search-button" class="onboarding-tour-action-button" data-l10n-id="onboarding.tour-search.button"></button>
        </aside>
      `;
      return div;
    },
  },
  "default": {
    id: "onboarding-tour-default-browser",
    tourNameId: "onboarding.tour-default-browser",
    getNotificationStrings(bundle) {
      return {
        title: bundle.formatStringFromName("onboarding.notification.onboarding-tour-default-browser.title", [BRAND_SHORT_NAME], 1),
        message: bundle.formatStringFromName("onboarding.notification.onboarding-tour-default-browser.message", [BRAND_SHORT_NAME], 1),
        button: bundle.GetStringFromName("onboarding.button.learnMore"),
      };
    },
    getPage(win) {
      let div = win.document.createElement("div");
      let defaultBrowserButtonId = win.matchMedia("(-moz-os-version: windows-win7)").matches ?
        "onboarding.tour-default-browser.win7.button" : "onboarding.tour-default-browser.button";
      // eslint-disable-next-line no-unsanitized/property
      div.innerHTML = `
        <section class="onboarding-tour-description">
          <h1 data-l10n-id="onboarding.tour-default-browser.title2"></h1>
          <p data-l10n-id="onboarding.tour-default-browser.description2"></p>
        </section>
        <section class="onboarding-tour-content">
          <img src="resource://onboarding/img/figure_default.svg" />
        </section>
        <aside class="onboarding-tour-button-container">
          <button id="onboarding-tour-default-browser-button" class="onboarding-tour-action-button" data-l10n-id="${defaultBrowserButtonId}"></button>
        </aside>
      `;
      return div;
    },
  },
  "sync": {
    id: "onboarding-tour-sync",
    tourNameId: "onboarding.tour-sync2",
    getNotificationStrings(bundle) {
      return {
        title: bundle.GetStringFromName("onboarding.notification.onboarding-tour-sync.title"),
        message: bundle.GetStringFromName("onboarding.notification.onboarding-tour-sync.message"),
        button: bundle.GetStringFromName("onboarding.button.learnMore"),
      };
    },
    getPage(win, bundle) {
      let div = win.document.createElement("div");
      div.classList.add("onboarding-no-button");
      div.innerHTML = `
        <section class="onboarding-tour-description">
          <h1 data-l10n-id="onboarding.tour-sync.title2"></h1>
          <p data-l10n-id="onboarding.tour-sync.description2"></p>
        </section>
        <section class="onboarding-tour-content">
          <form>
            <h3 data-l10n-id="onboarding.tour-sync.form.title"></h3>
            <p data-l10n-id="onboarding.tour-sync.form.description"></p>
            <input id="onboarding-tour-sync-email-input" type="text"></input><br />
            <button id="onboarding-tour-sync-button" class="onboarding-tour-action-button" data-l10n-id="onboarding.tour-sync.button"></button>
          </form>
          <img src="resource://onboarding/img/figure_sync.svg" />
        </section>
      `;
      div.querySelector("#onboarding-tour-sync-email-input").placeholder =
        bundle.GetStringFromName("onboarding.tour-sync.email-input.placeholder");
      return div;
    },
  },
};

/**
 * The script won't be initialized if we turned off onboarding by
 * setting "browser.onboarding.enabled" to false.
 */
class Onboarding {
  constructor(contentWindow) {
    this.init(contentWindow);
  }

  async init(contentWindow) {
    this._window = contentWindow;
    this._tourItems = [];
    this._tourPages = [];
    this._tours = [];

    let tourIds = this._getTourIDList(Services.prefs.getStringPref("browser.onboarding.tour-type", "update"));
    tourIds.forEach(tourId => {
      if (onboardingTourset[tourId]) {
        this._tours.push(onboardingTourset[tourId]);
      }
    });

    if (this._tours.length === 0) {
      return;
    }

    // We want to create and append elements after CSS is loaded so
    // no flash of style changes and no additional reflow.
    await this._loadCSS();
    this._bundle = Services.strings.createBundle(BUNDLE_URI);

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
    this._initNotification();
  }

  _getTourIDList(tourType) {
    let tours = Services.prefs.getStringPref(`browser.onboarding.${tourType}tour`, "");
    return tours.split(",").filter(tourId => tourId !== "").map(tourId => tourId.trim());
  }

  _initNotification() {
    let doc = this._window.document;
    if (doc.hidden) {
      // When the preloaded-browser feature is on,
      // it would preload an hidden about:newtab in the background.
      // We don't want to show notification in that hidden state.
      let onVisible = () => {
        if (!doc.hidden) {
          doc.removeEventListener("visibilitychange", onVisible);
          this.showNotification();
        }
      };
      doc.addEventListener("visibilitychange", onVisible);
    } else {
      this.showNotification();
    }
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
    this._tours.forEach(tour => {
      let tourId = tour.id;
      this._prefsObserved.set(`browser.onboarding.tour.${tourId}.completed`, () => {
        this.markTourCompletionState(tourId);
      });
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
      case "onboarding-notification-close-btn":
        this.hideNotification();
        break;
      case "onboarding-notification-action-btn":
        let tourId = this._notificationBar.dataset.targetTourId;
        this.toggleOverlay();
        this.gotoPage(tourId);
        break;
    }
    let classList = evt.target.classList;
    if (classList.contains("onboarding-tour-item")) {
      this.gotoPage(evt.target.id);
    } else if (classList.contains("onboarding-tour-action-button")) {
      let activeItem = this._tourItems.find(item => item.classList.contains("onboarding-active"));
      this.setToursCompleted([ activeItem.id ]);
    }
  }

  destroy() {
    this._clearPrefObserver();
    this._overlayIcon.remove();
    this._overlay.remove();
    if (this._notificationBar) {
      this._notificationBar.remove();
    }
  }

  toggleOverlay() {
    if (this._tourItems.length == 0) {
      // Lazy loading until first toggle.
      this._loadTours(this._tours);
    }

    this.hideNotification();
    this._overlay.classList.toggle("onboarding-opened");

    let hiddenCheckbox = this._window.document.getElementById("onboarding-tour-hidden-checkbox");
    if (hiddenCheckbox.checked) {
      this.hide();
    }
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

  isTourCompleted(tourId) {
    return Preferences.get(`browser.onboarding.tour.${tourId}.completed`, false);
  }

  setToursCompleted(tourIds) {
    let params = [];
    tourIds.forEach(id => {
      if (!this.isTourCompleted(id)) {
        params.push({
          name: `browser.onboarding.tour.${id}.completed`,
          value: true
        });
      }
    });
    if (params.length > 0) {
      this.sendMessageToChrome("set-prefs", params);
    }
  }

  markTourCompletionState(tourId) {
    // We are doing lazy load so there might be no items.
    if (this._tourItems.length > 0 && this.isTourCompleted(tourId)) {
      let targetItem = this._tourItems.find(item => item.id == tourId);
      targetItem.classList.add("onboarding-complete");
    }
  }

  showNotification() {
    if (Preferences.get("browser.onboarding.notification.finished", false)) {
      return;
    }

    // Pick out the next target tour to show
    let targetTour = null;

    // Take the last tour as the default last prompted
    // so below would start from the 1st one if found no the last prompted from the pref.
    let lastPromptedId = this._tours[this._tours.length - 1].id;
    lastPromptedId = Preferences.get("browser.onboarding.notification.lastPrompted", lastPromptedId);

    let lastTourIndex = this._tours.findIndex(tour => tour.id == lastPromptedId);
    if (lastTourIndex < 0) {
      // Couldn't find the tour.
      // This could be because the pref was manually modified into unknown value
      // or the tour version has been updated so have an new tours set.
      // Take the last tour as the last prompted so would start from the 1st one below.
      lastTourIndex = this._tours.length - 1;
    }

    // Form tours to notify into the order we want.
    // For example, There are tour #0 ~ #5 and the #3 is the last prompted.
    // This would form [#4, #5, #0, #1, #2, #3].
    // So the 1st met incomplete tour in #4 ~ #2 would be the one to show.
    // Or #3 would be the one to show if #4 ~ #2 are all completed.
    let toursToNotify = [ ...this._tours.slice(lastTourIndex + 1), ...this._tours.slice(0, lastTourIndex + 1) ];
    targetTour = toursToNotify.find(tour => !this.isTourCompleted(tour.id));


    if (!targetTour) {
      this.sendMessageToChrome("set-prefs", [{
        name: "browser.onboarding.notification.finished",
        value: true
      }]);
      return;
    }

    // Show the target tour notification
    this._notificationBar = this._renderNotificationBar();
    this._notificationBar.addEventListener("click", this);
    this._window.document.body.appendChild(this._notificationBar);

    this._notificationBar.dataset.targetTourId = targetTour.id;
    let notificationStrings = targetTour.getNotificationStrings(this._bundle);
    let actionBtn = this._notificationBar.querySelector("#onboarding-notification-action-btn");
    actionBtn.textContent = notificationStrings.button;
    let tourTitle = this._notificationBar.querySelector("#onboarding-notification-tour-title");
    tourTitle.textContent = notificationStrings.title;
    let tourMessage = this._notificationBar.querySelector("#onboarding-notification-tour-message");
    tourMessage.textContent = notificationStrings.message;
    this._notificationBar.classList.add("onboarding-opened");

    this.sendMessageToChrome("set-prefs", [{
      name: "browser.onboarding.notification.lastPrompted",
      value: targetTour.id
    }]);
  }

  hideNotification() {
    if (this._notificationBar) {
      this._notificationBar.classList.remove("onboarding-opened");
    }
  }

  _renderNotificationBar() {
    let div = this._window.document.createElement("div");
    div.id = "onboarding-notification-bar";
    // We use `innerHTML` for more friendly reading.
    // The security should be fine because this is not from an external input.
    div.innerHTML = `
      <div id="onboarding-notification-icon"></div>
      <section id="onboarding-notification-message-section">
        <div id="onboarding-notification-tour-icon"></div>
        <div id="onboarding-notification-body">
          <h6 id="onboarding-notification-tour-title"></h6>
          <span id="onboarding-notification-tour-message"></span>
        </div>
        <button id="onboarding-notification-action-btn"></button>
      </section>
      <button id="onboarding-notification-close-btn"></button>
    `;
    let toolTip = this._bundle.formatStringFromName("onboarding.notification-icon-tool-tip", [BRAND_SHORT_NAME], 1);
    div.querySelector("#onboarding-notification-icon").setAttribute("data-tooltip", toolTip);
    return div;
  }

  hide() {
    this.setToursCompleted(this._tours.map(tour => tour.id));
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
    // We use `innerHTML` for more friendly reading.
    // The security should be fine because this is not from an external input.
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
       this._bundle.GetStringFromName("onboarding.hidden-checkbox-label-text");
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
      let div = tour.getPage(this._window, this._bundle);

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
    tours.forEach(tour => this.markTourCompletionState(tour.id));

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
