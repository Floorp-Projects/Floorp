 /* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const InspectorUtils = require("InspectorUtils");
loader.lazyRequireGetter(this, "loadSheet", "devtools/shared/layout/utils", true);
loader.lazyRequireGetter(this, "removeSheet", "devtools/shared/layout/utils", true);

// How many text runs are we highlighting at a time. There may be many text runs, and we
// want to prevent performance problems.
const MAX_TEXT_RANGES = 100;

// This stylesheet is inserted into the page to customize the color of the selected text
// runs.
// Note that this color is defined as --highlighter-content-color in the highlighters.css
// file, and corresponds to the box-model content color. We want to give it an opacity of
// 0.6 here.
const STYLESHEET_URI = "data:text/css," +
  encodeURIComponent("::selection{background-color:hsl(197,71%,73%,.6)!important;}");

/**
 * This highlighter highlights runs of text in the page that have been rendered given a
 * certain font. The highlighting is done with window selection ranges, so no extra
 * markup is being inserted into the content page.
 */
class FontsHighlighter {
  constructor(highlighterEnv) {
    this.env = highlighterEnv;
  }

  destroy() {
    this.hide();
    this.env = this.currentNode = null;
  }

  get currentNodeDocument() {
    if (!this.currentNode) {
      return this.env.document;
    }

    if (this.currentNode.nodeType === this.currentNode.DOCUMENT_NODE) {
      return this.currentNode;
    }

    return this.currentNode.ownerDocument;
  }

  /**
   * Show the highlighter for a given node.
   * @param {DOMNode} node The node in which we want to search for text runs.
   * @param {Object} options A bunch of options that can be set:
   * - {String} name The actual font name to look for in the node.
   * - {String} CSSFamilyName The CSS font-family name given to this font.
   */
  show(node, options) {
    this.currentNode = node;
    const doc = this.currentNodeDocument;

    // Get all of the fonts used to render content inside the node.
    const searchRange = doc.createRange();
    searchRange.selectNodeContents(node);

    const fonts = InspectorUtils.getUsedFontFaces(searchRange, MAX_TEXT_RANGES);

    // Find the ones we want, based on the provided option.
    const matchingFonts = fonts.filter(f => f.CSSFamilyName === options.CSSFamilyName &&
                                          f.name === options.name);
    if (!matchingFonts.length) {
      return;
    }

    // Load the stylesheet that will customize the color of the highlighter (using a
    // ::selection rule).
    loadSheet(this.env.window, STYLESHEET_URI);

    // Create a multi-selection in the page to highlight the text runs.
    const selection = doc.defaultView.getSelection();
    selection.removeAllRanges();

    for (const matchingFont of matchingFonts) {
      for (const range of matchingFont.ranges) {
        selection.addRange(range);
      }
    }
  }

  hide() {
    // No node was highlighted before, don't need to continue any further.
    if (!this.currentNode) {
      return;
    }

    try {
      removeSheet(this.env.window, STYLESHEET_URI);
    } catch (e) {
      // Silently fail here as we might not have inserted the stylesheet at all.
    }

    // Simply remove all current ranges in the seletion.
    const doc = this.currentNodeDocument;
    const selection = doc.defaultView.getSelection();
    selection.removeAllRanges();
  }
}

exports.FontsHighlighter = FontsHighlighter;
