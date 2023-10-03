/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const osString = Services.appinfo.OS;

// Panels
loader.lazyGetter(
  this,
  "OptionsPanel",
  () =>
    require("resource://devtools/client/framework/toolbox-options.js")
      .OptionsPanel
);
loader.lazyGetter(
  this,
  "InspectorPanel",
  () => require("resource://devtools/client/inspector/panel.js").InspectorPanel
);
loader.lazyGetter(
  this,
  "WebConsolePanel",
  () =>
    require("resource://devtools/client/webconsole/panel.js").WebConsolePanel
);
loader.lazyGetter(
  this,
  "DebuggerPanel",
  () => require("resource://devtools/client/debugger/panel.js").DebuggerPanel
);
loader.lazyGetter(
  this,
  "StyleEditorPanel",
  () =>
    require("resource://devtools/client/styleeditor/panel.js").StyleEditorPanel
);
loader.lazyGetter(
  this,
  "MemoryPanel",
  () => require("resource://devtools/client/memory/panel.js").MemoryPanel
);
loader.lazyGetter(
  this,
  "NewPerformancePanel",
  () =>
    require("resource://devtools/client/performance-new/panel/panel.js")
      .PerformancePanel
);
loader.lazyGetter(
  this,
  "NetMonitorPanel",
  () =>
    require("resource://devtools/client/netmonitor/panel.js").NetMonitorPanel
);
loader.lazyGetter(
  this,
  "StoragePanel",
  () => require("resource://devtools/client/storage/panel.js").StoragePanel
);
loader.lazyGetter(
  this,
  "DomPanel",
  () => require("resource://devtools/client/dom/panel.js").DomPanel
);
loader.lazyGetter(
  this,
  "AccessibilityPanel",
  () =>
    require("resource://devtools/client/accessibility/panel.js")
      .AccessibilityPanel
);
loader.lazyGetter(
  this,
  "ApplicationPanel",
  () =>
    require("resource://devtools/client/application/panel.js").ApplicationPanel
);

// Other dependencies
loader.lazyRequireGetter(
  this,
  "ResponsiveUIManager",
  "resource://devtools/client/responsive/manager.js"
);

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  AppConstants: "resource://gre/modules/AppConstants.sys.mjs",
});
loader.lazyRequireGetter(
  this,
  "DevToolsExperimentalPrefs",
  "resource://devtools/client/devtools-experimental-prefs.js"
);
loader.lazyRequireGetter(
  this,
  "captureAndSaveScreenshot",
  "resource://devtools/client/shared/screenshot.js",
  true
);

const { LocalizationHelper } = require("resource://devtools/shared/l10n.js");
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

  isToolSupported() {
    return true;
  },

  build(iframeWindow, toolbox, commands) {
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
  onkey(panel, toolbox) {
    toolbox.nodePicker.togglePicker();
  },

  isToolSupported(toolbox) {
    return toolbox.target.hasActor("inspector");
  },

  build(iframeWindow, toolbox, commands) {
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
  onkey(panel, toolbox) {
    if (toolbox.splitConsole) {
      return toolbox.focusConsoleInput();
    }

    panel.focusInput();
    return undefined;
  },

  isToolSupported() {
    return true;
  },
  build(iframeWindow, toolbox, commands) {
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
  isToolSupported() {
    return true;
  },
  build(iframeWindow, toolbox, commands) {
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
  isToolSupported(toolbox) {
    return toolbox.target.hasActor("styleSheets");
  },

  build(iframeWindow, toolbox, commands) {
    return new StyleEditorPanel(iframeWindow, toolbox, commands);
  },
};

Tools.performance = {
  id: "performance",
  ordinal: 6,
  icon: "chrome://devtools/skin/images/tool-profiler.svg",
  url: "chrome://devtools/content/performance-new/panel/index.xhtml",
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
  isToolSupported(toolbox) {
    // Only use the new performance panel on local tab toolboxes, as they are guaranteed
    // to have a performance actor.
    // Remote tab toolboxes (eg about:devtools-toolbox from about:debugging) should not
    // use the performance panel; about:debugging provides a "Profile performance" button
    // which can be used instead, without having the overhead of starting a remote toolbox.
    // Also accept the Browser Toolbox, so that we can profile its process via a second browser toolbox.
    return (
      toolbox.commands.descriptorFront.isLocalTab || toolbox.isBrowserToolbox
    );
  },
  build(frame, toolbox, commands) {
    return new NewPerformancePanel(frame, toolbox, commands);
  },
};

Tools.memory = {
  id: "memory",
  ordinal: 7,
  icon: "chrome://devtools/skin/images/tool-memory.svg",
  url: "chrome://devtools/content/memory/index.xhtml",
  visibilityswitch: "devtools.memory.enabled",
  label: l10n("memory.label"),
  panelLabel: l10n("memory.panelLabel"),
  tooltip: l10n("memory.tooltip"),

  isToolSupported(toolbox) {
    const { descriptorFront } = toolbox.commands;
    return (
      !descriptorFront.isWebExtensionDescriptor &&
      !descriptorFront.isWorkerDescriptor
    );
  },

  build(frame, toolbox, commands) {
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

  isToolSupported(toolbox) {
    return (
      toolbox.target.getTrait("networkMonitor") &&
      !toolbox.target.isWorkerTarget
    );
  },

  build(iframeWindow, toolbox, commands) {
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

  isToolSupported(toolbox) {
    const { descriptorFront } = toolbox.commands;
    // Storage is available on all contexts debugging a BrowsingContext.
    // As of today, this is all but worker toolboxes.
    return (
      descriptorFront.isTabDescriptor ||
      descriptorFront.isParentProcessDescriptor ||
      descriptorFront.isWebExtensionDescriptor
    );
  },

  build(iframeWindow, toolbox, commands) {
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

  isToolSupported() {
    return true;
  },

  build(iframeWindow, toolbox, commands) {
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

  isToolSupported(toolbox) {
    return toolbox.target.hasActor("accessibility");
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

  isToolSupported(toolbox) {
    return toolbox.target.hasActor("manifest");
  },

  build(iframeWindow, toolbox, commands) {
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
    isToolSupported: () => !lazy.AppConstants.MOZILLA_OFFICIAL,
    onClick: (event, toolbox) => DevToolsExperimentalPrefs.showTooltip(toolbox),
    isChecked: () => DevToolsExperimentalPrefs.isAnyPreferenceEnabled(),
  },
  {
    id: "command-button-responsive",
    description: l10n(
      "toolbox.buttons.responsive",
      osString == "Darwin" ? "Cmd+Opt+M" : "Ctrl+Shift+M"
    ),
    isToolSupported: toolbox => toolbox.commands.descriptorFront.isLocalTab,
    onClick(event, toolbox) {
      const { localTab } = toolbox.commands.descriptorFront;
      const browserWindow = localTab.ownerDocument.defaultView;
      ResponsiveUIManager.toggle(browserWindow, localTab, {
        trigger: "toolbox",
      });
    },
    isChecked(toolbox) {
      const { localTab } = toolbox.commands.descriptorFront;
      if (!localTab) {
        return false;
      }
      return ResponsiveUIManager.isActiveForTab(localTab);
    },
    isToggle: true,
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
    isToolSupported: toolbox => {
      return (
        // @backward-compat { version 87 } We need to check for the screenshot actor as well
        // when connecting to older server that does not have the screenshotContentActor
        toolbox.target.hasActor("screenshotContent") ||
        toolbox.target.hasActor("screenshot")
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
  createHighlightButton(
    ["RulersHighlighter", "ViewportSizeHighlighter"],
    "rulers"
  ),
  createHighlightButton(["MeasuringToolHighlighter"], "measure"),
];

function createHighlightButton(highlighters, id) {
  return {
    id: `command-button-${id}`,
    description: l10n(`toolbox.buttons.${id}`),
    isToolSupported: toolbox =>
      toolbox.commands.descriptorFront.isTabDescriptor,
    async onClick(event, toolbox) {
      const inspectorFront = await toolbox.target.getFront("inspector");

      await Promise.all(
        highlighters.map(async name => {
          const highlighter = await inspectorFront.getOrCreateHighlighterByType(
            name
          );

          if (highlighter.isShown()) {
            await highlighter.hide();
          } else {
            await highlighter.show();
          }
        })
      );
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

      return highlighters.every(name =>
        inspectorFront.getKnownHighlighter(name)?.isShown()
      );
    },
    isToggle: true,
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
