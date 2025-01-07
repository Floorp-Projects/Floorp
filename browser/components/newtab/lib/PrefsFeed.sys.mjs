/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  actionCreators as ac,
  actionTypes as at,
} from "resource://activity-stream/common/Actions.mjs";
import { Prefs } from "resource://activity-stream/lib/ActivityStreamPrefs.sys.mjs";

import { FileUtils } from "resource://gre/modules/FileUtils.sys.mjs";

// We use importESModule here instead of static import so that
// the Karma test environment won't choke on this module. This
// is because the Karma test environment already stubs out
// AppConstants, and overrides importESModule to be a no-op (which
// can't be done for a static import statement).

// eslint-disable-next-line mozilla/use-static-import
const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
  Region: "resource://gre/modules/Region.sys.mjs",
});

export class PrefsFeed {
  constructor(prefMap) {
    this.imagesDataPath = {urls:[]};
    this.oneImageData = {url:"",data:"",extension:"",notExist:false};
    this.fetchedImages = {};
    this._prefMap = prefMap;
    this._prefs = new Prefs();
    this.onExperimentUpdated = this.onExperimentUpdated.bind(this);
    this.onPocketExperimentUpdated = this.onPocketExperimentUpdated.bind(this);
  }

  onPrefChanged(name, value) {
    const prefItem = this._prefMap.get(name);
    if (prefItem) {
      let action = "BroadcastToContent";
      if (prefItem.skipBroadcast) {
        action = "OnlyToMain";
        if (prefItem.alsoToPreloaded) {
          action = "AlsoToPreloaded";
        }
      }

      this.store.dispatch(
        ac[action]({
          type: at.PREF_CHANGED,
          data: { name, value },
        })
      );
      if (name === "floorp.background.type" && value === 3){
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
  onExperimentUpdated() {
    const value = lazy.NimbusFeatures.newtab.getAllVariables() || {};
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
    const value = lazy.NimbusFeatures.pocketNewtab.getAllVariables() || {};
    // Loaded experiments are set up inside init()
    if (
      reason !== "feature-experiment-loaded" &&
      reason !== "feature-rollout-loaded"
    ) {
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
  }

  init() {
    this._prefs.observeBranch(this);
    lazy.NimbusFeatures.newtab.onUpdate(this.onExperimentUpdated);
    lazy.NimbusFeatures.pocketNewtab.onUpdate(this.onPocketExperimentUpdated);

    this._storage = this.store.dbStorage.getDbTable("sectionPrefs");

    // Get the initial value of each activity stream pref
    const values = {};
    for (const name of this._prefMap.keys()) {
      values[name] = this._prefs.get(name);
    }

    // These are not prefs, but are needed to determine stuff in content that can only be
    // computed in main process
    values.isPrivateBrowsingEnabled = lazy.PrivateBrowsingUtils.enabled;
    values.platform = AppConstants.platform;

    // Save the geo pref if we have it
    if (lazy.Region.home) {
      values.region = lazy.Region.home;
      this.geo = values.region;
    } else if (this.geo !== "") {
      // Watch for geo changes and use a dummy value for now
      Services.obs.addObserver(this, lazy.Region.REGION_TOPIC);
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
    values["improvesearch.topSiteSearchShortcuts"] =
      searchTopSiteExperimentPrefValue;
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

    // Add experiment values and default values
    values.featureConfig = lazy.NimbusFeatures.newtab.getAllVariables() || {};
    values.pocketConfig =
      lazy.NimbusFeatures.pocketNewtab.getAllVariables() || {};
    this._setBoolPref(values, "logowordmark.alwaysVisible", false);
    this._setBoolPref(values, "feeds.section.topstories", false);
    this._setBoolPref(values, "discoverystream.enabled", false);
    this._setBoolPref(
      values,
      "discoverystream.sponsored-collections.enabled",
      false
    );
    this._setIntPref(values, "floorp.background.type", 0);
    this._setBoolPref(values, "floorp.newtab.backdrop.blur.disable", false);
    this._setBoolPref(values, "floorp.newtab.releasenote.hide", false);
    this._setBoolPref(values, "floorp.newtab.imagecredit.hide", false);
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
    if (Services.prefs.getIntPref("browser.newtabpage.activity-stream.floorp.background.type") === 3) {
      const tPath = PathUtils.join(Services.prefs.getStringPref("browser.newtabpage.activity-stream.floorp.background.images.folder", "") || PathUtils.join(Services.dirsvc.get("ProfD", Ci.nsIFile).path, "newtabImages"), "a").slice(0, -1)
      const folderExists = await IOUtils.exists(tPath)
      if (folderExists) {
        const imagesPath = await IOUtils.getChildren(tPath)
        const str = new RegExp(`\\.(${Services.prefs.getStringPref("browser.newtabpage.activity-stream.floorp.background.images.extensions", "").split(",").join("|").toLowerCase()})+$`)
        this.imagesDataPath = {urls:[]}

        this.store.dispatch(
          ac.BroadcastToContent({
            type: at.PREF_CHANGED,
            data: { name: "backgroundPaths", value: {data:{},urls:[]} },
          })
        );
        this.fetchedImages = {}
        if (imagesPath !== 0) {
          for (const elem of imagesPath) {
            if (elem.toLowerCase().match(str)) {
              const filePath = Services.io.newFileURI(FileUtils.File(elem)).asciiSpec
              this.imagesDataPath.urls.push(filePath)
            }
          }
        }
        this.store.dispatch(
          ac.BroadcastToContent({
            type: at.PREF_CHANGED,
            data: { name: "backgroundPaths", value: this.imagesDataPath },
          })
        );

      }
    } else if (Services.prefs.getIntPref("browser.newtabpage.activity-stream.floorp.background.type") === 4){
      const tmpPath = Services.prefs.getStringPref("browser.newtabpage.activity-stream.floorp.background.image.path") || PathUtils.join(Services.dirsvc.get("ProfD", Ci.nsIFile).path, "newtabImages","wallpaper.png")
      const fetchPath = Services.io.newFileURI(FileUtils.File(tmpPath)).asciiSpec
      if (tmpPath !== this.oneImageData.url){
        this.oneImageData = {"url":tmpPath,data:"","extension":"",notExist:false}
      }
      this.store.dispatch(
        ac.BroadcastToContent({
          type: at.PREF_CHANGED,
          data: { name: "oneImageData", value: this.oneImageData},
        })
      );
      if (!this.oneImageData.data && !this.oneImageData.notExist)
      {
        if (await IOUtils.exists(tmpPath)){
          const blobData = await (await fetch(fetchPath)).blob()
          this.oneImageData.extension = blobData.type
          const promise = new Promise(resolve => {
            const fr = new FileReader()
            fr.onload = e => resolve(e.target.result)
            fr.readAsArrayBuffer(blobData)
          })
          this.oneImageData.data  = await promise.catch(() => null)
          this.store.dispatch(
            ac.BroadcastToContent({
              type: at.PREF_CHANGED,
              data: { name: "oneImageData", value: this.oneImageData },
            })
          );
        } else {
          this.oneImageData.notExist = true
        }
      }
    }
  }
  uninit() {
    this.removeListeners();
  }

  removeListeners() {
    this._prefs.ignoreBranch(this);
    lazy.NimbusFeatures.newtab.offUpdate(this.onExperimentUpdated);
    lazy.NimbusFeatures.pocketNewtab.offUpdate(this.onPocketExperimentUpdated);
    if (this.geo === "") {
      Services.obs.removeObserver(this, lazy.Region.REGION_TOPIC);
    }
  }

  async _setIndexedDBPref(id, value) {
    const name = id === "topsites" ? id : `feeds.section.${id}`;
    try {
      await this._storage.set(name, value);
    } catch (e) {
      console.error("Could not set section preferences.");
    }
  }

  observe(subject, topic) {
    switch (topic) {
      case lazy.Region.REGION_TOPIC:
        this.store.dispatch(
          ac.BroadcastToContent({
            type: at.PREF_CHANGED,
            data: { name: "region", value: lazy.Region.home },
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
      case at.GET_IMAGE:
        this.sendImgReply(action.data.path)
    }
  }

  async sendImgReply(path){
    if(!(path in this.fetchedImages)){
      const blobData = await (await fetch(path)).blob()
      this.fetchedImages[path] = {}
      this.fetchedImages[path].type = blobData.type
      const promise = new Promise(resolve => {
        const fr = new FileReader()
        fr.onload = e => resolve(e.target.result)
        fr.readAsArrayBuffer(blobData)
      })
      this.fetchedImages[path].data = await promise.catch(() => null)
    }
    const returnValue = this.fetchedImages[path]
    this.store.dispatch(
      ac.BroadcastToContent({
        type: at.PREF_CHANGED,
        data: { name: `floorpBackgroundPathsVal_${path}`, value: returnValue },
      })
    );
  }
}
