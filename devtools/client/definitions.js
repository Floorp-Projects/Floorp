/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const osString = Services.appinfo.OS;

// Panels
loader.lazyGetter(
  this,
  "OptionsPanel",
  () => require("devtools/client/framework/toolbox-options").OptionsPanel
);
loader.lazyGetter(
  this,
  "InspectorPanel",
  () => require("devtools/client/inspector/panel").InspectorPanel
);
loader.lazyGetter(
  this,
  "WebConsolePanel",
  () => require("devtools/client/webconsole/panel").WebConsolePanel
);
loader.lazyGetter(
  this,
  "DebuggerPanel",
  () => require("devtools/client/debugger/panel").DebuggerPanel
);
loader.lazyGetter(
  this,
  "StyleEditorPanel",
  () => require("devtools/client/styleeditor/panel").StyleEditorPanel
);
loader.lazyGetter(
  this,
  "MemoryPanel",
  () => require("devtools/client/memory/panel").MemoryPanel
);
loader.lazyGetter(
  this,
  "PerformancePanel",
  () => require("devtools/client/performance/panel").PerformancePanel
);
loader.lazyGetter(
  this,
  "NewPerformancePanel",
  () => require("devtools/client/performance-new/panel").PerformancePanel
);
loader.lazyGetter(
  this,
  "NetMonitorPanel",
  () => require("devtools/client/netmonitor/panel").NetMonitorPanel
);
loader.lazyGetter(
  this,
  "StoragePanel",
  () => require("devtools/client/storage/panel").StoragePanel
);
loader.lazyGetter(
  this,
  "DomPanel",
  () => require("devtools/client/dom/panel").DomPanel
);
loader.lazyGetter(
  this,
  "AccessibilityPanel",
  () => require("devtools/client/accessibility/panel").AccessibilityPanel
);
loader.lazyGetter(
  this,
  "ApplicationPanel",
  () => require("devtools/client/application/panel").ApplicationPanel
);

// Other dependencies
loader.lazyRequireGetter(
  this,
  "ResponsiveUIManager",
  "devtools/client/responsive/manager"
);

loader.lazyRequireGetter(
  this,
  "AppConstants",
  "resource://gre/modules/AppConstants.jsm",
  true
);
loader.lazyRequireGetter(
  this,
  "DevToolsExperimentalPrefs",
  "devtools/client/devtools-experimental-prefs"
);
loader.lazyRequireGetter(
  this,
  "captureAndSaveScreenshot",
  "devtools/client/shared/screenshot",
  true
);

const { LocalizationHelper } = require("devtools/shared/l10n");
const L10N = new LocalizationHelper(
  "devtools/client/locales/startup.properties"
);
const CommandKeys = new Localization(
  ["devtools/startup/key-shortcuts.ftl"],
  true
);

var Tools = {};
exports.Tools = Tools;

// Definitions
Tools.options = {
  id: "options",
  ordinal: 0,
  url: "chrome://devtools/content/framework/toolbox-options.html",
  icon: "chrome://devtools/skin/images/settings.svg",
  bgTheme: "theme-body",
  label: l10n("options.label"),
  iconOnly: true,
  panelLabel: l10n("options.panelLabel"),
  tooltip: l10n("optionsButton.tooltip"),
  inMenu: false,

  isTargetSupported: function() {
    return true;
  },

  build: function(iframeWindow, toolbox, commands) {
    return new OptionsPanel(iframeWindow, toolbox, commands);
  },
};

Tools.inspector = {
  id: "inspector",
  accesskey: l10n("inspector.accesskey"),
  ordinal: 1,
  icon: "chrome://devtools/skin/images/tool-inspector.svg",
  url: "chrome://devtools/content/inspector/index.xhtml",
  label: l10n("inspector.label"),
  panelLabel: l10n("inspector.panelLabel"),
  get tooltip() {
    const key = commandkey("devtools-commandkey-inspector");
    if (osString == "Darwin") {
      const cmdShiftC = "Cmd+Shift+" + key;
      const cmdOptC = "Cmd+Opt+" + key;
      return l10n("inspector.mac.tooltip", cmdShiftC, cmdOptC);
    }

    const ctrlShiftC = "Ctrl+Shift+" + key;
    return l10n("inspector.tooltip2", ctrlShiftC);
  },
  inMenu: false,

  preventClosingOnKey: true,
  // preventRaisingOnKey is used to keep the focus on the content window for shortcuts
  // that trigger the element picker.
  preventRaisingOnKey: true,
  onkey: function(panel, toolbox) {
    toolbox.nodePicker.togglePicker();
  },

  isTargetSupported: function(target) {
    return target.hasActor("inspector");
  },

  build: function(iframeWindow, toolbox, commands) {
    return new InspectorPanel(iframeWindow, toolbox, commands);
  },
};
Tools.webConsole = {
  id: "webconsole",
  accesskey: l10n("webConsoleCmd.accesskey"),
  ordinal: 2,
  url: "chrome://devtools/content/webconsole/index.html",
  icon: "chrome://devtools/skin/images/tool-webconsole.svg",
  label: l10n("ToolboxTabWebconsole.label"),
  menuLabel: l10n("MenuWebconsole.label"),
  panelLabel: l10n("ToolboxWebConsole.panelLabel"),
  get tooltip() {
    return l10n(
      "ToolboxWebconsole.tooltip2",
      (osString == "Darwin" ? "Cmd+Opt+" : "Ctrl+Shift+") +
        commandkey("devtools-commandkey-webconsole")
    );
  },
  inMenu: false,

  preventClosingOnKey: true,
  onkey: function(panel, toolbox) {
    if (toolbox.splitConsole) {
      return toolbox.focusConsoleInput();
    }

    panel.focusInput();
    return undefined;
  },

  isTargetSupported: function() {
    return true;
  },
  build: function(iframeWindow, toolbox, commands) {
    return new WebConsolePanel(iframeWindow, toolbox, commands);
  },
};

Tools.jsdebugger = {
  id: "jsdebugger",
  accesskey: l10n("debuggerMenu.accesskey"),
  ordinal: 3,
  icon: "chrome://devtools/skin/images/tool-debugger.svg",
  url: "chrome://devtools/content/debugger/index.html",
  label: l10n("ToolboxDebugger.label"),
  panelLabel: l10n("ToolboxDebugger.panelLabel"),
  get tooltip() {
    return l10n(
      "ToolboxDebugger.tooltip4",
      (osString == "Darwin" ? "Cmd+Opt+" : "Ctrl+Shift+") +
        commandkey("devtools-commandkey-jsdebugger")
    );
  },
  inMenu: false,
  isTargetSupported: function() {
    return true;
  },
  build: function(iframeWindow, toolbox, commands) {
    return new DebuggerPanel(iframeWindow, toolbox, commands);
  },
};

Tools.styleEditor = {
  id: "styleeditor",
  ordinal: 5,
  visibilityswitch: "devtools.styleeditor.enabled",
  accesskey: l10n("open.accesskey"),
  icon: "chrome://devtools/skin/images/tool-styleeditor.svg",
  url: "chrome://devtools/content/styleeditor/index.xhtml",
  label: l10n("ToolboxStyleEditor.label"),
  panelLabel: l10n("ToolboxStyleEditor.panelLabel"),
  get tooltip() {
    return l10n(
      "ToolboxStyleEditor.tooltip3",
      "Shift+" + functionkey(commandkey("devtools-commandkey-styleeditor"))
    );
  },
  inMenu: false,
  isTargetSupported: function(target) {
    return target.hasActor("styleSheets");
  },

  build: function(iframeWindow, toolbox, commands) {
    return new StyleEditorPanel(iframeWindow, toolbox, commands);
  },
};

Tools.performance = {
  id: "performance",
  ordinal: 6,
  icon: "chrome://devtools/skin/images/tool-profiler.svg",
  visibilityswitch: "devtools.performance.enabled",
  label: l10n("performance.label"),
  panelLabel: l10n("performance.panelLabel"),
  get tooltip() {
    return l10n(
      "performance.tooltip",
      "Shift+" + functionkey(commandkey("devtools-commandkey-performance"))
    );
  },
  accesskey: l10n("performance.accesskey"),
  inMenu: false,
};

function switchPerformancePanel() {
  if (
    Services.prefs.getBoolPref("devtools.performance.new-panel-enabled", false)
  ) {
    Tools.performance.url =
      "chrome://devtools/content/performance-new/index.xhtml";
    Tools.performance.build = function(frame, toolbox, commands) {
      return new NewPerformancePanel(frame, toolbox, commands);
    };
    Tools.performance.isTargetSupported = function(target) {
      // Only use the new performance panel on local tab toolboxes, as they are guaranteed
      // to have a performance actor.
      // Remote tab toolboxes (eg about:devtools-toolbox from about:debugging) should not
      // use the performance panel; about:debugging provides a "Profile performance" button
      // which can be used instead, without having the overhead of starting a remote toolbox.
      return target.isLocalTab;
    };
  } else {
    Tools.performance.url = "chrome://devtools/content/performance/index.xhtml";
    Tools.performance.build = function(frame, toolbox, commands) {
      return new PerformancePanel(frame, toolbox, commands);
    };
    Tools.performance.isTargetSupported = function(target) {
      return target.hasActor("performance");
    };
  }
}
switchPerformancePanel();

const prefObserver = { observe: switchPerformancePanel };
Services.prefs.addObserver(
  "devtools.performance.new-panel-enabled",
  prefObserver
);
const unloadObserver = function(subject) {
  if (subject.wrappedJSObject == require("@loader/unload")) {
    Services.prefs.removeObserver(
      "devtools.performance.new-panel-enabled",
      prefObserver
    );
    Services.obs.removeObserver(unloadObserver, "devtools:loader:destroy");
  }
};
Services.obs.addObserver(unloadObserver, "devtools:loader:destroy");

Tools.memory = {
  id: "memory",
  ordinal: 7,
  icon: "chrome://devtools/skin/images/tool-memory.svg",
  url: "chrome://devtools/content/memory/index.xhtml",
  visibilityswitch: "devtools.memory.enabled",
  label: l10n("memory.label"),
  panelLabel: l10n("memory.panelLabel"),
  tooltip: l10n("memory.tooltip"),

  isTargetSupported: function(target) {
    return !target.isAddon && !target.isWorkerTarget;
  },

  build: function(frame, toolbox, commands) {
    return new MemoryPanel(frame, toolbox, commands);
  },
};

Tools.netMonitor = {
  id: "netmonitor",
  accesskey: l10n("netmonitor.accesskey"),
  ordinal: 4,
  visibilityswitch: "devtools.netmonitor.enabled",
  icon: "chrome://devtools/skin/images/tool-network.svg",
  url: "chrome://devtools/content/netmonitor/index.html",
  label: l10n("netmonitor.label"),
  panelLabel: l10n("netmonitor.panelLabel"),
  get tooltip() {
    return l10n(
      "netmonitor.tooltip2",
      (osString == "Darwin" ? "Cmd+Opt+" : "Ctrl+Shift+") +
        commandkey("devtools-commandkey-netmonitor")
    );
  },
  inMenu: false,

  isTargetSupported: function(target) {
    return target.getTrait("networkMonitor") && !target.isWorkerTarget;
  },

  build: function(iframeWindow, toolbox, commands) {
    return new NetMonitorPanel(iframeWindow, toolbox, commands);
  },
};

Tools.storage = {
  id: "storage",
  ordinal: 8,
  accesskey: l10n("storage.accesskey"),
  visibilityswitch: "devtools.storage.enabled",
  icon: "chrome://devtools/skin/images/tool-storage.svg",
  url: "chrome://devtools/content/storage/index.xhtml",
  label: l10n("storage.label"),
  menuLabel: l10n("storage.menuLabel"),
  panelLabel: l10n("storage.panelLabel"),
  get tooltip() {
    return l10n(
      "storage.tooltip3",
      "Shift+" + functionkey(commandkey("devtools-commandkey-storage"))
    );
  },
  inMenu: false,

  isTargetSupported: function(target) {
    return target.hasActor("storage");
  },

  build: function(iframeWindow, toolbox, commands) {
    return new StoragePanel(iframeWindow, toolbox, commands);
  },
};

Tools.dom = {
  id: "dom",
  accesskey: l10n("dom.accesskey"),
  ordinal: 11,
  visibilityswitch: "devtools.dom.enabled",
  icon: "chrome://devtools/skin/images/tool-dom.svg",
  url: "chrome://devtools/content/dom/index.html",
  label: l10n("dom.label"),
  panelLabel: l10n("dom.panelLabel"),
  get tooltip() {
    return l10n(
      "dom.tooltip",
      (osString == "Darwin" ? "Cmd+Opt+" : "Ctrl+Shift+") +
        commandkey("devtools-commandkey-dom")
    );
  },
  inMenu: false,

  isTargetSupported: function(target) {
    return true;
  },

  build: function(iframeWindow, toolbox, commands) {
    return new DomPanel(iframeWindow, toolbox, commands);
  },
};

Tools.accessibility = {
  id: "accessibility",
  accesskey: l10n("accessibility.accesskey"),
  ordinal: 9,
  modifiers: osString == "Darwin" ? "accel,alt" : "accel,shift",
  visibilityswitch: "devtools.accessibility.enabled",
  icon: "chrome://devtools/skin/images/tool-accessibility.svg",
  url: "chrome://devtools/content/accessibility/index.html",
  label: l10n("accessibility.label"),
  panelLabel: l10n("accessibility.panelLabel"),
  get tooltip() {
    return l10n(
      "accessibility.tooltip3",
      "Shift+" +
        functionkey(commandkey("devtools-commandkey-accessibility-f12"))
    );
  },
  inMenu: false,

  isTargetSupported(target) {
    return target.hasActor("accessibility");
  },

  build(iframeWindow, toolbox, commands) {
    return new AccessibilityPanel(iframeWindow, toolbox, commands);
  },
};

Tools.application = {
  id: "application",
  ordinal: 10,
  visibilityswitch: "devtools.application.enabled",
  icon: "chrome://devtools/skin/images/tool-application.svg",
  url: "chrome://devtools/content/application/index.html",
  label: l10n("application.label"),
  panelLabel: l10n("application.panelLabel"),
  tooltip: l10n("application.tooltip"),
  inMenu: false,

  isTargetSupported: function(target) {
    return target.hasActor("manifest");
  },

  build: function(iframeWindow, toolbox, commands) {
    return new ApplicationPanel(iframeWindow, toolbox, commands);
  },
};

var defaultTools = [
  Tools.options,
  Tools.webConsole,
  Tools.inspector,
  Tools.jsdebugger,
  Tools.styleEditor,
  Tools.performance,
  Tools.netMonitor,
  Tools.storage,
  Tools.memory,
  Tools.dom,
  Tools.accessibility,
  Tools.application,
];

exports.defaultTools = defaultTools;

Tools.darkTheme = {
  id: "dark",
  label: l10n("options.darkTheme.label2"),
  ordinal: 1,
  stylesheets: ["chrome://devtools/skin/dark-theme.css"],
  classList: ["theme-dark"],
};

Tools.lightTheme = {
  id: "light",
  label: l10n("options.lightTheme.label2"),
  ordinal: 2,
  stylesheets: ["chrome://devtools/skin/light-theme.css"],
  classList: ["theme-light"],
};

exports.defaultThemes = [Tools.darkTheme, Tools.lightTheme];

// List buttons that can be toggled to prevent adding prefs for
// addons that have manually inserted toolbarbuttons into DOM.
// (By default, supported target is only local tab)
exports.ToolboxButtons = [
  {
    id: "command-button-experimental-prefs",
    description: "DevTools Experimental preferences",
    isTargetSupported: target => !AppConstants.MOZILLA_OFFICIAL,
    onClick: (event, toolbox) => DevToolsExperimentalPrefs.showTooltip(toolbox),
    isChecked: () => DevToolsExperimentalPrefs.isAnyPreferenceEnabled(),
  },
  {
    id: "command-button-responsive",
    description: l10n(
      "toolbox.buttons.responsive",
      osString == "Darwin" ? "Cmd+Opt+M" : "Ctrl+Shift+M"
    ),
    isTargetSupported: target => target.isLocalTab,
    onClick(event, toolbox) {
      const { localTab } = toolbox.descriptorFront;
      const browserWindow = localTab.ownerDocument.defaultView;
      ResponsiveUIManager.toggle(browserWindow, localTab, {
        trigger: "toolbox",
      });
    },
    isChecked(toolbox) {
      const { localTab } = toolbox.descriptorFront;
      if (!localTab) {
        return false;
      }
      return ResponsiveUIManager.isActiveForTab(localTab);
    },
    setup(toolbox, onChange) {
      ResponsiveUIManager.on("on", onChange);
      ResponsiveUIManager.on("off", onChange);
    },
    teardown(toolbox, onChange) {
      ResponsiveUIManager.off("on", onChange);
      ResponsiveUIManager.off("off", onChange);
    },
  },
  {
    id: "command-button-screenshot",
    description: l10n("toolbox.buttons.screenshot"),
    isTargetSupported: targetFront => {
      return (
        // @backward-compat { version 87 } We need to check for the screenshot actor as well
        // when connecting to older server that does not have the screenshotContentActor
        targetFront.hasActor("screenshotContent") ||
        targetFront.hasActor("screenshot")
      );
    },
    async onClick(event, toolbox) {
      // Special case for screenshot button to check for clipboard preference
      const clipboardEnabled = Services.prefs.getBoolPref(
        "devtools.screenshot.clipboard.enabled"
      );

      // When screenshot to clipboard is enabled disabling saving to file
      const args = {
        fullpage: true,
        file: !clipboardEnabled,
        clipboard: clipboardEnabled,
      };

      const messages = await captureAndSaveScreenshot(
        toolbox.target,
        toolbox.win,
        args
      );
      const notificationBox = toolbox.getNotificationBox();
      const priorityMap = {
        error: notificationBox.PRIORITY_CRITICAL_HIGH,
        warn: notificationBox.PRIORITY_WARNING_HIGH,
      };
      for (const { text, level } of messages) {
        // captureAndSaveScreenshot returns "saved" messages, that indicate where the
        // screenshot was saved. In regular toolbox, we don't want to display them as
        // the download UI can be used to open them.
        // But in the browser toolbox, we can't see the download UI, so we'll display the
        // saved message so the user knows there the file was saved.
        if (
          !toolbox.isBrowserToolbox &&
          level !== "warn" &&
          level !== "error"
        ) {
          continue;
        }
        notificationBox.appendNotification(
          text,
          null,
          null,
          priorityMap[level] || notificationBox.PRIORITY_INFO_MEDIUM
        );
      }
    },
  },
  createHighlightButton("RulersHighlighter", "rulers"),
  createHighlightButton("MeasuringToolHighlighter", "measure"),
];

function createHighlightButton(highlighterName, id) {
  return {
    id: `command-button-${id}`,
    description: l10n(`toolbox.buttons.${id}`),
    isTargetSupported: target => !target.chrome,
    async onClick(event, toolbox) {
      const inspectorFront = await toolbox.target.getFront("inspector");
      const highlighter = await inspectorFront.getOrCreateHighlighterByType(
        highlighterName
      );
      if (highlighter.isShown()) {
        return highlighter.hide();
      }

      return highlighter.show();
    },
    isChecked(toolbox) {
      // if the inspector doesn't exist, then the highlighter has not yet been connected
      // to the front end.
      const inspectorFront = toolbox.target.getCachedFront("inspector");
      if (!inspectorFront) {
        // initialize the inspector front asyncronously. There is a potential for buggy
        // behavior here, but we need to change how the buttons get data (have them
        // consume data from reducers rather than writing our own version) in order to
        // fix this properly.
        return false;
      }
      const highlighter = inspectorFront.getKnownHighlighter(highlighterName);
      return highlighter && highlighter.isShown();
    },
  };
}

/**
 * Lookup l10n string from a string bundle.
 *
 * @param {string} name
 *        The key to lookup.
 * @param {...string} args
 *        Optional format argument.
 * @returns A localized version of the given key.
 */
function l10n(name, ...args) {
  try {
    return args ? L10N.getFormatStr(name, ...args) : L10N.getStr(name);
  } catch (ex) {
    console.log("Error reading '" + name + "'");
    throw new Error("l10n error with " + name);
  }
}

function commandkey(name) {
  try {
    return CommandKeys.formatValueSync(name);
  } catch (ex) {
    console.log("Error reading '" + name + "'");
    throw new Error("l10n error with " + name);
  }
}

function functionkey(shortkey) {
  return shortkey.split("_")[1];
}
