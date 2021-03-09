/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module provides helpers used by the other specialized `ext-devtools-*.js` modules
 * and the implementation of the `devtools_page`.
 */

ChromeUtils.defineModuleGetter(
  this,
  "DevToolsShim",
  "chrome://devtools-startup/content/DevToolsShim.jsm"
);

var { ExtensionParent } = ChromeUtils.import(
  "resource://gre/modules/ExtensionParent.jsm"
);

var { HiddenExtensionPage, watchExtensionProxyContextLoad } = ExtensionParent;

// Get the devtools preference given the extension id.
function getDevToolsPrefBranchName(extensionId) {
  return `devtools.webextensions.${extensionId}`;
}

/**
 * Retrieve the tabId for the given devtools toolbox.
 *
 * @param {Toolbox} toolbox
 *   A devtools toolbox instance.
 *
 * @returns {number}
 *   The corresponding WebExtensions tabId.
 */
global.getTargetTabIdForToolbox = toolbox => {
  let { descriptorFront } = toolbox;

  if (!descriptorFront.isLocalTab) {
    throw new Error(
      "Unexpected target type: only local tabs are currently supported."
    );
  }

  let parentWindow = descriptorFront.localTab.linkedBrowser.ownerGlobal;
  let tab = parentWindow.gBrowser.getTabForBrowser(
    descriptorFront.localTab.linkedBrowser
  );

  return tabTracker.getId(tab);
};

// Create an InspectedWindowFront instance for a given context (used in devtoools.inspectedWindow.eval
// and in sidebar.setExpression API methods).
global.getInspectedWindowFront = async function(context) {
  const target = await context.getCurrentDevToolsTarget();
  return DevToolsShim.createWebExtensionInspectedWindowFront(target);
};

// Get the WebExtensionInspectedWindowActor eval options (needed to provide the $0 and inspect
// binding provided to the evaluated js code).
global.getToolboxEvalOptions = async function(context) {
  const options = {};
  const toolbox = context.devToolsToolbox;
  const selectedNode = toolbox.selection;

  if (selectedNode && selectedNode.nodeFront) {
    // If there is a selected node in the inspector, we hand over
    // its actor id to the eval request in order to provide the "$0" binding.
    options.toolboxSelectedNodeActorID = selectedNode.nodeFront.actorID;
  }

  // Provide the console actor ID to implement the "inspect" binding.
  const consoleFront = await toolbox.target.getFront("console");
  options.toolboxConsoleActorID = consoleFront.actor;

  return options;
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
    this.unwatchExtensionProxyContextLoad = watchExtensionProxyContextLoad(
      this,
      context => {
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
      }
    );

    extensions.emit("extension-browser-inserted", this.browser, {
      devtoolsToolboxInfo: {
        inspectedWindowTabId: getTargetTabIdForToolbox(this.toolbox),
        themeName: DevToolsShim.getTheme(),
      },
    });

    this.browser.loadURI(this.url, {
      triggeringPrincipal: this.extension.principal,
    });

    await this.waitForTopLevelContext;
  }

  close() {
    if (this.closed) {
      throw new Error("Unable to shutdown a closed DevToolsPage instance");
    }

    this.closed = true;

    // Unregister the devtools page instance from the devtools page definition.
    this.devToolsPageDefinition.forgetForToolbox(this.toolbox);

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
    this.url = url;
    this.extension = extension;

    // Map[Toolbox -> DevToolsPage]
    this.devtoolsPageForToolbox = new Map();
  }

  onThemeChanged(themeName) {
    Services.ppmm.broadcastAsyncMessage("Extension:DevToolsThemeChanged", {
      themeName,
    });
  }

  buildForToolbox(toolbox) {
    if (
      !this.extension.canAccessWindow(
        toolbox.descriptorFront.localTab.ownerGlobal
      )
    ) {
      // We should never create a devtools page for a toolbox related to a private browsing window
      // if the extension is not allowed to access it.
      return;
    }

    if (this.devtoolsPageForToolbox.has(toolbox)) {
      return Promise.reject(
        new Error("DevtoolsPage has been already created for this toolbox")
      );
    }

    const devtoolsPage = new DevToolsPage(this.extension, {
      toolbox,
      url: this.url,
      devToolsPageDefinition: this,
    });

    // If this is the first DevToolsPage, subscribe to the theme-changed event
    if (this.devtoolsPageForToolbox.size === 0) {
      DevToolsShim.on("theme-changed", this.onThemeChanged);
    }
    this.devtoolsPageForToolbox.set(toolbox, devtoolsPage);

    return devtoolsPage.build();
  }

  shutdownForToolbox(toolbox) {
    if (this.devtoolsPageForToolbox.has(toolbox)) {
      const devtoolsPage = this.devtoolsPageForToolbox.get(toolbox);
      devtoolsPage.close();

      // `devtoolsPage.close()` should remove the instance from the map,
      // raise an exception if it is still there.
      if (this.devtoolsPageForToolbox.has(toolbox)) {
        throw new Error(
          `Leaked DevToolsPage instance for target "${toolbox.descriptorFront.url}", extension "${this.extension.policy.debugName}"`
        );
      }

      // If this was the last DevToolsPage, unsubscribe from the theme-changed event
      if (this.devtoolsPageForToolbox.size === 0) {
        DevToolsShim.off("theme-changed", this.onThemeChanged);
      }
      this.extension.emit("devtools-page-shutdown", toolbox);
    }
  }

  forgetForToolbox(toolbox) {
    this.devtoolsPageForToolbox.delete(toolbox);
  }

  /**
   * Build the devtools_page instances for all the existing toolboxes
   * (if the toolbox target is supported).
   */
  build() {
    // Iterate over the existing toolboxes and create the devtools page for them
    // (if the toolbox target is supported).
    for (let toolbox of DevToolsShim.getToolboxes()) {
      if (
        !toolbox.descriptorFront.isLocalTab ||
        !this.extension.canAccessWindow(
          toolbox.descriptorFront.localTab.ownerGlobal
        )
      ) {
        // Skip any non-local tab and private browsing windows if the extension
        // is not allowed to access them.
        continue;
      }

      // Ensure that the WebExtension is listed in the toolbox options.
      toolbox.registerWebExtension(this.extension.uuid, {
        name: this.extension.name,
        pref: `${getDevToolsPrefBranchName(this.extension.id)}.enabled`,
      });

      this.buildForToolbox(toolbox);
    }
  }

  /**
   * Shutdown all the devtools_page instances.
   */
  shutdown() {
    for (let toolbox of this.devtoolsPageForToolbox.keys()) {
      this.shutdownForToolbox(toolbox);
    }

    if (this.devtoolsPageForToolbox.size > 0) {
      throw new Error(
        `Leaked ${this.devtoolsPageForToolbox.size} DevToolsPage instances in devtoolsPageForToolbox Map`
      );
    }
  }
}

this.devtools = class extends ExtensionAPI {
  constructor(extension) {
    super(extension);

    this._initialized = false;

    // DevToolsPageDefinition instance (created in onManifestEntry).
    this.pageDefinition = null;

    this.onToolboxReady = this.onToolboxReady.bind(this);
    this.onToolboxDestroy = this.onToolboxDestroy.bind(this);

    /* eslint-disable mozilla/balanced-listeners */
    extension.on("add-permissions", (ignoreEvent, permissions) => {
      Services.prefs.setBoolPref(
        `${getDevToolsPrefBranchName(extension.id)}.enabled`,
        true
      );
      if (permissions.permissions.includes("devtools")) {
        this._initialize();
      }
    });

    extension.on("remove-permissions", (ignoreEvent, permissions) => {
      Services.prefs.setBoolPref(
        `${getDevToolsPrefBranchName(extension.id)}.enabled`,
        false
      );
      if (permissions.permissions.includes("devtools")) {
        this._uninitialize();
      }
    });
  }

  onManifestEntry() {
    this._initialize();
  }

  static onUninstall(extensionId) {
    // Remove the preference branch on uninstall.
    const prefBranch = Services.prefs.getBranch(
      `${getDevToolsPrefBranchName(extensionId)}.`
    );

    prefBranch.deleteBranch("");
  }

  _initialize() {
    const { extension } = this;

    if (!extension.hasPermission("devtools") || this._initialized) {
      return;
    }

    this.initDevToolsPref();

    // Create the devtools_page definition.
    this.pageDefinition = new DevToolsPageDefinition(
      extension,
      extension.manifest.devtools_page
    );

    // Build the extension devtools_page on all existing toolboxes (if the extension
    // devtools_page is not disabled by the related preference).
    if (!this.isDevToolsPageDisabled()) {
      this.pageDefinition.build();
    }

    DevToolsShim.on("toolbox-ready", this.onToolboxReady);
    DevToolsShim.on("toolbox-destroy", this.onToolboxDestroy);
    this._initialized = true;
  }

  _uninitialize() {
    // devtoolsPrefBranch is set in onManifestEntry, and nullified
    // later in onShutdown.  If it isn't set, then onManifestEntry
    // did not initialize devtools for the extension.
    if (!this._initialized) {
      return;
    }

    DevToolsShim.off("toolbox-ready", this.onToolboxReady);
    DevToolsShim.off("toolbox-destroy", this.onToolboxDestroy);

    // Shutdown the extension devtools_page from all existing toolboxes.
    this.pageDefinition.shutdown();
    this.pageDefinition = null;

    // Iterate over the existing toolboxes and unlist the devtools webextension from them.
    for (let toolbox of DevToolsShim.getToolboxes()) {
      toolbox.unregisterWebExtension(this.extension.uuid);
    }

    this.uninitDevToolsPref();
    this._initialized = false;
  }

  onShutdown() {
    this._uninitialize();
  }

  getAPI(context) {
    return {
      devtools: {},
    };
  }

  onToolboxReady(toolbox) {
    if (
      !toolbox.descriptorFront.isLocalTab ||
      !this.extension.canAccessWindow(
        toolbox.descriptorFront.localTab.ownerGlobal
      )
    ) {
      // Skip any non-local (as remote tabs are not yet supported, see Bug 1304378 for additional details
      // related to remote targets support), and private browsing windows if the extension
      // is not allowed to access them.
      return;
    }

    // Ensure that the WebExtension is listed in the toolbox options.
    toolbox.registerWebExtension(this.extension.uuid, {
      name: this.extension.name,
      pref: `${getDevToolsPrefBranchName(this.extension.id)}.enabled`,
    });

    // Do not build the devtools page if the extension has been disabled
    // (e.g. based on the devtools preference).
    if (toolbox.isWebExtensionEnabled(this.extension.uuid)) {
      this.pageDefinition.buildForToolbox(toolbox);
    }
  }

  onToolboxDestroy(toolbox) {
    if (!toolbox.descriptorFront.isLocalTab) {
      // Only local tabs are currently supported (See Bug 1304378 for additional details
      // related to remote targets support).
      return;
    }

    this.pageDefinition.shutdownForToolbox(toolbox);
  }

  /**
   * Initialize the DevTools preferences branch for the extension and
   * start to observe it for changes on the "enabled" preference.
   */
  initDevToolsPref() {
    const prefBranch = Services.prefs.getBranch(
      `${getDevToolsPrefBranchName(this.extension.id)}.`
    );

    // Initialize the devtools extension preference if it doesn't exist yet.
    if (prefBranch.getPrefType("enabled") === prefBranch.PREF_INVALID) {
      prefBranch.setBoolPref("enabled", true);
    }

    this.devtoolsPrefBranch = prefBranch;
    this.devtoolsPrefBranch.addObserver("enabled", this);
  }

  /**
   * Stop from observing the DevTools preferences branch for the extension.
   */
  uninitDevToolsPref() {
    this.devtoolsPrefBranch.removeObserver("enabled", this);
    this.devtoolsPrefBranch = null;
  }

  /**
   * Test if the extension's devtools_page has been disabled using the
   * DevTools preference.
   *
   * @returns {boolean}
   *          true if the devtools_page for this extension is disabled.
   */
  isDevToolsPageDisabled() {
    return !this.devtoolsPrefBranch.getBoolPref("enabled", false);
  }

  /**
   * Observes the changed preferences on the DevTools preferences branch
   * related to the extension.
   *
   * @param {nsIPrefBranch} subject  The observed preferences branch.
   * @param {string}        topic    The notified topic.
   * @param {string}        prefName The changed preference name.
   */
  observe(subject, topic, prefName) {
    // We are currently interested only in the "enabled" preference from the
    // WebExtension devtools preferences branch.
    if (subject !== this.devtoolsPrefBranch || prefName !== "enabled") {
      return;
    }

    // Shutdown or build the devtools_page on any existing toolbox.
    if (this.isDevToolsPageDisabled()) {
      this.pageDefinition.shutdown();
    } else {
      this.pageDefinition.build();
    }
  }
};
