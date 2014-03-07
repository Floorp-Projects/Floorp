/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cu, Cc, Ci} = require("chrome");

let promise = require("sdk/core/promise");
let EventEmitter = require("devtools/toolkit/event-emitter");

Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource:///modules/devtools/gDevTools.jsm");

exports.OptionsPanel = OptionsPanel;

XPCOMUtils.defineLazyGetter(this, "l10n", function() {
  let bundle = Services.strings.createBundle("chrome://browser/locale/devtools/toolbox.properties");
  let l10n = function(aName, ...aArgs) {
    try {
      if (aArgs.length == 0) {
        return bundle.GetStringFromName(aName);
      } else {
        return bundle.formatStringFromName(aName, aArgs, aArgs.length);
      }
    } catch (ex) {
      Services.console.logStringMessage("Error reading '" + aName + "'");
    }
  };
  return l10n;
});

/**
 * Represents the Options Panel in the Toolbox.
 */
function OptionsPanel(iframeWindow, toolbox) {
  this.panelDoc = iframeWindow.document;
  this.panelWin = iframeWindow;
  this.toolbox = toolbox;
  this.isReady = false;

  EventEmitter.decorate(this);
}

OptionsPanel.prototype = {

  get target() {
    return this.toolbox.target;
  },

  open: function() {
    let targetPromise;

    // For local debugging we need to make the target remote.
    if (!this.target.isRemote) {
      targetPromise = this.target.makeRemote();
    } else {
      targetPromise = promise.resolve(this.target);
    }

    return targetPromise.then(() => {
      this.setupToolsList();
      this.setupToolbarButtonsList();
      this.populatePreferences();

      this._disableJSClicked = this._disableJSClicked.bind(this);
      this._disableCacheClicked = this._disableCacheClicked.bind(this);

      let disableJSNode = this.panelDoc.getElementById("devtools-disable-javascript");
      disableJSNode.addEventListener("click", this._disableJSClicked, false);

      let disableCacheNode = this.panelDoc.getElementById("devtools-disable-cache");
      disableCacheNode.addEventListener("click", this._disableCacheClicked, false);
    }).then(() => {
      this.isReady = true;
      this.emit("ready");
      return this;
    }).then(null, function onError(aReason) {
      Cu.reportError("OptionsPanel open failed. " +
                     aReason.error + ": " + aReason.message);
    });
  },

  setupToolbarButtonsList: function() {
    let enabledToolbarButtonsBox = this.panelDoc.getElementById("enabled-toolbox-buttons-box");
    enabledToolbarButtonsBox.textContent = "";

    let toggleableButtons = this.toolbox.toolboxButtons;
    let setToolboxButtonsVisibility =
      this.toolbox.setToolboxButtonsVisibility.bind(this.toolbox);

    let onCheckboxClick = (checkbox) => {
      let toolDefinition = toggleableButtons.filter(tool => tool.id === checkbox.id)[0];
      Services.prefs.setBoolPref(toolDefinition.visibilityswitch, checkbox.checked);
      setToolboxButtonsVisibility();
    };

    let createCommandCheckbox = tool => {
      let checkbox = this.panelDoc.createElement("checkbox");
      checkbox.setAttribute("id", tool.id);
      checkbox.setAttribute("label", tool.label);
      checkbox.setAttribute("checked", this.getBoolPref(tool.visibilityswitch));
      checkbox.addEventListener("command", onCheckboxClick.bind(this, checkbox));
      return checkbox;
    };

    for (let tool of toggleableButtons) {
      enabledToolbarButtonsBox.appendChild(createCommandCheckbox(tool));
    }
  },

  getBoolPref: function(key) {
    try {
      return Services.prefs.getBoolPref(key);
    }
    catch (ex) {
      return true;
    }
  },

  setupToolsList: function() {
    let defaultToolsBox = this.panelDoc.getElementById("default-tools-box");
    let additionalToolsBox = this.panelDoc.getElementById("additional-tools-box");
    let toolsNotSupportedLabel = this.panelDoc.getElementById("tools-not-supported-label");
    let atleastOneToolNotSupported = false;

    defaultToolsBox.textContent = "";
    additionalToolsBox.textContent = "";

    let onCheckboxClick = function(id) {
      let toolDefinition = gDevTools._tools.get(id);
      // Set the kill switch pref boolean to true
      Services.prefs.setBoolPref(toolDefinition.visibilityswitch, this.checked);
      if (this.checked) {
        gDevTools.emit("tool-registered", id);
      }
      else {
        gDevTools.emit("tool-unregistered", toolDefinition);
      }
    };

    let createToolCheckbox = tool => {
      let checkbox = this.panelDoc.createElement("checkbox");
      checkbox.setAttribute("id", tool.id);
      checkbox.setAttribute("tooltiptext", tool.tooltip || "");
      if (tool.isTargetSupported(this.target)) {
        checkbox.setAttribute("label", tool.label);
      }
      else {
        atleastOneToolNotSupported = true;
        checkbox.setAttribute("label",
                              l10n("options.toolNotSupportedMarker", tool.label));
        checkbox.setAttribute("unsupported", "");
      }
      checkbox.setAttribute("checked", this.getBoolPref(tool.visibilityswitch));
      checkbox.addEventListener("command", onCheckboxClick.bind(checkbox, tool.id));
      return checkbox;
    };

    // Populating the default tools lists
    let toggleableTools = gDevTools.getDefaultTools().filter(tool => {
      return tool.visibilityswitch
    });
    for (let tool of toggleableTools) {
      defaultToolsBox.appendChild(createToolCheckbox(tool));
    }

    // Populating the additional tools list that came from add-ons.
    let atleastOneAddon = false;
    for (let tool of gDevTools.getAdditionalTools()) {
      atleastOneAddon = true;
      additionalToolsBox.appendChild(createToolCheckbox(tool));
    }

    if (!atleastOneAddon) {
      additionalToolsBox.style.display = "none";
      additionalToolsBox.previousSibling.style.display = "none";
    }

    if (!atleastOneToolNotSupported) {
      toolsNotSupportedLabel.style.display = "none";
    }

    this.panelWin.focus();
  },

  populatePreferences: function() {
    let prefCheckboxes = this.panelDoc.querySelectorAll("checkbox[data-pref]");
    for (let checkbox of prefCheckboxes) {
      checkbox.checked = Services.prefs.getBoolPref(checkbox.getAttribute("data-pref"));
      checkbox.addEventListener("command", function() {
        let data = {
          pref: this.getAttribute("data-pref"),
          newValue: this.checked
        };
        data.oldValue = Services.prefs.getBoolPref(data.pref);
        Services.prefs.setBoolPref(data.pref, data.newValue);
        gDevTools.emit("pref-changed", data);
      }.bind(checkbox));
    }
    let prefRadiogroups = this.panelDoc.querySelectorAll("radiogroup[data-pref]");
    for (let radiogroup of prefRadiogroups) {
      let selectedValue = Services.prefs.getCharPref(radiogroup.getAttribute("data-pref"));
      for (let radio of radiogroup.childNodes) {
        radiogroup.selectedIndex = -1;
        if (radio.getAttribute("value") == selectedValue) {
          radiogroup.selectedItem = radio;
          break;
        }
      }
      radiogroup.addEventListener("select", function() {
        let data = {
          pref: this.getAttribute("data-pref"),
          newValue: this.selectedItem.getAttribute("value")
        };
        data.oldValue = Services.prefs.getCharPref(data.pref);
        Services.prefs.setCharPref(data.pref, data.newValue);
        gDevTools.emit("pref-changed", data);
      }.bind(radiogroup));
    }
    let prefMenulists = this.panelDoc.querySelectorAll("menulist[data-pref]");
    for (let menulist of prefMenulists) {
      let pref = Services.prefs.getCharPref(menulist.getAttribute("data-pref"));
      let menuitems = menulist.querySelectorAll("menuitem");
      for (let menuitem of menuitems) {
        let value = menuitem.getAttribute("value");
        if (value === pref) {
          menulist.selectedItem = menuitem;
          break;
        }
      }
      menulist.addEventListener("command", function() {
        let data = {
          pref: this.getAttribute("data-pref"),
          newValue: this.value
        };
        data.oldValue = Services.prefs.getCharPref(data.pref);
        Services.prefs.setCharPref(data.pref, data.newValue);
        gDevTools.emit("pref-changed", data);
      }.bind(menulist));
    }

    this.target.client.attachTab(this.target.activeTab._actor, (response) => {
      this._origJavascriptEnabled = response.javascriptEnabled;
      this._origCacheEnabled = response.cacheEnabled;

      this._populateDisableJSCheckbox();
      this._populateDisableCacheCheckbox();
    });
  },

  _populateDisableJSCheckbox: function() {
    let cbx = this.panelDoc.getElementById("devtools-disable-javascript");
    cbx.checked = !this._origJavascriptEnabled;
  },

  _populateDisableCacheCheckbox: function() {
    let cbx = this.panelDoc.getElementById("devtools-disable-cache");
    cbx.checked = !this._origCacheEnabled;
  },

  /**
   * Disables JavaScript for the currently loaded tab. We force a page refresh
   * here because setting docShell.allowJavascript to true fails to block JS
   * execution from event listeners added using addEventListener(), AJAX calls
   * and timers. The page refresh prevents these things from being added in the
   * first place.
   *
   * @param {Event} event
   *        The event sent by checking / unchecking the disable JS checkbox.
   */
  _disableJSClicked: function(event) {
    let checked = event.target.checked;

    let options = {
      "javascriptEnabled": !checked
    };

    this.target.activeTab.reconfigure(options);
  },

  /**
   * Disables the cache for the currently loaded tab.
   *
   * @param {Event} event
   *        The event sent by checking / unchecking the disable cache checkbox.
   */
  _disableCacheClicked: function(event) {
    let checked = event.target.checked;

    let options = {
      "cacheEnabled": !checked
    };

    this.target.activeTab.reconfigure(options);
  },

  destroy: function() {
    if (this.destroyPromise) {
      return this.destroyPromise;
    }

    let deferred = promise.defer();

    this.destroyPromise = deferred.promise;

    let disableJSNode = this.panelDoc.getElementById("devtools-disable-javascript");
    disableJSNode.removeEventListener("click", this._disableJSClicked, false);

    let disableCacheNode = this.panelDoc.getElementById("devtools-disable-cache");
    disableCacheNode.removeEventListener("click", this._disableCacheClicked, false);

    this.panelWin = this.panelDoc = null;
    this._disableJSClicked = this._disableCacheClicked = null;

    // If the cache or JavaScript is disabled we need to revert them to their
    // original values.
    let options = {
      "cacheEnabled": this._origCacheEnabled,
      "javascriptEnabled": this._origJavascriptEnabled
    };
    this.target.activeTab.reconfigure(options, () => {
      this.toolbox = null;
      deferred.resolve();
    }, true);

    return deferred.promise;
  }
};
