/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = [ "TranslationDocument" ];

const TEXT_NODE = Ci.nsIDOMNode.TEXT_NODE;

ChromeUtils.import("resource://services-common/async.js");
Cu.importGlobalProperties(["DOMParser"]);

/**
 * This class represents a document that is being translated,
 * and it is responsible for parsing the document,
 * generating the data structures translation (the list of
 * translation items and roots), and managing the original
 * and translated texts on the translation items.
 *
 * @param document  The document to be translated
 */
var TranslationDocument = function(document) {
  this.itemsMap = new Map();
  this.roots = [];
  this._init(document);
};

this.TranslationDocument.prototype = {
  translatedFrom: "",
  translatedTo: "",
  translationError: false,
  originalShown: true,

  /**
   * Initializes the object and populates
   * the roots lists.
   *
   * @param document  The document to be translated
   */
  _init(document) {
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
  _createItemForNode(node, id, isRoot) {
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
   * If this function had already been called for the given item (determined
   * by the presence of the "original" array in the item), the text will
   * be regenerated from the "original" data instead of from the related
   * DOM nodes (because the nodes might contain translated data).
   *
   * @param item     A TranslationItem object
   *
   * @returns        A string representation of the TranslationItem.
   */
  generateTextForItem(item) {
    if (item.original) {
      return regenerateTextFromOriginalHelper(item);
    }

    if (item.isSimpleRoot) {
      let text = item.nodeRef.firstChild.nodeValue.trim();
      item.original = [text];
      return text;
    }

    let str = "";
    item.original = [];
    let wasLastItemPlaceholder = false;

    for (let child of item.nodeRef.childNodes) {
      if (child.nodeType == TEXT_NODE) {
        let x = child.nodeValue.trim();
        if (x != "") {
          item.original.push(x);
          str += x;
          wasLastItemPlaceholder = false;
        }
        continue;
      }

      let objInMap = this.itemsMap.get(child);
      if (objInMap && !objInMap.isRoot) {
        // If this childNode is present in the itemsMap, it means
        // it's a translation node: it has useful content for translation.
        // In this case, we need to stringify this node.
        // However, if this item is a root, we should skip it here in this
        // object's child list (and just add a placeholder for it), because
        // it will be stringfied separately for being a root.
        item.original.push(objInMap);
        str += this.generateTextForItem(objInMap);
        wasLastItemPlaceholder = false;
      } else if (!wasLastItemPlaceholder) {
        // Otherwise, if this node doesn't contain any useful content,
        // or if it is a root itself, we can replace it with a placeholder node.
        // We can't simply eliminate this node from our string representation
        // because that could change the HTML structure (e.g., it would
        // probably merge two separate text nodes).
        // It's not necessary to add more than one placeholder in sequence;
        // we can optimize them away.
        item.original.push(TranslationItem_NodePlaceholder);
        str += "<br>";
        wasLastItemPlaceholder = true;
      }
    }

    return generateTranslationHtmlForItem(item, str);
  },

  /**
   * Changes the document to display its translated
   * content.
   */
  showTranslation() {
    this.originalShown = false;
    this._swapDocumentContent("translation");
  },

  /**
   * Changes the document to display its original
   * content.
   */
  showOriginal() {
    this.originalShown = true;
    this._swapDocumentContent("original");
  },

  /**
   * Swap the document with the resulting translation,
   * or back with the original content.
   *
   * @param target   A string that is either "translation"
   *                 or "original".
   */
  _swapDocumentContent(target) {
    (async () => {
      // Let the event loop breath on every 100 nodes
      // that are replaced.
      const YIELD_INTERVAL = 100;
      let maybeYield = Async.jankYielder(YIELD_INTERVAL);
      for (let root of this.roots) {
        root.swapText(target);
        await maybeYield();
      }
    })();
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

  toString() {
    let rootType = "";
    if (this.isRoot) {
      if (this.isSimpleRoot) {
        rootType = " (simple root)";
      } else {
        rootType = " (non simple root)";
      }
    }
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
  parseResult(result) {
    if (this.isSimpleRoot) {
      this.translation = [result];
      return;
    }

    let domParser = new DOMParser();

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
  getChildById(id) {
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
  swapText(target) {
    swapTextForItem(this, target);
  }
};

/**
 * This object represents a placeholder item for translation. It's similar to
 * the TranslationItem class, but it represents nodes that have no meaningful
 * content for translation. These nodes will be replaced by "<br>" in a
 * translation request. It's necessary to keep them to use it as a mark
 * for correct positioning and spliting of text nodes.
 */
const TranslationItem_NodePlaceholder = {
  toString() {
    return "[object TranslationItem_NodePlaceholder]";
  }
};

/**
 * Generate the outer HTML representation for a given item.
 *
 * @param   item       A TranslationItem object.
 * param    content    The inner content for this item.
 * @returns string     The outer HTML needed for translation
 *                     of this item.
 */
function generateTranslationHtmlForItem(item, content) {
  let localName = item.isRoot ? "div" : "b";
  return "<" + localName + " id=n" + item.id + ">" +
         content +
         "</" + localName + ">";
}

 /**
 * Regenerate the text string that represents a TranslationItem object,
 * with data from its "original" array. The array must have already
 * been created by TranslationDocument.generateTextForItem().
 *
 * @param item     A TranslationItem object
 *
 * @returns        A string representation of the TranslationItem.
 */
function regenerateTextFromOriginalHelper(item) {
  if (item.isSimpleRoot) {
    return item.original[0];
  }

  let str = "";
  for (let child of item.original) {
    if (child instanceof TranslationItem) {
      str += regenerateTextFromOriginalHelper(child);
    } else if (child === TranslationItem_NodePlaceholder) {
      str += "<br>";
    } else {
      str += child;
    }
  }

  return generateTranslationHtmlForItem(item, str);
}

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
    } else if (child.localName == "br") {
      item.translation.push(TranslationItem_NodePlaceholder);
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
 * How it works:
 *
 * The function iterates through the target array (either the `original` or
 * `translation` array from the TranslationItem), while also keeping a pointer
 * to a current position in the child nodes from the actual DOM node that we
 * are modifying. This pointer is moved forward after each item of the array
 * is translated. If, at any given time, the pointer doesn't match the expected
 * node that was supposed to be seen, it means that the original and translated
 * contents have a different ordering, and thus we need to adjust that.
 *
 * A full example of the reordering process, swapping from Original to
 * Translation:
 *
 *    Original (en): <div>I <em>miss</em> <b>you</b></div>
 *
 * Translation (fr): <div><b>Tu</b> me <em>manques</em></div>
 *
 * Step 1:
 *   pointer points to firstChild of the DOM node, textnode "I "
 *   first item in item.translation is [object TranslationItem <b>]
 *
 *   pointer does not match the expected element, <b>. So let's move <b> to the
 *   pointer position.
 *
 *   Current state of the DOM:
 *     <div><b>you</b>I <em>miss</em> </div>
 *
 * Step 2:
 *   pointer moves forward to nextSibling, textnode "I " again.
 *   second item in item.translation is the string " me "
 *
 *   pointer points to a text node, and we were expecting a text node. Match!
 *   just replace the text content.
 *
 *   Current state of the DOM:
 *     <div><b>you</b> me <em>miss</em> </div>
 *
 * Step 3:
 *   pointer moves forward to nextSibling, <em>miss</em>
 *   third item in item.translation is [object TranslationItem <em>]
 *
 *   pointer points to the expected node. Match! Nothing to do.
 *
 * Step 4:
 *   all items in this item.translation were transformed. The remaining
 *   text nodes are cleared to "", and domNode.normalize() removes them.
 *
 *   Current state of the DOM:
 *     <div><b>you</b> me <em>miss</em></div>
 *
 * Further steps:
 *   After that, the function will visit the child items (from the visitStack),
 *   and the text inside the <b> and <em> nodes will be swapped as well,
 *   yielding the final result:
 *
 *     <div><b>Tu</b> me <em>manques</em></div>
 *
 *
 * @param item     A TranslationItem object
 * @param target   A string that is either "translation"
 *                 or "original".
 */
function swapTextForItem(item, target) {
  // visitStack is the stack of items that we still need to visit.
  // Let's start the process by adding the root item.
  let visitStack = [ item ];

  while (visitStack.length > 0) {
    let curItem = visitStack.shift();

    let domNode = curItem.nodeRef;
    if (!domNode) {
      // Skipping this item due to a missing node.
      continue;
    }

    if (!curItem[target]) {
      // Translation not found for this item. This could be due to
      // an error in the server response. For example, if a translation
      // was broken in various chunks, and one of the chunks failed,
      // the items from that chunk will be missing its "translation"
      // field.
      continue;
    }

    domNode.normalize();

    // curNode points to the child nodes of the DOM node that we are
    // modifying. During most of the process, while the target array is
    // being iterated (in the for loop below), it should walk together with
    // the array and be pointing to the correct node that needs to modified.
    // If it's not pointing to it, that means some sort of node reordering
    // will be necessary to produce the correct translation.
    // Note that text nodes don't need to be reordered, as we can just replace
    // the content of one text node with another.
    //
    // curNode starts in the firstChild...
    let curNode = domNode.firstChild;

    // ... actually, let's make curNode start at the first useful node (either
    // a non-blank text node or something else). This is not strictly necessary,
    // as the reordering algorithm would correctly handle this case. However,
    // this better aligns the resulting translation with the DOM content of the
    // page, avoiding cases that would need to be unecessarily reordered.
    //
    // An example of how this helps:
    //
    // ---- Original: <div>                <b>Hello </b> world.</div>
    //                       ^textnode 1      ^item 1      ^textnode 2
    //
    // - Translation: <div><b>Hallo </b> Welt.</div>
    //
    // Transformation process without this optimization:
    //   1 - start pointer at textnode 1
    //   2 - move item 1 to first position inside the <div>
    //
    //       Node now looks like: <div><b>Hello </b>[         ][ world.]</div>
    //                                         textnode 1^       ^textnode 2
    //
    //   3 - replace textnode 1 with " Welt."
    //   4 - clear remaining text nodes (in this case, textnode 2)
    //
    // Transformation process with this optimization:
    //   1 - start pointer at item 1
    //   2 - item 1 is already in position
    //   3 - replace textnode 2 with " Welt."
    //
    // which completely avoids any node reordering, and requires only one
    // text change instead of two (while also leaving the page closer to
    // its original state).
    while (curNode &&
           curNode.nodeType == TEXT_NODE &&
           curNode.nodeValue.trim() == "") {
      curNode = curNode.nextSibling;
    }

    // Now let's walk through all items in the `target` array of the
    // TranslationItem. This means either the TranslationItem.original or
    // TranslationItem.translation array.
    for (let targetItem of curItem[target]) {

      if (targetItem instanceof TranslationItem) {
        // If the array element is another TranslationItem object, let's
        // add it to the stack to be visited.
        visitStack.push(targetItem);

        let targetNode = targetItem.nodeRef;

            // If the node is not in the expected position, let's reorder
            // it into position...
        if (curNode != targetNode &&
            // ...unless the page has reparented this node under a totally
            // different node (or removed it). In this case, all bets are off
            // on being able to do anything correctly, so it's better not to
            // bring back the node to this parent.
            targetNode.parentNode == domNode) {

          // We don't need to null-check curNode because insertBefore(..., null)
          // does what we need in that case: reorder this node to the end
          // of child nodes.
          domNode.insertBefore(targetNode, curNode);
          curNode = targetNode;
        }

        // Move pointer forward. Since we do not add empty text nodes to the
        // list of translation items, we must skip them here too while
        // traversing the DOM in order to get better alignment between the
        // text nodes and the translation items.
        if (curNode) {
          curNode = getNextSiblingSkippingEmptyTextNodes(curNode);
        }

      } else if (targetItem === TranslationItem_NodePlaceholder) {
        // If the current item is a placeholder node, we need to move
        // our pointer "past" it, jumping from one side of a block of
        // elements + empty text nodes to the other side. Even if
        // non-placeholder elements exists inside the jumped block,
        // they will be pulled correctly later in the process when the
        // targetItem for those nodes are handled.

        while (curNode &&
               (curNode.nodeType != TEXT_NODE ||
                curNode.nodeValue.trim() == "")) {
          curNode = curNode.nextSibling;
        }

      } else {
        // Finally, if it's a text item, we just need to find the next
        // text node to use. Text nodes don't need to be reordered, so
        // the first one found can be used.
        while (curNode && curNode.nodeType != TEXT_NODE) {
          curNode = curNode.nextSibling;
        }

        // If none was found and we reached the end of the child nodes,
        // let's create a new one.
        if (!curNode) {
          // We don't know if the original content had a space or not,
          // so the best bet is to create the text node with " " which
          // will add one space at the beginning and one at the end.
          curNode = domNode.appendChild(domNode.ownerDocument.createTextNode(" "));
        }

        // A trailing and a leading space must be preserved because
        // they are meaningful in HTML.
        let preSpace = /^\s/.test(curNode.nodeValue) ? " " : "";
        let endSpace = /\s$/.test(curNode.nodeValue) ? " " : "";

        curNode.nodeValue = preSpace + targetItem + endSpace;
        curNode = getNextSiblingSkippingEmptyTextNodes(curNode);
      }
    }

    // The translated version of a node might have less text nodes than its
    // original version. If that's the case, let's clear the remaining nodes.
    if (curNode) {
      clearRemainingNonEmptyTextNodesFromElement(curNode);
    }

    // And remove any garbage "" nodes left after clearing.
    domNode.normalize();
  }
}

function getNextSiblingSkippingEmptyTextNodes(startSibling) {
  let item = startSibling.nextSibling;
  while (item &&
         item.nodeType == TEXT_NODE &&
         item.nodeValue.trim() == "") {
    item = item.nextSibling;
  }
  return item;
}

function clearRemainingNonEmptyTextNodesFromElement(startSibling) {
  let item = startSibling;
  while (item) {
    if (item.nodeType == TEXT_NODE &&
        item.nodeValue != "") {
      item.nodeValue = "";
    }
    item = item.nextSibling;
  }
}
