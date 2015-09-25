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
const { Heritage } = require("resource:///modules/devtools/client/shared/widgets/ViewHelpers.jsm");
const { AbstractTreeItem } = require("resource:///modules/devtools/client/shared/widgets/AbstractTreeItem.jsm");

const INDENTATION = exports.INDENTATION = 16; // px
const DEFAULT_AUTO_EXPAND_DEPTH = 2;
const COURSE_TYPES = ["objects", "scripts", "strings", "other"];

/**
 * Every instance of a `CensusView` represents a row in the census tree. The same
 * parent node is used for all rows.
 *
 * @param CensusView parent
 *        The CensusView considered the parent row. Should be null
 *        for root node.
 * @param {CensusTreeNode} censusTreeNode
 *        Data from `takeCensus` transformed via `CensusTreeNode`.
 *        @see browser/toolkit/heapsnapshot/census-tree-node.js
 * @param number level [optional]
 *        The indentation level in the call tree. The root node is at level 0.
 * @param boolean hidden [optional]
 *        Whether this node should be hidden and not contribute to depth/level
 *        calculations. Defaults to false.
 * @param number autoExpandDepth [optional]
 *        The depth to which the tree should automatically expand. Defaults to
 *        the caller's `autoExpandDepth` if a caller exists, otherwise defaults
 *        to DEFAULT_AUTO_EXPAND_DEPTH.
 */
function CensusView ({ caller, censusTreeNode, level, hidden, autoExpandDepth }) {
  AbstractTreeItem.call(this, { parent: caller, level: level|0 - (hidden ? 1 : 0) });

  this.autoExpandDepth = autoExpandDepth != null
    ? autoExpandDepth
    : caller ? caller.autoExpandDepth
             : DEFAULT_AUTO_EXPAND_DEPTH;

  this.caller = caller;
  this.censusTreeNode = censusTreeNode;
  this.hidden = hidden;
};

CensusView.prototype = Heritage.extend(AbstractTreeItem.prototype, {
  /**
   * Creates the view for this tree node.
   * @param nsIDOMNode document
   * @param nsIDOMNode arrowNode
   * @return nsIDOMNode
   */
  _displaySelf: function (document, arrowNode) {
    let data = this.censusTreeNode;

    let cells = [];

    // Only use an arrow if there are children
    if (data.children && data.children.length) {
      cells.push(arrowNode);
    }

    cells.push(this._createCell(document, data.name, "name"));

    if (data.bytes != null) {
      cells.push(this._createCell(document, data.bytes, "bytes"));
    }
    if (data.count != null) {
      cells.push(this._createCell(document, data.count, "count"));
    }

    let targetNode = document.createElement("li");
    targetNode.className = "heap-tree-item";
    targetNode.style.MozMarginStart = `${this.level * INDENTATION}px`;
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
  _populateSelf: function (children) {
    let newLevel = this.level + 1;
    let data = this.censusTreeNode;

    if (data.children) {
      for (let node of data.children) {
        children.push(new CensusView({
          caller: this,
          level: newLevel,
          censusTreeNode: node,
        }));
      }
    }
  },

  /**
   * Functions creating each cell in this call view.
   * Invoked by `_displaySelf`.
   */
  _createCell: function (doc, value, type) {
    let cell = doc.createElement("span");
    cell.className = "plain heap-tree-cell";
    cell.setAttribute("type", type);
    cell.innerHTML = value;
    return cell;
  },
});

exports.CensusView = CensusView;
