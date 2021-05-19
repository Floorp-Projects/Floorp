/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { ExtensionParent } = ChromeUtils.import(
  "resource://gre/modules/ExtensionParent.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "BroadcastConduit",
  "resource://gre/modules/ConduitsParent.jsm"
);

var { IconDetails, watchExtensionProxyContextLoad } = ExtensionParent;

var { promiseDocumentLoaded } = ExtensionUtils;

const WEBEXT_PANELS_URL = "chrome://browser/content/webext-panels.xhtml";

class BaseDevToolsPanel {
  constructor(context, panelOptions) {
    const toolbox = context.devToolsToolbox;
    if (!toolbox) {
      // This should never happen when this constructor is called with a valid
      // devtools extension context.
      throw Error("Missing mandatory toolbox");
    }

    this.context = context;
    this.extension = context.extension;
    this.toolbox = toolbox;
    this.viewType = "devtools_panel";
    this.panelOptions = panelOptions;
    this.id = panelOptions.id;

    this.unwatchExtensionProxyContextLoad = null;

    // References to the panel browser XUL element and the toolbox window global which
    // contains the devtools panel UI.
    this.browser = null;
    this.browserContainerWindow = null;
  }

  async createBrowserElement(window) {
    const { toolbox } = this;
    const { extension } = this.context;
    const { url } = this.panelOptions || { url: "about:blank" };

    this.browser = await window.getBrowser({
      extension,
      extensionUrl: url,
      browserStyle: false,
      viewType: "devtools_panel",
      browserInsertedData: {
        devtoolsToolboxInfo: {
          toolboxPanelId: this.id,
          inspectedWindowTabId: getTargetTabIdForToolbox(toolbox),
        },
      },
    });

    let hasTopLevelContext = false;

    // Listening to new proxy contexts.
    this.unwatchExtensionProxyContextLoad = watchExtensionProxyContextLoad(
      this,
      context => {
        // Keep track of the toolbox and target associated to the context, which is
        // needed by the API methods implementation.
        context.devToolsToolbox = toolbox;

        if (!hasTopLevelContext) {
          hasTopLevelContext = true;

          // Resolve the promise when the root devtools_panel context has been created.
          if (this._resolveTopLevelContext) {
            this._resolveTopLevelContext(context);
          }
        }
      }
    );

    this.browser.loadURI(url, { triggeringPrincipal: this.context.principal });
  }

  destroyBrowserElement() {
    const { browser, unwatchExtensionProxyContextLoad } = this;
    if (unwatchExtensionProxyContextLoad) {
      this.unwatchExtensionProxyContextLoad = null;
      unwatchExtensionProxyContextLoad();
    }

    if (browser) {
      browser.remove();
      this.browser = null;
    }
  }
}

/**
 * Represents an addon devtools panel in the main process.
 *
 * @param {ExtensionChildProxyContext} context
 *        A devtools extension proxy context running in a main process.
 * @param {object} options
 * @param {string} options.id
 *        The id of the addon devtools panel.
 * @param {string} options.icon
 *        The icon of the addon devtools panel.
 * @param {string} options.title
 *        The title of the addon devtools panel.
 * @param {string} options.url
 *        The url of the addon devtools panel, relative to the extension base URL.
 */
class ParentDevToolsPanel extends BaseDevToolsPanel {
  constructor(context, panelOptions) {
    super(context, panelOptions);

    this.visible = false;
    this.destroyed = false;

    this.context.callOnClose(this);

    this.conduit = new BroadcastConduit(this, {
      id: `${this.id}-parent`,
      send: ["PanelHidden", "PanelShown"],
    });

    this.onToolboxPanelSelect = this.onToolboxPanelSelect.bind(this);
    this.onToolboxHostWillChange = this.onToolboxHostWillChange.bind(this);
    this.onToolboxHostChanged = this.onToolboxHostChanged.bind(this);

    this.waitTopLevelContext = new Promise(resolve => {
      this._resolveTopLevelContext = resolve;
    });

    this.panelAdded = false;
    this.addPanel();
  }

  addPanel() {
    const { icon, title } = this.panelOptions;
    const extensionName = this.context.extension.name;

    this.toolbox.addAdditionalTool({
      id: this.id,
      extensionId: this.context.extension.id,
      url: WEBEXT_PANELS_URL,
      icon: icon,
      label: title,
      // panelLabel is used to set the aria-label attribute (See Bug 1570645).
      panelLabel: title,
      tooltip: `DevTools Panel added by "${extensionName}" add-on.`,
      isTargetSupported: target => target.isLocalTab,
      build: (window, toolbox) => {
        if (toolbox !== this.toolbox) {
          throw new Error(
            "Unexpected toolbox received on addAdditionalTool build property"
          );
        }

        const destroy = this.buildPanel(window);

        return { toolbox, destroy };
      },
    });

    this.panelAdded = true;
  }

  buildPanel(window) {
    const { toolbox } = this;

    this.createBrowserElement(window);

    // Store the last panel's container element (used to restore it when the toolbox
    // host is switched between docked and undocked).
    this.browserContainerWindow = window;

    toolbox.on("select", this.onToolboxPanelSelect);
    toolbox.on("host-will-change", this.onToolboxHostWillChange);
    toolbox.on("host-changed", this.onToolboxHostChanged);

    // Return a cleanup method that is when the panel is destroyed, e.g.
    // - when addon devtool panel has been disabled by the user from the toolbox preferences,
    //   its ParentDevToolsPanel instance is still valid, but the built devtools panel is removed from
    //   the toolbox (and re-built again if the user re-enables it from the toolbox preferences panel)
    // - when the creator context has been destroyed, the ParentDevToolsPanel close method is called,
    //   it removes the tool definition from the toolbox, which will call this destroy method.
    return () => {
      this.destroyBrowserElement();
      this.browserContainerWindow = null;
      toolbox.off("select", this.onToolboxPanelSelect);
      toolbox.off("host-will-change", this.onToolboxHostWillChange);
      toolbox.off("host-changed", this.onToolboxHostChanged);
    };
  }

  onToolboxHostWillChange() {
    // NOTE: Using a content iframe here breaks the devtools panel
    // switching between docked and undocked mode,
    // because of a swapFrameLoader exception (see bug 1075490),
    // destroy the browser and recreate it after the toolbox host has been
    // switched is a reasonable workaround to fix the issue on release and beta
    // Firefox versions (at least until the underlying bug can be fixed).
    if (this.browser) {
      // Fires a panel.onHidden event before destroying the browser element because
      // the toolbox hosts is changing.
      if (this.visible) {
        this.conduit.sendPanelHidden(this.id);
      }

      this.destroyBrowserElement();
    }
  }

  async onToolboxHostChanged() {
    if (this.browserContainerWindow) {
      this.createBrowserElement(this.browserContainerWindow);

      // Fires a panel.onShown event once the browser element has been recreated
      // after the toolbox hosts has been changed (needed to provide the new window
      // object to the extension page that has created the devtools panel).
      if (this.visible) {
        await this.waitTopLevelContext;
        this.conduit.sendPanelShown(this.id);
      }
    }
  }

  async onToolboxPanelSelect(id) {
    if (!this.waitTopLevelContext || !this.panelAdded) {
      return;
    }

    // Wait that the panel is fully loaded and emit show.
    await this.waitTopLevelContext;

    if (!this.visible && id === this.id) {
      this.visible = true;
      this.conduit.sendPanelShown(this.id);
    } else if (this.visible && id !== this.id) {
      this.visible = false;
      this.conduit.sendPanelHidden(this.id);
    }
  }

  close() {
    const { toolbox } = this;

    if (!toolbox) {
      throw new Error("Unable to destroy a closed devtools panel");
    }

    this.conduit.close();

    // Explicitly remove the panel if it is registered and the toolbox is not
    // closing itself.
    if (this.panelAdded && toolbox.isToolRegistered(this.id)) {
      this.destroyBrowserElement();
      toolbox.removeAdditionalTool(this.id);
    }

    this.waitTopLevelContext = null;
    this._resolveTopLevelContext = null;
    this.context = null;
    this.toolbox = null;
    this.browser = null;
    this.browserContainerWindow = null;
  }

  destroyBrowserElement() {
    super.destroyBrowserElement();

    // If the panel has been removed or disabled (e.g. from the toolbox preferences
    // or during the toolbox switching between docked and undocked),
    // we need to re-initialize the waitTopLevelContext Promise.
    this.waitTopLevelContext = new Promise(resolve => {
      this._resolveTopLevelContext = resolve;
    });
  }
}

class DevToolsSelectionObserver extends EventEmitter {
  constructor(context) {
    if (!context.devToolsToolbox) {
      // This should never happen when this constructor is called with a valid
      // devtools extension context.
      throw Error("Missing mandatory toolbox");
    }

    super();
    context.callOnClose(this);

    this.toolbox = context.devToolsToolbox;
    this.onSelected = this.onSelected.bind(this);
    this.initialized = false;
  }

  on(...args) {
    this.lazyInit();
    super.on.apply(this, args);
  }

  once(...args) {
    this.lazyInit();
    super.once.apply(this, args);
  }

  async lazyInit() {
    if (!this.initialized) {
      this.initialized = true;
      this.toolbox.on("selection-changed", this.onSelected);
    }
  }

  close() {
    if (this.destroyed) {
      throw new Error("Unable to close a destroyed DevToolsSelectionObserver");
    }

    if (this.initialized) {
      this.toolbox.off("selection-changed", this.onSelected);
    }

    this.toolbox = null;
    this.destroyed = true;
  }

  onSelected() {
    this.emit("selectionChanged");
  }
}

/**
 * Represents an addon devtools inspector sidebar in the main process.
 *
 * @param {ExtensionChildProxyContext} context
 *        A devtools extension proxy context running in a main process.
 * @param {object} options
 * @param {string} options.id
 *        The id of the addon devtools sidebar.
 * @param {string} options.title
 *        The title of the addon devtools sidebar.
 */
class ParentDevToolsInspectorSidebar extends BaseDevToolsPanel {
  constructor(context, panelOptions) {
    super(context, panelOptions);

    this.visible = false;
    this.destroyed = false;

    this.context.callOnClose(this);

    this.conduit = new BroadcastConduit(this, {
      id: `${this.id}-parent`,
      send: ["InspectorSidebarHidden", "InspectorSidebarShown"],
    });

    this.onSidebarSelect = this.onSidebarSelect.bind(this);
    this.onSidebarCreated = this.onSidebarCreated.bind(this);
    this.onExtensionPageMount = this.onExtensionPageMount.bind(this);
    this.onExtensionPageUnmount = this.onExtensionPageUnmount.bind(this);
    this.onToolboxHostWillChange = this.onToolboxHostWillChange.bind(this);
    this.onToolboxHostChanged = this.onToolboxHostChanged.bind(this);

    this.toolbox.once(
      `extension-sidebar-created-${this.id}`,
      this.onSidebarCreated
    );
    this.toolbox.on("inspector-sidebar-select", this.onSidebarSelect);
    this.toolbox.on("host-will-change", this.onToolboxHostWillChange);
    this.toolbox.on("host-changed", this.onToolboxHostChanged);

    // Set by setObject if the sidebar has not been created yet.
    this._initializeSidebar = null;

    // Set by _updateLastExpressionResult to keep track of the last
    // object value grip (to release the previous selected actor
    // on the remote debugging server when the actor changes).
    this._lastExpressionResult = null;

    this.toolbox.registerInspectorExtensionSidebar(this.id, {
      title: panelOptions.title,
    });
  }

  close() {
    if (this.destroyed) {
      throw new Error("Unable to close a destroyed DevToolsSelectionObserver");
    }

    this.conduit.close();

    if (this.extensionSidebar) {
      this.extensionSidebar.off(
        "extension-page-mount",
        this.onExtensionPageMount
      );
      this.extensionSidebar.off(
        "extension-page-unmount",
        this.onExtensionPageUnmount
      );
    }

    if (this.browser) {
      this.destroyBrowserElement();
      this.browser = null;
      this.containerEl = null;
    }

    this.toolbox.off(
      `extension-sidebar-created-${this.id}`,
      this.onSidebarCreated
    );
    this.toolbox.off("inspector-sidebar-select", this.onSidebarSelect);
    this.toolbox.off("host-changed", this.onToolboxHostChanged);
    this.toolbox.off("host-will-change", this.onToolboxHostWillChange);

    this.toolbox.unregisterInspectorExtensionSidebar(this.id);
    this.extensionSidebar = null;
    this._lazySidebarInit = null;

    this.destroyed = true;
  }

  onToolboxHostWillChange() {
    if (this.browser) {
      this.destroyBrowserElement();
    }
  }

  onToolboxHostChanged() {
    if (this.containerEl && this.panelOptions.url) {
      this.createBrowserElement(this.containerEl.contentWindow);
    }
  }

  onExtensionPageMount(containerEl) {
    this.containerEl = containerEl;

    // Wait the webext-panel.xhtml page to have been loaded in the
    // inspector sidebar panel.
    promiseDocumentLoaded(containerEl.contentDocument).then(() => {
      this.createBrowserElement(containerEl.contentWindow);
    });
  }

  onExtensionPageUnmount() {
    this.containerEl = null;
    this.destroyBrowserElement();
  }

  onSidebarCreated(sidebar) {
    this.extensionSidebar = sidebar;

    sidebar.on("extension-page-mount", this.onExtensionPageMount);
    sidebar.on("extension-page-unmount", this.onExtensionPageUnmount);

    const { _lazySidebarInit } = this;
    this._lazySidebarInit = null;

    if (typeof _lazySidebarInit === "function") {
      _lazySidebarInit();
    }
  }

  onSidebarSelect(id) {
    if (!this.extensionSidebar) {
      return;
    }

    if (!this.visible && id === this.id) {
      this.visible = true;
      this.conduit.sendInspectorSidebarShown(this.id);
    } else if (this.visible && id !== this.id) {
      this.visible = false;
      this.conduit.sendInspectorSidebarHidden(this.id);
    }
  }

  setPage(extensionPageURL) {
    this.panelOptions.url = extensionPageURL;

    if (this.extensionSidebar) {
      if (this.browser) {
        // Just load the new extension page url in the existing browser, if
        // it already exists.
        this.browser.loadURI(this.panelOptions.url, {
          triggeringPrincipal: this.context.extension.principal,
        });
      } else {
        // The browser element doesn't exist yet, but the sidebar has been
        // already created (e.g. because the inspector was already selected
        // in a open toolbox and the extension has been installed/reloaded/updated).
        this.extensionSidebar.setExtensionPage(WEBEXT_PANELS_URL);
      }
    } else {
      // Defer the sidebar.setExtensionPage call.
      this._setLazySidebarInit(() =>
        this.extensionSidebar.setExtensionPage(WEBEXT_PANELS_URL)
      );
    }
  }

  setObject(object, rootTitle) {
    delete this.panelOptions.url;

    this._updateLastExpressionResult(null);

    // Nest the object inside an object, as the value of the `rootTitle` property.
    if (rootTitle) {
      object = { [rootTitle]: object };
    }

    if (this.extensionSidebar) {
      this.extensionSidebar.setObject(object);
    } else {
      // Defer the sidebar.setObject call.
      this._setLazySidebarInit(() => this.extensionSidebar.setObject(object));
    }
  }

  _setLazySidebarInit(cb) {
    this._lazySidebarInit = cb;
  }

  setExpressionResult(expressionResult, rootTitle) {
    delete this.panelOptions.url;

    this._updateLastExpressionResult(expressionResult);

    if (this.extensionSidebar) {
      this.extensionSidebar.setExpressionResult(expressionResult, rootTitle);
    } else {
      // Defer the sidebar.setExpressionResult call.
      this._setLazySidebarInit(() => {
        this.extensionSidebar.setExpressionResult(expressionResult, rootTitle);
      });
    }
  }

  _updateLastExpressionResult(newExpressionResult = null) {
    const { _lastExpressionResult } = this;

    this._lastExpressionResult = newExpressionResult;

    const oldActor = _lastExpressionResult && _lastExpressionResult.actorID;
    const newActor = newExpressionResult && newExpressionResult.actorID;

    // Release the previously active actor on the remote debugging server.
    if (
      oldActor &&
      oldActor !== newActor &&
      typeof _lastExpressionResult.release === "function"
    ) {
      _lastExpressionResult.release();
    }
  }
}

const sidebarsById = new Map();

this.devtools_panels = class extends ExtensionAPI {
  getAPI(context) {
    // TODO - Bug 1448878: retrieve a more detailed callerInfo object,
    // like the filename and lineNumber of the actual extension called
    // in the child process.
    const callerInfo = {
      addonId: context.extension.id,
      url: context.extension.baseURI.spec,
    };

    // An incremental "per context" id used in the generated devtools panel id.
    let nextPanelId = 0;

    const toolboxSelectionObserver = new DevToolsSelectionObserver(context);

    function newBasePanelId() {
      return `${context.extension.id}-${context.contextId}-${nextPanelId++}`;
    }

    return {
      devtools: {
        panels: {
          elements: {
            onSelectionChanged: new EventManager({
              context,
              name: "devtools.panels.elements.onSelectionChanged",
              register: fire => {
                const listener = eventName => {
                  fire.async();
                };
                toolboxSelectionObserver.on("selectionChanged", listener);
                return () => {
                  toolboxSelectionObserver.off("selectionChanged", listener);
                };
              },
            }).api(),
            createSidebarPane(title) {
              const id = `devtools-inspector-sidebar-${makeWidgetId(
                newBasePanelId()
              )}`;

              const parentSidebar = new ParentDevToolsInspectorSidebar(
                context,
                { title, id }
              );
              sidebarsById.set(id, parentSidebar);

              context.callOnClose({
                close() {
                  sidebarsById.delete(id);
                },
              });

              // Resolved to the devtools sidebar id into the child addon process,
              // where it will be used to identify the messages related
              // to the panel API onShown/onHidden events.
              return Promise.resolve(id);
            },
            // The following methods are used internally to allow the sidebar API
            // piece that is running in the child process to asks the parent process
            // to execute the sidebar methods.
            Sidebar: {
              setPage(sidebarId, extensionPageURL) {
                const sidebar = sidebarsById.get(sidebarId);
                return sidebar.setPage(extensionPageURL);
              },
              setObject(sidebarId, jsonObject, rootTitle) {
                const sidebar = sidebarsById.get(sidebarId);
                return sidebar.setObject(jsonObject, rootTitle);
              },
              async setExpression(sidebarId, evalExpression, rootTitle) {
                const sidebar = sidebarsById.get(sidebarId);

                const toolboxEvalOptions = await getToolboxEvalOptions(context);

                const commands = await context.getDevToolsCommands();
                const target = commands.targetCommand.targetFront;
                const consoleFront = await target.getFront("console");
                toolboxEvalOptions.consoleFront = consoleFront;

                const evalResult = await commands.inspectedWindowCommand.eval(
                  callerInfo,
                  evalExpression,
                  toolboxEvalOptions
                );

                let jsonObject;

                if (evalResult.exceptionInfo) {
                  jsonObject = evalResult.exceptionInfo;

                  return sidebar.setObject(jsonObject, rootTitle);
                }

                return sidebar.setExpressionResult(evalResult, rootTitle);
              },
            },
          },
          create(title, icon, url) {
            // Get a fallback icon from the manifest data.
            if (icon === "" && context.extension.manifest.icons) {
              const iconInfo = IconDetails.getPreferredIcon(
                context.extension.manifest.icons,
                context.extension,
                128
              );
              icon = iconInfo ? iconInfo.icon : "";
            }

            icon = context.extension.baseURI.resolve(icon);
            url = context.extension.baseURI.resolve(url);

            const id = `webext-devtools-panel-${makeWidgetId(
              newBasePanelId()
            )}`;

            new ParentDevToolsPanel(context, { title, icon, url, id });

            // Resolved to the devtools panel id into the child addon process,
            // where it will be used to identify the messages related
            // to the panel API onShown/onHidden events.
            return Promise.resolve(id);
          },
        },
      },
    };
  }
};
