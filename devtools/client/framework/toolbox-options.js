/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const defer = require("devtools/shared/defer");
const {gDevTools} = require("devtools/client/framework/devtools");

const {LocalizationHelper} = require("devtools/shared/l10n");
const L10N = new LocalizationHelper("devtools/client/locales/toolbox.properties");

loader.lazyRequireGetter(this, "AppConstants", "resource://gre/modules/AppConstants.jsm", true);

exports.OptionsPanel = OptionsPanel;

function GetPref(name) {
  let type = Services.prefs.getPrefType(name);
  switch (type) {
    case Services.prefs.PREF_STRING:
      return Services.prefs.getCharPref(name);
    case Services.prefs.PREF_INT:
      return Services.prefs.getIntPref(name);
    case Services.prefs.PREF_BOOL:
      return Services.prefs.getBoolPref(name);
    default:
      throw new Error("Unknown type");
  }
}

function SetPref(name, value) {
  let type = Services.prefs.getPrefType(name);
  switch (type) {
    case Services.prefs.PREF_STRING:
      return Services.prefs.setCharPref(name, value);
    case Services.prefs.PREF_INT:
      return Services.prefs.setIntPref(name, value);
    case Services.prefs.PREF_BOOL:
      return Services.prefs.setBoolPref(name, value);
    default:
      throw new Error("Unknown type");
  }
}

function InfallibleGetBoolPref(key) {
  try {
    return Services.prefs.getBoolPref(key);
  } catch (ex) {
    return true;
  }
}

/**
 * Represents the Options Panel in the Toolbox.
 */
function OptionsPanel(iframeWindow, toolbox) {
  this.panelDoc = iframeWindow.document;
  this.panelWin = iframeWindow;

  this.toolbox = toolbox;
  this.isReady = false;

  this.setupToolsList = this.setupToolsList.bind(this);
  this._prefChanged = this._prefChanged.bind(this);
  this._themeRegistered = this._themeRegistered.bind(this);
  this._themeUnregistered = this._themeUnregistered.bind(this);
  this._disableJSClicked = this._disableJSClicked.bind(this);

  this.disableJSNode = this.panelDoc.getElementById("devtools-disable-javascript");

  this._addListeners();

  const EventEmitter = require("devtools/shared/event-emitter");
  EventEmitter.decorate(this);
}

OptionsPanel.prototype = {

  get target() {
    return this.toolbox.target;
  },

  async open() {
    // For local debugging we need to make the target remote.
    if (!this.target.isRemote) {
      await this.target.makeRemote();
    }

    this.setupToolsList();
    this.setupToolbarButtonsList();
    this.setupThemeList();
    this.setupNightlyOptions();
    await this.populatePreferences();
    this.isReady = true;
    this.emit("ready");
    return this;
  },

  _addListeners: function() {
    Services.prefs.addObserver("devtools.cache.disabled", this._prefChanged);
    Services.prefs.addObserver("devtools.theme", this._prefChanged);
    Services.prefs.addObserver("devtools.source-map.client-service.enabled",
                               this._prefChanged);
    gDevTools.on("theme-registered", this._themeRegistered);
    gDevTools.on("theme-unregistered", this._themeUnregistered);

    // Refresh the tools list when a new tool or webextension has been
    // registered to the toolbox.
    this.toolbox.on("tool-registered", this.setupToolsList);
    this.toolbox.on("webextension-registered", this.setupToolsList);
    // Refresh the tools list when a new tool or webextension has been
    // unregistered from the toolbox.
    this.toolbox.on("tool-unregistered", this.setupToolsList);
    this.toolbox.on("webextension-unregistered", this.setupToolsList);
  },

  _removeListeners: function() {
    Services.prefs.removeObserver("devtools.cache.disabled", this._prefChanged);
    Services.prefs.removeObserver("devtools.theme", this._prefChanged);
    Services.prefs.removeObserver("devtools.source-map.client-service.enabled",
                                  this._prefChanged);

    this.toolbox.off("tool-registered", this.setupToolsList);
    this.toolbox.off("tool-unregistered", this.setupToolsList);
    this.toolbox.off("webextension-registered", this.setupToolsList);
    this.toolbox.off("webextension-unregistered", this.setupToolsList);

    gDevTools.off("theme-registered", this._themeRegistered);
    gDevTools.off("theme-unregistered", this._themeUnregistered);
  },

  _prefChanged: function(subject, topic, prefName) {
    if (prefName === "devtools.cache.disabled") {
      let cacheDisabled = GetPref(prefName);
      let cbx = this.panelDoc.getElementById("devtools-disable-cache");
      cbx.checked = cacheDisabled;
    } else if (prefName === "devtools.theme") {
      this.updateCurrentTheme();
    } else if (prefName === "devtools.source-map.client-service.enabled") {
      this.updateSourceMapPref();
    }
  },

  _themeRegistered: function(themeId) {
    this.setupThemeList();
  },

  _themeUnregistered: function(theme) {
    let themeBox = this.panelDoc.getElementById("devtools-theme-box");
    let themeInput = themeBox.querySelector(`[value=${theme.id}]`);

    if (themeInput) {
      themeInput.parentNode.remove();
    }
  },

  async setupToolbarButtonsList() {
    // Ensure the toolbox is open, and the buttons are all set up.
    await this.toolbox.isOpen;

    let enabledToolbarButtonsBox = this.panelDoc.getElementById(
      "enabled-toolbox-buttons-box");

    let toolbarButtons = this.toolbox.toolbarButtons;

    if (!toolbarButtons) {
      console.warn("The command buttons weren't initiated yet.");
      return;
    }

    let onCheckboxClick = (checkbox) => {
      let commandButton = toolbarButtons.filter(
        toggleableButton => toggleableButton.id === checkbox.id)[0];
      Services.prefs.setBoolPref(
        commandButton.visibilityswitch, checkbox.checked);
      this.toolbox.updateToolboxButtonsVisibility();
    };

    let createCommandCheckbox = button => {
      let checkboxLabel = this.panelDoc.createElement("label");
      let checkboxSpanLabel = this.panelDoc.createElement("span");
      checkboxSpanLabel.textContent = button.description;
      let checkboxInput = this.panelDoc.createElement("input");
      checkboxInput.setAttribute("type", "checkbox");
      checkboxInput.setAttribute("id", button.id);
      if (button.isVisible) {
        checkboxInput.setAttribute("checked", true);
      }
      checkboxInput.addEventListener("change",
        onCheckboxClick.bind(this, checkboxInput));

      checkboxLabel.appendChild(checkboxInput);
      checkboxLabel.appendChild(checkboxSpanLabel);
      return checkboxLabel;
    };

    for (let button of toolbarButtons) {
      if (!button.isTargetSupported(this.toolbox.target)) {
        continue;
      }

      enabledToolbarButtonsBox.appendChild(createCommandCheckbox(button));
    }
  },

  setupToolsList: function() {
    let defaultToolsBox = this.panelDoc.getElementById("default-tools-box");
    let additionalToolsBox = this.panelDoc.getElementById(
      "additional-tools-box");
    let toolsNotSupportedLabel = this.panelDoc.getElementById(
      "tools-not-supported-label");
    let atleastOneToolNotSupported = false;

    const toolbox = this.toolbox;

    // Signal tool registering/unregistering globally (for the tools registered
    // globally) and per toolbox (for the tools registered to a single toolbox).
    // This event handler expect this to be binded to the related checkbox element.
    let onCheckboxClick = function(tool) {
      // Set the kill switch pref boolean to true
      Services.prefs.setBoolPref(tool.visibilityswitch, this.checked);

      if (!tool.isWebExtension) {
        gDevTools.emit(this.checked ? "tool-registered" : "tool-unregistered", tool.id);
      }
    };

    let createToolCheckbox = (tool) => {
      let checkboxLabel = this.panelDoc.createElement("label");
      let checkboxInput = this.panelDoc.createElement("input");
      checkboxInput.setAttribute("type", "checkbox");
      checkboxInput.setAttribute("id", tool.id);
      checkboxInput.setAttribute("title", tool.tooltip || "");

      let checkboxSpanLabel = this.panelDoc.createElement("span");
      if (tool.isTargetSupported(this.target)) {
        checkboxSpanLabel.textContent = tool.label;
      } else {
        atleastOneToolNotSupported = true;
        checkboxSpanLabel.textContent =
          L10N.getFormatStr("options.toolNotSupportedMarker", tool.label);
        checkboxInput.setAttribute("data-unsupported", "true");
        checkboxInput.setAttribute("disabled", "true");
      }

      if (InfallibleGetBoolPref(tool.visibilityswitch)) {
        checkboxInput.setAttribute("checked", "true");
      }

      checkboxInput.addEventListener("change", onCheckboxClick.bind(checkboxInput, tool));

      checkboxLabel.appendChild(checkboxInput);
      checkboxLabel.appendChild(checkboxSpanLabel);
      return checkboxLabel;
    };

    // Clean up any existent default tools content.
    for (let label of defaultToolsBox.querySelectorAll("label")) {
      label.remove();
    }

    // Populating the default tools lists
    let toggleableTools = gDevTools.getDefaultTools().filter(tool => {
      return tool.visibilityswitch && !tool.hiddenInOptions;
    });

    for (let tool of toggleableTools) {
      defaultToolsBox.appendChild(createToolCheckbox(tool));
    }

    // Clean up any existent additional tools content.
    for (let label of additionalToolsBox.querySelectorAll("label")) {
      label.remove();
    }

    // Populating the additional tools list.
    let atleastOneAddon = false;
    for (let tool of gDevTools.getAdditionalTools()) {
      atleastOneAddon = true;
      additionalToolsBox.appendChild(createToolCheckbox(tool));
    }

    // Populating the additional tools that came from the installed WebExtension add-ons.
    for (let {uuid, name, pref} of toolbox.listWebExtensions()) {
      atleastOneAddon = true;

      additionalToolsBox.appendChild(createToolCheckbox({
        isWebExtension: true,

        // Use the preference as the unified webextensions tool id.
        id: `webext-${uuid}`,
        tooltip: name,
        label: name,
        // Disable the devtools extension using the given pref name:
        // the toolbox options for the WebExtensions are not related to a single
        // tool (e.g. a devtools panel created from the extension devtools_page)
        // but to the entire devtools part of a webextension which is enabled
        // by the Addon Manager (but it may be disabled by its related
        // devtools about:config preference), and so the following
        visibilityswitch: pref,

        // Only local tabs are currently supported as targets.
        isTargetSupported: target => target.isLocalTab,
      }));
    }

    if (!atleastOneAddon) {
      additionalToolsBox.style.display = "none";
    } else {
      additionalToolsBox.style.display = "";
    }

    if (!atleastOneToolNotSupported) {
      toolsNotSupportedLabel.style.display = "none";
    } else {
      toolsNotSupportedLabel.style.display = "";
    }

    this.panelWin.focus();
  },

  setupThemeList: function() {
    let themeBox = this.panelDoc.getElementById("devtools-theme-box");
    let themeLabels = themeBox.querySelectorAll("label");
    for (let label of themeLabels) {
      label.remove();
    }

    let createThemeOption = theme => {
      let inputLabel = this.panelDoc.createElement("label");
      let inputRadio = this.panelDoc.createElement("input");
      inputRadio.setAttribute("type", "radio");
      inputRadio.setAttribute("value", theme.id);
      inputRadio.setAttribute("name", "devtools-theme-item");
      inputRadio.addEventListener("change", function(e) {
        SetPref(themeBox.getAttribute("data-pref"),
          e.target.value);
      });

      let inputSpanLabel = this.panelDoc.createElement("span");
      inputSpanLabel.textContent = theme.label;
      inputLabel.appendChild(inputRadio);
      inputLabel.appendChild(inputSpanLabel);

      return inputLabel;
    };

    // Populating the default theme list
    let themes = gDevTools.getThemeDefinitionArray();
    for (let theme of themes) {
      themeBox.appendChild(createThemeOption(theme));
    }

    this.updateCurrentTheme();
  },

  /**
   * Add common preferences enabled only on Nightly.
   */
  setupNightlyOptions: function() {
    let isNightly = AppConstants.NIGHTLY_BUILD;
    if (!isNightly) {
      return;
    }

    // Labels for these new buttons are nightly only and mostly intended for working on
    // devtools.
    let prefDefinitions = [{
      pref: "devtools.webconsole.new-frontend-enabled",
      label: L10N.getStr("toolbox.options.enableNewConsole.label"),
      id: "devtools-new-webconsole",
      parentId: "webconsole-options"
    }, {
      pref: "devtools.debugger.new-debugger-frontend",
      label: L10N.getStr("toolbox.options.enableNewDebugger.label"),
      id: "devtools-new-debugger",
      parentId: "debugger-options"
    }, {
      pref: "devtools.performance.new-panel-enabled",
      label: "Enable new performance recorder (then re-open DevTools)",
      id: "devtools-new-performance",
      parentId: "context-options"
    }];

    let createPreferenceOption = ({pref, label, id}) => {
      let inputLabel = this.panelDoc.createElement("label");
      let checkbox = this.panelDoc.createElement("input");
      checkbox.setAttribute("type", "checkbox");
      if (GetPref(pref)) {
        checkbox.setAttribute("checked", "checked");
      }
      checkbox.setAttribute("id", id);
      checkbox.addEventListener("change", e => {
        SetPref(pref, e.target.checked);
      });

      let inputSpanLabel = this.panelDoc.createElement("span");
      inputSpanLabel.textContent = label;
      inputLabel.appendChild(checkbox);
      inputLabel.appendChild(inputSpanLabel);

      return inputLabel;
    };

    for (let prefDefinition of prefDefinitions) {
      let parent = this.panelDoc.getElementById(prefDefinition.parentId);
      parent.appendChild(createPreferenceOption(prefDefinition));
      parent.removeAttribute("hidden");
    }
  },

  async populatePreferences() {
    let prefCheckboxes = this.panelDoc.querySelectorAll(
      "input[type=checkbox][data-pref]");
    for (let prefCheckbox of prefCheckboxes) {
      if (GetPref(prefCheckbox.getAttribute("data-pref"))) {
        prefCheckbox.setAttribute("checked", true);
      }
      prefCheckbox.addEventListener("change", function(e) {
        let checkbox = e.target;
        SetPref(checkbox.getAttribute("data-pref"), checkbox.checked);
      });
    }
    // Themes radio inputs are handled in setupThemeList
    let prefRadiogroups = this.panelDoc.querySelectorAll(
      ".radiogroup[data-pref]:not(#devtools-theme-box)");
    for (let radioGroup of prefRadiogroups) {
      let selectedValue = GetPref(radioGroup.getAttribute("data-pref"));

      for (let radioInput of radioGroup.querySelectorAll("input[type=radio]")) {
        if (radioInput.getAttribute("value") == selectedValue) {
          radioInput.setAttribute("checked", true);
        }

        radioInput.addEventListener("change", function(e) {
          SetPref(radioGroup.getAttribute("data-pref"),
            e.target.value);
        });
      }
    }
    let prefSelects = this.panelDoc.querySelectorAll("select[data-pref]");
    for (let prefSelect of prefSelects) {
      let pref = GetPref(prefSelect.getAttribute("data-pref"));
      let options = [...prefSelect.options];
      options.some(function(option) {
        let value = option.value;
        // non strict check to allow int values.
        if (value == pref) {
          prefSelect.selectedIndex = options.indexOf(option);
          return true;
        }
        return false;
      });

      prefSelect.addEventListener("change", function(e) {
        let select = e.target;
        SetPref(select.getAttribute("data-pref"),
          select.options[select.selectedIndex].value);
      });
    }

    if (this.target.activeTab) {
      let [ response ] = await this.target.client.attachTab(this.target.activeTab._actor);
      this._origJavascriptEnabled = !response.javascriptEnabled;
      this.disableJSNode.checked = this._origJavascriptEnabled;
      this.disableJSNode.addEventListener("click", this._disableJSClicked);
    } else {
      // Hide the checkbox and label
      this.disableJSNode.parentNode.style.display = "none";
    }
  },

  updateCurrentTheme: function() {
    let currentTheme = GetPref("devtools.theme");
    let themeBox = this.panelDoc.getElementById("devtools-theme-box");
    let themeRadioInput = themeBox.querySelector(`[value=${currentTheme}]`);

    if (themeRadioInput) {
      themeRadioInput.checked = true;
    } else {
      // If the current theme does not exist anymore, switch to light theme
      let lightThemeInputRadio = themeBox.querySelector("[value=light]");
      lightThemeInputRadio.checked = true;
    }
  },

  updateSourceMapPref: function() {
    const prefName = "devtools.source-map.client-service.enabled";
    let enabled = GetPref(prefName);
    let box = this.panelDoc.querySelector(`[data-pref="${prefName}"]`);
    box.checked = enabled;
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

  destroy: function() {
    if (this.destroyPromise) {
      return this.destroyPromise;
    }

    let deferred = defer();
    this.destroyPromise = deferred.promise;

    this._removeListeners();

    if (this.target.activeTab) {
      this.disableJSNode.removeEventListener("click", this._disableJSClicked);
      // FF41+ automatically cleans up state in actor on disconnect
      if (!this.target.activeTab.traits.noTabReconfigureOnClose) {
        let options = {
          "javascriptEnabled": this._origJavascriptEnabled,
          "performReload": false
        };
        this.target.activeTab.reconfigure(options, deferred.resolve);
      } else {
        deferred.resolve();
      }
    } else {
      deferred.resolve();
    }

    this.panelWin = this.panelDoc = this.disableJSNode = this.toolbox = null;

    return this.destroyPromise;
  }
};
