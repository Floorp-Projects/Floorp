/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const { gDevTools } = require("devtools/client/framework/devtools");

const { LocalizationHelper } = require("devtools/shared/l10n");
const L10N = new LocalizationHelper(
  "devtools/client/locales/toolbox.properties"
);

loader.lazyRequireGetter(
  this,
  "AppConstants",
  "resource://gre/modules/AppConstants.jsm",
  true
);
loader.lazyRequireGetter(
  this,
  "openDocLink",
  "devtools/client/shared/link",
  true
);

exports.OptionsPanel = OptionsPanel;

function GetPref(name) {
  const type = Services.prefs.getPrefType(name);
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
  const type = Services.prefs.getPrefType(name);
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
function OptionsPanel(iframeWindow, toolbox, commands) {
  this.panelDoc = iframeWindow.document;
  this.panelWin = iframeWindow;

  this.toolbox = toolbox;
  this.commands = commands;
  this.telemetry = toolbox.telemetry;

  this.setupToolsList = this.setupToolsList.bind(this);
  this._prefChanged = this._prefChanged.bind(this);
  this._themeRegistered = this._themeRegistered.bind(this);
  this._themeUnregistered = this._themeUnregistered.bind(this);
  this._disableJSClicked = this._disableJSClicked.bind(this);

  this.disableJSNode = this.panelDoc.getElementById(
    "devtools-disable-javascript"
  );

  this._addListeners();

  const EventEmitter = require("devtools/shared/event-emitter");
  EventEmitter.decorate(this);
}

OptionsPanel.prototype = {
  get target() {
    return this.toolbox.target;
  },

  async open() {
    this.setupToolsList();
    this.setupToolbarButtonsList();
    this.setupThemeList();
    this.setupAdditionalOptions();
    await this.populatePreferences();
    return this;
  },

  _addListeners: function() {
    Services.prefs.addObserver("devtools.cache.disabled", this._prefChanged);
    Services.prefs.addObserver("devtools.theme", this._prefChanged);
    Services.prefs.addObserver(
      "devtools.source-map.client-service.enabled",
      this._prefChanged
    );
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
    Services.prefs.removeObserver(
      "devtools.source-map.client-service.enabled",
      this._prefChanged
    );

    this.toolbox.off("tool-registered", this.setupToolsList);
    this.toolbox.off("tool-unregistered", this.setupToolsList);
    this.toolbox.off("webextension-registered", this.setupToolsList);
    this.toolbox.off("webextension-unregistered", this.setupToolsList);

    gDevTools.off("theme-registered", this._themeRegistered);
    gDevTools.off("theme-unregistered", this._themeUnregistered);
  },

  _prefChanged: function(subject, topic, prefName) {
    if (prefName === "devtools.cache.disabled") {
      const cacheDisabled = GetPref(prefName);
      const cbx = this.panelDoc.getElementById("devtools-disable-cache");
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
    const themeBox = this.panelDoc.getElementById("devtools-theme-box");
    const themeInput = themeBox.querySelector(`[value=${theme.id}]`);

    if (themeInput) {
      themeInput.parentNode.remove();
    }
  },

  async setupToolbarButtonsList() {
    // Ensure the toolbox is open, and the buttons are all set up.
    await this.toolbox.isOpen;

    const enabledToolbarButtonsBox = this.panelDoc.getElementById(
      "enabled-toolbox-buttons-box"
    );

    const toolbarButtons = this.toolbox.toolbarButtons;

    if (!toolbarButtons) {
      console.warn("The command buttons weren't initiated yet.");
      return;
    }

    const onCheckboxClick = checkbox => {
      const commandButton = toolbarButtons.filter(
        toggleableButton => toggleableButton.id === checkbox.id
      )[0];

      Services.prefs.setBoolPref(
        commandButton.visibilityswitch,
        checkbox.checked
      );
      this.toolbox.updateToolboxButtonsVisibility();
    };

    const createCommandCheckbox = button => {
      const checkboxLabel = this.panelDoc.createElement("label");
      const checkboxSpanLabel = this.panelDoc.createElement("span");
      checkboxSpanLabel.textContent = button.description;
      const checkboxInput = this.panelDoc.createElement("input");
      checkboxInput.setAttribute("type", "checkbox");
      checkboxInput.setAttribute("id", button.id);

      if (Services.prefs.getBoolPref(button.visibilityswitch, true)) {
        checkboxInput.setAttribute("checked", true);
      }
      checkboxInput.addEventListener(
        "change",
        onCheckboxClick.bind(this, checkboxInput)
      );

      checkboxLabel.appendChild(checkboxInput);
      checkboxLabel.appendChild(checkboxSpanLabel);

      return checkboxLabel;
    };

    for (const button of toolbarButtons) {
      if (!button.isTargetSupported(this.toolbox.target)) {
        continue;
      }

      enabledToolbarButtonsBox.appendChild(createCommandCheckbox(button));
    }
  },

  setupToolsList: function() {
    const defaultToolsBox = this.panelDoc.getElementById("default-tools-box");
    const additionalToolsBox = this.panelDoc.getElementById(
      "additional-tools-box"
    );
    const toolsNotSupportedLabel = this.panelDoc.getElementById(
      "tools-not-supported-label"
    );
    let atleastOneToolNotSupported = false;

    // Signal tool registering/unregistering globally (for the tools registered
    // globally) and per toolbox (for the tools registered to a single toolbox).
    // This event handler expect this to be binded to the related checkbox element.
    const onCheckboxClick = function(telemetry, tool) {
      // Set the kill switch pref boolean to true
      Services.prefs.setBoolPref(tool.visibilityswitch, this.checked);

      if (!tool.isWebExtension) {
        gDevTools.emit(
          this.checked ? "tool-registered" : "tool-unregistered",
          tool.id
        );
        // Record which tools were registered and unregistered.
        telemetry.keyedScalarSet(
          "devtools.tool.registered",
          tool.id,
          this.checked
        );
      }
    };

    const createToolCheckbox = tool => {
      const checkboxLabel = this.panelDoc.createElement("label");
      const checkboxInput = this.panelDoc.createElement("input");
      checkboxInput.setAttribute("type", "checkbox");
      checkboxInput.setAttribute("id", tool.id);
      checkboxInput.setAttribute("title", tool.tooltip || "");

      const checkboxSpanLabel = this.panelDoc.createElement("span");
      if (tool.isTargetSupported(this.target)) {
        checkboxSpanLabel.textContent = tool.label;
      } else {
        atleastOneToolNotSupported = true;
        checkboxSpanLabel.textContent = L10N.getFormatStr(
          "options.toolNotSupportedMarker",
          tool.label
        );
        checkboxInput.setAttribute("data-unsupported", "true");
        checkboxInput.setAttribute("disabled", "true");
      }

      if (InfallibleGetBoolPref(tool.visibilityswitch)) {
        checkboxInput.setAttribute("checked", "true");
      }

      checkboxInput.addEventListener(
        "change",
        onCheckboxClick.bind(checkboxInput, this.telemetry, tool)
      );

      checkboxLabel.appendChild(checkboxInput);
      checkboxLabel.appendChild(checkboxSpanLabel);

      // We shouldn't have deprecated tools anymore, but we might have one in the future,
      // when migrating the storage inspector to the application panel (Bug 1681059).
      // Let's keep this code for now so we keep the l10n property around and avoid
      // unnecessary translation work if we need it again in the future.
      if (tool.deprecated) {
        const deprecationURL = this.panelDoc.createElement("a");
        deprecationURL.title = deprecationURL.href = tool.deprecationURL;
        deprecationURL.textContent = L10N.getStr("options.deprecationNotice");
        // Cannot use a real link when we are in the Browser Toolbox.
        deprecationURL.addEventListener("click", e => {
          e.preventDefault();
          openDocLink(tool.deprecationURL, { relatedToCurrent: true });
        });

        const checkboxSpanDeprecated = this.panelDoc.createElement("span");
        checkboxSpanDeprecated.className = "deprecation-notice";
        checkboxLabel.appendChild(checkboxSpanDeprecated);
        checkboxSpanDeprecated.appendChild(deprecationURL);
      }

      return checkboxLabel;
    };

    // Clean up any existent default tools content.
    for (const label of defaultToolsBox.querySelectorAll("label")) {
      label.remove();
    }

    // Populating the default tools lists
    const toggleableTools = gDevTools.getDefaultTools().filter(tool => {
      return tool.visibilityswitch && !tool.hiddenInOptions;
    });

    const fragment = this.panelDoc.createDocumentFragment();
    for (const tool of toggleableTools) {
      fragment.appendChild(createToolCheckbox(tool));
    }

    const toolsNotSupportedLabelNode = this.panelDoc.getElementById(
      "tools-not-supported-label"
    );
    defaultToolsBox.insertBefore(fragment, toolsNotSupportedLabelNode);

    // Clean up any existent additional tools content.
    for (const label of additionalToolsBox.querySelectorAll("label")) {
      label.remove();
    }

    // Populating the additional tools list.
    let atleastOneAddon = false;
    for (const tool of gDevTools.getAdditionalTools()) {
      atleastOneAddon = true;
      additionalToolsBox.appendChild(createToolCheckbox(tool));
    }

    // Populating the additional tools that came from the installed WebExtension add-ons.
    for (const { uuid, name, pref } of this.toolbox.listWebExtensions()) {
      atleastOneAddon = true;

      additionalToolsBox.appendChild(
        createToolCheckbox({
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
        })
      );
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
    const themeBox = this.panelDoc.getElementById("devtools-theme-box");
    const themeLabels = themeBox.querySelectorAll("label");
    for (const label of themeLabels) {
      label.remove();
    }

    const createThemeOption = theme => {
      const inputLabel = this.panelDoc.createElement("label");
      const inputRadio = this.panelDoc.createElement("input");
      inputRadio.setAttribute("type", "radio");
      inputRadio.setAttribute("value", theme.id);
      inputRadio.setAttribute("name", "devtools-theme-item");
      inputRadio.addEventListener("change", function(e) {
        SetPref(themeBox.getAttribute("data-pref"), e.target.value);
      });

      const inputSpanLabel = this.panelDoc.createElement("span");
      inputSpanLabel.textContent = theme.label;
      inputLabel.appendChild(inputRadio);
      inputLabel.appendChild(inputSpanLabel);

      return inputLabel;
    };

    // Populating the default theme list
    themeBox.appendChild(
      createThemeOption({
        id: "auto",
        label: L10N.getStr("options.autoTheme.label"),
      })
    );

    const themes = gDevTools.getThemeDefinitionArray();
    for (const theme of themes) {
      themeBox.appendChild(createThemeOption(theme));
    }

    this.updateCurrentTheme();
  },

  /**
   * Add extra checkbox options bound to a boolean preference.
   */
  setupAdditionalOptions: function() {
    const prefDefinitions = [];

    // New performance panel can be used in NIGHTLY or DEV_EDITION. We keep the
    // setting hidden in RELEASE or BETA. Should be removed in Bug 1693316.
    const isNewPerfAllowed =
      AppConstants.NIGHTLY_BUILD || AppConstants.MOZ_DEV_EDITION;
    if (isNewPerfAllowed) {
      prefDefinitions.push({
        pref: "devtools.performance.new-panel-enabled",
        label: L10N.getStr("options.enableNewPerformancePanel"),
        id: "devtools-new-performance",
        parentId: "context-options",
      });
    }

    if (this.toolbox.isBrowserToolbox) {
      // The Multiprocess Browser Toolbox is only displayed in the settings
      // panel for the Browser Toolbox, or when debugging the main process in
      // remote debugging.
      prefDefinitions.push({
        pref: "devtools.browsertoolbox.fission",
        label: L10N.getStr("options.enableMultiProcessToolbox"),
        id: "devtools-browsertoolbox-fission",
        parentId: "context-options",
        // createPreferenceOption already updates the value of the preference
        // for the current profile when the checkbox changes. Here we need a
        // custom behavior for the Browser Toolbox, so we pass an additional
        // onChange callback.
        onChange: async checked => {
          if (!this.toolbox.isBrowserToolbox) {
            // If we are debugging a parent process, but the toolbox is not a
            // Browser Toolbox, it means we are remote debugging another
            // browser. In this case, the value of devtools.browsertoolbox.fission
            // should not be updated in the target browser.
            return;
          }

          // When setting this preference from the BrowserToolbox, we need to
          // update the preference on the debugged Firefox profile as well.
          // The devtools.browsertoolbox.fission preference is copied from the
          // regular Firefox Profile to the Browser Toolbox profile.
          // If the preference is not updated on the regular Firefox profile, the
          // new value will be lost on the next Browser Toolbox restart.
          const { mainRoot } = this.commands.client;
          const preferenceFront = await mainRoot.getFront("preference");
          preferenceFront.setBoolPref(
            "devtools.browsertoolbox.fission",
            checked
          );
        },
      });
    }

    const createPreferenceOption = ({ pref, label, id, onChange }) => {
      const inputLabel = this.panelDoc.createElement("label");
      const checkbox = this.panelDoc.createElement("input");
      checkbox.setAttribute("type", "checkbox");
      if (GetPref(pref)) {
        checkbox.setAttribute("checked", "checked");
      }
      checkbox.setAttribute("id", id);
      checkbox.addEventListener("change", e => {
        SetPref(pref, e.target.checked);
        if (onChange) {
          onChange(e.target.checked);
        }
      });

      const inputSpanLabel = this.panelDoc.createElement("span");
      inputSpanLabel.textContent = label;
      inputLabel.appendChild(checkbox);
      inputLabel.appendChild(inputSpanLabel);

      return inputLabel;
    };

    for (const prefDefinition of prefDefinitions) {
      const parent = this.panelDoc.getElementById(prefDefinition.parentId);
      // We want to insert the new definition after the last existing
      // definition, but before any other element.
      // For example in the "Advanced Settings" column there's indeed a <span>
      // text at the end, and we want that it stays at the end.
      // The reference element can be `null` if there's no label or if there's
      // no element after the last label. But that's OK and it will do what we
      // want.
      const referenceElement = parent.querySelector("label:last-of-type + *");
      parent.insertBefore(
        createPreferenceOption(prefDefinition),
        referenceElement
      );
    }
  },

  async populatePreferences() {
    const prefCheckboxes = this.panelDoc.querySelectorAll(
      "input[type=checkbox][data-pref]"
    );
    for (const prefCheckbox of prefCheckboxes) {
      if (GetPref(prefCheckbox.getAttribute("data-pref"))) {
        prefCheckbox.setAttribute("checked", true);
      }
      prefCheckbox.addEventListener("change", function(e) {
        const checkbox = e.target;
        SetPref(checkbox.getAttribute("data-pref"), checkbox.checked);
      });
    }
    // Themes radio inputs are handled in setupThemeList
    const prefRadiogroups = this.panelDoc.querySelectorAll(
      ".radiogroup[data-pref]:not(#devtools-theme-box)"
    );
    for (const radioGroup of prefRadiogroups) {
      const selectedValue = GetPref(radioGroup.getAttribute("data-pref"));

      for (const radioInput of radioGroup.querySelectorAll(
        "input[type=radio]"
      )) {
        if (radioInput.getAttribute("value") == selectedValue) {
          radioInput.setAttribute("checked", true);
        }

        radioInput.addEventListener("change", function(e) {
          SetPref(radioGroup.getAttribute("data-pref"), e.target.value);
        });
      }
    }
    const prefSelects = this.panelDoc.querySelectorAll("select[data-pref]");
    for (const prefSelect of prefSelects) {
      const pref = GetPref(prefSelect.getAttribute("data-pref"));
      const options = [...prefSelect.options];
      options.some(function(option) {
        const value = option.value;
        // non strict check to allow int values.
        if (value == pref) {
          prefSelect.selectedIndex = options.indexOf(option);
          return true;
        }
        return false;
      });

      prefSelect.addEventListener("change", function(e) {
        const select = e.target;
        SetPref(
          select.getAttribute("data-pref"),
          select.options[select.selectedIndex].value
        );
      });
    }

    if (!this.target.chrome) {
      const isJavascriptEnabled = await this.commands.targetConfigurationCommand.isJavascriptEnabled();
      this.disableJSNode.checked = !isJavascriptEnabled;
      this.disableJSNode.addEventListener("click", this._disableJSClicked);
    } else {
      // Hide the checkbox and label
      this.disableJSNode.parentNode.style.display = "none";

      const triggersPageRefreshLabel = this.panelDoc.getElementById(
        "triggers-page-refresh-label"
      );
      triggersPageRefreshLabel.style.display = "none";
    }
  },

  updateCurrentTheme: function() {
    const currentTheme = GetPref("devtools.theme");
    const themeBox = this.panelDoc.getElementById("devtools-theme-box");
    const themeRadioInput = themeBox.querySelector(`[value=${currentTheme}]`);

    if (themeRadioInput) {
      themeRadioInput.checked = true;
    } else {
      // If the current theme does not exist anymore, switch to auto theme
      const autoThemeInputRadio = themeBox.querySelector("[value=auto]");
      autoThemeInputRadio.checked = true;
    }
  },

  updateSourceMapPref: function() {
    const prefName = "devtools.source-map.client-service.enabled";
    const enabled = GetPref(prefName);
    const box = this.panelDoc.querySelector(`[data-pref="${prefName}"]`);
    box.checked = enabled;
  },

  /**
   * Disables JavaScript for the currently loaded tab. We force a page refresh
   * here because setting browsingContext.allowJavascript to true fails to block
   * JS execution from event listeners added using addEventListener(), AJAX
   * calls and timers. The page refresh prevents these things from being added
   * in the first place.
   *
   * @param {Event} event
   *        The event sent by checking / unchecking the disable JS checkbox.
   */
  _disableJSClicked: function(event) {
    const checked = event.target.checked;

    this.commands.targetConfigurationCommand.updateConfiguration({
      javascriptEnabled: !checked,
    });
  },

  destroy: function() {
    if (this.destroyed) {
      return;
    }
    this.destroyed = true;

    this._removeListeners();

    this.disableJSNode.removeEventListener("click", this._disableJSClicked);

    this.panelWin = this.panelDoc = this.disableJSNode = this.toolbox = null;
  },
};
