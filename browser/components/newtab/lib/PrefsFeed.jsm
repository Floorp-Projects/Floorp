/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { actionCreators: ac, actionTypes: at } = ChromeUtils.import(
  "resource://activity-stream/common/Actions.jsm"
);
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Prefs } = ChromeUtils.import(
  "resource://activity-stream/lib/ActivityStreamPrefs.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");

const { FileUtils } = ChromeUtils.import(
  "resource://gre/modules/FileUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  Region: "resource://gre/modules/Region.jsm",
});

this.PrefsFeed = class PrefsFeed {
  constructor(prefMap) {
    this._prefMap = prefMap;
    this._prefs = new Prefs();
    this.onExperimentUpdated = this.onExperimentUpdated.bind(this);
    this.onPocketExperimentUpdated = this.onPocketExperimentUpdated.bind(this);
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
      if (name == "floorp.background.type" && value == 3) {
        this.getImage()
      }
    }
  }

  _setStringPref(values, key, defaultValue) {
    this._setPref(values, key, defaultValue, Services.prefs.getStringPref);
  }

  _setBoolPref(values, key, defaultValue) {
    this._setPref(values, key, defaultValue, Services.prefs.getBoolPref);
  }

  _setIntPref(values, key, defaultValue) {
    this._setPref(values, key, defaultValue, Services.prefs.getIntPref);
  }

  _setPref(values, key, defaultValue, getPrefFunction) {
    let value = getPrefFunction(
      `browser.newtabpage.activity-stream.${key}`,
      defaultValue
    );
    values[key] = value;
    this._prefMap.set(key, { value });
  }

  /**
   * Handler for when experiment data updates.
   */
  onExperimentUpdated(event, reason) {
    const value = NimbusFeatures.newtab.getAllVariables() || {};
    this.store.dispatch(
      ac.BroadcastToContent({
        type: at.PREF_CHANGED,
        data: {
          name: "featureConfig",
          value,
        },
      })
    );
  }

  /**
   * Handler for Pocket specific experiment data updates.
   */
  onPocketExperimentUpdated(event, reason) {
    const value = NimbusFeatures.pocketNewtab.getAllVariables() || {};
    this.store.dispatch(
      ac.BroadcastToContent({
        type: at.PREF_CHANGED,
        data: {
          name: "pocketConfig",
          value,
        },
      })
    );
  }

  init() {
    this._prefs.observeBranch(this);
    NimbusFeatures.newtab.onUpdate(this.onExperimentUpdated);
    NimbusFeatures.pocketNewtab.onUpdate(this.onPocketExperimentUpdated);

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

    // Save the geo pref if we have it
    if (Region.home) {
      values.region = Region.home;
      this.geo = values.region;
    } else if (this.geo !== "") {
      // Watch for geo changes and use a dummy value for now
      Services.obs.addObserver(this, Region.REGION_TOPIC);
      this.geo = "";
    }

    // Get the firefox accounts url for links and to send firstrun metrics to.
    values.fxa_endpoint = Services.prefs.getStringPref(
      "browser.newtabpage.activity-stream.fxaccounts.endpoint",
      "https://accounts.firefox.com"
    );

    // Get the firefox update channel with values as default, nightly, beta or release
    values.appUpdateChannel = Services.prefs.getStringPref(
      "app.update.channel",
      ""
    );

    // Read the pref for search shortcuts top sites experiment from firefox.js and store it
    // in our internal list of prefs to watch
    let searchTopSiteExperimentPrefValue = Services.prefs.getBoolPref(
      "browser.newtabpage.activity-stream.improvesearch.topSiteSearchShortcuts"
    );
    values[
      "improvesearch.topSiteSearchShortcuts"
    ] = searchTopSiteExperimentPrefValue;
    this._prefMap.set("improvesearch.topSiteSearchShortcuts", {
      value: searchTopSiteExperimentPrefValue,
    });

    values.mayHaveSponsoredTopSites = Services.prefs.getBoolPref(
      "browser.topsites.useRemoteSetting"
    );

    // Read the pref for search hand-off from firefox.js and store it
    // in our internal list of prefs to watch
    let handoffToAwesomebarPrefValue = Services.prefs.getBoolPref(
      "browser.newtabpage.activity-stream.improvesearch.handoffToAwesomebar"
    );
    values["improvesearch.handoffToAwesomebar"] = handoffToAwesomebarPrefValue;
    this._prefMap.set("improvesearch.handoffToAwesomebar", {
      value: handoffToAwesomebarPrefValue,
    });

    // Read the pref for the cached default engine name from firefox.js and
    // store it in our internal list of prefs to watch
    let placeholderPrefValue = Services.prefs.getStringPref(
      "browser.urlbar.placeholderName",
      ""
    );
    values["urlbar.placeholderName"] = placeholderPrefValue;
    this._prefMap.set("urlbar.placeholderName", {
      value: placeholderPrefValue,
    });

    // Add experiment values and default values
    values.featureConfig = NimbusFeatures.newtab.getAllVariables() || {};
    values.pocketConfig = NimbusFeatures.pocketNewtab.getAllVariables() || {};
    this._setBoolPref(values, "logowordmark.alwaysVisible", false);
    this._setBoolPref(values, "feeds.section.topstories", false);
    this._setBoolPref(values, "discoverystream.enabled", false);
    this._setBoolPref(
      values,
      "discoverystream.sponsored-collections.enabled",
      false
    );
    this._setIntPref(values, "floorp.background.type", 0);
    this._setBoolPref(values, "discoverystream.isCollectionDismissible", false);
    this._setBoolPref(values, "discoverystream.hardcoded-basic-layout", false);
    this._setBoolPref(values, "discoverystream.personalization.enabled", false);
    this._setBoolPref(values, "discoverystream.personalization.override");
    this._setStringPref(
      values,
      "discoverystream.personalization.modelKeys",
      ""
    );
    this._setStringPref(values, "discoverystream.spocs-endpoint", "");
    this._setStringPref(values, "discoverystream.spocs-endpoint-query", "");
    this._setStringPref(values, "newNewtabExperience.colors", "");

    // Set the initial state of all prefs in redux
    this.store.dispatch(
      ac.BroadcastToContent({
        type: at.PREFS_INITIAL_VALUES,
        data: values,
        meta: {
          isStartup: true,
        },
      })
    );
    Services.prefs.addObserver("browser.newtabpage.activity-stream.floorp.background.images.folder", this.getImage.bind(this))
    Services.prefs.addObserver("browser.newtabpage.activity-stream.floorp.background.images.extensions", this.getImage.bind(this))
    Services.obs.addObserver(this.getImage.bind(this), "floorp-newtab-background-update");
    this.getImage()
  }

  async getImage() {
    if (Services.prefs.getIntPref("browser.newtabpage.activity-stream.floorp.background.type") == 3) {
      let tPath = OS.Path.join(Services.prefs.getStringPref("browser.newtabpage.activity-stream.floorp.background.images.folder", "") || OS.Path.join(OS.Constants.Path.profileDir, "newtabImages"), "a").slice(0, -1)
      let folderExists = await IOUtils.exists(tPath)
      if (folderExists) {
        let imagesPath = await IOUtils.getChildren(tPath)
        let str = new RegExp(`\\.(${Services.prefs.getStringPref("browser.newtabpage.activity-stream.floorp.background.images.extensions", "").split(",").join("|")})+$`)
        let imagesDataPath = {data:{},urls:[]}
        
        this.store.dispatch(
          ac.BroadcastToContent({
            type: at.PREF_CHANGED,
            data: { name: "backgroundPaths", value: {data:{},urls:[]} },
          })
        );
        if (imagesPath != 0) {
          for (let elem of imagesPath) {
            if (!str.test(elem)) continue
            let filePath = Services.io.newFileURI(FileUtils.File(elem)).asciiSpec
            imagesDataPath.urls.push(filePath)
            imagesDataPath.data[filePath] = {}

            let blobData = await (await fetch(filePath)).blob()
            imagesDataPath.data[filePath].type = blobData.type
            let promise = new Promise(resolve => {
              const fr = new FileReader()
              fr.onload = e => resolve(e.target.result)
              fr.readAsArrayBuffer(blobData)
            })
            imagesDataPath.data[filePath].data = await promise
          }

        }
        console.log(imagesDataPath)
        this.store.dispatch(
          ac.BroadcastToContent({
            type: at.PREF_CHANGED,
            data: { name: "backgroundPaths", value: imagesDataPath },
          })
        );

      }
    }
  }

  uninit() {
    this.removeListeners();
  }

  removeListeners() {
    this._prefs.ignoreBranch(this);
    NimbusFeatures.newtab.off(this.onExperimentUpdated);
    NimbusFeatures.pocketNewtab.off(this.onPocketExperimentUpdated);
    if (this.geo === "") {
      Services.obs.removeObserver(this, Region.REGION_TOPIC);
    }
  }

  async _setIndexedDBPref(id, value) {
    const name = id === "topsites" ? id : `feeds.section.${id}`;
    try {
      await this._storage.set(name, value);
    } catch (e) {
      Cu.reportError("Could not set section preferences.");
    }
  }

  observe(subject, topic, data) {
    switch (topic) {
      case Region.REGION_TOPIC:
        this.store.dispatch(
          ac.BroadcastToContent({
            type: at.PREF_CHANGED,
            data: { name: "region", value: Region.home },
          })
        );
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
