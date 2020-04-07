/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createElement,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const EventEmitter = require("devtools/shared/event-emitter");
const { Provider } = require("devtools/client/shared/vendor/react-redux");

const extensionsSidebarReducer = require("devtools/client/inspector/extensions/reducers/sidebar");
const {
  default: objectInspectorReducer,
} = require("devtools/client/debugger/packages/devtools-reps/src/object-inspector/reducer");

const ExtensionSidebarComponent = createFactory(
  require("devtools/client/inspector/extensions/components/ExtensionSidebar")
);

const {
  updateExtensionPage,
  updateObjectTreeView,
  updateExpressionResultView,
  removeExtensionSidebar,
} = require("devtools/client/inspector/extensions/actions/sidebar");

/**
 * ExtensionSidebar instances represents Inspector sidebars installed by add-ons
 * using the devtools.panels.elements.createSidebarPane WebExtensions API.
 *
 * The WebExtensions API registers the extensions' sidebars on the toolbox instance
 * (using the registerInspectorExtensionSidebar method) and, once the Inspector has been
 * created, the toolbox uses the Inpector createExtensionSidebar method to create the
 * ExtensionSidebar instances and then it registers them to the Inspector.
 *
 * @param {Inspector} inspector
 *        The inspector where the sidebar should be hooked to.
 * @param {Object} options
 * @param {String} options.id
 *        The unique id of the sidebar.
 * @param {String} options.title
 *        The title of the sidebar.
 */
class ExtensionSidebar {
  constructor(inspector, { id, title }) {
    EventEmitter.decorate(this);
    this.inspector = inspector;
    this.store = inspector.store;
    this.id = id;
    this.title = title;
    this.destroyed = false;

    this.store.injectReducer("extensionsSidebar", extensionsSidebarReducer);
    this.store.injectReducer("objectInspector", objectInspectorReducer);
  }

  /**
   * Lazily create a React ExtensionSidebarComponent wrapped into a Redux Provider.
   */
  get provider() {
    if (!this._provider) {
      this._provider = createElement(
        Provider,
        {
          store: this.store,
          key: this.id,
          title: this.title,
        },
        ExtensionSidebarComponent({
          id: this.id,
          onExtensionPageMount: containerEl => {
            this.emit("extension-page-mount", containerEl);
          },
          onExtensionPageUnmount: containerEl => {
            this.emit("extension-page-unmount", containerEl);
          },
          serviceContainer: {
            highlightDomElement: async (grip, options = {}) => {
              const nodeFront = await this.inspector.inspectorFront.getNodeFrontFromNodeGrip(
                grip
              );
              return nodeFront.highlighterFront.highlight(nodeFront, options);
            },
            unHighlightDomElement: async grip => {
              const nodeFront = await this.inspector.inspectorFront.getNodeFrontFromNodeGrip(
                grip
              );
              return nodeFront.highlighterFront.unhighlight();
            },
            openNodeInInspector: async grip => {
              const nodeFront = await this.inspector.inspectorFront.getNodeFrontFromNodeGrip(
                grip
              );
              const onInspectorUpdated = this.inspector.once(
                "inspector-updated"
              );
              const onNodeFrontSet = this.inspector.toolbox.selection.setNodeFront(
                nodeFront,
                {
                  reason: "inspector-extension-sidebar",
                }
              );

              return Promise.all([onNodeFrontSet, onInspectorUpdated]);
            },
          },
        })
      );
    }

    return this._provider;
  }

  /**
   * Destroy the ExtensionSidebar instance, dispatch a removeExtensionSidebar Redux action
   * (which removes the related state from the Inspector store) and clear any reference
   * to the inspector, the Redux store and the lazily created Redux Provider component.
   *
   * This method is called by the inspector when the ExtensionSidebar is being removed
   * (or when the inspector is being destroyed).
   */
  destroy() {
    if (this.destroyed) {
      throw new Error(
        `ExtensionSidebar instances cannot be destroyed more than once`
      );
    }

    // Remove the data related to this extension from the inspector store.
    this.store.dispatch(removeExtensionSidebar(this.id));

    this.inspector = null;
    this.store = null;
    this._provider = null;

    this.destroyed = true;
  }

  /**
   * Dispatch an objectTreeView action to change the SidebarComponent into an
   * ObjectTreeView React Component, which shows the passed javascript object
   * in the sidebar.
   */
  setObject(object) {
    if (this.removed) {
      throw new Error(
        "Unable to set an object preview on a removed ExtensionSidebar"
      );
    }

    this.store.dispatch(updateObjectTreeView(this.id, object));
  }

  /**
   * Dispatch an objectPreview action to change the SidebarComponent into an
   * ObjectPreview React Component, which shows the passed value grip
   * in the sidebar.
   */
  setExpressionResult(expressionResult, rootTitle) {
    if (this.removed) {
      throw new Error(
        "Unable to set an object preview on a removed ExtensionSidebar"
      );
    }

    this.store.dispatch(
      updateExpressionResultView(this.id, expressionResult, rootTitle)
    );
  }

  setExtensionPage(iframeURL) {
    if (this.removed) {
      throw new Error(
        "Unable to set an object preview on a removed ExtensionSidebar"
      );
    }

    this.store.dispatch(updateExtensionPage(this.id, iframeURL));
  }
}

module.exports = ExtensionSidebar;
