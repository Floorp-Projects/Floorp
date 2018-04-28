/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

ChromeUtils.import("resource://gre/modules/ExtensionParent.jsm");

ChromeUtils.defineModuleGetter(this, "E10SUtils",
                               "resource://gre/modules/E10SUtils.jsm");

var {
  IconDetails,
  watchExtensionProxyContextLoad,
} = ExtensionParent;

var {
  promiseEvent,
} = ExtensionUtils;

var XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

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
class ParentDevToolsPanel {
  constructor(context, panelOptions) {
    const toolbox = context.devToolsToolbox;
    if (!toolbox) {
      // This should never happen when this constructor is called with a valid
      // devtools extension context.
      throw Error("Missing mandatory toolbox");
    }

    this.extension = context.extension;
    this.viewType = "devtools_panel";

    this.visible = false;
    this.toolbox = toolbox;

    this.context = context;

    this.panelOptions = panelOptions;

    this.context.callOnClose(this);

    this.id = this.panelOptions.id;

    this.onToolboxPanelSelect = this.onToolboxPanelSelect.bind(this);
    this.onToolboxHostWillChange = this.onToolboxHostWillChange.bind(this);
    this.onToolboxHostChanged = this.onToolboxHostChanged.bind(this);

    this.unwatchExtensionProxyContextLoad = null;
    this.waitTopLevelContext = new Promise(resolve => {
      this._resolveTopLevelContext = resolve;
    });

    // References to the panel browser XUL element and the toolbox window global which
    // contains the devtools panel UI.
    this.browser = null;
    this.browserContainerWindow = null;

    this.panelAdded = false;
    this.addPanel();
  }

  addPanel() {
    const {icon, title} = this.panelOptions;
    const extensionName = this.context.extension.name;

    this.toolbox.addAdditionalTool({
      id: this.id,
      extensionId: this.context.extension.id,
      url: "chrome://browser/content/webext-panels.xul",
      icon: icon,
      label: title,
      tooltip: `DevTools Panel added by "${extensionName}" add-on.`,
      isTargetSupported: target => target.isLocalTab,
      build: (window, toolbox) => {
        if (toolbox !== this.toolbox) {
          throw new Error("Unexpected toolbox received on addAdditionalTool build property");
        }

        const destroy = this.buildPanel(window);

        return {toolbox, destroy};
      },
    });

    this.panelAdded = true;
  }

  buildPanel(window) {
    const {toolbox} = this;

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
        this.context.parentMessageManager.sendAsyncMessage("Extension:DevToolsPanelHidden", {
          toolboxPanelId: this.id,
        });
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

        this.context.parentMessageManager.sendAsyncMessage("Extension:DevToolsPanelShown", {
          toolboxPanelId: this.id,
        });
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
    } else if (this.visible && id !== this.id) {
      this.visible = false;
    }

    const extensionMessage = `Extension:DevToolsPanel${this.visible ? "Shown" : "Hidden"}`;
    this.context.parentMessageManager.sendAsyncMessage(extensionMessage, {
      toolboxPanelId: this.id,
    });
  }

  close() {
    const {toolbox} = this;

    if (!toolbox) {
      throw new Error("Unable to destroy a closed devtools panel");
    }

    // Explicitly remove the panel if it is registered and the toolbox is not
    // closing itself.
    if (this.panelAdded && toolbox.isToolRegistered(this.id)) {
      toolbox.removeAdditionalTool(this.id);
    }

    this.waitTopLevelContext = null;
    this._resolveTopLevelContext = null;
    this.context = null;
    this.toolbox = null;
    this.browser = null;
    this.browserContainerWindow = null;
  }

  createBrowserElement(window) {
    const {toolbox} = this;
    const {extension} = this.context;
    const {url} = this.panelOptions;
    const {document} = window;

    // TODO Bug 1442601: Refactor ext-devtools-panels.js to reuse the helpers
    // functions defined in webext-panels.xul (e.g. create the browser element
    // using an helper function defined in webext-panels.js and shared with the
    // extension sidebar pages).
    let stack = document.getElementById("webext-panels-stack");
    if (!stack) {
      stack = document.createElementNS(XUL_NS, "stack");
      stack.setAttribute("flex", "1");
      stack.setAttribute("id", "webext-panels-stack");
      document.documentElement.appendChild(stack);
    }

    const browser = document.createElementNS(XUL_NS, "browser");
    browser.setAttribute("id", "webext-panels-browser");
    browser.setAttribute("type", "content");
    browser.setAttribute("disableglobalhistory", "true");
    browser.setAttribute("flex", "1");
    browser.setAttribute("class", "webextension-devtoolsPanel-browser");
    browser.setAttribute("webextension-view-type", "devtools_panel");
    // TODO Bug 1442604: Add additional tests for the select and autocompletion
    // popups used in an extension devtools panels (in oop and non-oop mode).
    browser.setAttribute("selectmenulist", "ContentSelectDropdown");
    browser.setAttribute("autocompletepopup", "PopupAutoComplete");

    // Ensure that the devtools panel browser is going to run in the same
    // process of the other extension pages from the same addon.
    browser.sameProcessAsFrameLoader = extension.groupFrameLoader;

    this.browser = browser;

    let awaitFrameLoader = Promise.resolve();
    if (extension.remote) {
      browser.setAttribute("remote", "true");
      browser.setAttribute("remoteType", E10SUtils.EXTENSION_REMOTE_TYPE);
      awaitFrameLoader = promiseEvent(browser, "XULFrameLoaderCreated");
    }

    let hasTopLevelContext = false;

    // Listening to new proxy contexts.
    this.unwatchExtensionProxyContextLoad = watchExtensionProxyContextLoad(this, context => {
      // Keep track of the toolbox and target associated to the context, which is
      // needed by the API methods implementation.
      context.devToolsToolbox = toolbox;

      if (!hasTopLevelContext) {
        hasTopLevelContext = true;

        // Resolve the promise when the root devtools_panel context has been created.
        awaitFrameLoader.then(() => this._resolveTopLevelContext(context));
      }
    });

    stack.appendChild(browser);

    extensions.emit("extension-browser-inserted", browser, {
      devtoolsToolboxInfo: {
        toolboxPanelId: this.id,
        inspectedWindowTabId: getTargetTabIdForToolbox(toolbox),
      },
    });

    browser.loadURI(url, {
      triggeringPrincipal: extension.principal,
    });
  }

  destroyBrowserElement() {
    const {browser, unwatchExtensionProxyContextLoad} = this;
    if (unwatchExtensionProxyContextLoad) {
      this.unwatchExtensionProxyContextLoad = null;
      unwatchExtensionProxyContextLoad();
    }

    if (browser) {
      browser.remove();
      this.browser = null;
    }

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
class ParentDevToolsInspectorSidebar {
  constructor(context, sidebarOptions) {
    const toolbox = context.devToolsToolbox;
    if (!toolbox) {
      // This should never happen when this constructor is called with a valid
      // devtools extension context.
      throw Error("Missing mandatory toolbox");
    }

    this.extension = context.extension;
    this.visible = false;
    this.destroyed = false;

    this.toolbox = toolbox;
    this.context = context;
    this.sidebarOptions = sidebarOptions;

    this.context.callOnClose(this);

    this.id = this.sidebarOptions.id;
    this.onSidebarSelect = this.onSidebarSelect.bind(this);
    this.onSidebarCreated = this.onSidebarCreated.bind(this);

    this.toolbox.once(`extension-sidebar-created-${this.id}`, this.onSidebarCreated);
    this.toolbox.on(`inspector-sidebar-select`, this.onSidebarSelect);

    // Set by setObject if the sidebar has not been created yet.
    this._initializeSidebar = null;

    // Set by _updateLastObjectValueGrip to keep track of the last
    // object value grip (to release the previous selected actor
    // on the remote debugging server when the actor changes).
    this._lastObjectValueGrip = null;

    this.toolbox.registerInspectorExtensionSidebar(this.id, {
      title: sidebarOptions.title,
    });
  }

  close() {
    if (this.destroyed) {
      throw new Error("Unable to close a destroyed DevToolsSelectionObserver");
    }

    // Release the last selected actor on the remote debugging server.
    this._updateLastObjectValueGrip(null);

    this.toolbox.off(`extension-sidebar-created-${this.id}`, this.onSidebarCreated);
    this.toolbox.off(`inspector-sidebar-select`, this.onSidebarSelect);

    this.toolbox.unregisterInspectorExtensionSidebar(this.id);
    this.extensionSidebar = null;
    this._lazySidebarInit = null;

    this.destroyed = true;
  }

  onSidebarCreated(sidebar) {
    this.extensionSidebar = sidebar;

    const {_lazySidebarInit} = this;
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
      // TODO: Wait for top level context if extension page
      this.visible = true;
      this.context.parentMessageManager.sendAsyncMessage("Extension:DevToolsInspectorSidebarShown", {
        inspectorSidebarId: this.id,
      });
    } else if (this.visible && id !== this.id) {
      this.visible = false;
      this.context.parentMessageManager.sendAsyncMessage("Extension:DevToolsInspectorSidebarHidden", {
        inspectorSidebarId: this.id,
      });
    }
  }

  setObject(object, rootTitle) {
    this._updateLastObjectValueGrip(null);

    // Nest the object inside an object, as the value of the `rootTitle` property.
    if (rootTitle) {
      object = {[rootTitle]: object};
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

  setObjectValueGrip(objectValueGrip, rootTitle) {
    this._updateLastObjectValueGrip(objectValueGrip);

    if (this.extensionSidebar) {
      this.extensionSidebar.setObjectValueGrip(objectValueGrip, rootTitle);
    } else {
      // Defer the sidebar.setObjectValueGrip call.
      this._setLazySidebarInit(() => {
        this.extensionSidebar.setObjectValueGrip(objectValueGrip, rootTitle);
      });
    }
  }

  _updateLastObjectValueGrip(newObjectValueGrip = null) {
    const {_lastObjectValueGrip} = this;

    this._lastObjectValueGrip = newObjectValueGrip;

    const oldActor = _lastObjectValueGrip && _lastObjectValueGrip.actor;
    const newActor = newObjectValueGrip && newObjectValueGrip.actor;

    // Release the previously active actor on the remote debugging server.
    if (oldActor && oldActor !== newActor) {
      this.toolbox.target.client.release(oldActor);
    }
  }
}

const sidebarsById = new Map();

this.devtools_panels = class extends ExtensionAPI {
  getAPI(context) {
    // Lazily retrieved inspectedWindow actor front per child context
    // (used by Sidebar.setExpression).
    let waitForInspectedWindowFront;

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
                const listener = (eventName) => {
                  fire.async();
                };
                toolboxSelectionObserver.on("selectionChanged", listener);
                return () => {
                  toolboxSelectionObserver.off("selectionChanged", listener);
                };
              },
            }).api(),
            createSidebarPane(title) {
              const id = `devtools-inspector-sidebar-${makeWidgetId(newBasePanelId())}`;

              const parentSidebar = new ParentDevToolsInspectorSidebar(context, {title, id});
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
              setObject(sidebarId, jsonObject, rootTitle) {
                const sidebar = sidebarsById.get(sidebarId);
                return sidebar.setObject(jsonObject, rootTitle);
              },
              async setExpression(sidebarId, evalExpression, rootTitle) {
                const sidebar = sidebarsById.get(sidebarId);

                if (!waitForInspectedWindowFront) {
                  waitForInspectedWindowFront = getInspectedWindowFront(context);
                }

                const front = await waitForInspectedWindowFront;
                const evalOptions = Object.assign({
                  evalResultAsGrip: true,
                }, getToolboxEvalOptions(context));
                const evalResult = await front.eval(callerInfo, evalExpression, evalOptions);

                let jsonObject;

                if (evalResult.exceptionInfo) {
                  jsonObject = evalResult.exceptionInfo;

                  return sidebar.setObject(jsonObject, rootTitle);
                }

                return sidebar.setObjectValueGrip(evalResult.valueGrip, rootTitle);
              },
            },
          },
          create(title, icon, url) {
            // Get a fallback icon from the manifest data.
            if (icon === "" && context.extension.manifest.icons) {
              const iconInfo = IconDetails.getPreferredIcon(context.extension.manifest.icons,
                                                            context.extension, 128);
              icon = iconInfo ? iconInfo.icon : "";
            }

            icon = context.extension.baseURI.resolve(icon);
            url = context.extension.baseURI.resolve(url);

            const id = `webext-devtools-panel-${makeWidgetId(newBasePanelId())}`;

            new ParentDevToolsPanel(context, {title, icon, url, id});

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
