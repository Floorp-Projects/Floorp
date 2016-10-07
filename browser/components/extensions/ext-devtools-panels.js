/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/ExtensionParent.jsm");
Cu.import("resource://gre/modules/ExtensionUtils.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "E10SUtils",
                                  "resource:///modules/E10SUtils.jsm");

const {
  watchExtensionProxyContextLoad,
} = ExtensionParent;

const {
  IconDetails,
  promiseEvent,
} = ExtensionUtils;

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

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
    this.onToolboxReady = this.onToolboxReady.bind(this);

    this.panelAdded = false;

    if (this.toolbox.isReady) {
      this.onToolboxReady();
    } else {
      this.toolbox.once("ready", this.onToolboxReady);
    }

    this.waitTopLevelContext = new Promise(resolve => {
      this._resolveTopLevelContext = resolve;
    });
  }

  addPanel() {
    const {icon, title} = this.panelOptions;
    const extensionName = this.context.extension.name;

    this.toolbox.addAdditionalTool({
      id: this.id,
      url: "about:blank",
      icon: icon,
      label: title,
      tooltip: `DevTools Panel added by "${extensionName}" add-on.`,
      invertIconForLightTheme: true,
      visibilityswitch:  `devtools.webext-${this.id}.enabled`,
      isTargetSupported: target => target.isLocalTab,
      build: (window, toolbox) => {
        if (toolbox !== this.toolbox) {
          throw new Error("Unexpected toolbox received on addAdditionalTool build property");
        }

        const destroy = this.buildPanel(window, toolbox);

        return {toolbox, destroy};
      },
    });
  }

  buildPanel(window, toolbox) {
    const {url} = this.panelOptions;
    const {document} = window;

    const browser = document.createElementNS(XUL_NS, "browser");
    browser.setAttribute("type", "content");
    browser.setAttribute("disableglobalhistory", "true");
    browser.setAttribute("style", "width: 100%; height: 100%;");
    browser.setAttribute("transparent", "true");
    browser.setAttribute("class", "webextension-devtoolsPanel-browser");
    browser.setAttribute("webextension-view-type", "devtools_panel");
    browser.setAttribute("flex", "1");

    this.browser = browser;

    const {extension} = this.context;

    let awaitFrameLoader = Promise.resolve();
    if (extension.remote) {
      browser.setAttribute("remote", "true");
      browser.setAttribute("remoteType", E10SUtils.EXTENSION_REMOTE_TYPE);
      awaitFrameLoader = promiseEvent(browser, "XULFrameLoaderCreated");
    } else if (!AppConstants.RELEASE_OR_BETA) {
      // NOTE: Using a content iframe here breaks the devtools panel
      // switching between docked and undocked mode,
      // because of a swapFrameLoader exception (see bug 1075490).
      browser.setAttribute("type", "chrome");
      browser.setAttribute("forcemessagemanager", true);
    }

    let hasTopLevelContext = false;

    // Listening to new proxy contexts.
    const unwatchExtensionProxyContextLoad = watchExtensionProxyContextLoad(this, context => {
      // Keep track of the toolbox and target associated to the context, which is
      // needed by the API methods implementation.
      context.devToolsToolbox = toolbox;

      if (!hasTopLevelContext) {
        hasTopLevelContext = true;

        // Resolve the promise when the root devtools_panel context has been created.
        awaitFrameLoader.then(() => this._resolveTopLevelContext(context));
      }
    });

    document.body.setAttribute("style", "margin: 0; padding: 0;");
    document.body.appendChild(browser);

    extensions.emit("extension-browser-inserted", browser, {
      devtoolsToolboxInfo: {
        toolboxPanelId: this.id,
        inspectedWindowTabId: getTargetTabIdForToolbox(toolbox),
      },
    });

    browser.loadURI(url);

    toolbox.on("select", this.onToolboxPanelSelect);

    // Return a cleanup method that is when the panel is destroyed, e.g.
    // - when addon devtool panel has been disabled by the user from the toolbox preferences,
    //   its ParentDevToolsPanel instance is still valid, but the built devtools panel is removed from
    //   the toolbox (and re-built again if the user re-enable it from the toolbox preferences panel)
    // - when the creator context has been destroyed, the ParentDevToolsPanel close method is called,
    //   it remove the tool definition from the toolbox, which will call this destroy method.
    return () => {
      unwatchExtensionProxyContextLoad();
      browser.remove();
      toolbox.off("select", this.onToolboxPanelSelect);
    };
  }

  onToolboxReady() {
    if (!this.panelAdded) {
      this.panelAdded = true;
      this.addPanel();
    }
  }

  onToolboxPanelSelect(what, id) {
    if (!this.waitTopLevelContext || !this.panelAdded) {
      return;
    }

    if (!this.visible && id === this.id) {
      // Wait that the panel is fully loaded and emit show.
      this.waitTopLevelContext.then(() => {
        this.visible = true;
        this.context.parentMessageManager.sendAsyncMessage("Extension:DevToolsPanelShown", {
          toolboxPanelId: this.id,
        });
      });
    } else if (this.visible && id !== this.id) {
      this.visible = false;
      this.context.parentMessageManager.sendAsyncMessage("Extension:DevToolsPanelHidden", {
        toolboxPanelId: this.id,
      });
    }
  }

  close() {
    const {toolbox} = this;

    if (!toolbox) {
      throw new Error("Unable to destroy a closed devtools panel");
    }

    toolbox.off("ready", this.onToolboxReady);

    // Explicitly remove the panel if it is registered and the toolbox is not
    // closing itself.
    if (toolbox.isToolRegistered(this.id) && !toolbox._destroyer) {
      toolbox.removeAdditionalTool(this.id);
    }

    this.context = null;
    this.toolbox = null;
  }
}

extensions.registerSchemaAPI("devtools.panels", "devtools_parent", context => {
  // An incremental "per context" id used in the generated devtools panel id.
  let nextPanelId = 0;

  return {
    devtools: {
      panels: {
        create(title, icon, url) {
          // Get a fallback icon from the manifest data.
          if (icon === "" && context.extension.manifest.icons) {
            const iconInfo = IconDetails.getPreferredIcon(context.extension.manifest.icons,
                                                          context.extension, 128);
            icon = iconInfo ? iconInfo.icon : "";
          }

          icon = context.extension.baseURI.resolve(icon);
          url = context.extension.baseURI.resolve(url);

          const baseId = `${context.extension.id}-${context.contextId}-${nextPanelId++}`;
          const id = `${makeWidgetId(baseId)}-devtools-panel`;

          new ParentDevToolsPanel(context, {title, icon, url, id});

          // Resolved to the devtools panel id into the child addon process,
          // where it will be used to identify the messages related
          // to the panel API onShown/onHidden events.
          return Promise.resolve(id);
        },
      },
    },
  };
});
