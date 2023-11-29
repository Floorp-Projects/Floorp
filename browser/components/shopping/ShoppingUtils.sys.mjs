/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  isProductURL: "chrome://global/content/shopping/ShoppingProduct.mjs",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
});

XPCOMUtils.defineLazyModuleGetters(lazy, {
  ASRouter: "resource://activity-stream/lib/ASRouter.jsm",
});

const OPTED_IN_PREF = "browser.shopping.experience2023.optedIn";
const ACTIVE_PREF = "browser.shopping.experience2023.active";
const LAST_AUTO_ACTIVATE_PREF =
  "browser.shopping.experience2023.lastAutoActivate";
const AUTO_ACTIVATE_COUNT_PREF =
  "browser.shopping.experience2023.autoActivateCount";
const ADS_USER_ENABLED_PREF = "browser.shopping.experience2023.ads.userEnabled";

const CFR_FEATURES_PREF =
  "browser.newtabpage.activity-stream.asrouter.userprefs.cfr.features";

export const ShoppingUtils = {
  initialized: false,
  registered: false,
  handledAutoActivate: false,
  nimbusEnabled: false,
  nimbusControl: false,

  _updateNimbusVariables() {
    this.nimbusEnabled =
      lazy.NimbusFeatures.shopping2023.getVariable("enabled");
    this.nimbusControl =
      lazy.NimbusFeatures.shopping2023.getVariable("control");
  },

  onNimbusUpdate() {
    this._updateNimbusVariables();
    if (this.nimbusEnabled) {
      ShoppingUtils.init();
      Glean.shoppingSettings.nimbusDisabledShopping.set(false);
    } else {
      ShoppingUtils.uninit();
      Glean.shoppingSettings.nimbusDisabledShopping.set(true);
    }
  },

  // Runs once per session:
  // * at application startup, with startup idle tasks,
  // * or after the user is enrolled in the Nimbus experiment.
  init() {
    if (this.initialized) {
      return;
    }
    this.onNimbusUpdate = this.onNimbusUpdate.bind(this);

    if (!this.registered) {
      // Note (bug 1855545): we must set `this.registered` before calling
      // `onUpdate`, as it will immediately invoke `this.onNimbusUpdate`,
      // which in turn calls `ShoppingUtils.init`, creating an infinite loop.
      this.registered = true;
      lazy.NimbusFeatures.shopping2023.onUpdate(this.onNimbusUpdate);
      this._updateNimbusVariables();
    }

    if (!this.nimbusEnabled) {
      return;
    }

    // Do startup-time stuff here, like recording startup-time glean events
    // or adjusting onboarding-related prefs once per session.

    this.setOnUpdate(undefined, undefined, this.optedIn);
    this.recordUserAdsPreference();

    this.initialized = true;
  },

  // Runs once per session:
  // * when the user is unenrolled from the Nimbus experiment,
  // * or at shutdown, after quit-application-granted.
  uninit() {
    if (!this.initialized) {
      return;
    }

    // Do shutdown-time stuff here, like firing glean pings or modifying any
    // prefs for onboarding.

    this.initialized = false;
  },

  isProductPageNavigation(aLocationURI, aFlags) {
    if (!lazy.isProductURL(aLocationURI)) {
      return false;
    }

    // Ignore same-document navigation, except in the case of Walmart
    // as they use pushState to navigate between pages.
    let isWalmart = aLocationURI.host.includes("walmart");
    let isNewDocument = !aFlags;

    let isSameDocument =
      aFlags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT;
    let isReload = aFlags & Ci.nsIWebProgressListener.LOCATION_CHANGE_RELOAD;
    let isSessionRestore =
      aFlags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SESSION_STORE;

    // Unfortunately, Walmart sometimes double-fires history manipulation
    // events when navigating between product pages. To dedupe, cache the
    // last visited Walmart URL just for a few milliseconds, so we can avoid
    // double-counting such navigations.
    if (isWalmart) {
      if (
        this.lastWalmartURI &&
        aLocationURI.equalsExceptRef(this.lastWalmartURI)
      ) {
        return false;
      }
      this.lastWalmartURI = aLocationURI;
      lazy.setTimeout(() => {
        this.lastWalmartURI = null;
      }, 100);
    }

    return (
      // On initial visit to a product page, even from another domain, both a page
      // load and a pushState will be triggered by Walmart, so this will
      // capture only a single displayed event.
      (!isWalmart && (isNewDocument || isReload || isSessionRestore)) ||
      (isWalmart && isSameDocument)
    );
  },

  // For users in either the nimbus control or treatment groups, increment a
  // counter when they visit supported product pages.
  maybeRecordExposure(aLocationURI, aFlags) {
    if (
      (this.nimbusEnabled || this.nimbusControl) &&
      ShoppingUtils.isProductPageNavigation(aLocationURI, aFlags)
    ) {
      Glean.shopping.productPageVisits.add(1);
    }
  },

  setOnUpdate(_pref, _prev, current) {
    Glean.shoppingSettings.componentOptedOut.set(current === 2);
    Glean.shoppingSettings.hasOnboarded.set(current > 0);
  },

  recordUserAdsPreference() {
    Glean.shoppingSettings.disabledAds.set(!ShoppingUtils.adsUserEnabled);
  },

  /**
   * If the user has not opted in, automatically set the sidebar to `active` if:
   * 1. The sidebar has not already been automatically set to `active` twice.
   * 2. It's been at least 24 hours since the user last saw the sidebar because
   *    of this auto-activation behavior.
   * 3. This method has not already been called (handledAutoActivate is false)
   */
  handleAutoActivateOnProduct() {
    if (!this.handledAutoActivate && !this.optedIn && this.cfrFeatures) {
      let autoActivateCount = Services.prefs.getIntPref(
        AUTO_ACTIVATE_COUNT_PREF,
        0
      );
      let lastAutoActivate = Services.prefs.getIntPref(
        LAST_AUTO_ACTIVATE_PREF,
        0
      );
      let now = Date.now() / 1000;
      // If we automatically set `active` to true in a previous session less
      // than 24 hours ago, set it to false now. This is done to prevent the
      // auto-activation state from persisting between sessions. Effectively,
      // the auto-activation will persist until either 1) the sidebar is closed,
      // or 2) Firefox restarts.
      if (now - lastAutoActivate < 24 * 60 * 60) {
        Services.prefs.setBoolPref(ACTIVE_PREF, false);
      }
      // Set active to true if we haven't done so recently nor more than twice.
      else if (autoActivateCount < 2) {
        Services.prefs.setBoolPref(ACTIVE_PREF, true);
        Services.prefs.setIntPref(
          AUTO_ACTIVATE_COUNT_PREF,
          autoActivateCount + 1
        );
        Services.prefs.setIntPref(LAST_AUTO_ACTIVATE_PREF, now);
      }
    }
    this.handledAutoActivate = true;
  },

  /**
   * Send a Shopping-related trigger message to ASRouter.
   *
   * @param {object} trigger The trigger object to send to ASRouter.
   * @param {object} trigger.context Additional trigger properties to pass to
   *   the targeting context.
   * @param {string} trigger.id The id of the trigger.
   * @param {MozBrowser} trigger.browser The browser to associate with the
   *   trigger. (This can determine the tab/window the message is shown in,
   *   depending on the message surface)
   */
  async sendTrigger(trigger) {
    await lazy.ASRouter.waitForInitialized;
    await lazy.ASRouter.sendTriggerMessage(trigger);
  },
};

XPCOMUtils.defineLazyPreferenceGetter(
  ShoppingUtils,
  "optedIn",
  OPTED_IN_PREF,
  0,
  ShoppingUtils.setOnUpdate
);

XPCOMUtils.defineLazyPreferenceGetter(
  ShoppingUtils,
  "cfrFeatures",
  CFR_FEATURES_PREF,
  true
);

XPCOMUtils.defineLazyPreferenceGetter(
  ShoppingUtils,
  "adsUserEnabled",
  ADS_USER_ENABLED_PREF,
  false,
  ShoppingUtils.recordUserAdsPreference
);
