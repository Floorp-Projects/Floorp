/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci, Cc } = require("chrome");
const nodeFilterConstants = require("devtools/shared/dom-node-filter-constants");

loader.lazyRequireGetter(this, "setIgnoreLayoutChanges", "devtools/server/actors/reflow", true);
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
  if (!utilsCache.has(win)) {
    utilsCache.set(win, win.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDOMWindowUtils));
  }
  return utilsCache.get(win);
}

/**
 * like win.top, but goes through mozbrowsers and mozapps iframes.
 *
 * @param {DOMWindow} win
 * @return {DOMWindow}
 */
function getTopWindow(win) {
  let docShell = win.QueryInterface(Ci.nsIInterfaceRequestor)
                    .getInterface(Ci.nsIWebNavigation)
                    .QueryInterface(Ci.nsIDocShell);

  if (!docShell.isMozBrowser) {
    return win.top;
  }

  let topDocShell =
    docShell.getSameTypeRootTreeItemIgnoreBrowserBoundaries();

  return topDocShell
          ? topDocShell.contentViewer.DOMDocument.defaultView
          : null;
}

exports.getTopWindow = getTopWindow;

/**
 * Returns `true` is the window given is a top level window.
 * like win.top === win, but goes through mozbrowsers and mozapps iframes.
 *
 * @param {DOMWindow} win
 * @return {Boolean}
 */
const isTopWindow = win => win && getTopWindow(win) === win;
exports.isTopWindow = isTopWindow;

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

  let parent = getParentWindow(win);

  if (!parent || parent === win) {
    return false;
  }

  return isWindowIncluded(boundaryWindow, parent);
}
exports.isWindowIncluded = isWindowIncluded;

/**
 * like win.parent, but goes through mozbrowsers and mozapps iframes.
 *
 * @param {DOMWindow} win
 * @return {DOMWindow}
 */
function getParentWindow(win) {
  if (isTopWindow(win)) {
    return null;
  }

  let docShell = win.QueryInterface(Ci.nsIInterfaceRequestor)
                 .getInterface(Ci.nsIWebNavigation)
                 .QueryInterface(Ci.nsIDocShell);

  if (!docShell.isMozBrowser) {
    return win.parent;
  }

  let parentDocShell =
    docShell.getSameTypeParentIgnoreBrowserBoundaries();

  return parentDocShell
          ? parentDocShell.contentViewer.DOMDocument.defaultView
          : null;
}

exports.getParentWindow = getParentWindow;

/**
 * like win.frameElement, but goes through mozbrowsers and mozapps iframes.
 *
 * @param {DOMWindow} win
 *        The window to get the frame for
 * @return {DOMNode}
 *         The element in which the window is embedded.
 */
const getFrameElement = (win) =>
  isTopWindow(win) ? null : utilsFor(win).containerElement;
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
  let scale = getCurrentZoom(node);

  if (boundaryWindow === null) {
    boundaryWindow = getTopWindow(frameWin);
  } else if (typeof boundaryWindow === "undefined") {
    throw new Error("No boundaryWindow given. Use null for the default one.");
  }

  while (frameWin !== boundaryWindow) {
    let frameElement = getFrameElement(frameWin);
    if (!frameElement) {
      break;
    }

    // We are in an iframe.
    // We take into account the parent iframe position and its
    // offset (borders and padding).
    let frameRect = frameElement.getBoundingClientRect();

    let [offsetTop, offsetLeft] = getFrameContentOffset(frameElement);

    xOffset += frameRect.left + offsetLeft;
    yOffset += frameRect.top + offsetTop;

    frameWin = getParentWindow(frameWin);
  }

  return [xOffset * scale, yOffset * scale];
}
exports.getFrameOffsets = getFrameOffsets;

/**
 * Get box quads adjusted for iframes and zoom level.
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
 * @return {Array}
 *        An array of objects that have the same structure as quads returned by
 *        getBoxQuads. An empty array if the node has no quads or is invalid.
 */
function getAdjustedQuads(boundaryWindow, node, region) {
  if (!node || !node.getBoxQuads) {
    return [];
  }

  let quads = node.getBoxQuads({
    box: region
  });

  if (!quads.length) {
    return [];
  }

  let [xOffset, yOffset] = getFrameOffsets(boundaryWindow, node);
  let scale = getCurrentZoom(node);

  let adjustedQuads = [];
  for (let quad of quads) {
    adjustedQuads.push({
      p1: {
        w: quad.p1.w * scale,
        x: quad.p1.x * scale + xOffset,
        y: quad.p1.y * scale + yOffset,
        z: quad.p1.z * scale
      },
      p2: {
        w: quad.p2.w * scale,
        x: quad.p2.x * scale + xOffset,
        y: quad.p2.y * scale + yOffset,
        z: quad.p2.z * scale
      },
      p3: {
        w: quad.p3.w * scale,
        x: quad.p3.x * scale + xOffset,
        y: quad.p3.y * scale + yOffset,
        z: quad.p3.z * scale
      },
      p4: {
        w: quad.p4.w * scale,
        x: quad.p4.x * scale + xOffset,
        y: quad.p4.y * scale + yOffset,
        z: quad.p4.z * scale
      },
      bounds: {
        bottom: quad.bounds.bottom * scale + yOffset,
        height: quad.bounds.height * scale,
        left: quad.bounds.left * scale + xOffset,
        right: quad.bounds.right * scale + xOffset,
        top: quad.bounds.top * scale + yOffset,
        width: quad.bounds.width * scale,
        x: quad.bounds.x * scale + xOffset,
        y: quad.bounds.y * scale + yOffset
      }
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
  let clientRect = node.getBoundingClientRect();

  if (boundaryWindow === null) {
    boundaryWindow = getTopWindow(frameWin);
  } else if (typeof boundaryWindow === "undefined") {
    throw new Error("No boundaryWindow given. Use null for the default one.");
  }

  // Go up in the tree of frames to determine the correct rectangle.
  // clientRect is read-only, we need to be able to change properties.
  let rect = {
    top: clientRect.top + contentWindow.pageYOffset,
    left: clientRect.left + contentWindow.pageXOffset,
    width: clientRect.width,
    height: clientRect.height
  };

  // We iterate through all the parent windows.
  while (frameWin !== boundaryWindow) {
    let frameElement = getFrameElement(frameWin);
    if (!frameElement) {
      break;
    }

    // We are in an iframe.
    // We take into account the parent iframe position and its
    // offset (borders and padding).
    let frameRect = frameElement.getBoundingClientRect();

    let [offsetTop, offsetLeft] = getFrameContentOffset(frameElement);

    rect.top += frameRect.top + offsetTop;
    rect.left += frameRect.left + offsetLeft;

    frameWin = getParentWindow(frameWin);
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

  let scale = getCurrentZoom(node);

  // Find out the offset of the node in its current frame
  let offsetLeft = 0;
  let offsetTop = 0;
  let el = node;
  while (el && el.parentNode) {
    offsetLeft += el.offsetLeft;
    offsetTop += el.offsetTop;
    el = el.offsetParent;
  }

  // Also take scrolled containers into account
  el = node;
  while (el && el.parentNode) {
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
  xOffset += offsetLeft;
  yOffset += offsetTop;

  xOffset *= scale;
  yOffset *= scale;

  // Get the width and height
  let width = node.offsetWidth * scale;
  let height = node.offsetHeight * scale;

  return {
    p1: {x: xOffset, y: yOffset},
    p2: {x: xOffset + width, y: yOffset},
    p3: {x: xOffset + width, y: yOffset + height},
    p4: {x: xOffset, y: yOffset + height}
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

  let walker = Cc["@mozilla.org/inspector/deep-tree-walker;1"]
               .createInstance(Ci.inIDeepTreeWalker);
  walker.showSubDocuments = true;
  walker.showDocumentsAsNodes = true;
  walker.init(frame, nodeFilterConstants.SHOW_ALL);
  walker.currentNode = frame;

  let document = walker.nextNode();
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
  let style = safelyGetContentWindow(frame).getComputedStyle(frame, null);

  // In some cases, the computed style is null
  if (!style) {
    return [0, 0];
  }

  let paddingTop = parseInt(style.getPropertyValue("padding-top"), 10);
  let paddingLeft = parseInt(style.getPropertyValue("padding-left"), 10);

  let borderTop = parseInt(style.getPropertyValue("border-top-width"), 10);
  let borderLeft = parseInt(style.getPropertyValue("border-left-width"), 10);

  return [borderTop + paddingTop, borderLeft + paddingLeft];
}

/**
 * Find an element from the given coordinates. This method descends through
 * frames to find the element the user clicked inside frames.
 *
 * @param {DOMDocument} document
 *        The document to look into.
 * @param {Number} x
 * @param {Number} y
 * @return {DOMNode}
 *         the element node found at the given coordinates, or null if no node
 *         was found
 */
function getElementFromPoint(document, x, y) {
  let node = document.elementFromPoint(x, y);
  if (node && node.contentDocument) {
    if (node instanceof Ci.nsIDOMHTMLIFrameElement) {
      let rect = node.getBoundingClientRect();

      // Gap between the frame and its content window.
      let [offsetTop, offsetLeft] = getFrameContentOffset(node);

      x -= rect.left + offsetLeft;
      y -= rect.top + offsetTop;

      if (x < 0 || y < 0) {
        // Didn't reach the content document, still over the frame.
        return node;
      }
    }
    if (node instanceof Ci.nsIDOMHTMLIFrameElement ||
        node instanceof Ci.nsIDOMHTMLFrameElement) {
      let subnode = getElementFromPoint(node.contentDocument, x, y);
      if (subnode) {
        node = subnode;
      }
    }
  }
  return node;
}
exports.getElementFromPoint = getElementFromPoint;

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
    return !(node.compareDocumentPosition(node.ownerDocument.documentElement) &
             node.DOCUMENT_POSITION_DISCONNECTED);
  } catch (e) {
    // "can't access dead object" error
    return false;
  }
}
exports.isNodeConnected = isNodeConnected;

/**
 * Traverse getBindingParent until arriving upon the bound element
 * responsible for the generation of the specified node.
 * See https://developer.mozilla.org/en-US/docs/XBL/XBL_1.0_Reference/DOM_Interfaces#getBindingParent.
 *
 * @param {DOMNode} node
 * @return {DOMNode}
 *         If node is not anonymous, this will return node. Otherwise,
 *         it will return the bound element
 *
 */
function getRootBindingParent(node) {
  let parent;
  let doc = node.ownerDocument;
  if (!doc) {
    return node;
  }
  while ((parent = doc.getBindingParent(node))) {
    node = parent;
  }
  return node;
}
exports.getRootBindingParent = getRootBindingParent;

function getBindingParent(node) {
  let doc = node.ownerDocument;
  if (!doc) {
    return null;
  }

  // If there is no binding parent then it is not anonymous.
  let parent = doc.getBindingParent(node);
  if (!parent) {
    return null;
  }

  return parent;
}
exports.getBindingParent = getBindingParent;

/**
 * Determine whether a node is anonymous by determining if there
 * is a bindingParent.
 *
 * @param {DOMNode} node
 * @return {Boolean}
 *
 */
const isAnonymous = (node) => getRootBindingParent(node) !== node;
exports.isAnonymous = isAnonymous;

/**
 * Determine whether a node has a bindingParent.
 *
 * @param {DOMNode} node
 * @return {Boolean}
 *
 */
const hasBindingParent = (node) => !!getBindingParent(node);

/**
 * Determine whether a node is native anonymous content (as opposed
 * to XBL anonymous or shadow DOM).
 * Native anonymous content includes elements like internals to form
 * controls and ::before/::after.
 *
 * @param {DOMNode} node
 * @return {Boolean}
 *
 */
const isNativeAnonymous = (node) =>
  hasBindingParent(node) && !(isXBLAnonymous(node) || isShadowAnonymous(node));

exports.isNativeAnonymous = isNativeAnonymous;

/**
 * Determine whether a node is XBL anonymous content (as opposed
 * to native anonymous or shadow DOM).
 * See https://developer.mozilla.org/en-US/docs/XBL/XBL_1.0_Reference/Anonymous_Content.
 *
 * @param {DOMNode} node
 * @return {Boolean}
 *
 */
function isXBLAnonymous(node) {
  let parent = getBindingParent(node);
  if (!parent) {
    return false;
  }

  // Shadow nodes also show up in getAnonymousNodes, so return false.
  if (parent.shadowRoot && parent.shadowRoot.contains(node)) {
    return false;
  }

  let anonNodes = [...node.ownerDocument.getAnonymousNodes(parent) || []];
  return anonNodes.indexOf(node) > -1;
}
exports.isXBLAnonymous = isXBLAnonymous;

/**
 * Determine whether a node is a child of a shadow root.
 * See https://w3c.github.io/webcomponents/spec/shadow/
 *
 * @param {DOMNode} node
 * @return {Boolean}
 */
function isShadowAnonymous(node) {
  let parent = getBindingParent(node);
  if (!parent) {
    return false;
  }

  // If there is a shadowRoot and this is part of it then this
  // is not native anonymous
  return parent.shadowRoot && parent.shadowRoot.contains(node);
}
exports.isShadowAnonymous = isShadowAnonymous;

/**
 * Get the current zoom factor applied to the container window of a given node.
 * Container windows are used as a weakmap key to store the corresponding
 * nsIDOMWindowUtils instance to avoid querying it every time.
 *
 * @param {DOMNode|DOMWindow}
 *        The node for which the zoom factor should be calculated, or its
 *        owner window.
 * @return {Number}
 */
function getCurrentZoom(node) {
  let win = getWindowFor(node);

  if (!win) {
    throw new Error("Unable to get the zoom from the given argument.");
  }

  return utilsFor(win).fullZoom;
}
exports.getCurrentZoom = getCurrentZoom;

/**
 * Return the default view for a given node, where node can be:
 * - a DOM node
 * - the document node
 * - the window itself
 * @param {DOMNode|DOMWindow|DOMDocument} node The node to get the window for.
 * @return {DOMWindow}
 */
function getWindowFor(node) {
  if (node instanceof Ci.nsIDOMNode) {
    if (node.nodeType === node.DOCUMENT_NODE) {
      return node.defaultView;
    }
    return node.ownerDocument.defaultView;
  } else if (node instanceof Ci.nsIDOMWindow) {
    return node;
  }
  return null;
}
