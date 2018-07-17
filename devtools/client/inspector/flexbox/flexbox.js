/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { throttle } = require("devtools/client/inspector/shared/utils");

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
    this.store = inspector.store;
    this.walker = inspector.walker;

    this.onHighlighterShown = this.onHighlighterShown.bind(this);
    this.onHighlighterHidden = this.onHighlighterHidden.bind(this);
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

    this.document.addEventListener("mousemove", () => {
      this.highlighters.on("flexbox-highlighter-hidden", this.onHighlighterHidden);
      this.highlighters.on("flexbox-highlighter-shown", this.onHighlighterShown);
    }, { once: true });

    this.inspector.sidebar.on("select", this.onSidebarSelect);

    this.onSidebarSelect();
  }

  destroy() {
    if (this._highlighters) {
      this.highlighters.off("flexbox-highlighter-hidden", this.onHighlighterHidden);
      this.highlighters.off("flexbox-highlighter-shown", this.onHighlighterShown);
    }

    this.inspector.selection.off("new-node-front", this.onUpdatePanel);
    this.inspector.sidebar.off("select", this.onSidebarSelect);
    this.inspector.off("new-root", this.onUpdatePanel);

    this.inspector.reflowTracker.untrackReflows(this, this.onReflow);

    this._highlighters = null;
    this.document = null;
    this.hasGetCurrentFlexbox = null;
    this.inspector = null;
    this.layoutInspector = null;
    this.store = null;
    this.walker = null;
  }

  getComponentProps() {
    return {
      getSwatchColorPickerTooltip: this.getSwatchColorPickerTooltip,
      onSetFlexboxOverlayColor: this.onSetFlexboxOverlayColor,
      onToggleFlexboxHighlighter: this.onToggleFlexboxHighlighter,
    };
  }

  /**
   * Returns an object containing the custom flexbox colors for different hosts.
   *
   * @return {Object} that maps a host name to a custom flexbox color for a given host.
   */
  async getCustomFlexboxColors() {
    return await asyncStorage.getItem("flexboxInspectorHostColors") || {};
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

    if (flexbox.nodeFront === nodeFront && flexbox.highlighted !== highlighted) {
      this.store.dispatch(updateFlexboxHighlighted(highlighted));
    }
  }

  /**
   * Handler for the "reflow" event fired by the inspector's reflow tracker. On reflows,
   * updates the flexbox panel because the shape of the flexbox on the page may have
   * changed.
   *
   * TODO: In the future, we will want to compare the flex item fragment data returned
   * for rendering the flexbox outline.
   */
  async onReflow() {
    if (!this.isPanelVisible() || !this.store || !this.inspector.selection.nodeFront) {
      return;
    }

    const { flexbox } = this.store.getState();

    let flexboxFront;
    try {
      if (!this.hasGetCurrentFlexbox) {
        return;
      }

      flexboxFront = await this.layoutInspector.getCurrentFlexbox(
        this.inspector.selection.nodeFront);
    } catch (e) {
      // This call might fail if called asynchrously after the toolbox is finished
      // closing.
      return;
    }

    // Clear the flexbox panel if there is no flex container for the current node
    // selection.
    if (!flexboxFront) {
      this.store.dispatch(clearFlexbox());
      return;
    }

    // Do nothing because the same flex container is still selected.
    if (flexbox.actorID == flexboxFront.actorID) {
      return;
    }

    // Update the flexbox panel with the new flexbox front contents.
    this.update(flexboxFront);
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
      this.highlighters.showFlexboxHighlighter(flexbox.nodeFront);
    }

    const currentUrl = this.inspector.target.url;
    // Get the hostname, if there is no hostname, fall back on protocol
    // ex: `data:` uri, and `about:` pages
    const hostname = parseURL(currentUrl).hostname || parseURL(currentUrl).protocol;
    const customFlexboxColors = await this.getCustomFlexboxColors();

    customFlexboxColors[hostname] = color;
    await asyncStorage.setItem("flexboxInspectorHostColors", customFlexboxColors);
  }

  /**
   * Handler for the inspector sidebar "select" event. Updates the flexbox panel if it
   * is visible.
   */
  onSidebarSelect() {
    if (!this.isPanelVisible()) {
      this.inspector.reflowTracker.untrackReflows(this, this.onReflow);
      this.inspector.selection.off("new-node-front", this.onUpdatePanel);
      this.inspector.off("new-root", this.onUpdatePanel);
      return;
    }

    this.inspector.reflowTracker.trackReflows(this, this.onReflow);
    this.inspector.selection.on("new-node-front", this.onUpdatePanel);
    this.inspector.on("new-root", this.onUpdatePanel);

    this.update();
  }

  /**
   * Handler for a change in the input checkboxes in the FlexboxItem component.
   * Toggles on/off the flexbox highlighter for the provided flex container element.
   *
   * @param  {NodeFront} node
   *         The NodeFront of the flexb container element for which the flexbox
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
   * @param  {FlexboxFront|Null} flexboxFront
   *         The FlexboxFront of the flex container for the current node selection.
   */
  async update(flexboxFront) {
    // Stop refreshing if the inspector or store is already destroyed or no node is
    // selected.
    if (!this.inspector || !this.store || !this.inspector.selection.nodeFront) {
      return;
    }

    // Fetch the current flexbox if no flexbox front was passed into this update.
    if (!flexboxFront) {
      try {
        if (!this.hasGetCurrentFlexbox) {
          return;
        }

        flexboxFront = await this.layoutInspector.getCurrentFlexbox(
          this.inspector.selection.nodeFront);
      } catch (e) {
        // This call might fail if called asynchrously after the toolbox is finished
        // closing.
        return;
      }
    }

    // Clear the flexbox panel if there is no flex container for the current node
    // selection.
    if (!flexboxFront) {
      this.store.dispatch(clearFlexbox());
      return;
    }

    let nodeFront = flexboxFront.containerNodeFront;

    // If the FlexboxFront doesn't yet have access to the NodeFront for its container,
    // then get it from the walker. This happens when the walker hasn't seen this
    // particular DOM Node in the tree yet or when we are connected to an older server.
    if (!nodeFront) {
      try {
        nodeFront = await this.walker.getNodeFromActor(flexboxFront.actorID,
          ["containerEl"]);
      } catch (e) {
        // This call might fail if called asynchrously after the toolbox is finished
        // closing.
        return;
      }
    }

    const highlighted = this._highlighters &&
      nodeFront == this.highlighters.flexboxHighlighterShown;

    const currentUrl = this.inspector.target.url;
    // Get the hostname, if there is no hostname, fall back on protocol
    // ex: `data:` uri, and `about:` pages
    const hostname = parseURL(currentUrl).hostname || parseURL(currentUrl).protocol;
    const customColors = await this.getCustomFlexboxColors();
    const color = customColors[hostname] ? customColors[hostname] : FLEXBOX_COLOR;

    this.store.dispatch(updateFlexbox({
      actorID: flexboxFront.actorID,
      color,
      highlighted,
      nodeFront,
    }));
  }
}

module.exports = FlexboxInspector;
