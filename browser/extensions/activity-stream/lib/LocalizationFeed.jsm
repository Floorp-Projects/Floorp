/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {utils: Cu} = Components;
Cu.import("resource://gre/modules/Services.jsm");
Cu.importGlobalProperties(["fetch"]);

const {actionCreators: ac, actionTypes: at} = Cu.import("resource://activity-stream/common/Actions.jsm", {});

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
    // Order locales based on what we have available with fallback always first
    const locales = Services.locale.negotiateLanguages(
      Services.locale.getAppLocalesAsLangTags(), // desired locales
      Object.keys(this.allStrings), // available locales
      DEFAULT_LOCALE // fallback
    ).reverse();

    // Start with default (first) locale then merge in strings of better locales
    const strings = Object.assign({}, ...locales.map(l => this.allStrings[l]));

    // Use the best (last) locale as the primary locale
    const locale = locales.pop();
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
