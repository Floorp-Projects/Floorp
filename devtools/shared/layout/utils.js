/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci, Cc } = require("chrome");
const nodeFilterConstants = require("devtools/shared/dom-node-filter-constants");
loader.lazyRequireGetter(
  this,
  "DevToolsUtils",
  "devtools/shared/DevToolsUtils"
);
loader.lazyRequireGetter(
  this,
  "NetUtil",
  "resource://gre/modules/NetUtil.jsm",
  true
);

const SHEET_TYPE = {
  agent: "AGENT_SHEET",
  user: "USER_SHEET",
  author: "AUTHOR_SHEET",
};

// eslint-disable-next-line no-unused-vars
loader.lazyRequireGetter(
  this,
  "setIgnoreLayoutChanges",
  "devtools/server/actors/reflow",
  true
);
exports.setIgnoreLayoutChanges = (...args) =>
  this.setIgnoreLayoutChanges(...args);

/**
 * Returns the `DOMWindowUtils` for the window given.
 *
 * @param {DOMWindow} win
 * @returns {DOMWindowUtils}
 */
const utilsCache = new WeakMap();
function utilsFor(win) {
  // XXXbz Given that we now have a direct getter for the DOMWindowUtils, is
  // this weakmap cache path any faster than just calling the getter?
  if (!utilsCache.has(win)) {
    utilsCache.set(win, win.windowUtils);
  }
  return utilsCache.get(win);
}

/**
 * Check a window is part of the boundary window given.
 *
 * @param {DOMWindow} boundaryWindow
 * @param {DOMWindow} win
 * @return {Boolean}
 */
function isWindowIncluded(boundaryWindow, win) {
  if (win === boundaryWindow) {
    return true;
  }

  const parent = win.parent;

  if (!parent || parent === win) {
    return false;
  }

  return isWindowIncluded(boundaryWindow, parent);
}
exports.isWindowIncluded = isWindowIncluded;

/**
 * like win.frameElement, but goes through mozbrowsers and mozapps iframes.
 *
 * @param {DOMWindow} win
 *        The window to get the frame for
 * @return {DOMNode}
 *         The element in which the window is embedded.
 */
const getFrameElement = win => {
  const isTopWindow = win && DevToolsUtils.getTopWindow(win) === win;
  return isTopWindow ? null : win.browsingContext.embedderElement;
};
exports.getFrameElement = getFrameElement;

/**
 * Get the x/y offsets for of all the parent frames of a given node, limited to
 * the boundary window given.
 *
 * @param {DOMWindow} boundaryWindow
 *        The window where to stop to iterate. If `null` is given, the top
 *        window is used.
 * @param {DOMNode} node
 *        The node for which we are to get the offset
 * @return {Array}
 *         The frame offset [x, y]
 */
function getFrameOffsets(boundaryWindow, node) {
  let xOffset = 0;
  let yOffset = 0;

  let frameWin = getWindowFor(node);
  const scale = getCurrentZoom(node);

  if (boundaryWindow === null) {
    boundaryWindow = DevToolsUtils.getTopWindow(frameWin);
  } else if (typeof boundaryWindow === "undefined") {
    throw new Error("No boundaryWindow given. Use null for the default one.");
  }

  while (frameWin !== boundaryWindow) {
    const frameElement = getFrameElement(frameWin);
    if (!frameElement) {
      break;
    }

    // We are in an iframe.
    // We take into account the parent iframe position and its
    // offset (borders and padding).
    const frameRect = frameElement.getBoundingClientRect();

    const [offsetTop, offsetLeft] = getFrameContentOffset(frameElement);

    xOffset += frameRect.left + offsetLeft;
    yOffset += frameRect.top + offsetTop;

    frameWin = frameWin.parent;
  }

  return [xOffset * scale, yOffset * scale];
}
exports.getFrameOffsets = getFrameOffsets;

/**
 * Get box quads adjusted for iframes and zoom level.
 *
 * Warning: this function returns things that look like DOMQuad objects but
 * aren't (they resemble an old version of the spec). Unlike the return value
 * of node.getBoxQuads, they have a .bounds property and not a .getBounds()
 * method.
 *
 * @param {DOMWindow} boundaryWindow
 *        The window where to stop to iterate. If `null` is given, the top
 *        window is used.
 * @param {DOMNode} node
 *        The node for which we are to get the box model region
 *        quads.
 * @param {String} region
 *        The box model region to return: "content", "padding", "border" or
 *        "margin".
 * @param {Object} [options.ignoreZoom=false]
 *        Ignore zoom used in the context of e.g. canvas.
 * @return {Array}
 *        An array of objects that have the same structure as quads returned by
 *        getBoxQuads. An empty array if the node has no quads or is invalid.
 */
function getAdjustedQuads(
  boundaryWindow,
  node,
  region,
  { ignoreZoom, ignoreScroll } = {}
) {
  if (!node || !node.getBoxQuads) {
    return [];
  }

  const quads = node.getBoxQuads({
    box: region,
    relativeTo: boundaryWindow.document,
    createFramesForSuppressedWhitespace: false,
  });

  if (!quads.length) {
    return [];
  }

  const scale = ignoreZoom ? 1 : getCurrentZoom(node);
  const { scrollX, scrollY } = ignoreScroll
    ? { scrollX: 0, scrollY: 0 }
    : boundaryWindow;

  const xOffset = scrollX * scale;
  const yOffset = scrollY * scale;

  const adjustedQuads = [];
  for (const quad of quads) {
    const bounds = quad.getBounds();
    adjustedQuads.push({
      p1: {
        w: quad.p1.w * scale,
        x: quad.p1.x * scale + xOffset,
        y: quad.p1.y * scale + yOffset,
        z: quad.p1.z * scale,
      },
      p2: {
        w: quad.p2.w * scale,
        x: quad.p2.x * scale + xOffset,
        y: quad.p2.y * scale + yOffset,
        z: quad.p2.z * scale,
      },
      p3: {
        w: quad.p3.w * scale,
        x: quad.p3.x * scale + xOffset,
        y: quad.p3.y * scale + yOffset,
        z: quad.p3.z * scale,
      },
      p4: {
        w: quad.p4.w * scale,
        x: quad.p4.x * scale + xOffset,
        y: quad.p4.y * scale + yOffset,
        z: quad.p4.z * scale,
      },
      bounds: {
        bottom: bounds.bottom * scale + yOffset,
        height: bounds.height * scale,
        left: bounds.left * scale + xOffset,
        right: bounds.right * scale + xOffset,
        top: bounds.top * scale + yOffset,
        width: bounds.width * scale,
        x: bounds.x * scale + xOffset,
        y: bounds.y * scale + yOffset,
      },
    });
  }

  return adjustedQuads;
}
exports.getAdjustedQuads = getAdjustedQuads;

/**
 * Compute the absolute position and the dimensions of a node, relativalely
 * to the root window.

 * @param {DOMWindow} boundaryWindow
 *        The window where to stop to iterate. If `null` is given, the top
 *        window is used.
 * @param {DOMNode} node
 *        a DOM element to get the bounds for
 * @param {DOMWindow} contentWindow
 *        the content window holding the node
 * @return {Object}
 *         A rect object with the {top, left, width, height} properties
 */
function getRect(boundaryWindow, node, contentWindow) {
  let frameWin = node.ownerDocument.defaultView;
  const clientRect = node.getBoundingClientRect();

  if (boundaryWindow === null) {
    boundaryWindow = DevToolsUtils.getTopWindow(frameWin);
  } else if (typeof boundaryWindow === "undefined") {
    throw new Error("No boundaryWindow given. Use null for the default one.");
  }

  // Go up in the tree of frames to determine the correct rectangle.
  // clientRect is read-only, we need to be able to change properties.
  const rect = {
    top: clientRect.top + contentWindow.pageYOffset,
    left: clientRect.left + contentWindow.pageXOffset,
    width: clientRect.width,
    height: clientRect.height,
  };

  // We iterate through all the parent windows.
  while (frameWin !== boundaryWindow) {
    const frameElement = getFrameElement(frameWin);
    if (!frameElement) {
      break;
    }

    // We are in an iframe.
    // We take into account the parent iframe position and its
    // offset (borders and padding).
    const frameRect = frameElement.getBoundingClientRect();

    const [offsetTop, offsetLeft] = getFrameContentOffset(frameElement);

    rect.top += frameRect.top + offsetTop;
    rect.left += frameRect.left + offsetLeft;

    frameWin = frameWin.parent;
  }

  return rect;
}
exports.getRect = getRect;

/**
 * Get the 4 bounding points for a node taking iframes into account.
 * Note that for transformed nodes, this will return the untransformed bound.
 *
 * @param {DOMWindow} boundaryWindow
 *        The window where to stop to iterate. If `null` is given, the top
 *        window is used.
 * @param {DOMNode} node
 * @return {Object}
 *         An object with p1,p2,p3,p4 properties being {x,y} objects
 */
function getNodeBounds(boundaryWindow, node) {
  if (!node) {
    return null;
  }
  const { scrollX, scrollY } = boundaryWindow;
  const scale = getCurrentZoom(node);

  // Find out the offset of the node in its current frame
  let offsetLeft = 0;
  let offsetTop = 0;
  let el = node;
  while (el?.parentNode) {
    offsetLeft += el.offsetLeft;
    offsetTop += el.offsetTop;
    el = el.offsetParent;
  }

  // Also take scrolled containers into account
  el = node;
  while (el?.parentNode) {
    if (el.scrollTop) {
      offsetTop -= el.scrollTop;
    }
    if (el.scrollLeft) {
      offsetLeft -= el.scrollLeft;
    }
    el = el.parentNode;
  }

  // And add the potential frame offset if the node is nested
  let [xOffset, yOffset] = getFrameOffsets(boundaryWindow, node);
  xOffset += (offsetLeft + scrollX) * scale;
  yOffset += (offsetTop + scrollY) * scale;

  // Get the width and height
  const width = node.offsetWidth * scale;
  const height = node.offsetHeight * scale;

  return {
    p1: { x: xOffset, y: yOffset },
    p2: { x: xOffset + width, y: yOffset },
    p3: { x: xOffset + width, y: yOffset + height },
    p4: { x: xOffset, y: yOffset + height },
    top: yOffset,
    right: xOffset + width,
    bottom: yOffset + height,
    left: xOffset,
    width,
    height,
  };
}
exports.getNodeBounds = getNodeBounds;

/**
 * Same as doing iframe.contentWindow but works with all types of container
 * elements that act like frames (e.g. <embed>), where 'contentWindow' isn't a
 * property that can be accessed.
 * This uses the inIDeepTreeWalker instead.
 * @param {DOMNode} frame
 * @return {Window}
 */
function safelyGetContentWindow(frame) {
  if (frame.contentWindow) {
    return frame.contentWindow;
  }

  const walker = Cc["@mozilla.org/inspector/deep-tree-walker;1"].createInstance(
    Ci.inIDeepTreeWalker
  );
  walker.showSubDocuments = true;
  walker.showDocumentsAsNodes = true;
  walker.init(frame, nodeFilterConstants.SHOW_ALL);
  walker.currentNode = frame;

  const document = walker.nextNode();
  if (!document || !document.defaultView) {
    throw new Error("Couldn't get the content window inside frame " + frame);
  }

  return document.defaultView;
}

/**
 * Returns a frame's content offset (frame border + padding).
 * Note: this function shouldn't need to exist, had the platform provided a
 * suitable API for determining the offset between the frame's content and
 * its bounding client rect. Bug 626359 should provide us with such an API.
 *
 * @param {DOMNode} frame
 *        The frame.
 * @return {Array} [offsetTop, offsetLeft]
 *         offsetTop is the distance from the top of the frame and the top of
 *         the content document.
 *         offsetLeft is the distance from the left of the frame and the left
 *         of the content document.
 */
function getFrameContentOffset(frame) {
  const style = safelyGetContentWindow(frame).getComputedStyle(frame);

  // In some cases, the computed style is null
  if (!style) {
    return [0, 0];
  }

  const paddingTop = parseInt(style.getPropertyValue("padding-top"), 10);
  const paddingLeft = parseInt(style.getPropertyValue("padding-left"), 10);

  const borderTop = parseInt(style.getPropertyValue("border-top-width"), 10);
  const borderLeft = parseInt(style.getPropertyValue("border-left-width"), 10);

  return [borderTop + paddingTop, borderLeft + paddingLeft];
}

/**
 * Check if a node and its document are still alive
 * and attached to the window.
 *
 * @param {DOMNode} node
 * @return {Boolean}
 */
function isNodeConnected(node) {
  if (!node.ownerDocument || !node.ownerDocument.defaultView) {
    return false;
  }

  try {
    return !(
      node.compareDocumentPosition(node.ownerDocument.documentElement) &
      node.DOCUMENT_POSITION_DISCONNECTED
    );
  } catch (e) {
    // "can't access dead object" error
    return false;
  }
}
exports.isNodeConnected = isNodeConnected;

/**
 * Determine whether a node is anonymous.
 *
 * @param {DOMNode} node
 * @return {Boolean}
 *
 * FIXME(bug 1597411): Remove one of these (or both, as
 * `node.isNativeAnonymous` is quite clear).
 */
const isAnonymous = node => node.isNativeAnonymous;
exports.isAnonymous = isAnonymous;
exports.isNativeAnonymous = isAnonymous;

/**
 * Determine whether a node is a template element.
 *
 * @param {DOMNode} node
 * @return {Boolean}
 */
function isTemplateElement(node) {
  return (
    node.ownerGlobal && node.ownerGlobal.HTMLTemplateElement.isInstance(node)
  );
}
exports.isTemplateElement = isTemplateElement;

/**
 * Determine whether a node is a shadow root.
 *
 * @param {DOMNode} node
 * @return {Boolean}
 */
const isShadowRoot = node => node.containingShadowRoot == node;
exports.isShadowRoot = isShadowRoot;

/*
 * Gets the shadow root mode (open or closed).
 *
 * @param {DOMNode} node
 * @return {String|null}
 */
function getShadowRootMode(node) {
  return isShadowRoot(node) ? node.mode : null;
}
exports.getShadowRootMode = getShadowRootMode;

/**
 * Determine whether a node is a shadow host, ie. an element that has a shadowRoot
 * attached to itself.
 *
 * @param {DOMNode} node
 * @return {Boolean}
 */
function isShadowHost(node) {
  const shadowRoot = node.openOrClosedShadowRoot;
  return shadowRoot && shadowRoot.nodeType === Node.DOCUMENT_FRAGMENT_NODE;
}
exports.isShadowHost = isShadowHost;

/**
 * Determine whether a node is a child of a shadow host. Even if the element has been
 * assigned to a slot in the attached shadow DOM, the parent node for this element is
 * still considered to be the "host" element, and we need to walk them differently.
 *
 * @param {DOMNode} node
 * @return {Boolean}
 */
function isDirectShadowHostChild(node) {
  // Pseudo elements and native anonymous elements are always part of the anonymous tree.
  if (
    isMarkerPseudoElement(node) ||
    isBeforePseudoElement(node) ||
    isAfterPseudoElement(node) ||
    node.isNativeAnonymous
  ) {
    return false;
  }

  const parentNode = node.parentNode;
  return parentNode && !!parentNode.openOrClosedShadowRoot;
}
exports.isDirectShadowHostChild = isDirectShadowHostChild;

/**
 * Determine whether a node is a ::marker pseudo.
 *
 * @param {DOMNode} node
 * @return {Boolean}
 */
function isMarkerPseudoElement(node) {
  return node.nodeName === "_moz_generated_content_marker";
}
exports.isMarkerPseudoElement = isMarkerPseudoElement;

/**
 * Determine whether a node is a ::before pseudo.
 *
 * @param {DOMNode} node
 * @return {Boolean}
 */
function isBeforePseudoElement(node) {
  return node.nodeName === "_moz_generated_content_before";
}
exports.isBeforePseudoElement = isBeforePseudoElement;

/**
 * Determine whether a node is a ::after pseudo.
 *
 * @param {DOMNode} node
 * @return {Boolean}
 */
function isAfterPseudoElement(node) {
  return node.nodeName === "_moz_generated_content_after";
}
exports.isAfterPseudoElement = isAfterPseudoElement;

/**
 * Get the current zoom factor applied to the container window of a given node.
 * Container windows are used as a weakmap key to store the corresponding
 * nsIDOMWindowUtils instance to avoid querying it every time.
 *
 * XXXbz Given that we now have a direct getter for the DOMWindowUtils, is
 * this weakmap cache path any faster than just calling the getter?
 *
 * @param {DOMNode|DOMWindow}
 *        The node for which the zoom factor should be calculated, or its
 *        owner window.
 * @return {Number}
 */
function getCurrentZoom(node) {
  const win = getWindowFor(node);

  if (!win) {
    throw new Error("Unable to get the zoom from the given argument.");
  }

  return utilsFor(win).fullZoom;
}
exports.getCurrentZoom = getCurrentZoom;

/**
 * Get the display pixel ratio for a given window.
 * The `devicePixelRatio` property is affected by the zoom (see bug 809788), so we have to
 * divide by the zoom value in order to get just the display density, expressed as pixel
 * ratio (the physical display pixel compares to a pixel on a “normal” density screen).
 *
 * @param {DOMNode|DOMWindow}
 *        The node for which the zoom factor should be calculated, or its
 *        owner window.
 * @return {Number}
 */
function getDisplayPixelRatio(node) {
  const win = getWindowFor(node);
  return win.devicePixelRatio / utilsFor(win).fullZoom;
}
exports.getDisplayPixelRatio = getDisplayPixelRatio;

/**
 * Returns the window's dimensions for the `window` given.
 *
 * @return {Object} An object with `width` and `height` properties, representing the
 * number of pixels for the document's size.
 */
function getWindowDimensions(window) {
  // First we'll try without flushing layout, because it's way faster.
  const windowUtils = utilsFor(window);
  let { width, height } = windowUtils.getRootBounds();

  if (!width || !height) {
    // We need a flush after all :'(
    width = window.innerWidth + window.scrollMaxX - window.scrollMinX;
    height = window.innerHeight + window.scrollMaxY - window.scrollMinY;

    const scrollbarHeight = {};
    const scrollbarWidth = {};
    windowUtils.getScrollbarSize(false, scrollbarWidth, scrollbarHeight);
    width -= scrollbarWidth.value;
    height -= scrollbarHeight.value;
  }

  return { width, height };
}
exports.getWindowDimensions = getWindowDimensions;

/**
 * Returns the viewport's dimensions for the `window` given.
 *
 * @return {Object} An object with `width` and `height` properties, representing the
 * number of pixels for the viewport's size.
 */
function getViewportDimensions(window) {
  const windowUtils = utilsFor(window);

  const scrollbarHeight = {};
  const scrollbarWidth = {};
  windowUtils.getScrollbarSize(false, scrollbarWidth, scrollbarHeight);

  const width = window.innerWidth - scrollbarWidth.value;
  const height = window.innerHeight - scrollbarHeight.value;

  return { width, height };
}
exports.getViewportDimensions = getViewportDimensions;

/**
 * Return the default view for a given node, where node can be:
 * - a DOM node
 * - the document node
 * - the window itself
 * @param {DOMNode|DOMWindow|DOMDocument} node The node to get the window for.
 * @return {DOMWindow}
 */
function getWindowFor(node) {
  if (Node.isInstance(node)) {
    if (node.nodeType === node.DOCUMENT_NODE) {
      return node.defaultView;
    }
    return node.ownerDocument.defaultView;
  } else if (node instanceof Ci.nsIDOMWindow) {
    return node;
  }
  return null;
}

/**
 * Synchronously loads a style sheet from `uri` and adds it to the list of
 * additional style sheets of the document.
 * The sheets added takes effect immediately, and only on the document of the
 * `window` given.
 *
 * @param {DOMWindow} window
 * @param {String} url
 * @param {String} [type="agent"]
 */
function loadSheet(window, url, type = "agent") {
  if (!(type in SHEET_TYPE)) {
    type = "agent";
  }

  const windowUtils = utilsFor(window);
  try {
    windowUtils.loadSheetUsingURIString(url, windowUtils[SHEET_TYPE[type]]);
  } catch (e) {
    // The method fails if the url is already loaded.
  }
}
exports.loadSheet = loadSheet;

/**
 * Remove the document style sheet at `sheetURI` from the list of additional
 * style sheets of the document. The removal takes effect immediately.
 *
 * @param {DOMWindow} window
 * @param {String} url
 * @param {String} [type="agent"]
 */
function removeSheet(window, url, type = "agent") {
  if (!(type in SHEET_TYPE)) {
    type = "agent";
  }

  const windowUtils = utilsFor(window);
  try {
    windowUtils.removeSheetUsingURIString(url, windowUtils[SHEET_TYPE[type]]);
  } catch (e) {
    // The method fails if the url is already removed.
  }
}
exports.removeSheet = removeSheet;

/**
 * Get the untransformed coordinates for a node.
 *
 * @param  {DOMNode} node
 *         The node for which the DOMQuad is to be returned.
 * @param  {String} region
 *         The box model region to return: "content", "padding", "border" or
 *         "margin".
 * @return {DOMQuad}
 *         A DOMQuad representation of the node.
 */
function getUntransformedQuad(node, region = "border") {
  // Get the inverse transformation matrix for the node.
  const matrix = node.getTransformToViewport();
  const inverse = matrix.inverse();
  const win = node.ownerGlobal;

  // Get the adjusted quads for the node (including scroll offsets).
  const quads = getAdjustedQuads(win, node, region, {
    ignoreZoom: true,
  });

  // Create DOMPoints from the transformed node position.
  const p1 = new DOMPoint(quads[0].p1.x, quads[0].p1.y);
  const p2 = new DOMPoint(quads[0].p2.x, quads[0].p2.y);
  const p3 = new DOMPoint(quads[0].p3.x, quads[0].p3.y);
  const p4 = new DOMPoint(quads[0].p4.x, quads[0].p4.y);

  // Apply the inverse transformation matrix to the points to get the
  // untransformed points.
  const ip1 = inverse.transformPoint(p1);
  const ip2 = inverse.transformPoint(p2);
  const ip3 = inverse.transformPoint(p3);
  const ip4 = inverse.transformPoint(p4);

  // Save the results in a DOMQuad.
  const quad = new DOMQuad(
    { x: ip1.x, y: ip1.y },
    { x: ip2.x, y: ip2.y },
    { x: ip3.x, y: ip3.y },
    { x: ip4.x, y: ip4.y }
  );

  // Remove the border offsets because we include them when calculating
  // offsets in the while loop.
  const style = win.getComputedStyle(node);
  const leftAdjustment = parseInt(style.borderLeftWidth, 10) || 0;
  const topAdjustment = parseInt(style.borderTopWidth, 10) || 0;

  quad.p1.x -= leftAdjustment;
  quad.p2.x -= leftAdjustment;
  quad.p3.x -= leftAdjustment;
  quad.p4.x -= leftAdjustment;
  quad.p1.y -= topAdjustment;
  quad.p2.y -= topAdjustment;
  quad.p3.y -= topAdjustment;
  quad.p4.y -= topAdjustment;

  // Calculate offsets.
  while (node) {
    const nodeStyle = win.getComputedStyle(node);
    const borderLeftWidth = parseInt(nodeStyle.borderLeftWidth, 10) || 0;
    const borderTopWidth = parseInt(nodeStyle.borderTopWidth, 10) || 0;
    const leftOffset = node.offsetLeft - node.scrollLeft + borderLeftWidth;
    const topOffset = node.offsetTop - node.scrollTop + borderTopWidth;

    quad.p1.x += leftOffset;
    quad.p2.x += leftOffset;
    quad.p3.x += leftOffset;
    quad.p4.x += leftOffset;
    quad.p1.y += topOffset;
    quad.p2.y += topOffset;
    quad.p3.y += topOffset;
    quad.p4.y += topOffset;

    node = node.offsetParent;
  }

  return quad;
}
exports.getUntransformedQuad = getUntransformedQuad;

/**
 * Calculate the total of the node and all of its ancestor's scrollTop and
 * scrollLeft values.
 *
 * @param  {DOMNode} node
 *         The node for which the absolute scroll offsets should be calculated.
 * @return {Object} object
 *         An object containing scrollTop and scrollLeft values.
 * @return {Number} object.scrollLeft
 *         The total scrollLeft values of the node and all of its ancestors.
 * @return {Number} object.scrollTop
 *         The total scrollTop values of the node and all of its ancestors.
 */
function getAbsoluteScrollOffsetsForNode(node) {
  const doc = node.ownerDocument;

  // Our walker will only iterate up to document.body so we start by saving the
  // scroll values for `document.documentElement`.
  let scrollTop = doc.documentElement.scrollTop;
  let scrollLeft = doc.documentElement.scrollLeft;
  const walker = doc.createTreeWalker(doc.body, NodeFilter.SHOW_ELEMENT);
  walker.currentNode = node;
  let currentNode = walker.currentNode;

  // Iterate from `node` up the tree to `document.body` adding scroll offsets
  // as we go.
  while (currentNode) {
    const nodeScrollTop = currentNode.scrollTop;
    const nodeScrollLeft = currentNode.scrollLeft;

    if (nodeScrollTop || nodeScrollLeft) {
      scrollTop += nodeScrollTop;
      scrollLeft += nodeScrollLeft;
    }

    currentNode = walker.parentNode();
  }

  return {
    scrollLeft,
    scrollTop,
  };
}
exports.getAbsoluteScrollOffsetsForNode = getAbsoluteScrollOffsetsForNode;

/**
 * Check if the provided node is a <frame> or <iframe> element.
 *
 * @param {DOMNode} node
 * @returns {Boolean}
 */
function isFrame(node) {
  const className = ChromeUtils.getClassName(node);
  return className == "HTMLIFrameElement" || className == "HTMLFrameElement";
}

/**
 * Check if the provided node is representing a remote <browser> element.
 *
 * @param  {DOMNode} node
 * @return {Boolean}
 */
function isRemoteBrowserElement(node) {
  return (
    ChromeUtils.getClassName(node) == "XULFrameElement" &&
    !node.childNodes.length &&
    node.getAttribute("remote") == "true"
  );
}
exports.isRemoteBrowserElement = isRemoteBrowserElement;

/**
 * Check if the provided node is representing a remote frame.
 *
 * - In the context of the browser toolbox, a remote frame can be the <browser remote>
 * element found inside each tab.
 * - In the context of the content toolbox, a remote frame can be a <iframe> that contains
 * a different origin document.
 *
 * @param  {DOMNode} node
 * @return {Boolean}
 */
function isRemoteFrame(node) {
  if (isFrame(node)) {
    return node.frameLoader?.isRemoteFrame;
  }

  if (isRemoteBrowserElement(node)) {
    return true;
  }

  return false;
}
exports.isRemoteFrame = isRemoteFrame;

/**
 * Check if the provided node is representing a frame that has its own dedicated child target.
 *
 * @param {BrowsingContextTargetActor} targetActor
 * @param {DOMNode} node
 * @returns {Boolean}
 */
function isFrameWithChildTarget(targetActor, node) {
  // If the iframe is blocked because of CSP, it won't have a document (and no associated targets)
  if (isFrameBlockedByCSP(node)) {
    return false;
  }

  return isRemoteFrame(node) || (isFrame(node) && targetActor.ignoreSubFrames);
}

exports.isFrameWithChildTarget = isFrameWithChildTarget;

/**
 * Check if the provided node is representing a frame that is blocked by CSP.
 *
 * @param {DOMNode} node
 * @returns {Boolean}
 */
function isFrameBlockedByCSP(node) {
  if (!isFrame(node)) {
    return false;
  }

  if (!node.src) {
    return false;
  }

  let uri;
  try {
    uri = NetUtil.newURI(node.src);
  } catch (e) {
    return false;
  }

  const res = node.ownerDocument.csp.shouldLoad(
    Ci.nsIContentPolicy.TYPE_SUBDOCUMENT,
    null, // nsICSPEventListener
    uri,
    null, // aOriginalURIIfRedirect
    false, // aSendViolationReports
    null, // aNonce
    false // aParserCreated
  );

  return res !== Ci.nsIContentPolicy.ACCEPT;
}

exports.isFrameBlockedByCSP = isFrameBlockedByCSP;
