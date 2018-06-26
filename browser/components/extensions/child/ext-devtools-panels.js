/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

ChromeUtils.defineModuleGetter(this, "ExtensionChildDevToolsUtils",
                               "resource://gre/modules/ExtensionChildDevToolsUtils.jsm");

var {
  promiseDocumentLoaded,
} = ExtensionUtils;

/**
 * Represents an addon devtools panel in the child process.
 *
 * @param {DevtoolsExtensionContext}
 *   A devtools extension context running in a child process.
 * @param {object} panelOptions
 * @param {string} panelOptions.id
 *   The id of the addon devtools panel registered in the main process.
 */
class ChildDevToolsPanel extends ExtensionCommon.EventEmitter {
  constructor(context, {id}) {
    super();

    this.context = context;
    this.context.callOnClose(this);

    this.id = id;
    this._panelContext = null;

    this.mm = context.messageManager;
    this.mm.addMessageListener("Extension:DevToolsPanelShown", this);
    this.mm.addMessageListener("Extension:DevToolsPanelHidden", this);
  }

  get panelContext() {
    if (this._panelContext) {
      return this._panelContext;
    }

    for (let view of this.context.extension.devtoolsViews) {
      if (view.viewType === "devtools_panel" &&
          view.devtoolsToolboxInfo.toolboxPanelId === this.id) {
        this._panelContext = view;

        // Reset the cached _panelContext property when the view is closed.
        view.callOnClose({
          close: () => {
            this._panelContext = null;
          },
        });
        return view;
      }
    }

    return null;
  }

  receiveMessage({name, data}) {
    // Filter out any message that is not related to the id of this
    // toolbox panel.
    if (!data || data.toolboxPanelId !== this.id) {
      return;
    }

    switch (name) {
      case "Extension:DevToolsPanelShown":
        // Filter out *Shown message received while the panel context do not yet
        // exist.
        if (!this.panelContext || !this.panelContext.contentWindow) {
          return;
        }
        this.onParentPanelShown();
        break;
      case "Extension:DevToolsPanelHidden":
        this.onParentPanelHidden();
        break;
    }
  }

  onParentPanelShown() {
    const {document} = this.panelContext.contentWindow;

    // Ensure that the onShown event is fired when the panel document has
    // been fully loaded.
    promiseDocumentLoaded(document).then(() => {
      this.emit("shown", this.panelContext.contentWindow);
    });
  }

  onParentPanelHidden() {
    this.emit("hidden");
  }

  api() {
    return {
      onShown: new EventManager({
        context: this.context,
        name: "devtoolsPanel.onShown",
        register: fire => {
          const listener = (eventName, panelContentWindow) => {
            fire.asyncWithoutClone(panelContentWindow);
          };
          this.on("shown", listener);
          return () => {
            this.off("shown", listener);
          };
        },
      }).api(),

      onHidden: new EventManager({
        context: this.context,
        name: "devtoolsPanel.onHidden",
        register: fire => {
          const listener = () => {
            fire.async();
          };
          this.on("hidden", listener);
          return () => {
            this.off("hidden", listener);
          };
        },
      }).api(),

      // TODO(rpl): onSearch event and createStatusBarButton method
    };
  }

  close() {
    this.mm.removeMessageListener("Extension:DevToolsPanelShown", this);
    this.mm.removeMessageListener("Extension:DevToolsPanelHidden", this);

    this._panelContext = null;
    this.context = null;
  }
}

/**
 * Represents an addon devtools inspector sidebar in the child process.
 *
 * @param {DevtoolsExtensionContext}
 *   A devtools extension context running in a child process.
 * @param {object} sidebarOptions
 * @param {string} sidebarOptions.id
 *   The id of the addon devtools sidebar registered in the main process.
 */
class ChildDevToolsInspectorSidebar extends ExtensionCommon.EventEmitter {
  constructor(context, {id}) {
    super();

    this.context = context;
    this.context.callOnClose(this);

    this.id = id;

    this.mm = context.messageManager;
    this.mm.addMessageListener("Extension:DevToolsInspectorSidebarShown", this);
    this.mm.addMessageListener("Extension:DevToolsInspectorSidebarHidden", this);
  }

  close() {
    this.mm.removeMessageListener("Extension:DevToolsInspectorSidebarShown", this);
    this.mm.removeMessageListener("Extension:DevToolsInspectorSidebarHidden", this);

    this.content = null;
  }

  receiveMessage({name, data}) {
    // Filter out any message that is not related to the id of this
    // toolbox panel.
    if (!data || data.inspectorSidebarId !== this.id) {
      return;
    }

    switch (name) {
      case "Extension:DevToolsInspectorSidebarShown":
        this.onParentSidebarShown();
        break;
      case "Extension:DevToolsInspectorSidebarHidden":
        this.onParentSidebarHidden();
        break;
    }
  }

  onParentSidebarShown() {
    // TODO: wait and emit sidebar contentWindow once sidebar.setPage is supported.
    this.emit("shown");
  }

  onParentSidebarHidden() {
    this.emit("hidden");
  }

  api() {
    const {context, id} = this;

    return {
      onShown: new EventManager({
        context,
        name: "devtoolsInspectorSidebar.onShown",
        register: fire => {
          const listener = (eventName, panelContentWindow) => {
            fire.asyncWithoutClone(panelContentWindow);
          };
          this.on("shown", listener);
          return () => {
            this.off("shown", listener);
          };
        },
      }).api(),

      onHidden: new EventManager({
        context,
        name: "devtoolsInspectorSidebar.onHidden",
        register: fire => {
          const listener = () => {
            fire.async();
          };
          this.on("hidden", listener);
          return () => {
            this.off("hidden", listener);
          };
        },
      }).api(),

      setObject(jsonObject, rootTitle) {
        return context.cloneScope.Promise.resolve().then(() => {
          return context.childManager.callParentAsyncFunction(
            "devtools.panels.elements.Sidebar.setObject",
            [id, jsonObject, rootTitle]
          );
        });
      },

      setExpression(evalExpression, rootTitle) {
        return context.cloneScope.Promise.resolve().then(() => {
          return context.childManager.callParentAsyncFunction(
            "devtools.panels.elements.Sidebar.setExpression",
            [id, evalExpression, rootTitle]
          );
        });
      },
    };
  }
}

this.devtools_panels = class extends ExtensionAPI {
  getAPI(context) {
    const themeChangeObserver = ExtensionChildDevToolsUtils.getThemeChangeObserver();

    return {
      devtools: {
        panels: {
          elements: {
            createSidebarPane(title) {
              // NOTE: this is needed to be able to return to the caller (the extension)
              // a promise object that it had the privileges to use (e.g. by marking this
              // method async we will return a promise object which can only be used by
              // chrome privileged code).
              return context.cloneScope.Promise.resolve().then(async () => {
                const sidebarId = await context.childManager.callParentAsyncFunction(
                  "devtools.panels.elements.createSidebarPane", [title]);

                const sidebar = new ChildDevToolsInspectorSidebar(context, {id: sidebarId});

                const sidebarAPI = Cu.cloneInto(sidebar.api(),
                                                context.cloneScope,
                                                {cloneFunctions: true});

                return sidebarAPI;
              });
            },
          },
          create(title, icon, url) {
            // NOTE: this is needed to be able to return to the caller (the extension)
            // a promise object that it had the privileges to use (e.g. by marking this
            // method async we will return a promise object which can only be used by
            // chrome privileged code).
            return context.cloneScope.Promise.resolve().then(async () => {
              const panelId = await context.childManager.callParentAsyncFunction(
                "devtools.panels.create", [title, icon, url]);

              const devtoolsPanel = new ChildDevToolsPanel(context, {id: panelId});

              const devtoolsPanelAPI = Cu.cloneInto(devtoolsPanel.api(),
                                                    context.cloneScope,
                                                    {cloneFunctions: true});
              return devtoolsPanelAPI;
            });
          },
          get themeName() {
            return themeChangeObserver.themeName;
          },
          onThemeChanged: new EventManager({
            context,
            name: "devtools.panels.onThemeChanged",
            register: fire => {
              const listener = (eventName, themeName) => {
                fire.async(themeName);
              };
              themeChangeObserver.on("themeChanged", listener);
              return () => {
                themeChangeObserver.off("themeChanged", listener);
              };
            },
          }).api(),
        },
      },
    };
  }
};
