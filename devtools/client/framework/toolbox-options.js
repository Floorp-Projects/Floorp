/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const defer = require("devtools/shared/defer");
const {Task} = require("devtools/shared/task");
const {gDevTools} = require("devtools/client/framework/devtools");

const {LocalizationHelper} = require("devtools/shared/l10n");
const L10N = new LocalizationHelper("devtools/client/locales/toolbox.properties");

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

  this._prefChanged = this._prefChanged.bind(this);
  this._themeRegistered = this._themeRegistered.bind(this);
  this._themeUnregistered = this._themeUnregistered.bind(this);
  this._disableJSClicked = this._disableJSClicked.bind(this);

  this.disableJSNode = this.panelDoc.getElementById(
    "devtools-disable-javascript");

  this._addListeners();

  const EventEmitter = require("devtools/shared/event-emitter");
  EventEmitter.decorate(this);
}

OptionsPanel.prototype = {

  get target() {
    return this.toolbox.target;
  },

  open: Task.async(function* () {
    // For local debugging we need to make the target remote.
    if (!this.target.isRemote) {
      yield this.target.makeRemote();
    }

    this.setupToolsList();
    this.setupToolbarButtonsList();
    this.setupThemeList();
    yield this.populatePreferences();
    this.isReady = true;
    this.emit("ready");
    return this;
  }),

  _addListeners: function () {
    Services.prefs.addObserver("devtools.cache.disabled", this._prefChanged, false);
    Services.prefs.addObserver("devtools.theme", this._prefChanged, false);
    gDevTools.on("theme-registered", this._themeRegistered);
    gDevTools.on("theme-unregistered", this._themeUnregistered);
  },

  _removeListeners: function () {
    Services.prefs.removeObserver("devtools.cache.disabled", this._prefChanged);
    Services.prefs.removeObserver("devtools.theme", this._prefChanged);
    gDevTools.off("theme-registered", this._themeRegistered);
    gDevTools.off("theme-unregistered", this._themeUnregistered);
  },

  _prefChanged: function (subject, topic, prefName) {
    if (prefName === "devtools.cache.disabled") {
      let cacheDisabled = data.newValue;
      let cbx = this.panelDoc.getElementById("devtools-disable-cache");

      cbx.checked = cacheDisabled;
    } else if (prefName === "devtools.theme") {
      this.updateCurrentTheme();
    }
  },

  _themeRegistered: function (event, themeId) {
    this.setupThemeList();
  },

  _themeUnregistered: function (event, theme) {
    let themeBox = this.panelDoc.getElementById("devtools-theme-box");
    let themeInput = themeBox.querySelector(`[value=${theme.id}]`);

    if (themeInput) {
      themeInput.parentNode.remove();
    }
  },

  setupToolbarButtonsList: function () {
    let enabledToolbarButtonsBox = this.panelDoc.getElementById(
      "enabled-toolbox-buttons-box");

    let toggleableButtons = this.toolbox.toolboxButtons;
    let setToolboxButtonsVisibility =
      this.toolbox.setToolboxButtonsVisibility.bind(this.toolbox);

    let onCheckboxClick = (checkbox) => {
      let toolDefinition = toggleableButtons.filter(
        toggleableButton => toggleableButton.id === checkbox.id)[0];
      Services.prefs.setBoolPref(
        toolDefinition.visibilityswitch, checkbox.checked);
      setToolboxButtonsVisibility();
    };

    let createCommandCheckbox = tool => {
      let checkboxLabel = this.panelDoc.createElement("label");
      let checkboxSpanLabel = this.panelDoc.createElement("span");
      checkboxSpanLabel.textContent = tool.label;
      let checkboxInput = this.panelDoc.createElement("input");
      checkboxInput.setAttribute("type", "checkbox");
      checkboxInput.setAttribute("id", tool.id);
      if (InfallibleGetBoolPref(tool.visibilityswitch)) {
        checkboxInput.setAttribute("checked", true);
      }
      checkboxInput.addEventListener("change",
        onCheckboxClick.bind(this, checkboxInput));

      checkboxLabel.appendChild(checkboxInput);
      checkboxLabel.appendChild(checkboxSpanLabel);
      return checkboxLabel;
    };

    for (let tool of toggleableButtons) {
      if (!tool.isTargetSupported(this.toolbox.target)) {
        continue;
      }

      enabledToolbarButtonsBox.appendChild(createCommandCheckbox(tool));
    }
  },

  setupToolsList: function () {
    let defaultToolsBox = this.panelDoc.getElementById("default-tools-box");
    let additionalToolsBox = this.panelDoc.getElementById(
      "additional-tools-box");
    let toolsNotSupportedLabel = this.panelDoc.getElementById(
      "tools-not-supported-label");
    let atleastOneToolNotSupported = false;

    let onCheckboxClick = function (id) {
      let toolDefinition = gDevTools._tools.get(id);
      // Set the kill switch pref boolean to true
      Services.prefs.setBoolPref(toolDefinition.visibilityswitch, this.checked);
      gDevTools.emit(this.checked ? "tool-registered" : "tool-unregistered", id);
    };

    let createToolCheckbox = tool => {
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

      checkboxInput.addEventListener("change",
        onCheckboxClick.bind(checkboxInput, tool.id));

      checkboxLabel.appendChild(checkboxInput);
      checkboxLabel.appendChild(checkboxSpanLabel);
      return checkboxLabel;
    };

    // Populating the default tools lists
    let toggleableTools = gDevTools.getDefaultTools().filter(tool => {
      return tool.visibilityswitch && !tool.hiddenInOptions;
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
    }

    if (!atleastOneToolNotSupported) {
      toolsNotSupportedLabel.style.display = "none";
    }

    this.panelWin.focus();
  },

  setupThemeList: function () {
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
      inputRadio.addEventListener("change", function (e) {
        setPrefAndEmit(themeBox.getAttribute("data-pref"),
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

  populatePreferences: function () {
    let prefCheckboxes = this.panelDoc.querySelectorAll(
      "input[type=checkbox][data-pref]");
    for (let prefCheckbox of prefCheckboxes) {
      if (GetPref(prefCheckbox.getAttribute("data-pref"))) {
        prefCheckbox.setAttribute("checked", true);
      }
      prefCheckbox.addEventListener("change", function (e) {
        let checkbox = e.target;
        setPrefAndEmit(checkbox.getAttribute("data-pref"), checkbox.checked);
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

        radioInput.addEventListener("change", function (e) {
          setPrefAndEmit(radioGroup.getAttribute("data-pref"),
            e.target.value);
        });
      }
    }
    let prefSelects = this.panelDoc.querySelectorAll("select[data-pref]");
    for (let prefSelect of prefSelects) {
      let pref = GetPref(prefSelect.getAttribute("data-pref"));
      let options = [...prefSelect.options];
      options.some(function (option) {
        let value = option.value;
        // non strict check to allow int values.
        if (value == pref) {
          prefSelect.selectedIndex = options.indexOf(option);
          return true;
        }
      });

      prefSelect.addEventListener("change", function (e) {
        let select = e.target;
        setPrefAndEmit(select.getAttribute("data-pref"),
          select.options[select.selectedIndex].value);
      });
    }

    if (this.target.activeTab) {
      return this.target.client.attachTab(this.target.activeTab._actor)
        .then(([response, client]) => {
          this._origJavascriptEnabled = !response.javascriptEnabled;
          this.disableJSNode.checked = this._origJavascriptEnabled;
          this.disableJSNode.addEventListener("click",
            this._disableJSClicked, false);
        });
    }
    this.disableJSNode.hidden = true;
  },

  updateCurrentTheme: function () {
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
  _disableJSClicked: function (event) {
    let checked = event.target.checked;

    let options = {
      "javascriptEnabled": !checked
    };

    this.target.activeTab.reconfigure(options);
  },

  destroy: function () {
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

/* Set a pref and emit the pref-changed event if needed. */
function setPrefAndEmit(prefName, newValue) {
  let data = {
    pref: prefName,
    newValue: newValue
  };
  data.oldValue = GetPref(data.pref);
  SetPref(data.pref, data.newValue);

  if (data.newValue != data.oldValue) {
    gDevTools.emit("pref-changed", data);
  }
}
