/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */
/* global AddonsComponent, DebuggerClient, DebuggerServer, React,
   RuntimesComponent, WorkersComponent */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;
const { loader } = Components.utils.import(
  "resource://devtools/shared/Loader.jsm", {});

loader.lazyRequireGetter(this, "AddonsComponent",
  "devtools/client/aboutdebugging/components/addons", true);
loader.lazyRequireGetter(this, "DebuggerClient",
  "devtools/shared/client/main", true);
loader.lazyRequireGetter(this, "DebuggerServer",
  "devtools/server/main", true);
loader.lazyRequireGetter(this, "Telemetry",
  "devtools/client/shared/telemetry");
loader.lazyRequireGetter(this, "WorkersComponent",
  "devtools/client/aboutdebugging/components/workers", true);
loader.lazyRequireGetter(this, "Services");

loader.lazyImporter(this, "AddonManager",
  "resource://gre/modules/AddonManager.jsm");

const Strings = Services.strings.createBundle(
  "chrome://devtools/locale/aboutdebugging.properties");

var AboutDebugging = {
  _prefListeners: [],

  _categories: null,
  get categories() {
    // If needed, initialize the list of available categories.
    if (!this._categories) {
      let elements = document.querySelectorAll(".category");
      this._categories = Array.map(elements, element => {
        let value = element.getAttribute("value");
        element.addEventListener("click", this.showTab.bind(this, value));
        return value;
      });
    }
    return this._categories;
  },

  showTab(category) {
    // If no category was specified, try the URL hash.
    if (!category) {
      category = location.hash.substr(1);
    }
    // If no corresponding category can be found, use the first available.
    let categories = this.categories;
    if (categories.indexOf(category) < 0) {
      category = categories[0];
    }
    // Show the corresponding tab and hide the others.
    document.querySelector(".tab.active").classList.remove("active");
    document.querySelector("#tab-" + category).classList.add("active");
    document.querySelector(".category[selected]").removeAttribute("selected");
    document.querySelector(".category[value=" + category + "]")
      .setAttribute("selected", "true");
    location.hash = "#" + category;
  },

  init() {
    let telemetry = this._telemetry = new Telemetry();
    telemetry.toolOpened("aboutdebugging");

    // Show the first available tab.
    this.showTab();
    window.addEventListener("hashchange", () => this.showTab());

    // Link checkboxes to prefs.
    let elements = document.querySelectorAll("input[type=checkbox][data-pref]");
    Array.map(elements, element => {
      let pref = element.dataset.pref;
      let updatePref = () => {
        Services.prefs.setBoolPref(pref, element.checked);
      };
      element.addEventListener("change", updatePref, false);
      let updateCheckbox = () => {
        element.checked = Services.prefs.getBoolPref(pref);
      };
      Services.prefs.addObserver(pref, updateCheckbox, false);
      this._prefListeners.push([pref, updateCheckbox]);
      updateCheckbox();
    });

    // Link buttons to their associated actions.
    let loadAddonButton = document.getElementById("load-addon-from-file");
    loadAddonButton.addEventListener("click", this.loadAddonFromFile);

    if (!DebuggerServer.initialized) {
      DebuggerServer.init();
      DebuggerServer.addBrowserActors();
    }
    DebuggerServer.allowChromeProcess = true;
    let client = new DebuggerClient(DebuggerServer.connectPipe());

    client.connect(() => {
      React.render(React.createElement(AddonsComponent, { client }),
        document.querySelector("#addons"));
      React.render(React.createElement(WorkersComponent, { client }),
        document.querySelector("#workers"));
    });
  },

  loadAddonFromFile() {
    let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
    fp.init(window,
      Strings.GetStringFromName("selectAddonFromFile"),
      Ci.nsIFilePicker.modeOpen);
    let res = fp.show();
    if (res == Ci.nsIFilePicker.returnCancel || !fp.file) {
      return;
    }
    let file = fp.file;
    // AddonManager.installTemporaryAddon accepts either
    // addon directory or final xpi file.
    if (!file.isDirectory() && !file.leafName.endsWith(".xpi")) {
      file = file.parent;
    }
    try {
      AddonManager.installTemporaryAddon(file);
    } catch(e) {
      alert("Error while installing the addon:\n" + e.message + "\n");
      throw e;
    }
  },

  destroy() {
    let telemetry = this._telemetry;
    telemetry.toolClosed("aboutdebugging");
    telemetry.destroy();

    this._prefListeners.forEach(([pref, listener]) => {
      Services.prefs.removeObserver(pref, listener);
    });
    this._prefListeners = [];

    React.unmountComponentAtNode(document.querySelector("#addons"));
    React.unmountComponentAtNode(document.querySelector("#workers"));
  },
};

window.addEventListener("DOMContentLoaded", function load() {
  window.removeEventListener("DOMContentLoaded", load);
  AboutDebugging.init();
});

window.addEventListener("unload", function unload() {
  window.removeEventListener("unload", unload);
  AboutDebugging.destroy();
});
