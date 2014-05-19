/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

this.EXPORTED_SYMBOLS = [ "TranslationDocument" ];

const SHOW_ELEMENT = Ci.nsIDOMNodeFilter.SHOW_ELEMENT;
const SHOW_TEXT = Ci.nsIDOMNodeFilter.SHOW_TEXT;
const TEXT_NODE = Ci.nsIDOMNode.TEXT_NODE;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Promise.jsm");


/**
 * This class represents a document that is being translated,
 * and it is responsible for parsing the document,
 * generating the data structures translation (the list of
 * translation items and roots), and managing the original
 * and translated texts on the translation items.
 *
 * @param document  The document to be translated
 */
this.TranslationDocument = function(document) {
  this.itemsMap = new Map();
  this.roots = [];
  this._init(document);
};

this.TranslationDocument.prototype = {
  /**
   * Initializes the object and populates
   * the roots lists.
   *
   * @param document  The document to be translated
   */
  _init: function(document) {
    let window = document.defaultView;
    let winUtils = window.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIDOMWindowUtils);

    // Get all the translation nodes in the document's body:
    // a translation node is a node from the document which
    // contains useful content for translation, and therefore
    // must be included in the translation process.
    let nodeList = winUtils.getTranslationNodes(document.body);

    let length = nodeList.length;

    for (let i = 0; i < length; i++) {
      let node = nodeList.item(i);
      let isRoot = nodeList.isTranslationRootAtIndex(i);

      // Create a TranslationItem object for this node.
      // This function will also add it to the this.roots array.
      this._createItemForNode(node, i, isRoot);
    }

    // At first all roots are stored in the roots list, and only after
    // the process has finished we're able to determine which roots are
    // simple, and which ones are not.

    // A simple root is defined by a root with no children items, which
    // basically represents an element from a page with only text content
    // inside.

    // This distinction is useful for optimization purposes: we treat a
    // simple root as plain-text in the translation process and with that
    // we are able to reduce their data payload sent to the translation service.

    for (let root of this.roots) {
      if (root.children.length == 0 &&
          root.nodeRef.childElementCount == 0) {
        root.isSimpleRoot = true;
      }
    }
  },

  /**
   * Creates a TranslationItem object, which should be called
   * for each node returned by getTranslationNodes.
   *
   * @param node        The DOM node for this item.
   * @param id          A unique, numeric id for this item.
   * @parem isRoot      A boolean saying whether this item is a root.
   *
   * @returns           A TranslationItem object.
   */
  _createItemForNode: function(node, id, isRoot) {
    if (this.itemsMap.has(node)) {
      return this.itemsMap.get(node);
    }

    let item = new TranslationItem(node, id, isRoot);

    if (isRoot) {
      // Root items do not have a parent item.
      this.roots.push(item);
    } else {
      let parentItem = this.itemsMap.get(node.parentNode);
      if (parentItem) {
        parentItem.children.push(item);
      }
    }

    this.itemsMap.set(node, item);
    return item;
  },

  /**
   * Generate the text string that represents a TranslationItem object.
   * Besides generating the string, it's also stored in the "original"
   * field of the TranslationItem object, which needs to be stored for
   * later to be used in the "Show Original" functionality.
   *
   * @param item     A TranslationItem object
   *
   * @returns        A string representation of the TranslationItem.
   */
  generateTextForItem: function(item) {
    if (item.isSimpleRoot) {
      let text = item.nodeRef.firstChild.nodeValue.trim();
      item.original = [text];
      return text;
    }

    let localName = item.isRoot ? "div" : "b";
    let str = '<' + localName + ' id="n' + item.id + '">';

    item.original = [];

    for (let child of item.nodeRef.childNodes) {
      if (child.nodeType == TEXT_NODE) {
        let x = child.nodeValue.trim();
        str += x;
        item.original.push(x);
        continue;
      }

      let objInMap = this.itemsMap.get(child);
      if (objInMap) {
        // If this childNode is present in the itemsMap, it means
        // it's a translation node: it has useful content for translation.
        // In this case, we need to stringify this node.
        item.original.push(objInMap);
        str += this.generateTextForItem(objInMap);
      } else {
        // Otherwise, if this node doesn't contain any useful content,
        // we can simply replace it by a placeholder node.
        // We can't simply eliminate this node from our string representation
        // because that could change the HTML structure (e.g., it would
        // probably merge two separate text nodes).
        str += '<br/>';
      }
    }

    str += '</' + localName + '>';
    return str;
  }
};

/**
 * This class represents an item for translation. It's basically our
 * wrapper class around a node returned by getTranslationNode, with
 * more data and structural information on it.
 */
function TranslationItem(node, id, isRoot) {
  this.nodeRef = node;
  this.id = id;
  this.isRoot = isRoot;
  this.children = [];
}

TranslationItem.prototype = {
  isRoot: false,
  isSimpleRoot: false,

  toString: function() {
    let rootType = this._isRoot
                   ? (this._isSimpleRoot ? ' (simple root)' : ' (non simple root)')
                   : '';
    return "[object TranslationItem: <" + this.nodeRef.localName + ">"
           + rootType + "]";
  }
}
