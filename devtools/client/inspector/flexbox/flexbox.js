/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { throttle } = require("resource://devtools/shared/throttle.js");

const {
  clearFlexbox,
  updateFlexbox,
  updateFlexboxColor,
  updateFlexboxHighlighted,
} = require("resource://devtools/client/inspector/flexbox/actions/flexbox.js");
const flexboxReducer = require("resource://devtools/client/inspector/flexbox/reducers/flexbox.js");

loader.lazyRequireGetter(
  this,
  "parseURL",
  "resource://devtools/client/shared/source-utils.js",
  true
);
loader.lazyRequireGetter(
  this,
  "asyncStorage",
  "resource://devtools/shared/async-storage.js"
);

const FLEXBOX_COLOR = "#9400FF";

class FlexboxInspector {
  constructor(inspector, window) {
    this.document = window.document;
    this.inspector = inspector;
    this.selection = inspector.selection;
    this.store = inspector.store;

    this.store.injectReducer("flexbox", flexboxReducer);

    this.onHighlighterShown = this.onHighlighterShown.bind(this);
    this.onHighlighterHidden = this.onHighlighterHidden.bind(this);
    this.onNavigate = this.onNavigate.bind(this);
    this.onReflow = throttle(this.onReflow, 500, this);
    this.onSetFlexboxOverlayColor = this.onSetFlexboxOverlayColor.bind(this);
    this.onSidebarSelect = this.onSidebarSelect.bind(this);
    this.onUpdatePanel = this.onUpdatePanel.bind(this);

    this.init();
  }

  init() {
    if (!this.inspector) {
      return;
    }

    this.inspector.highlighters.on(
      "highlighter-shown",
      this.onHighlighterShown
    );
    this.inspector.highlighters.on(
      "highlighter-hidden",
      this.onHighlighterHidden
    );
    this.inspector.sidebar.on("select", this.onSidebarSelect);

    this.onSidebarSelect();
  }

  destroy() {
    this.selection.off("new-node-front", this.onUpdatePanel);
    this.inspector.off("new-root", this.onNavigate);
    this.inspector.off("reflow-in-selected-target", this.onReflow);
    this.inspector.highlighters.off(
      "highlighter-shown",
      this.onHighlighterShown
    );
    this.inspector.highlighters.off(
      "highlighter-hidden",
      this.onHighlighterHidden
    );
    this.inspector.sidebar.off("select", this.onSidebarSelect);

    this._customHostColors = null;
    this._overlayColor = null;
    this.document = null;
    this.inspector = null;
    this.selection = null;
    this.store = null;
  }

  getComponentProps() {
    return {
      onSetFlexboxOverlayColor: this.onSetFlexboxOverlayColor,
    };
  }

  /**
   * Returns an object containing the custom flexbox colors for different hosts.
   *
   * @return {Object} that maps a host name to a custom flexbox color for a given host.
   */
  async getCustomHostColors() {
    if (this._customHostColors) {
      return this._customHostColors;
    }

    // Cache the custom host colors to avoid refetching from async storage.
    this._customHostColors =
      (await asyncStorage.getItem("flexboxInspectorHostColors")) || {};
    return this._customHostColors;
  }

  /**
   * Returns the flex container properties for a given node. If the given node is a flex
   * item, it attempts to fetch the flex container of the parent node of the given node.
   *
   * @param  {NodeFront} nodeFront
   *         The NodeFront to fetch the flex container properties.
   * @param  {Boolean} onlyLookAtParents
   *         Whether or not to only consider the parent node of the given node.
   * @return {Object} consisting of the given node's flex container's properties.
   */
  async getFlexContainerProps(nodeFront, onlyLookAtParents = false) {
    const layoutFront = await nodeFront.walkerFront.getLayoutInspector();
    const flexboxFront = await layoutFront.getCurrentFlexbox(
      nodeFront,
      onlyLookAtParents
    );

    if (!flexboxFront) {
      return null;
    }

    // If the FlexboxFront doesn't yet have access to the NodeFront for its container,
    // then get it from the walker. This happens when the walker hasn't seen this
    // particular DOM Node in the tree yet or when we are connected to an older server.
    let containerNodeFront = flexboxFront.containerNodeFront;
    if (!containerNodeFront) {
      containerNodeFront = await flexboxFront.walkerFront.getNodeFromActor(
        flexboxFront.actorID,
        ["containerEl"]
      );
    }

    const flexItems = await this.getFlexItems(flexboxFront);

    // If the current selected node is a flex item, display its flex item sizing
    // properties.
    let flexItemShown = null;
    if (onlyLookAtParents) {
      flexItemShown = this.selection.nodeFront.actorID;
    } else {
      const selectedFlexItem = flexItems.find(
        item => item.nodeFront === this.selection.nodeFront
      );
      if (selectedFlexItem) {
        flexItemShown = selectedFlexItem.nodeFront.actorID;
      }
    }

    return {
      actorID: flexboxFront.actorID,
      flexItems,
      flexItemShown,
      isFlexItemContainer: onlyLookAtParents,
      nodeFront: containerNodeFront,
      properties: flexboxFront.properties,
    };
  }

  /**
   * Returns an array of flex items object for the given flex container front.
   *
   * @param  {FlexboxFront} flexboxFront
   *         A flex container FlexboxFront.
   * @return {Array} of objects containing the flex item front properties.
   */
  async getFlexItems(flexboxFront) {
    const flexItemFronts = await flexboxFront.getFlexItems();
    const flexItems = [];

    for (const flexItemFront of flexItemFronts) {
      // Fetch the NodeFront of the flex items.
      let itemNodeFront = flexItemFront.nodeFront;
      if (!itemNodeFront) {
        itemNodeFront = await flexItemFront.walkerFront.getNodeFromActor(
          flexItemFront.actorID,
          ["element"]
        );
      }

      flexItems.push({
        actorID: flexItemFront.actorID,
        computedStyle: flexItemFront.computedStyle,
        flexItemSizing: flexItemFront.flexItemSizing,
        nodeFront: itemNodeFront,
        properties: flexItemFront.properties,
      });
    }

    return flexItems;
  }

  /**
   * Returns the custom overlay color for the current host or the default flexbox color.
   *
   * @return {String} overlay color.
   */
  async getOverlayColor() {
    if (this._overlayColor) {
      return this._overlayColor;
    }

    // Cache the overlay color for the current host to avoid repeatably parsing the host
    // and fetching the custom color from async storage.
    const customColors = await this.getCustomHostColors();
    const currentUrl = this.inspector.currentTarget.url;
    // Get the hostname, if there is no hostname, fall back on protocol
    // ex: `data:` uri, and `about:` pages
    const hostname =
      parseURL(currentUrl).hostname || parseURL(currentUrl).protocol;
    this._overlayColor = customColors[hostname]
      ? customColors[hostname]
      : FLEXBOX_COLOR;
    return this._overlayColor;
  }

  /**
   * Returns true if the layout panel is visible, and false otherwise.
   */
  isPanelVisible() {
    return (
      this.inspector &&
      this.inspector.toolbox &&
      this.inspector.sidebar &&
      this.inspector.toolbox.currentToolId === "inspector" &&
      this.inspector.sidebar.getCurrentTabID() === "layoutview"
    );
  }

  /**
   * Handler for "highlighter-shown" events emitted by HighlightersOverlay.
   * If the event is dispatched on behalf of a flex highlighter, toggle the
   * corresponding flex container's highlighted state in the Redux store.
   *
   * @param {Object} data
   *        Object with data associated with the highlighter event.
   *        {NodeFront} data.nodeFront
   *        The NodeFront of the flex container element for which the flexbox
   *        highlighter is shown for.
   *        {String} data.type
   *        Highlighter type
   */
  onHighlighterShown(data) {
    if (data.type === this.inspector.highlighters.TYPES.FLEXBOX) {
      this.onHighlighterChange(true, data.nodeFront);
    }
  }

  /**
   * Handler for "highlighter-shown" events emitted by HighlightersOverlay.
   * If the event is dispatched on behalf of a flex highlighter, toggle the
   * corresponding flex container's highlighted state in the Redux store.
   *
   * @param {Object} data
   *        Object with data associated with the highlighter event.
   *        {NodeFront} data.nodeFront
   *        The NodeFront of the flex container element for which the flexbox
   *        highlighter was previously shown for.
   *        {String} data.type
   *        Highlighter type
   */
  onHighlighterHidden(data) {
    if (data.type === this.inspector.highlighters.TYPES.FLEXBOX) {
      this.onHighlighterChange(false, data.nodeFront);
    }
  }

  /**
   * Updates the flex container highlighted state in the Redux store if the provided
   * NodeFront is the current selected flex container.
   *
   * @param  {Boolean} highlighted
   *         Whether the change is to highlight or hide the overlay.
   * @param  {NodeFront} nodeFront
   *         The NodeFront of the flex container element for which the flexbox
   *         highlighter is shown for.
   */
  onHighlighterChange(highlighted, nodeFront) {
    const { flexbox } = this.store.getState();

    if (
      flexbox.flexContainer.nodeFront === nodeFront &&
      flexbox.highlighted !== highlighted
    ) {
      this.store.dispatch(updateFlexboxHighlighted(highlighted));
    }
  }

  /**
   * Handler for the "new-root" event fired by the inspector. Clears the cached overlay
   * color for the flexbox highlighter and updates the panel.
   */
  onNavigate() {
    this._overlayColor = null;
    this.onUpdatePanel();
  }

  /**
   * Handler for reflow events fired by the inspector when a node is selected. On reflows,
   * update the flexbox panel because the shape of the flexbox on the page may have
   * changed.
   */
  async onReflow() {
    if (
      !this.isPanelVisible() ||
      !this.store ||
      !this.selection.nodeFront ||
      this._isUpdating
    ) {
      return;
    }

    try {
      const flexContainer = await this.getFlexContainerProps(
        this.selection.nodeFront
      );

      // Clear the flexbox panel if there is no flex container for the current node
      // selection.
      if (!flexContainer) {
        this.store.dispatch(clearFlexbox());
        return;
      }

      const { flexbox } = this.store.getState();

      // Compare the new flexbox state of the current selected nodeFront with the old
      // flexbox state to determine if we need to update.
      if (hasFlexContainerChanged(flexbox.flexContainer, flexContainer)) {
        this.update(flexContainer);
        return;
      }

      let flexItemContainer = null;
      // If the current selected node is also the flex container node, check if it is
      // a flex item of a parent flex container.
      if (flexContainer.nodeFront === this.selection.nodeFront) {
        flexItemContainer = await this.getFlexContainerProps(
          this.selection.nodeFront,
          true
        );
      }

      // Compare the new and old state of the parent flex container properties.
      if (
        hasFlexContainerChanged(flexbox.flexItemContainer, flexItemContainer)
      ) {
        this.update(flexContainer, flexItemContainer);
      }
    } catch (e) {
      // This call might fail if called asynchrously after the toolbox is finished
      // closing.
    }
  }

  /**
   * Handler for a change in the flexbox overlay color picker for a flex container.
   *
   * @param  {String} color
   *         A hex string representing the color to use.
   */
  async onSetFlexboxOverlayColor(color) {
    this.store.dispatch(updateFlexboxColor(color));

    const { flexbox } = this.store.getState();

    if (flexbox.highlighted) {
      this.inspector.highlighters.showFlexboxHighlighter(
        flexbox.flexContainer.nodeFront
      );
    }

    this._overlayColor = color;

    const currentUrl = this.inspector.currentTarget.url;
    // Get the hostname, if there is no hostname, fall back on protocol
    // ex: `data:` uri, and `about:` pages
    const hostname =
      parseURL(currentUrl).hostname || parseURL(currentUrl).protocol;
    const customColors = await this.getCustomHostColors();
    customColors[hostname] = color;
    this._customHostColors = customColors;
    await asyncStorage.setItem("flexboxInspectorHostColors", customColors);
  }

  /**
   * Handler for the inspector sidebar "select" event. Updates the flexbox panel if it
   * is visible.
   */
  onSidebarSelect() {
    if (!this.isPanelVisible()) {
      this.inspector.off("reflow-in-selected-target", this.onReflow);
      this.inspector.off("new-root", this.onNavigate);
      this.selection.off("new-node-front", this.onUpdatePanel);
      return;
    }

    this.inspector.on("reflow-in-selected-target", this.onReflow);
    this.inspector.on("new-root", this.onNavigate);
    this.selection.on("new-node-front", this.onUpdatePanel);

    this.update();
  }

  /**
   * Handler for "new-root" event fired by the inspector and "new-node-front" event fired
   * by the inspector selection. Updates the flexbox panel if it is visible.
   *
   * @param  {Object}
   *         This callback is sometimes executed on "new-node-front" events which means
   *         that a first param is passed here (the nodeFront), which we don't care about.
   * @param  {String} reason
   *         On "new-node-front" events, a reason is passed here, and we need it to detect
   *         if this update was caused by a node selection from the markup-view.
   */
  onUpdatePanel(_, reason) {
    if (!this.isPanelVisible()) {
      return;
    }

    this.update(null, null, reason === "treepanel");
  }

  /**
   * Updates the flexbox panel by dispatching the new flexbox data. This is called when
   * the layout view becomes visible or a new node is selected and needs to be update
   * with new flexbox data.
   *
   * @param  {Object|null} flexContainer
   *         An object consisting of the current flex container's flex items and
   *         properties.
   * @param  {Object|null} flexItemContainer
   *         An object consisting of the parent flex container's flex items and
   *         properties.
   * @param  {Boolean} initiatedByMarkupViewSelection
   *         True if the update was due to a node selection in the markup-view.
   */
  async update(
    flexContainer,
    flexItemContainer,
    initiatedByMarkupViewSelection
  ) {
    this._isUpdating = true;

    // Stop refreshing if the inspector or store is already destroyed or no node is
    // selected.
    if (!this.inspector || !this.store || !this.selection.nodeFront) {
      this._isUpdating = false;
      return;
    }

    try {
      // Fetch the current flexbox if no flexbox front was passed into this update.
      if (!flexContainer) {
        flexContainer = await this.getFlexContainerProps(
          this.selection.nodeFront
        );
      }

      // Clear the flexbox panel if there is no flex container for the current node
      // selection.
      if (!flexContainer) {
        this.store.dispatch(clearFlexbox());
        this._isUpdating = false;
        return;
      }

      if (
        !flexItemContainer &&
        flexContainer.nodeFront === this.selection.nodeFront
      ) {
        flexItemContainer = await this.getFlexContainerProps(
          this.selection.nodeFront,
          true
        );
      }

      const highlighted =
        flexContainer.nodeFront ===
        this.inspector.highlighters.getNodeForActiveHighlighter(
          this.inspector.highlighters.TYPES.FLEXBOX
        );
      const color = await this.getOverlayColor();

      this.store.dispatch(
        updateFlexbox({
          color,
          flexContainer,
          flexItemContainer,
          highlighted,
          initiatedByMarkupViewSelection,
        })
      );
    } catch (e) {
      // This call might fail if called asynchrously after the toolbox is finished
      // closing.
    }

    this._isUpdating = false;
  }
}

/**
 * For a given flex container object, returns the flex container properties that can be
 * used to check if 2 flex container objects are the same.
 *
 * @param  {Object|null} flexContainer
 *         Object consisting of the flex container's properties.
 * @return {Object|null} consisting of the comparable flex container's properties.
 */
function getComparableFlexContainerProperties(flexContainer) {
  if (!flexContainer) {
    return null;
  }

  return {
    flexItems: getComparableFlexItemsProperties(flexContainer.flexItems),
    nodeFront: flexContainer.nodeFront.actorID,
    properties: flexContainer.properties,
  };
}

/**
 * Given an array of flex item objects, returns the relevant flex item properties that can
 * be compared to check if any changes has occurred.
 *
 * @param  {Array} flexItems
 *         Array of objects containing the flex item properties.
 * @return {Array} of objects consisting of the comparable flex item's properties.
 */
function getComparableFlexItemsProperties(flexItems) {
  return flexItems.map(item => {
    return {
      computedStyle: item.computedStyle,
      flexItemSizing: item.flexItemSizing,
      nodeFront: item.nodeFront.actorID,
      properties: item.properties,
    };
  });
}

/**
 * Compares the old and new flex container properties
 *
 * @param  {Object} oldFlexContainer
 *         Object consisting of the old flex container's properties.
 * @param  {Object} newFlexContainer
 *         Object consisting of the new flex container's properties.
 * @return {Boolean} true if the flex container properties are the same, false otherwise.
 */
function hasFlexContainerChanged(oldFlexContainer, newFlexContainer) {
  return (
    JSON.stringify(getComparableFlexContainerProperties(oldFlexContainer)) !==
    JSON.stringify(getComparableFlexContainerProperties(newFlexContainer))
  );
}

module.exports = FlexboxInspector;
