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
Cu.import("resource://services-common/utils.js");
Cu.import("resource://gre/modules/Task.jsm");

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
  },

  /**
   * Changes the document to display its translated
   * content.
   */
  showTranslation: function() {
    this._swapDocumentContent("translation");
  },

  /**
   * Changes the document to display its original
   * content.
   */
  showOriginal: function() {
    this._swapDocumentContent("original");
  },

  /**
   * Swap the document with the resulting translation,
   * or back with the original content.
   *
   * @param target   A string that is either "translation"
   *                 or "original".
   */
  _swapDocumentContent: function(target) {
    Task.spawn(function *() {
      // Let the event loop breath on every 100 nodes
      // that are replaced.
      const YIELD_INTERVAL = 100;
      let count = YIELD_INTERVAL;

      for (let root of this.roots) {
        root.swapText(target);
        if (count-- == 0) {
          count = YIELD_INTERVAL;
          yield CommonUtils.laterTickResolvingPromise();
        }
      }
    }.bind(this));
  }
};

/**
 * This class represents an item for translation. It's basically our
 * wrapper class around a node returned by getTranslationNode, with
 * more data and structural information on it.
 *
 * At the end of the translation process, besides the properties below,
 * a TranslationItem will contain two other properties: one called "original"
 * and one called "translation". They are twin objects, one which reflect
 * the structure of that node in its original state, and the other in its
 * translated state.
 *
 * The "original" array is generated in the generateTextForItem function,
 * and the "translation" array is generated when the translation results
 * are parsed.
 *
 * They are both arrays, which contain a mix of strings and references to
 * child TranslationItems. The references in both arrays point to the * same *
 * TranslationItem object, but they might appear in different orders between the
 * "original" and "translation" arrays.
 *
 * An example:
 *
 *  English: <div id="n1">Welcome to <b id="n2">Mozilla's</b> website</div>
 *  Portuguese: <div id="n1">Bem vindo a pagina <b id="n2">da Mozilla</b></div>
 *
 *  TranslationItem n1 = {
 *    id: 1,
 *    original: ["Welcome to", ptr to n2, "website"]
 *    translation: ["Bem vindo a pagina", ptr to n2]
 *  }
 *
 *  TranslationItem n2 = {
 *    id: 2,
 *    original: ["Mozilla's"],
 *    translation: ["da Mozilla"]
 *  }
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
  },

  /**
   * This function will parse the result of the translation of one translation
   * item. If this item was a simple root, all we sent was a plain-text version
   * of it, so the result is also straightforward text.
   *
   * For non-simple roots, we sent a simplified HTML representation of that
   * node, and we'll first parse that into an HTML doc and then call the
   * parseResultNode helper function to parse it.
   *
   * While parsing, the result is stored in the "translation" field of the
   * TranslationItem, which will be used to display the final translation when
   * all items are finished. It remains stored too to allow back-and-forth
   * switching between the "Show Original" and "Show Translation" functions.
   *
   * @param result    A string with the textual result received from the server,
   *                  which can be plain-text or a serialized HTML doc.
   */
  parseResult: function(result) {
    if (this.isSimpleRoot) {
      this.translation = [result];
      return;
    }

    let domParser = Cc["@mozilla.org/xmlextras/domparser;1"]
                      .createInstance(Ci.nsIDOMParser);

    let doc = domParser.parseFromString(result, "text/html");
    parseResultNode(this, doc.body.firstChild);
  },

  /**
   * This function finds a child TranslationItem
   * with the given id.
   * @param id        The id to look for, in the format "n#"
   * @returns         A TranslationItem with the given id, or null if
   *                  it was not found.
   */
  getChildById: function(id) {
    for (let child of this.children) {
      if (("n" + child.id) == id) {
        return child;
      }
    }
    return null;
  },

  /**
   * Swap the text of this TranslationItem between
   * its original and translated states.
   *
   * @param target   A string that is either "translation"
   *                 or "original".
   */
  swapText: function(target) {
    swapTextForItem(this, target);
  }
};

/**
 * Helper function to parse a HTML doc result.
 * How it works:
 *
 * An example result string is:
 *
 * <div id="n1">Hello <b id="n2">World</b> of Mozilla.</div>
 *
 * For an element node, we look at its id and find the corresponding
 * TranslationItem that was associated with this node, and then we
 * walk down it repeating the process.
 *
 * For text nodes we simply add it as a string.
 */
function parseResultNode(item, node) {
  item.translation = [];
  for (let child of node.childNodes) {
    if (child.nodeType == TEXT_NODE) {
      item.translation.push(child.nodeValue);
    } else {
      let translationItemChild = item.getChildById(child.id);

      if (translationItemChild) {
        item.translation.push(translationItemChild);
        parseResultNode(translationItemChild, child);
      }
    }
  }
}

/**
 * Helper function to swap the text of a TranslationItem
 * between its original and translated states.
 *
 * @param item     A TranslationItem object
 * @param target   A string that is either "translation"
 *                 or "original".
 */
function swapTextForItem(item, target) {
  // visitStack is the stack of items that we still need to visit.
  // Let's start the process by adding the root item.
  let visitStack = [ item ];
  let source = target == "translation" ? "original" : "translation";

  while (visitStack.length > 0) {
    let curItem = visitStack.shift();

    let domNode = curItem.nodeRef;
    if (!domNode) {
      // Skipping this item due to a missing node.
      continue;
    }

    let sourceNodeCount = 0;

    if (!curItem[target]) {
      // Translation not found for this item. This could be due to
      // an error in the server response. For example, if a translation
      // was broken in various chunks, and one of the chunks failed,
      // the items from that chunk will be missing its "translation"
      // field.
      continue;
    }

    // Now let's walk through all items in the `target` array of the
    // TranslationItem. This means either the TranslationItem.original or
    // TranslationItem.translation array.
    for (let child of curItem[target]) {
      // If the array element is another TranslationItem object, let's
      // add it to the stack to be visited
      if (child instanceof TranslationItem) {
        // Adding this child to the stack.
        visitStack.push(child);
        continue;
      }

      // If it's a string, say, the Nth string of the `target` array, let's
      // replace the Nth child TextNode of this element with this string.
      // During our translation process we skipped all empty text nodes, so we
      // must also skip them here. If there are not enough text nodes to be used,
      // a new text node will be created and appended to the end of the element.
      let targetTextNode = getNthNonEmptyTextNodeFromElement(sourceNodeCount++, domNode);

      // A trailing and a leading space must be preserved because they are meaningful in HTML.
      let preSpace = targetTextNode.nodeValue.startsWith(" ") ? " " : "";
      let endSpace = targetTextNode.nodeValue.endsWith(" ") ? " " : "";
      targetTextNode.nodeValue = preSpace + child + endSpace;
    }

    // The translated version of a node might have less text nodes than its original
    // version. If that's the case, let's clear the remaining nodes.
    if (sourceNodeCount > 0) {
      clearRemainingNonEmptyTextNodesFromElement(sourceNodeCount, domNode);
    }
  }
}

function getNthNonEmptyTextNodeFromElement(n, element) {
  for (let childNode of element.childNodes) {
    if (childNode.nodeType == Ci.nsIDOMNode.TEXT_NODE &&
        childNode.nodeValue.trim() != "") {
      if (n-- == 0)
        return childNode;
    }
  }

  // If there are not enough DOM nodes, let's create a new one.
  return element.appendChild(element.ownerDocument.createTextNode(""));
}

function clearRemainingNonEmptyTextNodesFromElement(start, element) {
  let count = 0;
  for (let childNode of element.childNodes) {
    if (childNode.nodeType == Ci.nsIDOMNode.TEXT_NODE &&
        childNode.nodeValue.trim() != "") {
      if (count++ >= start) {
        childNode.nodeValue = "";
      }
    }
  }
}
