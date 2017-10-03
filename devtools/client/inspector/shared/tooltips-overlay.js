/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * The tooltip overlays are tooltips that appear when hovering over property values and
 * editor tooltips that appear when clicking swatch based editors.
 */

const { Task } = require("devtools/shared/task");
const Services = require("Services");
const {
  VIEW_NODE_VALUE_TYPE,
  VIEW_NODE_IMAGE_URL_TYPE,
} = require("devtools/client/inspector/shared/node-types");
const { getColor } = require("devtools/client/shared/theme");
const { HTMLTooltip } = require("devtools/client/shared/widgets/tooltip/HTMLTooltip");

loader.lazyRequireGetter(this, "getCssProperties",
  "devtools/shared/fronts/css-properties", true);

loader.lazyRequireGetter(this, "getImageDimensions",
  "devtools/client/shared/widgets/tooltip/ImageTooltipHelper", true);
loader.lazyRequireGetter(this, "setImageTooltip",
  "devtools/client/shared/widgets/tooltip/ImageTooltipHelper", true);
loader.lazyRequireGetter(this, "setBrokenImageTooltip",
  "devtools/client/shared/widgets/tooltip/ImageTooltipHelper", true);

const PREF_IMAGE_TOOLTIP_SIZE = "devtools.inspector.imagePreviewTooltipSize";

// Types of existing tooltips
const TOOLTIP_IMAGE_TYPE = "image";
const TOOLTIP_FONTFAMILY_TYPE = "font-family";

/**
 * Manages all tooltips in the style-inspector.
 *
 * @param {CssRuleView|CssComputedView} view
 *        Either the rule-view or computed-view panel
 */
function TooltipsOverlay(view) {
  this.view = view;
  this._instances = new Map();

  this._onNewSelection = this._onNewSelection.bind(this);
  this.view.inspector.selection.on("new-node-front", this._onNewSelection);

  this.addToView();
}

TooltipsOverlay.prototype = {
  get _cssProperties() {
    delete TooltipsOverlay.prototype._cssProperties;
    let properties = getCssProperties(this.view.inspector.toolbox);
    TooltipsOverlay.prototype._cssProperties = properties;
    return properties;
  },

  get isEditing() {
    for (let [, tooltip] of this._instances) {
      if (typeof (tooltip.isEditing) == "function" && tooltip.isEditing()) {
        return true;
      }
    }
    return false;
  },

  /**
   * Add the tooltips overlay to the view. This will start tracking mouse
   * movements and display tooltips when needed
   */
  addToView: function () {
    if (this._isStarted || this._isDestroyed) {
      return;
    }

    this._isStarted = true;

    // For now, preview tooltip has to be instanciated on startup in order to
    // call tooltip.startTogglingOnHover. Ideally startTogglingOnHover wouldn't be part
    // of HTMLTooltip and offer a way to lazy load this tooltip.
    this.getTooltip("previewTooltip");
  },

  /**
   * Lazily fetch and initialize the different tooltips that are used in the inspector.
   * These tooltips are attached to the toolbox document if they require a popup panel.
   * Otherwise, it is attached to the inspector panel document if it is an inline editor.
   *
   * @param {String} name
   *        Identifier name for the tooltip
   */
  getTooltip: function (name) {
    let tooltip = this._instances.get(name);
    if (tooltip) {
      return tooltip;
    }
    let { doc } = this.view.inspector.toolbox;
    switch (name) {
      case "colorPicker":
        const SwatchColorPickerTooltip =
          require("devtools/client/shared/widgets/tooltip/SwatchColorPickerTooltip");
        tooltip = new SwatchColorPickerTooltip(doc, this.view.inspector,
          this._cssProperties);
        break;
      case "cubicBezier":
        const SwatchCubicBezierTooltip =
          require("devtools/client/shared/widgets/tooltip/SwatchCubicBezierTooltip");
        tooltip = new SwatchCubicBezierTooltip(doc);
        break;
      case "filterEditor":
        const SwatchFilterTooltip =
          require("devtools/client/shared/widgets/tooltip/SwatchFilterTooltip");
        tooltip = new SwatchFilterTooltip(doc,
          this._cssProperties.getValidityChecker(this.view.inspector.panelDoc));
        break;
      case "previewTooltip":
        tooltip = new HTMLTooltip(doc, {
          type: "arrow",
          useXulWrapper: true
        });
        tooltip.startTogglingOnHover(this.view.element,
          this._onPreviewTooltipTargetHover.bind(this));
        break;
      default:
        throw new Error(`Unsupported tooltip '${name}'`);
    }
    this._instances.set(name, tooltip);
    return tooltip;
  },

  /**
   * Remove the tooltips overlay from the view. This will stop tracking mouse
   * movements and displaying tooltips
   */
  removeFromView: function () {
    if (!this._isStarted || this._isDestroyed) {
      return;
    }

    for (let [, tooltip] of this._instances) {
      tooltip.destroy();
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
  _getTooltipType: function ({type, value: prop}) {
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
   * @return {Promise}
   */
  _onPreviewTooltipTargetHover: Task.async(function* (target) {
    let nodeInfo = this.view.getNodeInfo(target);
    if (!nodeInfo) {
      // The hovered node isn't something we care about
      return false;
    }

    let type = this._getTooltipType(nodeInfo);
    if (!type) {
      // There is no tooltip type defined for the hovered node
      return false;
    }

    for (let [, tooltip] of this._instances) {
      if (tooltip.isVisible()) {
        tooltip.revert();
        tooltip.hide();
      }
    }

    let inspector = this.view.inspector;

    if (type === TOOLTIP_IMAGE_TYPE) {
      try {
        yield this._setImagePreviewTooltip(nodeInfo.value.url);
      } catch (e) {
        yield setBrokenImageTooltip(this.getTooltip("previewTooltip"),
          this.view.inspector.panelDoc);
      }
      return true;
    }

    if (type === TOOLTIP_FONTFAMILY_TYPE) {
      let font = nodeInfo.value.value;
      let nodeFront = inspector.selection.nodeFront;
      yield this._setFontPreviewTooltip(font, nodeFront);
      return true;
    }

    return false;
  }),

  /**
   * Set the content of the preview tooltip to display an image preview. The image URL can
   * be relative, a call will be made to the debuggee to retrieve the image content as an
   * imageData URI.
   *
   * @param {String} imageUrl
   *        The image url value (may be relative or absolute).
   * @return {Promise} A promise that resolves when the preview tooltip content is ready
   */
  _setImagePreviewTooltip: Task.async(function* (imageUrl) {
    let doc = this.view.inspector.panelDoc;
    let maxDim = Services.prefs.getIntPref(PREF_IMAGE_TOOLTIP_SIZE);

    let naturalWidth, naturalHeight;
    if (imageUrl.startsWith("data:")) {
      // If the imageUrl already is a data-url, save ourselves a round-trip
      let size = yield getImageDimensions(doc, imageUrl);
      naturalWidth = size.naturalWidth;
      naturalHeight = size.naturalHeight;
    } else {
      let inspectorFront = this.view.inspector.inspector;
      let {data, size} = yield inspectorFront.getImageDataFromURL(imageUrl, maxDim);
      imageUrl = yield data.string();
      naturalWidth = size.naturalWidth;
      naturalHeight = size.naturalHeight;
    }

    yield setImageTooltip(this.getTooltip("previewTooltip"), doc, imageUrl,
      {maxDim, naturalWidth, naturalHeight});
  }),

  /**
   * Set the content of the preview tooltip to display a font family preview.
   *
   * @param {String} font
   *        The font family value.
   * @param {object} nodeFront
   *        The NodeActor that will used to retrieve the dataURL for the font
   *        family tooltip contents.
   * @return {Promise} A promise that resolves when the preview tooltip content is ready
   */
  _setFontPreviewTooltip: Task.async(function* (font, nodeFront) {
    if (!font || !nodeFront || typeof nodeFront.getFontFamilyDataURL !== "function") {
      throw new Error("Unable to create font preview tooltip content.");
    }

    font = font.replace(/"/g, "'");
    font = font.replace("!important", "");
    font = font.trim();

    let fillStyle = getColor("body-color");
    let {data, size: maxDim} = yield nodeFront.getFontFamilyDataURL(font, fillStyle);

    let imageUrl = yield data.string();
    let doc = this.view.inspector.panelDoc;
    let {naturalWidth, naturalHeight} = yield getImageDimensions(doc, imageUrl);

    yield setImageTooltip(this.getTooltip("previewTooltip"), doc, imageUrl,
      {hideDimensionLabel: true, hideCheckeredBackground: true,
       maxDim, naturalWidth, naturalHeight});
  }),

  _onNewSelection: function () {
    for (let [, tooltip] of this._instances) {
      tooltip.hide();
    }
  },

  /**
   * Destroy this overlay instance, removing it from the view
   */
  destroy: function () {
    this.removeFromView();

    this.view.inspector.selection.off("new-node-front", this._onNewSelection);
    this.view = null;

    this._isDestroyed = true;
  }
};

module.exports = TooltipsOverlay;
