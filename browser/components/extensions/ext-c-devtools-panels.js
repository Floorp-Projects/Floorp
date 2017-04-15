/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "EventEmitter",
                                  "resource://devtools/shared/event-emitter.js");

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
class ChildDevToolsPanel extends EventEmitter {
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
    // Filter out any message received while the panel context do not yet
    // exist.
    if (!this.panelContext || !this.panelContext.contentWindow) {
      return;
    }

    // Filter out any message that is not related to the id of this
    // toolbox panel.
    if (!data || data.toolboxPanelId !== this.id) {
      return;
    }

    switch (name) {
      case "Extension:DevToolsPanelShown":
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
      onShown: new SingletonEventManager(
        this.context, "devtoolsPanel.onShown", fire => {
          const listener = (eventName, panelContentWindow) => {
            fire.asyncWithoutClone(panelContentWindow);
          };
          this.on("shown", listener);
          return () => {
            this.off("shown", listener);
          };
        }).api(),

      onHidden: new SingletonEventManager(
        this.context, "devtoolsPanel.onHidden", fire => {
          const listener = () => {
            fire.async();
          };
          this.on("hidden", listener);
          return () => {
            this.off("hidden", listener);
          };
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

this.devtools_panels = class extends ExtensionAPI {
  getAPI(context) {
    return {
      devtools: {
        panels: {
          create(title, icon, url) {
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
        },
      },
    };
  }
};
