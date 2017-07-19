/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PREVIEW_MAX_DIM_PREF = "devtools.inspector.imagePreviewTooltipSize";

const promise = require("promise");
const Services = require("Services");
const {Task} = require("devtools/shared/task");
const nodeConstants = require("devtools/shared/dom-node-constants");
const clipboardHelper = require("devtools/shared/platform/clipboard");
const {setImageTooltip, setBrokenImageTooltip} =
      require("devtools/client/shared/widgets/tooltip/ImageTooltipHelper");
const MarkupContainer = require("devtools/client/inspector/markup/views/markup-container");
const ElementEditor = require("devtools/client/inspector/markup/views/element-editor");
const {extend} = require("devtools/client/inspector/markup/utils");

// Lazy load this module as _buildEventTooltipContent is only called on click
loader.lazyRequireGetter(this, "setEventTooltip",
  "devtools/client/shared/widgets/tooltip/EventTooltipHelper", true);

/**
 * An implementation of MarkupContainer for Elements that can contain
 * child nodes.
 * Allows editing of tag name, attributes, expanding / collapsing.
 *
 * @param  {MarkupView} markupView
 *         The markup view that owns this container.
 * @param  {NodeFront} node
 *         The node to display.
 */
function MarkupElementContainer(markupView, node) {
  MarkupContainer.prototype.initialize.call(this, markupView, node,
    "elementcontainer");

  if (node.nodeType === nodeConstants.ELEMENT_NODE) {
    this.editor = new ElementEditor(this, node);
  } else {
    throw new Error("Invalid node for MarkupElementContainer");
  }

  this.tagLine.appendChild(this.editor.elt);
}

MarkupElementContainer.prototype = extend(MarkupContainer.prototype, {
  _buildEventTooltipContent: Task.async(function* (target, tooltip) {
    if (target.hasAttribute("data-event")) {
      yield tooltip.hide();

      let listenerInfo = yield this.node.getEventListenerInfo();

      let toolbox = this.markup.toolbox;

      setEventTooltip(tooltip, listenerInfo, toolbox);
      // Disable the image preview tooltip while we display the event details
      this.markup._disableImagePreviewTooltip();
      tooltip.once("hidden", () => {
        // Enable the image preview tooltip after closing the event details
        this.markup._enableImagePreviewTooltip();
      });
      tooltip.show(target);
    }
  }),

  /**
   * Generates the an image preview for this Element. The element must be an
   * image or canvas (@see isPreviewable).
   *
   * @return {Promise} that is resolved with an object of form
   *         { data, size: { naturalWidth, naturalHeight, resizeRatio } } where
   *         - data is the data-uri for the image preview.
   *         - size contains information about the original image size and if
   *         the preview has been resized.
   *
   * If this element is not previewable or the preview cannot be generated for
   * some reason, the Promise is rejected.
   */
  _getPreview: function () {
    if (!this.isPreviewable()) {
      return promise.reject("_getPreview called on a non-previewable element.");
    }

    if (this.tooltipDataPromise) {
      // A preview request is already pending. Re-use that request.
      return this.tooltipDataPromise;
    }

    // Fetch the preview from the server.
    this.tooltipDataPromise = Task.spawn(function* () {
      let maxDim = Services.prefs.getIntPref(PREVIEW_MAX_DIM_PREF);
      let preview = yield this.node.getImageData(maxDim);
      let data = yield preview.data.string();

      // Clear the pending preview request. We can't reuse the results later as
      // the preview contents might have changed.
      this.tooltipDataPromise = null;
      return { data, size: preview.size };
    }.bind(this));

    return this.tooltipDataPromise;
  },

  /**
   * Executed by MarkupView._isImagePreviewTarget which is itself called when
   * the mouse hovers over a target in the markup-view.
   * Checks if the target is indeed something we want to have an image tooltip
   * preview over and, if so, inserts content into the tooltip.
   *
   * @return {Promise} that resolves when the tooltip content is ready. Resolves
   * true if the tooltip should be displayed, false otherwise.
   */
  isImagePreviewTarget: Task.async(function* (target, tooltip) {
    // Is this Element previewable.
    if (!this.isPreviewable()) {
      return false;
    }

    // If the Element has an src attribute, the tooltip is shown when hovering
    // over the src url. If not, the tooltip is shown when hovering over the tag
    // name.
    let src = this.editor.getAttributeElement("src");
    let expectedTarget = src ? src.querySelector(".link") : this.editor.tag;
    if (target !== expectedTarget) {
      return false;
    }

    try {
      let { data, size } = yield this._getPreview();
      // The preview is ready.
      let options = {
        naturalWidth: size.naturalWidth,
        naturalHeight: size.naturalHeight,
        maxDim: Services.prefs.getIntPref(PREVIEW_MAX_DIM_PREF)
      };

      setImageTooltip(tooltip, this.markup.doc, data, options);
    } catch (e) {
      // Indicate the failure but show the tooltip anyway.
      setBrokenImageTooltip(tooltip, this.markup.doc);
    }
    return true;
  }),

  copyImageDataUri: function () {
    // We need to send again a request to gettooltipData even if one was sent
    // for the tooltip, because we want the full-size image
    this.node.getImageData().then(data => {
      data.data.string().then(str => {
        clipboardHelper.copyString(str);
      });
    });
  },

  setInlineTextChild: function (inlineTextChild) {
    this.inlineTextChild = inlineTextChild;
    this.editor.updateTextEditor();
  },

  clearInlineTextChild: function () {
    this.inlineTextChild = undefined;
    this.editor.updateTextEditor();
  },

  /**
   * Trigger new attribute field for input.
   */
  addAttribute: function () {
    this.editor.newAttr.editMode();
  },

  /**
   * Trigger attribute field for editing.
   */
  editAttribute: function (attrName) {
    this.editor.attrElements.get(attrName).editMode();
  },

  /**
   * Remove attribute from container.
   * This is an undoable action.
   */
  removeAttribute: function (attrName) {
    let doMods = this.editor._startModifyingAttributes();
    let undoMods = this.editor._startModifyingAttributes();
    this.editor._saveAttribute(attrName, undoMods);
    doMods.removeAttribute(attrName);
    this.undo.do(() => {
      doMods.apply();
    }, () => {
      undoMods.apply();
    });
  }
});

module.exports = MarkupElementContainer;
