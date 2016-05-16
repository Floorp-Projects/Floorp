/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * This file contains the tree view, displaying all the samples and frames
 * received from the proviler in a tree-like structure.
 */

const { Cc, Ci, Cu, Cr } = require("chrome");
const { L10N } = require("devtools/client/performance/modules/global");
const { Heritage } = require("devtools/client/shared/widgets/view-helpers");
const { AbstractTreeItem } = require("resource://devtools/client/shared/widgets/AbstractTreeItem.jsm");

const URL_LABEL_TOOLTIP = L10N.getStr("table.url.tooltiptext");
const VIEW_OPTIMIZATIONS_TOOLTIP = L10N.getStr("table.view-optimizations.tooltiptext2");

const CALL_TREE_INDENTATION = 16; // px

// Used for rendering values in cells
const FORMATTERS = {
  TIME: (value) => L10N.getFormatStr("table.ms2", L10N.numberWithDecimals(value, 2)),
  PERCENT: (value) => L10N.getFormatStr("table.percentage2", L10N.numberWithDecimals(value, 2)),
  NUMBER: (value) => value || 0,
  BYTESIZE: (value) => L10N.getFormatStr("table.bytes", (value || 0))
};

/**
 * Definitions for rendering cells. Triads of class name, property name from
 * `frame.getInfo()`, and a formatter function.
 */
const CELLS = {
  duration:       ["duration", "totalDuration", FORMATTERS.TIME],
  percentage:     ["percentage", "totalPercentage", FORMATTERS.PERCENT],
  selfDuration:   ["self-duration", "selfDuration", FORMATTERS.TIME],
  selfPercentage: ["self-percentage", "selfPercentage", FORMATTERS.PERCENT],
  samples:        ["samples", "samples", FORMATTERS.NUMBER],

  selfSize:            ["self-size", "selfSize", FORMATTERS.BYTESIZE],
  selfSizePercentage:  ["self-size-percentage", "selfSizePercentage", FORMATTERS.PERCENT],
  selfCount:           ["self-count", "selfCount", FORMATTERS.NUMBER],
  selfCountPercentage: ["self-count-percentage", "selfCountPercentage", FORMATTERS.PERCENT],
  size:                ["size", "totalSize", FORMATTERS.BYTESIZE],
  sizePercentage:      ["size-percentage", "totalSizePercentage", FORMATTERS.PERCENT],
  count:               ["count", "totalCount", FORMATTERS.NUMBER],
  countPercentage:     ["count-percentage", "totalCountPercentage", FORMATTERS.PERCENT],
};
const CELL_TYPES = Object.keys(CELLS);

const DEFAULT_SORTING_PREDICATE = (frameA, frameB) => {
  let dataA = frameA.getDisplayedData();
  let dataB = frameB.getDisplayedData();
  let isAllocations = "totalSize" in dataA;

  if (isAllocations) {
    return this.inverted && dataA.selfSize !== dataB.selfSize ?
           (dataA.selfSize < dataB.selfSize ? 1 : - 1) :
           (dataA.totalSize < dataB.totalSize ? 1 : -1);
  }

  return this.inverted && dataA.selfPercentage !== dataB.selfPercentage ?
         (dataA.selfPercentage < dataB.selfPercentage ? 1 : - 1) :
         (dataA.totalPercentage < dataB.totalPercentage ? 1 : -1);
};

const DEFAULT_AUTO_EXPAND_DEPTH = 3; // depth
const DEFAULT_VISIBLE_CELLS = {
  duration: true,
  percentage: true,
  selfDuration: true,
  selfPercentage: true,
  samples: true,
  function: true,

  // allocation columns
  count: false,
  selfCount: false,
  size: false,
  selfSize: false,
  countPercentage: false,
  selfCountPercentage: false,
  sizePercentage: false,
  selfSizePercentage: false,
};

const clamp = (val, min, max) => Math.max(min, Math.min(max, val));
const sum = vals => vals.reduce((a, b) => a + b, 0);

/**
 * An item in a call tree view, which looks like this:
 *
 *   Time (ms)  |   Cost   | Calls | Function
 * ============================================================================
 *     1,000.00 |  100.00% |       | ▼ (root)
 *       500.12 |   50.01% |   300 |   ▼ foo                          Categ. 1
 *       300.34 |   30.03% |  1500 |     ▼ bar                        Categ. 2
 *        10.56 |    0.01% |    42 |       ▶ call_with_children       Categ. 3
 *        90.78 |    0.09% |    25 |         call_without_children    Categ. 4
 *
 * Every instance of a `CallView` represents a row in the call tree. The same
 * parent node is used for all rows.
 *
 * @param CallView caller
 *        The CallView considered the "caller" frame. This newly created
 *        instance will be represent the "callee". Should be null for root nodes.
 * @param ThreadNode | FrameNode frame
 *        Details about this function, like { samples, duration, calls } etc.
 * @param number level [optional]
 *        The indentation level in the call tree. The root node is at level 0.
 * @param boolean hidden [optional]
 *        Whether this node should be hidden and not contribute to depth/level
 *        calculations. Defaults to false.
 * @param boolean inverted [optional]
 *        Whether the call tree has been inverted (bottom up, rather than
 *        top-down). Defaults to false.
 * @param function sortingPredicate [optional]
 *        The predicate used to sort the tree items when created. Defaults to
 *        the caller's `sortingPredicate` if a caller exists, otherwise defaults
 *        to DEFAULT_SORTING_PREDICATE. The two passed arguments are FrameNodes.
 * @param number autoExpandDepth [optional]
 *        The depth to which the tree should automatically expand. Defualts to
 *        the caller's `autoExpandDepth` if a caller exists, otherwise defaults
 *        to DEFAULT_AUTO_EXPAND_DEPTH.
 * @param object visibleCells
 *        An object specifying which cells are visible in the tree. Defaults to
 *        the caller's `visibleCells` if a caller exists, otherwise defaults
 *        to DEFAULT_VISIBLE_CELLS.
 * @param boolean showOptimizationHint [optional]
 *        Whether or not to show an icon indicating if the frame has optimization
 *        data.
 */
function CallView({
  caller, frame, level, hidden, inverted,
  sortingPredicate, autoExpandDepth, visibleCells,
  showOptimizationHint
}) {
  AbstractTreeItem.call(this, {
    parent: caller,
    level: level|0 - (hidden ? 1 : 0)
  });

  this.sortingPredicate = sortingPredicate != null
    ? sortingPredicate
    : caller ? caller.sortingPredicate
             : DEFAULT_SORTING_PREDICATE

  this.autoExpandDepth = autoExpandDepth != null
    ? autoExpandDepth
    : caller ? caller.autoExpandDepth
             : DEFAULT_AUTO_EXPAND_DEPTH;

  this.visibleCells = visibleCells != null
    ? visibleCells
    : caller ? caller.visibleCells
             : Object.create(DEFAULT_VISIBLE_CELLS);

  this.caller = caller;
  this.frame = frame;
  this.hidden = hidden;
  this.inverted = inverted;
  this.showOptimizationHint = showOptimizationHint;

  this._onUrlClick = this._onUrlClick.bind(this);
};

CallView.prototype = Heritage.extend(AbstractTreeItem.prototype, {
  /**
   * Creates the view for this tree node.
   * @param nsIDOMNode document
   * @param nsIDOMNode arrowNode
   * @return nsIDOMNode
   */
  _displaySelf: function(document, arrowNode) {
    let frameInfo = this.getDisplayedData();
    let cells = [];

    for (let type of CELL_TYPES) {
      if (this.visibleCells[type]) {
        // Inline for speed, but pass in the formatted value via
        // cell definition, as well as the element type.
        cells.push(this._createCell(document, CELLS[type][2](frameInfo[CELLS[type][1]]), CELLS[type][0]));
      }
    }

    if (this.visibleCells.function) {
      cells.push(this._createFunctionCell(document, arrowNode, frameInfo.name, frameInfo, this.level));
    }

    let targetNode = document.createElement("hbox");
    targetNode.className = "call-tree-item";
    targetNode.setAttribute("origin", frameInfo.isContent ? "content" : "chrome");
    targetNode.setAttribute("category", frameInfo.categoryData.abbrev || "");
    targetNode.setAttribute("tooltiptext", frameInfo.tooltiptext);

    if (this.hidden) {
      targetNode.style.display = "none";
    }

    for (let i = 0; i < cells.length; i++) {
      targetNode.appendChild(cells[i]);
    }

    return targetNode;
  },

  /**
   * Populates this node in the call tree with the corresponding "callees".
   * These are defined in the `frame` data source for this call view.
   * @param array:AbstractTreeItem children
   */
  _populateSelf: function(children) {
    let newLevel = this.level + 1;

    for (let newFrame of this.frame.calls) {
      children.push(new CallView({
        caller: this,
        frame: newFrame,
        level: newLevel,
        inverted: this.inverted
      }));
    }

    // Sort the "callees" asc. by samples, before inserting them in the tree,
    // if no other sorting predicate was specified on this on the root item.
    children.sort(this.sortingPredicate.bind(this));
  },

  /**
   * Functions creating each cell in this call view.
   * Invoked by `_displaySelf`.
   */
  _createCell: function (doc, value, type) {
    let cell = doc.createElement("description");
    cell.className = "plain call-tree-cell";
    cell.setAttribute("type", type);
    cell.setAttribute("crop", "end");
    // Add a tabulation to the cell text in case it's is selected and copied.
    cell.textContent = value + "\t";
    return cell;
  },

  _createFunctionCell: function(doc, arrowNode, frameName, frameInfo, frameLevel) {
    let cell = doc.createElement("hbox");
    cell.className = "call-tree-cell";
    cell.style.marginInlineStart = (frameLevel * CALL_TREE_INDENTATION) + "px";
    cell.setAttribute("type", "function");
    cell.appendChild(arrowNode);

    // Render optimization hint if this frame has opt data.
    if (this.root.showOptimizationHint && frameInfo.hasOptimizations && !frameInfo.isMetaCategory) {
      let icon = doc.createElement("description");
      icon.setAttribute("tooltiptext", VIEW_OPTIMIZATIONS_TOOLTIP);
      icon.className = "opt-icon";
      cell.appendChild(icon);
    }

    // Don't render a name label node if there's no function name. A different
    // location label node will be rendered instead.
    if (frameName) {
      let nameNode = doc.createElement("description");
      nameNode.className = "plain call-tree-name";
      nameNode.textContent = frameName;
      cell.appendChild(nameNode);
    }

    // Don't render detailed labels for meta category frames
    if (!frameInfo.isMetaCategory) {
      this._appendFunctionDetailsCells(doc, cell, frameInfo);
    }

    // Don't render an expando-arrow for leaf nodes.
    let hasDescendants = Object.keys(this.frame.calls).length > 0;
    if (!hasDescendants) {
      arrowNode.setAttribute("invisible", "");
    }

    // Add a line break to the last description of the row in case it's selected
    // and copied.
    let lastDescription = cell.querySelector('description:last-of-type');
    lastDescription.textContent = lastDescription.textContent + "\n";

    // Add spaces as frameLevel indicators in case the row is selected and
    // copied. These spaces won't be displayed in the cell content.
    let firstDescription = cell.querySelector('description:first-of-type');
    let levelIndicator = frameLevel > 0 ? " ".repeat(frameLevel) : "";
    firstDescription.textContent = levelIndicator + firstDescription.textContent;

    return cell;
  },

  _appendFunctionDetailsCells: function(doc, cell, frameInfo) {
    if (frameInfo.fileName) {
      let urlNode = doc.createElement("description");
      urlNode.className = "plain call-tree-url";
      urlNode.textContent = frameInfo.fileName;
      urlNode.setAttribute("tooltiptext", URL_LABEL_TOOLTIP + " → " + frameInfo.url);
      urlNode.addEventListener("mousedown", this._onUrlClick);
      cell.appendChild(urlNode);
    }

    if (frameInfo.line) {
      let lineNode = doc.createElement("description");
      lineNode.className = "plain call-tree-line";
      lineNode.textContent = ":" + frameInfo.line;
      cell.appendChild(lineNode);
    }

    if (frameInfo.column) {
      let columnNode = doc.createElement("description");
      columnNode.className = "plain call-tree-column";
      columnNode.textContent = ":" + frameInfo.column;
      cell.appendChild(columnNode);
    }

    if (frameInfo.host) {
      let hostNode = doc.createElement("description");
      hostNode.className = "plain call-tree-host";
      hostNode.textContent = frameInfo.host;
      cell.appendChild(hostNode);
    }

    if (frameInfo.categoryData.label) {
      let categoryNode = doc.createElement("description");
      categoryNode.className = "plain call-tree-category";
      categoryNode.style.color = frameInfo.categoryData.color;
      categoryNode.textContent = frameInfo.categoryData.label;
      cell.appendChild(categoryNode);
    }
  },

  /**
   * Gets the data displayed about this tree item, based on the FrameNode
   * model associated with this view.
   *
   * @return object
   */
  getDisplayedData: function() {
    if (this._cachedDisplayedData) {
      return this._cachedDisplayedData;
    }

    return this._cachedDisplayedData = this.frame.getInfo({
      root: this.root.frame,
      allocations: (this.visibleCells.count || this.visibleCells.selfCount)
    });

    /**
     * When inverting call tree, the costs and times are dependent on position
     * in the tree. We must only count leaf nodes with self cost, and total costs
     * dependent on how many times the leaf node was found with a full stack path.
     *
     *   Total |  Self | Calls | Function
     * ============================================================================
     *  100%   |  100% |   100 | ▼ C
     *   50%   |   0%  |    50 |   ▼ B
     *   50%   |   0%  |    50 |     ▼ A
     *   50%   |   0%  |    50 |   ▼ B
     *
     * Every instance of a `CallView` represents a row in the call tree. The same
     * container node is used for all rows.
     */
  },

  /**
   * Toggles the category information hidden or visible.
   * @param boolean visible
   */
  toggleCategories: function(visible) {
    if (!visible) {
      this.container.setAttribute("categories-hidden", "");
    } else {
      this.container.removeAttribute("categories-hidden");
    }
  },

  /**
   * Handler for the "click" event on the url node of this call view.
   */
  _onUrlClick: function(e) {
    e.preventDefault();
    e.stopPropagation();
    // Only emit for left click events
    if (e.button === 0) {
      this.root.emit("link", this);
    }
  },
});

exports.CallView = CallView;
