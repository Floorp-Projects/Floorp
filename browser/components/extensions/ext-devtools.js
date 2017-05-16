/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/* global getTargetTabIdForToolbox */

/**
 * This module provides helpers used by the other specialized `ext-devtools-*.js` modules
 * and the implementation of the `devtools_page`.
 */

XPCOMUtils.defineLazyModuleGetter(this, "gDevTools",
                                  "resource://devtools/client/framework/gDevTools.jsm");

Cu.import("resource://gre/modules/ExtensionParent.jsm");

var {
  HiddenExtensionPage,
  watchExtensionProxyContextLoad,
} = ExtensionParent;

// Map[extension -> DevToolsPageDefinition]
let devtoolsPageDefinitionMap = new Map();

let initDevTools;

/**
 * Retrieve the devtools target for the devtools extension proxy context
 * (lazily cloned from the target of the toolbox associated to the context
 * the first time that it is accessed).
 *
 * @param {DevToolsExtensionPageContextParent} context
 *   A devtools extension proxy context.
 *
 * @returns {Promise<TabTarget>}
 *   The cloned devtools target associated to the context.
 */
global.getDevToolsTargetForContext = async (context) => {
  if (context.devToolsTarget) {
    await context.devToolsTarget.makeRemote();
    return context.devToolsTarget;
  }

  if (!context.devToolsToolbox || !context.devToolsToolbox.target) {
    throw new Error("Unable to get a TabTarget for a context not associated to any toolbox");
  }

  if (!context.devToolsToolbox.target.isLocalTab) {
    throw new Error("Unexpected target type: only local tabs are currently supported.");
  }

  const {TabTarget} = require("devtools/client/framework/target");

  context.devToolsTarget = new TabTarget(context.devToolsToolbox.target.tab);
  await context.devToolsTarget.makeRemote();

  return context.devToolsTarget;
};

/**
 * Retrieve the devtools target for the devtools extension proxy context
 * (lazily cloned from the target of the toolbox associated to the context
 * the first time that it is accessed).
 *
 * @param {Toolbox} toolbox
 *   A devtools toolbox instance.
 *
 * @returns {number}
 *   The corresponding WebExtensions tabId.
 */
global.getTargetTabIdForToolbox = (toolbox) => {
  let {target} = toolbox;

  if (!target.isLocalTab) {
    throw new Error("Unexpected target type: only local tabs are currently supported.");
  }

  let parentWindow = target.tab.linkedBrowser.ownerGlobal;
  let tab = parentWindow.gBrowser.getTabForBrowser(target.tab.linkedBrowser);

  return tabTracker.getId(tab);
};

/**
 * The DevToolsPage represents the "devtools_page" related to a particular
 * Toolbox and WebExtension.
 *
 * The devtools_page contexts are invisible WebExtensions contexts, similar to the
 * background page, associated to a single developer toolbox (e.g. If an add-on
 * registers a devtools_page and the user opens 3 developer toolbox in 3 webpages,
 * 3 devtools_page contexts will be created for that add-on).
 *
 * @param {Extension}              extension
 *   The extension that owns the devtools_page.
 * @param {Object}                 options
 * @param {Toolbox}                options.toolbox
 *   The developer toolbox instance related to this devtools_page.
 * @param {string}                 options.url
 *   The path to the devtools page html page relative to the extension base URL.
 * @param {DevToolsPageDefinition} options.devToolsPageDefinition
 *   The instance of the devToolsPageDefinition class related to this DevToolsPage.
 */
class DevToolsPage extends HiddenExtensionPage {
  constructor(extension, options) {
    super(extension, "devtools_page");

    this.url = extension.baseURI.resolve(options.url);
    this.toolbox = options.toolbox;
    this.devToolsPageDefinition = options.devToolsPageDefinition;

    this.unwatchExtensionProxyContextLoad = null;

    this.waitForTopLevelContext = new Promise(resolve => {
      this.resolveTopLevelContext = resolve;
    });
  }

  async build() {
    await this.createBrowserElement();

    // Listening to new proxy contexts.
    this.unwatchExtensionProxyContextLoad = watchExtensionProxyContextLoad(this, context => {
      // Keep track of the toolbox and target associated to the context, which is
      // needed by the API methods implementation.
      context.devToolsToolbox = this.toolbox;

      if (!this.topLevelContext) {
        this.topLevelContext = context;

        // Ensure this devtools page is destroyed, when the top level context proxy is
        // closed.
        this.topLevelContext.callOnClose(this);

        this.resolveTopLevelContext(context);
      }
    });

    extensions.emit("extension-browser-inserted", this.browser, {
      devtoolsToolboxInfo: {
        inspectedWindowTabId: getTargetTabIdForToolbox(this.toolbox),
      },
    });

    this.browser.loadURI(this.url);

    await this.waitForTopLevelContext;
  }

  close() {
    if (this.closed) {
      throw new Error("Unable to shutdown a closed DevToolsPage instance");
    }

    this.closed = true;

    // Unregister the devtools page instance from the devtools page definition.
    this.devToolsPageDefinition.forgetForTarget(this.toolbox.target);

    // Unregister it from the resources to cleanup when the context has been closed.
    if (this.topLevelContext) {
      this.topLevelContext.forgetOnClose(this);
    }

    // Stop watching for any new proxy contexts from the devtools page.
    if (this.unwatchExtensionProxyContextLoad) {
      this.unwatchExtensionProxyContextLoad();
      this.unwatchExtensionProxyContextLoad = null;
    }

    super.shutdown();
  }
}

/**
 * The DevToolsPageDefinitions class represents the "devtools_page" manifest property
 * of a WebExtension.
 *
 * A DevToolsPageDefinition instance is created automatically when a WebExtension
 * which contains the "devtools_page" manifest property has been loaded, and it is
 * automatically destroyed when the related WebExtension has been unloaded,
 * and so there will be at most one DevtoolsPageDefinition per add-on.
 *
 * Every time a developer tools toolbox is opened, the DevToolsPageDefinition creates
 * and keep track of a DevToolsPage instance (which represents the actual devtools_page
 * instance related to that particular toolbox).
 *
 * @param {Extension} extension
 *   The extension that owns the devtools_page.
 * @param {string}    url
 *   The path to the devtools page html page relative to the extension base URL.
 */
class DevToolsPageDefinition {
  constructor(extension, url) {
    initDevTools();

    this.url = url;
    this.extension = extension;

    // Map[TabTarget -> DevToolsPage]
    this.devtoolsPageForTarget = new Map();
  }

  buildForToolbox(toolbox) {
    if (this.devtoolsPageForTarget.has(toolbox.target)) {
      return Promise.reject(new Error("DevtoolsPage has been already created for this toolbox"));
    }

    const devtoolsPage = new DevToolsPage(this.extension, {
      toolbox, url: this.url, devToolsPageDefinition: this,
    });
    this.devtoolsPageForTarget.set(toolbox.target, devtoolsPage);

    return devtoolsPage.build();
  }

  shutdownForTarget(target) {
    if (this.devtoolsPageForTarget.has(target)) {
      const devtoolsPage = this.devtoolsPageForTarget.get(target);
      devtoolsPage.close();

      // `devtoolsPage.close()` should remove the instance from the map,
      // raise an exception if it is still there.
      if (this.devtoolsPageForTarget.has(target)) {
        throw new Error(`Leaked DevToolsPage instance for target "${target.toString()}"`);
      }
    }
  }

  forgetForTarget(target) {
    this.devtoolsPageForTarget.delete(target);
  }

  shutdown() {
    for (let target of this.devtoolsPageForTarget.keys()) {
      this.shutdownForTarget(target);
    }

    if (this.devtoolsPageForTarget.size > 0) {
      throw new Error(
        `Leaked ${this.devtoolsPageForTarget.size} DevToolsPage instances in devtoolsPageForTarget Map`
      );
    }
  }
}


let devToolsInitialized = false;
initDevTools = function() {
  if (devToolsInitialized) {
    return;
  }

  /* eslint-disable mozilla/balanced-listeners */
  // Create a devtools page context for a new opened toolbox,
  // based on the registered devtools_page definitions.
  gDevTools.on("toolbox-created", (evt, toolbox) => {
    if (!toolbox.target.isLocalTab) {
      // Only local tabs are currently supported (See Bug 1304378 for additional details
      // related to remote targets support).
      let msg = `Ignoring DevTools Toolbox for target "${toolbox.target.toString()}": ` +
                `"${toolbox.target.name}" ("${toolbox.target.url}"). ` +
                "Only local tab are currently supported by the WebExtensions DevTools API.";
      let scriptError = Cc["@mozilla.org/scripterror;1"].createInstance(Ci.nsIScriptError);
      scriptError.init(msg, null, null, null, null, Ci.nsIScriptError.warningFlag, "content javascript");
      let consoleService = Cc["@mozilla.org/consoleservice;1"].getService(Ci.nsIConsoleService);
      consoleService.logMessage(scriptError);

      return;
    }

    for (let devtoolsPage of devtoolsPageDefinitionMap.values()) {
      devtoolsPage.buildForToolbox(toolbox);
    }
  });

  // Destroy a devtools page context for a destroyed toolbox,
  // based on the registered devtools_page definitions.
  gDevTools.on("toolbox-destroy", (evt, target) => {
    if (!target.isLocalTab) {
      // Only local tabs are currently supported (See Bug 1304378 for additional details
      // related to remote targets support).
      return;
    }

    for (let devtoolsPageDefinition of devtoolsPageDefinitionMap.values()) {
      devtoolsPageDefinition.shutdownForTarget(target);
    }
  });
  /* eslint-enable mozilla/balanced-listeners */

  devToolsInitialized = true;
};

this.devtools = class extends ExtensionAPI {
  onManifestEntry(entryName) {
    let {extension} = this;
    let {manifest} = extension;

    // Create and register a new devtools_page definition as specified in the
    // "devtools_page" property in the extension manifest.
    let devtoolsPageDefinition = new DevToolsPageDefinition(extension, manifest.devtools_page);
    devtoolsPageDefinitionMap.set(extension, devtoolsPageDefinition);
  }

  onShutdown(reason) {
    let {extension} = this;

    // Destroy the registered devtools_page definition on extension shutdown.
    if (devtoolsPageDefinitionMap.has(extension)) {
      devtoolsPageDefinitionMap.get(extension).shutdown();
      devtoolsPageDefinitionMap.delete(extension);
    }
  }

  getAPI(context) {
    return {
      devtools: {},
    };
  }
};
