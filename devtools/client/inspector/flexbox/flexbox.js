/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const flags = require("devtools/shared/flags");
const { throttle } = require("devtools/shared/throttle");

const {
  clearFlexbox,
  updateFlexbox,
  updateFlexboxColor,
  updateFlexboxHighlighted,
} = require("./actions/flexbox");

loader.lazyRequireGetter(this, "parseURL", "devtools/client/shared/source-utils", true);
loader.lazyRequireGetter(this, "asyncStorage", "devtools/shared/async-storage");

const FLEXBOX_COLOR = "#9400FF";

class FlexboxInspector {
  constructor(inspector, window) {
    this.document = window.document;
    this.inspector = inspector;
    this.selection = inspector.selection;
    this.store = inspector.store;
    this.walker = inspector.walker;

    this.onHighlighterShown = this.onHighlighterShown.bind(this);
    this.onHighlighterHidden = this.onHighlighterHidden.bind(this);
    this.onNavigate = this.onNavigate.bind(this);
    this.onReflow = throttle(this.onReflow, 500, this);
    this.onSetFlexboxOverlayColor = this.onSetFlexboxOverlayColor.bind(this);
    this.onSidebarSelect = this.onSidebarSelect.bind(this);
    this.onToggleFlexboxHighlighter = this.onToggleFlexboxHighlighter.bind(this);
    this.onUpdatePanel = this.onUpdatePanel.bind(this);

    this.init();
  }

  // Get the highlighters overlay from the Inspector.
  get highlighters() {
    if (!this._highlighters) {
      // highlighters is a lazy getter in the inspector.
      this._highlighters = this.inspector.highlighters;
    }

    return this._highlighters;
  }

  async init() {
    if (!this.inspector) {
      return;
    }
    try {
      this.hasGetCurrentFlexbox = await this.inspector.target.actorHasMethod("layout",
        "getCurrentFlexbox");
      this.layoutInspector = await this.walker.getLayoutInspector();
    } catch (e) {
      // These calls might fail if called asynchrously after the toolbox is finished
      // closing.
      return;
    }

    if (flags.testing) {
      // In tests, we start listening immediately to avoid having to simulate a mousemove.
      this.highlighters.on("flexbox-highlighter-hidden", this.onHighlighterHidden);
      this.highlighters.on("flexbox-highlighter-shown", this.onHighlighterShown);
    } else {
      this.document.addEventListener("mousemove", () => {
        this.highlighters.on("flexbox-highlighter-hidden", this.onHighlighterHidden);
        this.highlighters.on("flexbox-highlighter-shown", this.onHighlighterShown);
      }, { once: true });
    }

    this.inspector.sidebar.on("select", this.onSidebarSelect);

    this.onSidebarSelect();
  }

  destroy() {
    if (this._highlighters) {
      this.highlighters.off("flexbox-highlighter-hidden", this.onHighlighterHidden);
      this.highlighters.off("flexbox-highlighter-shown", this.onHighlighterShown);
    }

    this.selection.off("new-node-front", this.onUpdatePanel);
    this.inspector.sidebar.off("select", this.onSidebarSelect);
    this.inspector.off("new-root", this.onNavigate);

    this.inspector.reflowTracker.untrackReflows(this, this.onReflow);

    this._customHostColors = null;
    this._highlighters = null;
    this._overlayColor = null;
    this.document = null;
    this.hasGetCurrentFlexbox = null;
    this.inspector = null;
    this.layoutInspector = null;
    this.selection = null;
    this.store = null;
    this.walker = null;
  }

  /**
   * If the current selected node is a flex container, check if it is also a flex item of
   * a parent flex container and get its parent flex container if any and returns an
   * object that consists of the parent flex container's items and properties.
   *
   * @param  {NodeFront} containerNodeFront
   *         The current flex container of the selected node.
   * @return {Object} consiting of the parent flex container's flex items and properties.
   */
  async getAsFlexItem(containerNodeFront) {
    // If the current selected node is not a flex container, we know it is a flex item.
    // No need to look for the parent flex container.
    if (containerNodeFront !== this.selection.nodeFront) {
      return null;
    }

    const flexboxFront = await this.layoutInspector.getCurrentFlexbox(
      this.selection.nodeFront, true);

    if (!flexboxFront) {
      return null;
    }

    containerNodeFront = flexboxFront.containerNodeFront;
    if (!containerNodeFront) {
      containerNodeFront = await this.walker.getNodeFromActor(flexboxFront.actorID,
        ["containerEl"]);
    }

    let flexItemContainer = null;
    if (flexboxFront) {
      const flexItems = await this.getFlexItems(flexboxFront);
      flexItemContainer = {
        actorID: flexboxFront.actorID,
        flexItems,
        flexItemShown: this.selection.nodeFront.actorID,
        isFlexItemContainer: true,
        nodeFront: containerNodeFront,
        properties: flexboxFront.properties,
      };
    }

    return flexItemContainer;
  }

  getComponentProps() {
    return {
      onSetFlexboxOverlayColor: this.onSetFlexboxOverlayColor,
      onToggleFlexboxHighlighter: this.onToggleFlexboxHighlighter,
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
    this._customHostColors = await asyncStorage.getItem("flexboxInspectorHostColors")
      || {};
    return this._customHostColors;
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
        itemNodeFront = await this.walker.getNodeFromActor(flexItemFront.actorID,
          ["element"]);
      }

      flexItems.push({
        actorID: flexItemFront.actorID,
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
    const currentUrl = this.inspector.target.url;
    // Get the hostname, if there is no hostname, fall back on protocol
    // ex: `data:` uri, and `about:` pages
    const hostName = parseURL(currentUrl).hostname || parseURL(currentUrl).protocol;
    this._overlayColor = customColors[hostName] ? customColors[hostName] : FLEXBOX_COLOR;
    return this._overlayColor;
  }

  /**
   * Returns true if the layout panel is visible, and false otherwise.
   */
  isPanelVisible() {
    return this.inspector && this.inspector.toolbox && this.inspector.sidebar &&
           this.inspector.toolbox.currentToolId === "inspector" &&
           this.inspector.sidebar.getCurrentTabID() === "layoutview";
  }
  /**
   * Handler for "flexbox-highlighter-shown" events emitted from the
   * HighlightersOverlay. Passes nodefront and highlight status to
   * handleHighlighterChange.  Required since on and off events need
   * the same reference object.
   *
   * @param  {NodeFront} nodeFront
   *         The NodeFront of the flex container element for which the flexbox
   *         highlighter is shown for.
   */
  onHighlighterShown(nodeFront) {
    return this.onHighlighterChange(true, nodeFront);
  }

  /**
   * Handler for "flexbox-highlighter-hidden" events emitted from the
   * HighlightersOverlay. Passes nodefront and highlight status to
   * handleHighlighterChange.  Required since on and off events need
   * the same reference object.
   *
   * @param  {NodeFront} nodeFront
   *         The NodeFront of the flex container element for which the flexbox
   *         highlighter is shown for.
   */
  onHighlighterHidden(nodeFront) {
    return this.onHighlighterChange(false, nodeFront);
  }

  /**
   * Handler for "flexbox-highlighter-shown" and "flexbox-highlighter-hidden" events
   * emitted from the HighlightersOverlay. Updates the flex container highlighted state
   * only if the provided NodeFront is the current selected flex container.
   *
   * @param  {Boolean} highlighted
   *          If the change is to highlight or hide the overlay.
   * @param  {NodeFront} nodeFront
   *         The NodeFront of the flex container element for which the flexbox
   *         highlighter is shown for.
   */
  onHighlighterChange(highlighted, nodeFront) {
    const { flexbox } = this.store.getState();

    if (flexbox.flexContainer.nodeFront === nodeFront &&
        flexbox.highlighted !== highlighted) {
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
   * Handler for the "reflow" event fired by the inspector's reflow tracker. On reflows,
   * updates the flexbox panel because the shape of the flexbox on the page may have
   * changed.
   */
  async onReflow() {
    if (!this.isPanelVisible() ||
        !this.store ||
        !this.selection.nodeFront ||
        !this.hasGetCurrentFlexbox) {
      return;
    }

    try {
      const flexboxFront = await this.layoutInspector.getCurrentFlexbox(
        this.selection.nodeFront);

      // Clear the flexbox panel if there is no flex container for the current node
      // selection.
      if (!flexboxFront) {
        this.store.dispatch(clearFlexbox());
        return;
      }

      const { flexbox } = this.store.getState();

      // Do nothing because the same flex container is still selected.
      if (flexbox.actorID == flexboxFront.actorID) {
        return;
      }

      // Update the flexbox panel with the new flexbox front contents.
      this.update(flexboxFront);
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
      this.highlighters.showFlexboxHighlighter(flexbox.flexContainer.nodeFront);
    }

    const currentUrl = this.inspector.target.url;
    // Get the hostname, if there is no hostname, fall back on protocol
    // ex: `data:` uri, and `about:` pages
    const hostName = parseURL(currentUrl).hostName || parseURL(currentUrl).protocol;
    const customColors = await this.getCustomHostColors();
    customColors[hostName] = color;
    this._customHostColors = customColors;
    await asyncStorage.setItem("flexboxInspectorHostColors", customColors);
  }

  /**
   * Handler for the inspector sidebar "select" event. Updates the flexbox panel if it
   * is visible.
   */
  onSidebarSelect() {
    if (!this.isPanelVisible()) {
      this.inspector.reflowTracker.untrackReflows(this, this.onReflow);
      this.inspector.off("new-root", this.onNavigate);
      this.selection.off("new-node-front", this.onUpdatePanel);
      return;
    }

    this.inspector.reflowTracker.trackReflows(this, this.onReflow);
    this.inspector.on("new-root", this.onNavigate);
    this.selection.on("new-node-front", this.onUpdatePanel);

    this.update();
  }

  /**
   * Handler for a change in the input checkboxes in the FlexboxItem component.
   * Toggles on/off the flexbox highlighter for the provided flex container element.
   *
   * @param  {NodeFront} node
   *         The NodeFront of the flex container element for which the flexbox
   *         highlighter is toggled on/off for.
   */
  onToggleFlexboxHighlighter(node) {
    this.highlighters.toggleFlexboxHighlighter(node);
    this.store.dispatch(updateFlexboxHighlighted(node !==
      this.highlighters.flexboxHighlighterShow));
  }

  /**
   * Handler for "new-root" event fired by the inspector and "new-node-front" event fired
   * by the inspector selection. Updates the flexbox panel if it is visible.
   */
  onUpdatePanel() {
    if (!this.isPanelVisible()) {
      return;
    }

    this.update();
  }

  /**
   * Updates the flexbox panel by dispatching the new flexbox data. This is called when
   * the layout view becomes visible or a new node is selected and needs to be update
   * with new flexbox data.
   *
   * @param  {FlexboxFront|null} flexboxFront
   *         The FlexboxFront of the flex container for the current node selection.
   */
  async update(flexboxFront) {
    // Stop refreshing if the inspector or store is already destroyed or no node is
    // selected.
    if (!this.inspector ||
        !this.store ||
        !this.selection.nodeFront ||
        !this.hasGetCurrentFlexbox) {
      return;
    }

    try {
      // Fetch the current flexbox if no flexbox front was passed into this update.
      if (!flexboxFront) {
        flexboxFront = await this.layoutInspector.getCurrentFlexbox(
          this.selection.nodeFront);
      }

      // Clear the flexbox panel if there is no flex container for the current node
      // selection.
      if (!flexboxFront) {
        this.store.dispatch(clearFlexbox());
        return;
      }

      // If the FlexboxFront doesn't yet have access to the NodeFront for its container,
      // then get it from the walker. This happens when the walker hasn't seen this
      // particular DOM Node in the tree yet or when we are connected to an older server.
      let containerNodeFront = flexboxFront.containerNodeFront;
      if (!containerNodeFront) {
        containerNodeFront = await this.walker.getNodeFromActor(flexboxFront.actorID,
          ["containerEl"]);
      }

      const flexItemContainer = await this.getAsFlexItem(containerNodeFront);
      const flexItems = await this.getFlexItems(flexboxFront);
      // If the current selected node is a flex item, display its flex item sizing
      // properties.
      const flexItemShown = flexItems.find(item =>
        item.nodeFront === this.selection.nodeFront);
      const highlighted = this._highlighters &&
        containerNodeFront == this.highlighters.flexboxHighlighterShown;
      const color = await this.getOverlayColor();

      this.store.dispatch(updateFlexbox({
        color,
        flexContainer: {
          actorID: flexboxFront.actorID,
          flexItems,
          flexItemShown: flexItemShown ? flexItemShown.nodeFront.actorID : null,
          isFlexItemContainer: false,
          nodeFront: containerNodeFront,
          properties: flexboxFront.properties,
        },
        flexItemContainer,
        highlighted,
      }));
    } catch (e) {
      // This call might fail if called asynchrously after the toolbox is finished
      // closing.
    }
  }
}

module.exports = FlexboxInspector;
