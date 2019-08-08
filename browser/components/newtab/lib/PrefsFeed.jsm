/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { actionCreators: ac, actionTypes: at } = ChromeUtils.import(
  "resource://activity-stream/common/Actions.jsm"
);
const { Prefs } = ChromeUtils.import(
  "resource://activity-stream/lib/ActivityStreamPrefs.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "AppConstants",
  "resource://gre/modules/AppConstants.jsm"
);

this.PrefsFeed = class PrefsFeed {
  constructor(prefMap) {
    this._prefMap = prefMap;
    this._prefs = new Prefs();
  }

  onPrefChanged(name, value) {
    const prefItem = this._prefMap.get(name);
    if (prefItem) {
      this.store.dispatch(
        ac[prefItem.skipBroadcast ? "OnlyToMain" : "BroadcastToContent"]({
          type: at.PREF_CHANGED,
          data: { name, value },
        })
      );
    }
  }

  init() {
    this._prefs.observeBranch(this);
    this._storage = this.store.dbStorage.getDbTable("sectionPrefs");

    // Get the initial value of each activity stream pref
    const values = {};
    for (const name of this._prefMap.keys()) {
      values[name] = this._prefs.get(name);
    }

    // These are not prefs, but are needed to determine stuff in content that can only be
    // computed in main process
    values.isPrivateBrowsingEnabled = PrivateBrowsingUtils.enabled;
    values.platform = AppConstants.platform;

    // Get the firefox accounts url for links and to send firstrun metrics to.
    values.fxa_endpoint = Services.prefs.getStringPref(
      "browser.newtabpage.activity-stream.fxaccounts.endpoint",
      "https://accounts.firefox.com"
    );

    // Read the pref for search shortcuts top sites experiment from firefox.js and store it
    // in our interal list of prefs to watch
    let searchTopSiteExperimentPrefValue = Services.prefs.getBoolPref(
      "browser.newtabpage.activity-stream.improvesearch.topSiteSearchShortcuts"
    );
    values[
      "improvesearch.topSiteSearchShortcuts"
    ] = searchTopSiteExperimentPrefValue;
    this._prefMap.set("improvesearch.topSiteSearchShortcuts", {
      value: searchTopSiteExperimentPrefValue,
    });

    // Read the pref for search hand-off from firefox.js and store it
    // in our interal list of prefs to watch
    let handoffToAwesomebarPrefValue = Services.prefs.getBoolPref(
      "browser.newtabpage.activity-stream.improvesearch.handoffToAwesomebar"
    );
    values["improvesearch.handoffToAwesomebar"] = handoffToAwesomebarPrefValue;
    this._prefMap.set("improvesearch.handoffToAwesomebar", {
      value: handoffToAwesomebarPrefValue,
    });

    // Set the initial state of all prefs in redux
    this.store.dispatch(
      ac.BroadcastToContent({ type: at.PREFS_INITIAL_VALUES, data: values })
    );
  }

  removeListeners() {
    this._prefs.ignoreBranch(this);
  }

  async _setIndexedDBPref(id, value) {
    const name = id === "topsites" ? id : `feeds.section.${id}`;
    try {
      await this._storage.set(name, value);
    } catch (e) {
      Cu.reportError("Could not set section preferences.");
    }
  }

  onAction(action) {
    switch (action.type) {
      case at.INIT:
        this.init();
        break;
      case at.UNINIT:
        this.removeListeners();
        break;
      case at.CLEAR_PREF:
        Services.prefs.clearUserPref(this._prefs._branchStr + action.data.name);
        break;
      case at.SET_PREF:
        this._prefs.set(action.data.name, action.data.value);
        break;
      case at.UPDATE_SECTION_PREFS:
        this._setIndexedDBPref(action.data.id, action.data.value);
        break;
    }
  }
};

const EXPORTED_SYMBOLS = ["PrefsFeed"];
