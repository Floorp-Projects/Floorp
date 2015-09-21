/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Utilities for interfacing with census reports from dbg.memory.takeCensus().
 */

const COARSE_TYPES = new Set(["objects", "scripts", "strings", "other"]);

/**
 * Takes a report from a census (`dbg.memory.takeCensus()`) and the breakdown
 * used to generate the census and returns a structure used to render
 * a tree to display the data.
 *
 * Returns a recursive "CensusTreeNode" object, looking like:
 *
 * CensusTreeNode = {
 *   // `children` if it exists, is sorted by `bytes`, if they are leaf nodes.
 *   children: ?[<CensusTreeNode...>],
 *   name: <?String>
 *   count: <?Number>
 *   bytes: <?Number>
 * }
 *
 * @param {Object} breakdown
 * @param {Object} report
 * @param {?String} name
 * @return {Object}
 */
function CensusTreeNode (breakdown, report, name) {
  this.name = name;
  this.bytes = void 0;
  this.count = void 0;
  this.children = void 0;

  CensusTreeNodeBreakdowns[breakdown.by](this, breakdown, report);

  if (this.children) {
    this.children.sort(sortByBytes);
  }
}

CensusTreeNode.prototype = null;

/**
 * A series of functions to handle different breakdowns used by CensusTreeNode
 */
const CensusTreeNodeBreakdowns = Object.create(null);

CensusTreeNodeBreakdowns.count = function (node, breakdown, report) {
  if (breakdown.bytes === true) {
    node.bytes = report.bytes;
  }
  if (breakdown.count === true) {
    node.count = report.count;
  }
};

CensusTreeNodeBreakdowns.internalType = function (node, breakdown, report) {
  node.children = [];
  for (let key of Object.keys(report)) {
    node.children.push(new CensusTreeNode(breakdown.then, report[key], key));
  }
}

CensusTreeNodeBreakdowns.objectClass = function (node, breakdown, report) {
  node.children = [];
  for (let key of Object.keys(report)) {
    let bd = key === "other" ? breakdown.other : breakdown.then;
    node.children.push(new CensusTreeNode(bd, report[key], key));
  }
}

CensusTreeNodeBreakdowns.coarseType = function (node, breakdown, report) {
  node.children = [];
  for (let type of Object.keys(breakdown).filter(type => COARSE_TYPES.has(type))) {
    node.children.push(new CensusTreeNode(breakdown[type], report[type], type));
  }
}

function sortByBytes (a, b) {
  return (b.bytes || 0) - (a.bytes || 0);
}

exports.CensusTreeNode = CensusTreeNode;
