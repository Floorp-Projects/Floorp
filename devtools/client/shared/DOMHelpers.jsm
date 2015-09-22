/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Ci = Components.interfaces;
const Cu = Components.utils;
Cu.import("resource://gre/modules/Services.jsm");

this.EXPORTED_SYMBOLS = ["DOMHelpers"];

/**
 * DOMHelpers
 * Makes DOM traversal easier. Goes through iframes.
 *
 * @constructor
 * @param nsIDOMWindow aWindow
 *        The content window, owning the document to traverse.
 */
this.DOMHelpers = function DOMHelpers(aWindow) {
  if (!aWindow) {
    throw new Error("window can't be null or undefined");
  }
  this.window = aWindow;
};

DOMHelpers.prototype = {
  getParentObject: function Helpers_getParentObject(node)
  {
    let parentNode = node ? node.parentNode : null;

    if (!parentNode) {
      // Documents have no parentNode; Attr, Document, DocumentFragment, Entity,
      // and Notation. top level windows have no parentNode
      if (node && node == this.window.Node.DOCUMENT_NODE) {
        // document type
        if (node.defaultView) {
          let embeddingFrame = node.defaultView.frameElement;
          if (embeddingFrame)
            return embeddingFrame.parentNode;
        }
      }
      // a Document object without a parentNode or window
      return null;  // top level has no parent
    }

    if (parentNode.nodeType == this.window.Node.DOCUMENT_NODE) {
      if (parentNode.defaultView) {
        return parentNode.defaultView.frameElement;
      }
      // parent is document element, but no window at defaultView.
      return null;
    }

    if (!parentNode.localName)
      return null;

    return parentNode;
  },

  getChildObject: function Helpers_getChildObject(node, index, previousSibling,
                                                showTextNodesWithWhitespace)
  {
    if (!node)
      return null;

    if (node.contentDocument) {
      // then the node is a frame
      if (index == 0) {
        return node.contentDocument.documentElement;  // the node's HTMLElement
      }
      return null;
    }

    if (node.getSVGDocument) {
      let svgDocument = node.getSVGDocument();
      if (svgDocument) {
        // then the node is a frame
        if (index == 0) {
          return svgDocument.documentElement;  // the node's SVGElement
        }
        return null;
      }
    }

    let child = null;
    if (previousSibling)  // then we are walking
      child = this.getNextSibling(previousSibling);
    else
      child = this.getFirstChild(node);

    if (showTextNodesWithWhitespace)
      return child;

    for (; child; child = this.getNextSibling(child)) {
      if (!this.isWhitespaceText(child))
        return child;
    }

    return null;  // we have no children worth showing.
  },

  getFirstChild: function Helpers_getFirstChild(node)
  {
    let SHOW_ALL = Components.interfaces.nsIDOMNodeFilter.SHOW_ALL;
    this.treeWalker = node.ownerDocument.createTreeWalker(node,
      SHOW_ALL, null);
    return this.treeWalker.firstChild();
  },

  getNextSibling: function Helpers_getNextSibling(node)
  {
    let next = this.treeWalker.nextSibling();

    if (!next)
      delete this.treeWalker;

    return next;
  },

  isWhitespaceText: function Helpers_isWhitespaceText(node)
  {
    return node.nodeType == this.window.Node.TEXT_NODE &&
                            !/[^\s]/.exec(node.nodeValue);
  },

  destroy: function Helpers_destroy()
  {
    delete this.window;
    delete this.treeWalker;
  },

  /**
   * A simple way to be notified (once) when a window becomes
   * interactive (DOMContentLoaded).
   *
   * It is based on the chromeEventHandler. This is useful when
   * chrome iframes are loaded in content docshells (in Firefox
   * tabs for example).
   */
  onceDOMReady: function Helpers_onLocationChange(callback) {
    let window = this.window;
    let docShell = window.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIWebNavigation)
                         .QueryInterface(Ci.nsIDocShell);
    let onReady = function(event) {
      if (event.target == window.document) {
        docShell.chromeEventHandler.removeEventListener("DOMContentLoaded", onReady, false);
        // If in `callback` the URL of the window is changed and a listener to DOMContentLoaded
        // is attached, the event we just received will be also be caught by the new listener.
        // We want to avoid that so we execute the callback in the next queue.
        Services.tm.mainThread.dispatch(callback, 0);
      }
    }
    docShell.chromeEventHandler.addEventListener("DOMContentLoaded", onReady, false);
  }
};
