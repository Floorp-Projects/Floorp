/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  createElement,
} = require("devtools/client/shared/vendor/react");
const { Provider } = require("devtools/client/shared/vendor/react-redux");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const LocalizationProvider = createFactory(FluentReact.LocalizationProvider);

const compatibilityReducer = require("devtools/client/inspector/compatibility/reducers/compatibility");
const {
  appendNode,
  clearDestroyedNodes,
  initUserSettings,
  removeNode,
  updateNodes,
  updateSelectedNode,
  updateTopLevelTarget,
  updateNode,
} = require("devtools/client/inspector/compatibility/actions/compatibility");

const CompatibilityApp = createFactory(
  require("devtools/client/inspector/compatibility/components/CompatibilityApp")
);

class CompatibilityView {
  constructor(inspector, window) {
    this.inspector = inspector;

    this.inspector.store.injectReducer("compatibility", compatibilityReducer);

    this._parseMarkup = this._parseMarkup.bind(this);
    this._onChangeAdded = this._onChangeAdded.bind(this);
    this._onPanelSelected = this._onPanelSelected.bind(this);
    this._onSelectedNodeChanged = this._onSelectedNodeChanged.bind(this);
    this._onTopLevelTargetChanged = this._onTopLevelTargetChanged.bind(this);
    this._onResourceAvailable = this._onResourceAvailable.bind(this);
    this._onMarkupMutation = this._onMarkupMutation.bind(this);

    this._init();
  }

  destroy() {
    try {
      this.resourceWatcher.unwatchResources(
        [this.resourceWatcher.TYPES.CSS_CHANGE],
        {
          onAvailable: this._onResourceAvailable,
        }
      );
    } catch (e) {
      // If unwatchResources is called before finishing process of watchResources,
      // unwatchResources throws an error during stopping listener.
    }

    this.inspector.off("new-root", this._onTopLevelTargetChanged);
    this.inspector.off("markupmutation", this._onMarkupMutation);
    this.inspector.selection.off("new-node-front", this._onSelectedNodeChanged);
    this.inspector.sidebar.off(
      "compatibilityview-selected",
      this._onPanelSelected
    );
    this.inspector = null;
  }

  get resourceWatcher() {
    return this.inspector.toolbox.resourceWatcher;
  }

  async _init() {
    const { setSelectedNode } = this.inspector.getCommonComponentProps();
    const compatibilityApp = new CompatibilityApp({
      setSelectedNode,
    });

    this.provider = createElement(
      Provider,
      {
        id: "compatibilityview",
        store: this.inspector.store,
      },
      LocalizationProvider(
        {
          bundles: this.inspector.fluentL10n.getBundles(),
          parseMarkup: this._parseMarkup,
        },
        compatibilityApp
      )
    );

    this.inspector.store.dispatch(initUserSettings());

    this.inspector.on("new-root", this._onTopLevelTargetChanged);
    this.inspector.on("markupmutation", this._onMarkupMutation);
    this.inspector.selection.on("new-node-front", this._onSelectedNodeChanged);
    this.inspector.sidebar.on(
      "compatibilityview-selected",
      this._onPanelSelected
    );

    await this.resourceWatcher.watchResources(
      [this.resourceWatcher.TYPES.CSS_CHANGE],
      {
        onAvailable: this._onResourceAvailable,
        // CSS changes made before opening Compatibility View are already applied to
        // corresponding DOM at this point, so existing resources can be ignored here.
        ignoreExistingResources: true,
      }
    );

    this.inspector.emitForTests("compatibilityview-initialized");
  }

  _isAvailable() {
    return (
      this.inspector &&
      this.inspector.sidebar &&
      this.inspector.sidebar.getCurrentTabID() === "compatibilityview" &&
      this.inspector.selection &&
      this.inspector.selection.isConnected()
    );
  }

  _parseMarkup(str) {
    // Using a BrowserLoader for the inspector is currently blocked on performance regressions,
    // see Bug 1471853.
    throw new Error(
      "The inspector cannot use tags in ftl strings because it does not run in a BrowserLoader"
    );
  }

  _onChangeAdded({ selector }) {
    if (!this._isAvailable()) {
      // In order to update this panel if a change is added while hiding this panel.
      this._isChangeAddedWhileHidden = true;
      return;
    }

    this._isChangeAddedWhileHidden = false;

    // We need to debounce updating nodes since "add-change" event on changes actor is
    // fired for every typed character until fixing bug 1503036.
    if (this._previousChangedSelector === selector) {
      clearTimeout(this._updateNodesTimeoutId);
    }
    this._previousChangedSelector = selector;

    this._updateNodesTimeoutId = setTimeout(() => {
      // TODO: In case of keyframes changes, the selector given from changes actor is
      // keyframe-selector such as "from" and "100%", not selector for node. Thus,
      // we need to address this case.
      this.inspector.store.dispatch(updateNodes(selector));
    }, 500);
  }

  _onMarkupMutation(mutations) {
    const attributeMutation = mutations.filter(
      mutation =>
        mutation.type === "attributes" &&
        (mutation.attributeName === "style" ||
          mutation.attributeName === "class")
    );
    const childListMutation = mutations.filter(
      mutation => mutation.type === "childList"
    );

    if (attributeMutation.length === 0 && childListMutation.length === 0) {
      return;
    }

    if (!this._isAvailable()) {
      // In order to update this panel if a change is added while hiding this panel.
      this._isChangeAddedWhileHidden = true;
      return;
    }

    this._isChangeAddedWhileHidden = false;

    // Resource Watcher doesn't respond to programmatic inline CSS
    // change. This check can be removed once the following bug is resolved
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1506160
    for (const { target } of attributeMutation) {
      this.inspector.store.dispatch(updateNode(target));
    }

    // Destroyed nodes can be cleaned up
    // once at the end if necessary
    let cleanupDestroyedNodes = false;
    for (const { removed, target } of childListMutation) {
      if (!removed.length) {
        this.inspector.store.dispatch(appendNode(target));
        continue;
      }

      const retainedNodes = removed.filter(node => node && !node.isDestroyed());
      cleanupDestroyedNodes =
        cleanupDestroyedNodes || retainedNodes.length !== removed.length;

      for (const retainedNode of retainedNodes) {
        this.inspector.store.dispatch(removeNode(retainedNode));
      }
    }

    if (cleanupDestroyedNodes) {
      this.inspector.store.dispatch(clearDestroyedNodes());
    }
  }

  _onPanelSelected() {
    const {
      selectedNode,
      topLevelTarget,
    } = this.inspector.store.getState().compatibility;

    // Update if the selected node is changed or new change is added while the panel was hidden.
    if (
      this.inspector.selection.nodeFront !== selectedNode ||
      this._isChangeAddedWhileHidden
    ) {
      this._onSelectedNodeChanged();
    }

    // Update if the top target has changed or new change is added while the panel was hidden.
    if (
      this.inspector.toolbox.target !== topLevelTarget ||
      this._isChangeAddedWhileHidden
    ) {
      this._onTopLevelTargetChanged();
    }

    this._isChangeAddedWhileHidden = false;
  }

  _onSelectedNodeChanged() {
    if (!this._isAvailable()) {
      return;
    }

    this.inspector.store.dispatch(
      updateSelectedNode(this.inspector.selection.nodeFront)
    );
  }

  _onResourceAvailable(resources) {
    for (const resource of resources) {
      // Style changes applied inline directly to
      // the element and its changes are monitored by
      // _onMarkupMutation via markupmutation events.
      // Hence those changes can be ignored here
      if (resource.source?.type !== "element") {
        this._onChangeAdded(resource);
      }
    }
  }

  _onTopLevelTargetChanged() {
    if (!this._isAvailable()) {
      return;
    }

    this.inspector.store.dispatch(
      updateTopLevelTarget(this.inspector.toolbox.target)
    );
  }
}

module.exports = CompatibilityView;
