/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const Tree = createFactory(require("devtools/client/shared/components/VirtualizedTree"));
const WaterfallTreeRow = createFactory(require("./waterfall-tree-row"));

// Keep in sync with var(--waterfall-tree-row-height) in performance.css
const WATERFALL_TREE_ROW_HEIGHT = 15; // px

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
  let mStart = e.start | 0;
  let mEnd = e.end | 0;

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

class WaterfallTree extends Component {
  static get propTypes() {
    return {
      marker: PropTypes.object.isRequired,
      startTime: PropTypes.number.isRequired,
      endTime: PropTypes.number.isRequired,
      dataScale: PropTypes.number.isRequired,
      sidebarWidth: PropTypes.number.isRequired,
      waterfallWidth: PropTypes.number.isRequired,
      onFocus: PropTypes.func,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      focused: null,
      expanded: new Set()
    };

    this._getRoots = this._getRoots.bind(this);
    this._getParent = this._getParent.bind(this);
    this._getChildren = this._getChildren.bind(this);
    this._getKey = this._getKey.bind(this);
    this._isExpanded = this._isExpanded.bind(this);
    this._onExpand = this._onExpand.bind(this);
    this._onCollapse = this._onCollapse.bind(this);
    this._onFocus = this._onFocus.bind(this);
    this._filter = this._filter.bind(this);
    this._renderItem = this._renderItem.bind(this);
  }

  _getRoots(node) {
    let roots = this.props.marker.submarkers || [];
    return roots.filter(this._filter);
  }

  /**
   * Find the parent node of 'node' with a depth-first search of the marker tree
   */
  _getParent(node) {
    function findParent(marker) {
      if (marker.submarkers) {
        for (let submarker of marker.submarkers) {
          if (submarker === node) {
            return marker;
          }

          let parent = findParent(submarker);
          if (parent) {
            return parent;
          }
        }
      }

      return null;
    }

    let rootMarker = this.props.marker;
    let parent = findParent(rootMarker);

    // We are interested only in parent markers that are rendered,
    // which rootMarker is not. Return null if the parent is rootMarker.
    return parent !== rootMarker ? parent : null;
  }

  _getChildren(node) {
    let submarkers = node.submarkers || [];
    return submarkers.filter(this._filter);
  }

  _getKey(node) {
    return `marker-${node.index}`;
  }

  _isExpanded(node) {
    return this.state.expanded.has(node);
  }

  _onExpand(node) {
    this.setState(state => {
      let expanded = new Set(state.expanded);
      expanded.add(node);
      return { expanded };
    });
  }

  _onCollapse(node) {
    this.setState(state => {
      let expanded = new Set(state.expanded);
      expanded.delete(node);
      return { expanded };
    });
  }

  _onFocus(node) {
    this.setState({ focused: node });
    if (this.props.onFocus) {
      this.props.onFocus(node);
    }
  }

  _filter(node) {
    let { startTime, endTime } = this.props;
    return isMarkerInRange(node, startTime, endTime);
  }

  _renderItem(marker, level, focused, arrow, expanded) {
    let { startTime, dataScale, sidebarWidth } = this.props;
    return WaterfallTreeRow({
      marker,
      level,
      arrow,
      expanded,
      focused,
      startTime,
      dataScale,
      sidebarWidth
    });
  }

  render() {
    return Tree({
      getRoots: this._getRoots,
      getParent: this._getParent,
      getChildren: this._getChildren,
      getKey: this._getKey,
      isExpanded: this._isExpanded,
      onExpand: this._onExpand,
      onCollapse: this._onCollapse,
      onFocus: this._onFocus,
      renderItem: this._renderItem,
      focused: this.state.focused,
      itemHeight: WATERFALL_TREE_ROW_HEIGHT
    });
  }
}

module.exports = WaterfallTree;
