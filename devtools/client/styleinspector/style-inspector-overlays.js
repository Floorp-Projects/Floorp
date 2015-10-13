/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// The style-inspector overlays are:
// - tooltips that appear when hovering over property values
// - editor tooltips that appear when clicking color swatches, etc.
// - in-content highlighters that appear when hovering over property values
// - etc.

const {Cc, Ci, Cu} = require("chrome");
const {
  Tooltip,
  SwatchColorPickerTooltip,
  SwatchCubicBezierTooltip,
  CssDocsTooltip,
  SwatchFilterTooltip
} = require("devtools/client/shared/widgets/Tooltip");
const EventEmitter = require("devtools/shared/event-emitter");
const promise = require("promise");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const PREF_IMAGE_TOOLTIP_SIZE = "devtools.inspector.imagePreviewTooltipSize";

// Types of existing tooltips
const TOOLTIP_IMAGE_TYPE = "image";
const TOOLTIP_FONTFAMILY_TYPE = "font-family";

// Types of nodes in the rule/computed-view
const VIEW_NODE_SELECTOR_TYPE = exports.VIEW_NODE_SELECTOR_TYPE = 1;
const VIEW_NODE_PROPERTY_TYPE = exports.VIEW_NODE_PROPERTY_TYPE = 2;
const VIEW_NODE_VALUE_TYPE = exports.VIEW_NODE_VALUE_TYPE = 3;
const VIEW_NODE_IMAGE_URL_TYPE = exports.VIEW_NODE_IMAGE_URL_TYPE = 4;
const VIEW_NODE_LOCATION_TYPE = exports.VIEW_NODE_LOCATION_TYPE = 5;

/**
 * Manages all highlighters in the style-inspector.
 *
 * @param {CssRuleView|CssComputedView} view
 *        Either the rule-view or computed-view panel
 */
function HighlightersOverlay(view) {
  this.view = view;

  let {CssRuleView} = require("devtools/client/styleinspector/rule-view");
  this.isRuleView = view instanceof CssRuleView;

  this.highlighterUtils = this.view.inspector.toolbox.highlighterUtils;

  this._onMouseMove = this._onMouseMove.bind(this);
  this._onMouseLeave = this._onMouseLeave.bind(this);

  this.highlighters = {};

  // Only initialize the overlay if at least one of the highlighter types is
  // supported
  this.supportsHighlighters =
    this.highlighterUtils.supportsCustomHighlighters();

  EventEmitter.decorate(this);
}

exports.HighlightersOverlay = HighlightersOverlay;

HighlightersOverlay.prototype = {
  /**
   * Add the highlighters overlay to the view. This will start tracking mouse
   * movements and display highlighters when needed
   */
  addToView: function() {
    if (!this.supportsHighlighters || this._isStarted || this._isDestroyed) {
      return;
    }

    let el = this.view.element;
    el.addEventListener("mousemove", this._onMouseMove, false);
    el.addEventListener("mouseleave", this._onMouseLeave, false);

    this._isStarted = true;
  },

  /**
   * Remove the overlay from the current view. This will stop tracking mouse
   * movement and showing highlighters
   */
  removeFromView: function() {
    if (!this.supportsHighlighters || !this._isStarted || this._isDestroyed) {
      return;
    }

    this._hideCurrent();

    let el = this.view.element;
    el.removeEventListener("mousemove", this._onMouseMove, false);
    el.removeEventListener("mouseleave", this._onMouseLeave, false);

    this._isStarted = false;
  },

  _onMouseMove: function(event) {
    // Bail out if the target is the same as for the last mousemove
    if (event.target === this._lastHovered) {
      return;
    }

    // Only one highlighter can be displayed at a time, hide the currently shown
    this._hideCurrent();

    this._lastHovered = event.target;

    let nodeInfo = this.view.getNodeInfo(event.target);
    if (!nodeInfo) {
      return;
    }

    // Choose the type of highlighter required for the hovered node
    let type;
    if (this._isRuleViewTransform(nodeInfo) ||
        this._isComputedViewTransform(nodeInfo)) {
      type = "CssTransformHighlighter";
    }

    if (type) {
      this.highlighterShown = type;
      let node = this.view.inspector.selection.nodeFront;
      this._getHighlighter(type)
          .then(highlighter => highlighter.show(node))
          .then(shown => {
            if (shown) {
              this.emit("highlighter-shown");
            }
          });
    }
  },

  _onMouseLeave: function() {
    this._lastHovered = null;
    this._hideCurrent();
  },

  /**
   * Is the current hovered node a css transform property value in the rule-view
   *
   * @param {Object} nodeInfo
   * @return {Boolean}
   */
  _isRuleViewTransform: function(nodeInfo) {
    let isTransform = nodeInfo.type === VIEW_NODE_VALUE_TYPE &&
                      nodeInfo.value.property === "transform";
    let isEnabled = nodeInfo.value.enabled &&
                    !nodeInfo.value.overridden &&
                    !nodeInfo.value.pseudoElement;
    return this.isRuleView && isTransform && isEnabled;
  },

  /**
   * Is the current hovered node a css transform property value in the
   * computed-view
   *
   * @param {Object} nodeInfo
   * @return {Boolean}
   */
  _isComputedViewTransform: function(nodeInfo) {
    let isTransform = nodeInfo.type === VIEW_NODE_VALUE_TYPE &&
                      nodeInfo.value.property === "transform";
    return !this.isRuleView && isTransform;
  },

  /**
   * Hide the currently shown highlighter
   */
  _hideCurrent: function() {
    if (!this.highlighterShown || !this.highlighters[this.highlighterShown]) {
      return;
    }

    // For some reason, the call to highlighter.hide doesn't always return a
    // promise. This causes some tests to fail when trying to install a
    // rejection handler on the result of the call. To avoid this, check
    // whether the result is truthy before installing the handler.
    let promise = this.highlighters[this.highlighterShown].hide();
    if (promise) {
      promise.then(null, e => console.error(e));
    }

    this.highlighterShown = null;
    this.emit("highlighter-hidden");
  },

  /**
   * Get a highlighter front given a type. It will only be initialized once
   * @param {String} type The highlighter type. One of this.highlighters
   * @return a promise that resolves to the highlighter
   */
  _getHighlighter: function(type) {
    let utils = this.highlighterUtils;

    if (this.highlighters[type]) {
      return promise.resolve(this.highlighters[type]);
    }

    return utils.getHighlighterByType(type).then(highlighter => {
      this.highlighters[type] = highlighter;
      return highlighter;
    });
  },

  /**
   * Destroy this overlay instance, removing it from the view and destroying
   * all initialized highlighters
   */
  destroy: function() {
    this.removeFromView();

    for (let type in this.highlighters) {
      if (this.highlighters[type]) {
        this.highlighters[type].finalize();
        this.highlighters[type] = null;
      }
    }

    this.view = null;
    this.highlighterUtils = null;

    this._isDestroyed = true;
  }
};

/**
 * Manages all tooltips in the style-inspector.
 *
 * @param {CssRuleView|CssComputedView} view
 *        Either the rule-view or computed-view panel
 */
function TooltipsOverlay(view) {
  this.view = view;

  let {CssRuleView} = require("devtools/client/styleinspector/rule-view");
  this.isRuleView = view instanceof CssRuleView;

  this._onNewSelection = this._onNewSelection.bind(this);
  this.view.inspector.selection.on("new-node-front", this._onNewSelection);
}

exports.TooltipsOverlay = TooltipsOverlay;

TooltipsOverlay.prototype = {
  get isEditing() {
    return this.colorPicker.tooltip.isShown() ||
           this.colorPicker.eyedropperOpen ||
           this.cubicBezier.tooltip.isShown() ||
           this.filterEditor.tooltip.isShown();
  },

  /**
   * Add the tooltips overlay to the view. This will start tracking mouse
   * movements and display tooltips when needed
   */
  addToView: function() {
    if (this._isStarted || this._isDestroyed) {
      return;
    }

    let panelDoc = this.view.inspector.panelDoc;

    // Image, fonts, ... preview tooltip
    this.previewTooltip = new Tooltip(panelDoc);
    this.previewTooltip.startTogglingOnHover(this.view.element,
      this._onPreviewTooltipTargetHover.bind(this));

    // MDN CSS help tooltip
    this.cssDocs = new CssDocsTooltip(panelDoc);

    if (this.isRuleView) {
      // Color picker tooltip
      this.colorPicker = new SwatchColorPickerTooltip(panelDoc);
      // Cubic bezier tooltip
      this.cubicBezier = new SwatchCubicBezierTooltip(panelDoc);
      // Filter editor tooltip
      this.filterEditor = new SwatchFilterTooltip(panelDoc);
    }

    this._isStarted = true;
  },

  /**
   * Remove the tooltips overlay from the view. This will stop tracking mouse
   * movements and displaying tooltips
   */
  removeFromView: function() {
    if (!this._isStarted || this._isDestroyed) {
      return;
    }

    this.previewTooltip.stopTogglingOnHover(this.view.element);
    this.previewTooltip.destroy();

    if (this.colorPicker) {
      this.colorPicker.destroy();
    }

    if (this.cubicBezier) {
      this.cubicBezier.destroy();
    }

    if (this.cssDocs) {
      this.cssDocs.destroy();
    }

    if (this.filterEditor) {
      this.filterEditor.destroy();
    }

    this._isStarted = false;
  },

  /**
   * Given a hovered node info, find out which type of tooltip should be shown,
   * if any
   *
   * @param {Object} nodeInfo
   * @return {String} The tooltip type to be shown, or null
   */
  _getTooltipType: function({type, value: prop}) {
    let tooltipType = null;
    let inspector = this.view.inspector;

    // Image preview tooltip
    if (type === VIEW_NODE_IMAGE_URL_TYPE &&
        inspector.hasUrlToImageDataResolver) {
      tooltipType = TOOLTIP_IMAGE_TYPE;
    }

    // Font preview tooltip
    if (type === VIEW_NODE_VALUE_TYPE && prop.property === "font-family") {
      let value = prop.value.toLowerCase();
      if (value !== "inherit" && value !== "unset" && value !== "initial") {
        tooltipType = TOOLTIP_FONTFAMILY_TYPE;
      }
    }

    return tooltipType;
  },

  /**
   * Executed by the tooltip when the pointer hovers over an element of the
   * view. Used to decide whether the tooltip should be shown or not and to
   * actually put content in it.
   * Checks if the hovered target is a css value we support tooltips for.
   *
   * @param {DOMNode} target The currently hovered node
   */
  _onPreviewTooltipTargetHover: function(target) {
    let nodeInfo = this.view.getNodeInfo(target);
    if (!nodeInfo) {
      // The hovered node isn't something we care about
      return promise.reject(false);
    }

    let type = this._getTooltipType(nodeInfo);
    if (!type) {
      // There is no tooltip type defined for the hovered node
      return promise.reject(false);
    }

    if (this.isRuleView && this.colorPicker.tooltip.isShown()) {
      this.colorPicker.revert();
      this.colorPicker.hide();
    }

    if (this.isRuleView && this.cubicBezier.tooltip.isShown()) {
      this.cubicBezier.revert();
      this.cubicBezier.hide();
    }

    if (this.isRuleView && this.cssDocs.tooltip.isShown()) {
      this.cssDocs.hide();
    }

    if (this.isRuleView && this.filterEditor.tooltip.isShown()) {
      this.filterEditor.revert();
      this.filterEdtior.hide();
    }

    let inspector = this.view.inspector;

    if (type === TOOLTIP_IMAGE_TYPE) {
      let dim = Services.prefs.getIntPref(PREF_IMAGE_TOOLTIP_SIZE);
      // nodeInfo contains an absolute uri
      let uri = nodeInfo.value.url;
      return this.previewTooltip.setRelativeImageContent(uri,
        inspector.inspector, dim);
    }

    if (type === TOOLTIP_FONTFAMILY_TYPE) {
      return this.previewTooltip.setFontFamilyContent(nodeInfo.value.value,
        inspector.selection.nodeFront);
    }
  },

  _onNewSelection: function() {
    if (this.previewTooltip) {
      this.previewTooltip.hide();
    }

    if (this.colorPicker) {
      this.colorPicker.hide();
    }

    if (this.cubicBezier) {
      this.cubicBezier.hide();
    }

    if (this.cssDocs) {
      this.cssDocs.hide();
    }

    if (this.filterEditor) {
      this.filterEditor.hide();
    }
  },

  /**
   * Destroy this overlay instance, removing it from the view
   */
  destroy: function() {
    this.removeFromView();

    this.view.inspector.selection.off("new-node-front", this._onNewSelection);
    this.view = null;

    this._isDestroyed = true;
  }
};
