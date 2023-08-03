/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { LocalizationHelper } = require("resource://devtools/shared/l10n.js");
const L10N = new LocalizationHelper(
  "devtools/client/locales/inspector.properties"
);

const XHTML_NS = "http://www.w3.org/1999/xhtml";

// Default image tooltip max dimension
const MAX_DIMENSION = 200;
const CONTAINER_MIN_WIDTH = 100;
// Should remain synchronized with tooltips.css --image-tooltip-image-padding
const IMAGE_PADDING = 4;
// Should remain synchronized with tooltips.css --image-tooltip-label-height
const LABEL_HEIGHT = 20;

/**
 * Image preview tooltips should be provided with the naturalHeight and
 * naturalWidth value for the image to display. This helper loads the provided
 * image URL in an image object in order to retrieve the image dimensions after
 * the load.
 *
 * @param {Document} doc the document element to use to create the image object
 * @param {String} imageUrl the url of the image to measure
 * @return {Promise} returns a promise that will resolve after the iamge load:
 *         - {Number} naturalWidth natural width of the loaded image
 *         - {Number} naturalHeight natural height of the loaded image
 */
function getImageDimensions(doc, imageUrl) {
  return new Promise(resolve => {
    const imgObj = new doc.defaultView.Image();
    imgObj.onload = () => {
      imgObj.onload = null;
      const { naturalWidth, naturalHeight } = imgObj;
      resolve({ naturalWidth, naturalHeight });
    };
    imgObj.src = imageUrl;
  });
}

/**
 * Set the tooltip content of a provided HTMLTooltip instance to display an
 * image preview matching the provided imageUrl.
 *
 * @param {HTMLTooltip} tooltip
 *        The tooltip instance on which the image preview content should be set
 * @param {Document} doc
 *        A document element to create the HTML elements needed for the tooltip
 * @param {String} imageUrl
 *        Absolute URL of the image to display in the tooltip
 * @param {Object} options
 *        - {Number} naturalWidth mandatory, width of the image to display
 *        - {Number} naturalHeight mandatory, height of the image to display
 *        - {Number} maxDim optional, max width/height of the preview
 *        - {Boolean} hideDimensionLabel optional, pass true to hide the label
 *        - {Boolean} hideCheckeredBackground optional, pass true to hide
                      the checkered background
 */
function setImageTooltip(tooltip, doc, imageUrl, options) {
  let {
    naturalWidth,
    naturalHeight,
    hideDimensionLabel,
    hideCheckeredBackground,
    maxDim,
  } = options;
  maxDim = maxDim || MAX_DIMENSION;

  let imgHeight = naturalHeight;
  let imgWidth = naturalWidth;
  if (imgHeight > maxDim || imgWidth > maxDim) {
    const scale = maxDim / Math.max(imgHeight, imgWidth);
    // Only allow integer values to avoid rounding errors.
    imgHeight = Math.floor(scale * naturalHeight);
    imgWidth = Math.ceil(scale * naturalWidth);
  }

  // Create tooltip content
  const container = doc.createElementNS(XHTML_NS, "div");
  container.classList.add("devtools-tooltip-image-container");

  const wrapper = doc.createElementNS(XHTML_NS, "div");
  wrapper.classList.add("devtools-tooltip-image-wrapper");
  container.appendChild(wrapper);

  const img = doc.createElementNS(XHTML_NS, "img");
  img.classList.add("devtools-tooltip-image");
  img.classList.toggle("devtools-tooltip-tiles", !hideCheckeredBackground);
  img.style.height = imgHeight;
  img.src = encodeURI(imageUrl);
  wrapper.appendChild(img);

  if (!hideDimensionLabel) {
    const dimensions = doc.createElementNS(XHTML_NS, "div");
    dimensions.classList.add("devtools-tooltip-image-dimensions");
    container.appendChild(dimensions);

    const label = naturalWidth + " \u00D7 " + naturalHeight;
    const span = doc.createElementNS(XHTML_NS, "span");
    span.classList.add("devtools-tooltip-caption");
    span.textContent = label;
    dimensions.appendChild(span);
  }

  tooltip.panel.innerHTML = "";
  tooltip.panel.appendChild(container);

  // Calculate tooltip dimensions
  const width = Math.max(CONTAINER_MIN_WIDTH, imgWidth + 2 * IMAGE_PADDING);
  let height = imgHeight + 2 * IMAGE_PADDING;
  if (!hideDimensionLabel) {
    height += parseFloat(LABEL_HEIGHT);
  }

  tooltip.setContentSize({ width, height });
}

/*
 * Set the tooltip content of a provided HTMLTooltip instance to display a
 * fallback error message when an image preview tooltip can not be displayed.
 *
 * @param {HTMLTooltip} tooltip
 *        The tooltip instance on which the image preview content should be set
 * @param {Document} doc
 *        A document element to create the HTML elements needed for the tooltip
 */
function setBrokenImageTooltip(tooltip, doc) {
  const div = doc.createElementNS(XHTML_NS, "div");
  div.className = "devtools-tooltip-image-broken";
  const message = L10N.getStr("previewTooltip.image.brokenImage");
  div.textContent = message;

  tooltip.panel.innerHTML = "";
  tooltip.panel.appendChild(div);
  tooltip.setContentSize({ width: "auto", height: "auto" });
}

module.exports.getImageDimensions = getImageDimensions;
module.exports.setImageTooltip = setImageTooltip;
module.exports.setBrokenImageTooltip = setBrokenImageTooltip;
