/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cu} = require("chrome");

loader.lazyRequireGetter(this, "colorUtils", "devtools/shared/css/color", true);
loader.lazyRequireGetter(this, "AsyncUtils", "devtools/shared/async-utils");
loader.lazyRequireGetter(this, "flags", "devtools/shared/flags");
loader.lazyRequireGetter(this, "DevToolsUtils", "devtools/shared/DevToolsUtils");
loader.lazyRequireGetter(this, "nodeFilterConstants", "devtools/shared/dom-node-filter-constants");

loader.lazyRequireGetter(this, "isNativeAnonymous", "devtools/shared/layout/utils", true);
loader.lazyRequireGetter(this, "isXBLAnonymous", "devtools/shared/layout/utils", true);

loader.lazyRequireGetter(this, "CssLogic", "devtools/server/actors/inspector/css-logic", true);

const XHTML_NS = "http://www.w3.org/1999/xhtml";
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const IMAGE_FETCHING_TIMEOUT = 500;

/**
 * Returns the properly cased version of the node's tag name, which can be
 * used when displaying said name in the UI.
 *
 * @param  {Node} rawNode
 *         Node for which we want the display name
 * @return {String}
 *         Properly cased version of the node tag name
 */
const getNodeDisplayName = function(rawNode) {
  if (rawNode.nodeName && !rawNode.localName) {
    // The localName & prefix APIs have been moved from the Node interface to the Element
    // interface. Use Node.nodeName as a fallback.
    return rawNode.nodeName;
  }
  return (rawNode.prefix ? rawNode.prefix + ":" : "") + rawNode.localName;
};

function nodeDocument(node) {
  if (Cu.isDeadWrapper(node)) {
    return null;
  }
  return node.ownerDocument ||
         (node.nodeType == Node.DOCUMENT_NODE ? node : null);
}

function isNodeDead(node) {
  return !node || !node.rawNode || Cu.isDeadWrapper(node.rawNode);
}

function isInXULDocument(el) {
  const doc = nodeDocument(el);
  return doc &&
         doc.documentElement &&
         doc.documentElement.namespaceURI === XUL_NS;
}

/**
 * This DeepTreeWalker filter skips whitespace text nodes and anonymous
 * content with the exception of ::marker, ::before, and ::after, plus anonymous
 * content in XUL document (needed to show all elements in the browser toolbox).
 */
function standardTreeWalkerFilter(node) {
  // ::marker, ::before, and ::after are native anonymous content, but we always
  // want to show them
  if (node.nodeName === "_moz_generated_content_marker" ||
      node.nodeName === "_moz_generated_content_before" ||
      node.nodeName === "_moz_generated_content_after") {
    return nodeFilterConstants.FILTER_ACCEPT;
  }

  // Ignore empty whitespace text nodes that do not impact the layout.
  if (isWhitespaceTextNode(node)) {
    return nodeHasSize(node)
           ? nodeFilterConstants.FILTER_ACCEPT
           : nodeFilterConstants.FILTER_SKIP;
  }

  // Ignore all native and XBL anonymous content inside a non-XUL document.
  // We need to do this to skip things like form controls, scrollbars,
  // video controls, etc (see bug 1187482).
  if (!isInXULDocument(node) && (isXBLAnonymous(node) ||
                                  isNativeAnonymous(node))) {
    return nodeFilterConstants.FILTER_SKIP;
  }

  return nodeFilterConstants.FILTER_ACCEPT;
}

/**
 * This DeepTreeWalker filter is like standardTreeWalkerFilter except that
 * it also includes all anonymous content (like internal form controls).
 */
function allAnonymousContentTreeWalkerFilter(node) {
  // Ignore empty whitespace text nodes that do not impact the layout.
  if (isWhitespaceTextNode(node)) {
    return nodeHasSize(node)
           ? nodeFilterConstants.FILTER_ACCEPT
           : nodeFilterConstants.FILTER_SKIP;
  }
  return nodeFilterConstants.FILTER_ACCEPT;
}

/**
 * This DeepTreeWalker filter only accepts <scrollbar> anonymous content.
 */
function scrollbarTreeWalkerFilter(node) {
  if (node.nodeName === "scrollbar" && nodeHasSize(node)) {
    return nodeFilterConstants.FILTER_ACCEPT;
  }

  return nodeFilterConstants.FILTER_SKIP;
}

/**
 * Is the given node a text node composed of whitespace only?
 * @param {DOMNode} node
 * @return {Boolean}
 */
function isWhitespaceTextNode(node) {
  return node.nodeType == Node.TEXT_NODE && !/[^\s]/.exec(node.nodeValue);
}

/**
 * Does the given node have non-0 width and height?
 * @param {DOMNode} node
 * @return {Boolean}
 */
function nodeHasSize(node) {
  if (!node.getBoxQuads) {
    return false;
  }

  const quads = node.getBoxQuads();
  return quads.some(quad => {
    const bounds = quad.getBounds();
    return bounds.width && bounds.height;
  });
}

/**
 * Returns a promise that is settled once the given HTMLImageElement has
 * finished loading.
 *
 * @param {HTMLImageElement} image - The image element.
 * @param {Number} timeout - Maximum amount of time the image is allowed to load
 * before the waiting is aborted. Ignored if flags.testing is set.
 *
 * @return {Promise} that is fulfilled once the image has loaded. If the image
 * fails to load or the load takes too long, the promise is rejected.
 */
function ensureImageLoaded(image, timeout) {
  const { HTMLImageElement } = image.ownerGlobal;
  if (!(image instanceof HTMLImageElement)) {
    return Promise.reject("image must be an HTMLImageELement");
  }

  if (image.complete) {
    // The image has already finished loading.
    return Promise.resolve();
  }

  // This image is still loading.
  const onLoad = AsyncUtils.listenOnce(image, "load");

  // Reject if loading fails.
  const onError = AsyncUtils.listenOnce(image, "error").then(() => {
    return Promise.reject("Image '" + image.src + "' failed to load.");
  });

  // Don't timeout when testing. This is never settled.
  let onAbort = new Promise(() => {});

  if (!flags.testing) {
    // Tests are not running. Reject the promise after given timeout.
    onAbort = DevToolsUtils.waitForTime(timeout).then(() => {
      return Promise.reject("Image '" + image.src + "' took too long to load.");
    });
  }

  // See which happens first.
  return Promise.race([onLoad, onError, onAbort]);
}

/**
 * Given an <img> or <canvas> element, return the image data-uri. If @param node
 * is an <img> element, the method waits a while for the image to load before
 * the data is generated. If the image does not finish loading in a reasonable
 * time (IMAGE_FETCHING_TIMEOUT milliseconds) the process aborts.
 *
 * @param {HTMLImageElement|HTMLCanvasElement} node - The <img> or <canvas>
 * element, or Image() object. Other types cause the method to reject.
 * @param {Number} maxDim - Optionally pass a maximum size you want the longest
 * side of the image to be resized to before getting the image data.

 * @return {Promise} A promise that is fulfilled with an object containing the
 * data-uri and size-related information:
 * { data: "...",
 *   size: {
 *     naturalWidth: 400,
 *     naturalHeight: 300,
 *     resized: true }
 *  }.
 *
 * If something goes wrong, the promise is rejected.
 */
const imageToImageData = async function(node, maxDim) {
  const { HTMLCanvasElement, HTMLImageElement } = node.ownerGlobal;

  const isImg = node instanceof HTMLImageElement;
  const isCanvas = node instanceof HTMLCanvasElement;

  if (!isImg && !isCanvas) {
    throw new Error("node is not a <canvas> or <img> element.");
  }

  if (isImg) {
    // Ensure that the image is ready.
    await ensureImageLoaded(node, IMAGE_FETCHING_TIMEOUT);
  }

  // Get the image resize ratio if a maxDim was provided
  let resizeRatio = 1;
  const imgWidth = node.naturalWidth || node.width;
  const imgHeight = node.naturalHeight || node.height;
  const imgMax = Math.max(imgWidth, imgHeight);
  if (maxDim && imgMax > maxDim) {
    resizeRatio = maxDim / imgMax;
  }

  // Extract the image data
  let imageData;
  // The image may already be a data-uri, in which case, save ourselves the
  // trouble of converting via the canvas.drawImage.toDataURL method, but only
  // if the image doesn't need resizing
  if (isImg && node.src.startsWith("data:") && resizeRatio === 1) {
    imageData = node.src;
  } else {
    // Create a canvas to copy the rawNode into and get the imageData from
    const canvas = node.ownerDocument.createElementNS(XHTML_NS, "canvas");
    canvas.width = imgWidth * resizeRatio;
    canvas.height = imgHeight * resizeRatio;
    const ctx = canvas.getContext("2d");

    // Copy the rawNode image or canvas in the new canvas and extract data
    ctx.drawImage(node, 0, 0, canvas.width, canvas.height);
    imageData = canvas.toDataURL("image/png");
  }

  return {
    data: imageData,
    size: {
      naturalWidth: imgWidth,
      naturalHeight: imgHeight,
      resized: resizeRatio !== 1,
    },
  };
};

/**
 * Finds the computed background color of the closest parent with a set background color.
 *
 * @param  {DOMNode}  node
 *         Node for which we want to find closest background color.
 * @return {String}
 *         String with the background color of the form rgba(r, g, b, a). Defaults to
 *         rgba(255, 255, 255, 1) if no background color is found.
 */
function getClosestBackgroundColor(node) {
  let current = node;

  while (current) {
    const computedStyle = CssLogic.getComputedStyle(current);
    if (computedStyle) {
      const currentStyle = computedStyle.getPropertyValue("background-color");
      if (colorUtils.isValidCSSColor(currentStyle)) {
        const currentCssColor = new colorUtils.CssColor(currentStyle);
        if (!currentCssColor.isTransparent()) {
          return currentCssColor.rgba;
        }
      }
    }

    current = current.parentNode;
  }

  return "rgba(255, 255, 255, 1)";
}

/**
 * Finds the background image of the closest parent where it is set.
 *
 * @param  {DOMNode}  node
 *         Node for which we want to find the background image.
 * @return {String}
 *         String with the value of the background iamge property. Defaults to "none" if
 *         no background image is found.
 */
function getClosestBackgroundImage(node) {
  let current = node;

  while (current) {
    const computedStyle = CssLogic.getComputedStyle(current);
    if (computedStyle) {
      const currentBackgroundImage = computedStyle.getPropertyValue("background-image");
      if (currentBackgroundImage !== "none") {
        return currentBackgroundImage;
      }
    }

    current = current.parentNode;
  }

  return "none";
}

module.exports = {
  allAnonymousContentTreeWalkerFilter,
  getClosestBackgroundColor,
  getClosestBackgroundImage,
  getNodeDisplayName,
  imageToImageData,
  isNodeDead,
  nodeDocument,
  scrollbarTreeWalkerFilter,
  standardTreeWalkerFilter,
};
