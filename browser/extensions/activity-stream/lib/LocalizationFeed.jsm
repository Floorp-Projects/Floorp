/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {utils: Cu} = Components;
const {actionTypes: at, actionCreators: ac} = Cu.import("resource://activity-stream/common/Actions.jsm", {});

Cu.importGlobalProperties(["fetch"]);
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");

// What is our default locale for the app?
const DEFAULT_LOCALE = "en-US";
// Event from LocaleService when locales are assigned
const LOCALES_CHANGE_TOPIC = "intl:requested-locales-changed";
// Where is the packaged locales json with all strings?
const LOCALES_FILE = "resource://activity-stream/data/locales.json";

this.LocalizationFeed = class LocalizationFeed {
  async init() {
    Services.obs.addObserver(this, LOCALES_CHANGE_TOPIC);

    let response = await fetch(LOCALES_FILE);
    this.allStrings = await response.json();

    this.updateLocale();
  }
  uninit() {
    Services.obs.removeObserver(this, LOCALES_CHANGE_TOPIC);
  }

  updateLocale() {
    let locale = Services.locale.getRequestedLocale() || DEFAULT_LOCALE;
    let strings = this.allStrings[locale];

    // Use the default strings for any that are missing
    if (locale !== DEFAULT_LOCALE) {
      strings = Object.assign({}, this.allStrings[DEFAULT_LOCALE], strings || {});
    }

    this.store.dispatch(ac.BroadcastToContent({
      type: at.LOCALE_UPDATED,
      data: {
        locale,
        strings
      }
    }));
  }

  observe(subject, topic, data) {
    switch (topic) {
      case LOCALES_CHANGE_TOPIC:
        this.updateLocale();
        break;
    }
  }

  onAction(action) {
    switch (action.type) {
      case at.INIT:
        this.init();
        break;
      case at.UNINIT:
        this.uninit();
        break;
    }
  }
};

this.EXPORTED_SYMBOLS = ["LocalizationFeed"];
