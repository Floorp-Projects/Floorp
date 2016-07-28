/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * This file contains the "marker" view, essentially a detailed list
 * of all the markers in the timeline data.
 */

const { Heritage } = require("devtools/client/shared/widgets/view-helpers");
const { AbstractTreeItem } = require("resource://devtools/client/shared/widgets/AbstractTreeItem.jsm");
const { MarkerBlueprintUtils } = require("devtools/client/performance/modules/marker-blueprint-utils");

// px
const LEVEL_INDENT = 10;
// px
const ARROW_NODE_OFFSET = -15;
// px
const WATERFALL_MARKER_SIDEBAR_SAFE_BOUNDS = 20;
// px
const WATERFALL_MARKER_SIDEBAR_WIDTH = 175;
// px
const WATERFALL_MARKER_TIMEBAR_WIDTH_MIN = 5;

/**
 * A detailed waterfall view for the timeline data.
 *
 * @param MarkerView owner
 *        The MarkerView considered the "owner" marker. This newly created
 *        instance will be represent the "submarker". Should be null for root nodes.
 * @param object marker
 *        Details about this marker, like { name, start, end, submarkers } etc.
 * @param number level [optional]
 *        The indentation level in the waterfall tree. The root node is at level 0.
 * @param boolean hidden [optional]
 *        Whether this node should be hidden and not contribute to depth/level
 *        calculations. Defaults to false.
 */
function MarkerView({ owner, marker, level, hidden }) {
  AbstractTreeItem.call(this, {
    parent: owner,
    level: level | 0 - (hidden ? 1 : 0)
  });

  this.marker = marker;
  this.hidden = !!hidden;

  this._onItemBlur = this._onItemBlur.bind(this);
  this._onItemFocus = this._onItemFocus.bind(this);
}

MarkerView.prototype = Heritage.extend(AbstractTreeItem.prototype, {
  /**
   * Calculates and stores the available width for the waterfall.
   * This should be invoked every time the container node is resized.
   */
  recalculateBounds: function () {
    this.root._waterfallWidth = this.bounds.width
      - WATERFALL_MARKER_SIDEBAR_WIDTH
      - WATERFALL_MARKER_SIDEBAR_SAFE_BOUNDS;
  },

  /**
   * Sets a list of marker types to be filtered out of this view.
   * @param Array<String> filter
   */
  set filter(filter) {
    this.root._filter = filter;
  },
  get filter() {
    return this.root._filter;
  },

  /**
   * Sets the { startTime, endTime }, in milliseconds.
   * @param object interval
   */
  set interval(interval) {
    this.root._interval = interval;
  },
  get interval() {
    return this.root._interval;
  },

  /**
   * Gets the current waterfall width.
   * @return number
   */
  getWaterfallWidth: function () {
    return this._waterfallWidth;
  },

  /**
   * Gets the data scale amount for the current width and interval.
   * @return number
   */
  getDataScale: function () {
    let startTime = this.root._interval.startTime|0;
    let endTime = this.root._interval.endTime|0;
    return this.root._waterfallWidth / (endTime - startTime);
  },

  /**
   * Creates the view for this waterfall node.
   * @param nsIDOMNode document
   * @param nsIDOMNode arrowNode
   * @return nsIDOMNode
   */
  _displaySelf: function (document, arrowNode) {
    let targetNode = document.createElement("hbox");
    targetNode.className = "waterfall-tree-item";
    targetNode.setAttribute("otmt", this.marker.isOffMainThread);

    if (this == this.root) {
      // Bounds are needed for properly positioning and scaling markers in
      // the waterfall, but it's sufficient to make those calculations only
      // for the root node.
      this.root.recalculateBounds();
      // The AbstractTreeItem propagates events to the root, so we don't
      // need to listen them on descendant items in the tree.
      this._addEventListeners();
    } else {
      // Root markers are an implementation detail and shouldn't be shown.
      this._buildMarkerCells(document, targetNode, arrowNode);
    }

    if (this.hidden) {
      targetNode.style.display = "none";
    }

    return targetNode;
  },

  /**
   * Populates this node in the waterfall tree with the corresponding "markers".
   * @param array:AbstractTreeItem children
   */
  _populateSelf: function (children) {
    let submarkers = this.marker.submarkers;
    if (!submarkers || !submarkers.length) {
      return;
    }
    let startTime = this.root._interval.startTime;
    let endTime = this.root._interval.endTime;
    let newLevel = this.level + 1;

    for (let i = 0, len = submarkers.length; i < len; i++) {
      let marker = submarkers[i];

      // Skip filtered markers
      if (!MarkerBlueprintUtils.shouldDisplayMarker(marker, this.filter)) {
        continue;
      }

      if (!isMarkerInRange(marker, startTime|0, endTime|0)) {
        continue;
      }

      children.push(new MarkerView({
        owner: this,
        marker: marker,
        level: newLevel,
        inverted: this.inverted
      }));
    }
  },

  /**
   * Builds all the nodes representing a marker in the waterfall.
   * @param nsIDOMNode document
   * @param nsIDOMNode targetNode
   * @param nsIDOMNode arrowNode
   */
  _buildMarkerCells: function (doc, targetNode, arrowNode) {
    let marker = this.marker;
    let blueprint = MarkerBlueprintUtils.getBlueprintFor(marker);
    let startTime = this.root._interval.startTime;
    let endTime = this.root._interval.endTime;

    let sidebarCell = this._buildMarkerSidebar(doc, blueprint, marker);
    let timebarCell = this._buildMarkerTimebar(doc, blueprint, marker, startTime,
                                               endTime, arrowNode);

    targetNode.appendChild(sidebarCell);
    targetNode.appendChild(timebarCell);

    // Don't render an expando-arrow for leaf nodes.
    let submarkers = this.marker.submarkers;
    let hasDescendants = submarkers && submarkers.length > 0;
    if (hasDescendants) {
      targetNode.setAttribute("expandable", "");
    } else {
      arrowNode.setAttribute("invisible", "");
    }

    targetNode.setAttribute("level", this.level);
  },

  /**
   * Functions creating each cell in this waterfall view.
   * Invoked by `_displaySelf`.
   */
  _buildMarkerSidebar: function (doc, style, marker) {
    let cell = doc.createElement("hbox");
    cell.className = "waterfall-sidebar theme-sidebar";
    cell.setAttribute("width", WATERFALL_MARKER_SIDEBAR_WIDTH);
    cell.setAttribute("align", "center");

    let bullet = doc.createElement("hbox");
    bullet.className = `waterfall-marker-bullet marker-color-${style.colorName}`;
    bullet.style.transform = `translateX(${this.level * LEVEL_INDENT}px)`;
    bullet.setAttribute("type", marker.name);
    cell.appendChild(bullet);

    let name = doc.createElement("description");
    let label = MarkerBlueprintUtils.getMarkerLabel(marker);
    name.className = "plain waterfall-marker-name";
    name.style.transform = `translateX(${this.level * LEVEL_INDENT}px)`;
    name.setAttribute("crop", "end");
    name.setAttribute("flex", "1");
    name.setAttribute("value", label);
    name.setAttribute("tooltiptext", label);
    cell.appendChild(name);

    return cell;
  },
  _buildMarkerTimebar: function (doc, style, marker, startTime, endTime, arrowNode) {
    let cell = doc.createElement("hbox");
    cell.className = "waterfall-marker waterfall-background-ticks";
    cell.setAttribute("align", "center");
    cell.setAttribute("flex", "1");

    let dataScale = this.getDataScale();
    let offset = (marker.start - startTime) * dataScale;
    let width = (marker.end - marker.start) * dataScale;

    arrowNode.style.transform = `translateX(${offset + ARROW_NODE_OFFSET}px)`;
    cell.appendChild(arrowNode);

    let bar = doc.createElement("hbox");
    bar.className = `waterfall-marker-bar marker-color-${style.colorName}`;
    bar.style.transform = `translateX(${offset}px)`;
    bar.setAttribute("type", marker.name);
    bar.setAttribute("width", Math.max(width, WATERFALL_MARKER_TIMEBAR_WIDTH_MIN));
    cell.appendChild(bar);

    return cell;
  },

  /**
   * Adds the event listeners for this particular tree item.
   */
  _addEventListeners: function () {
    this.on("focus", this._onItemFocus);
    this.on("blur", this._onItemBlur);
  },

  /**
   * Handler for the "blur" event on the root item.
   */
  _onItemBlur: function () {
    this.root.emit("unselected");
  },

  /**
   * Handler for the "mousedown" event on the root item.
   */
  _onItemFocus: function (e, item) {
    this.root.emit("selected", item.marker);
  }
});

/**
 * Checks if a given marker is in the specified time range.
 *
 * @param object e
 *        The marker containing the { start, end } timestamps.
 * @param number start
 *        The earliest allowed time.
 * @param number end
 *        The latest allowed time.
 * @return boolean
 *         True if the marker fits inside the specified time range.
 */
function isMarkerInRange(e, start, end) {
  let mStart = e.start|0;
  let mEnd = e.end|0;

  return (
    // bounds inside
    (mStart >= start && mEnd <= end) ||
    // bounds outside
    (mStart < start && mEnd > end) ||
    // overlap start
    (mStart < start && mEnd >= start && mEnd <= end) ||
    // overlap end
    (mEnd > end && mStart >= start && mStart <= end)
  );
}

exports.MarkerView = MarkerView;
exports.WATERFALL_MARKER_SIDEBAR_SAFE_BOUNDS = WATERFALL_MARKER_SIDEBAR_SAFE_BOUNDS;
exports.WATERFALL_MARKER_SIDEBAR_WIDTH = WATERFALL_MARKER_SIDEBAR_WIDTH;
