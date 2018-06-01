/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * A single row (node) in the waterfall tree
 */

const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { MarkerBlueprintUtils } = require("../modules/marker-blueprint-utils");

const LEVEL_INDENT = 10; // px
const ARROW_NODE_OFFSET = -14; // px
const WATERFALL_MARKER_TIMEBAR_WIDTH_MIN = 5; // px

function buildMarkerSidebar(blueprint, props) {
  const { marker, level, sidebarWidth } = props;

  const bullet = dom.div({
    className: `waterfall-marker-bullet marker-color-${blueprint.colorName}`,
    style: { transform: `translateX(${level * LEVEL_INDENT}px)` },
    "data-type": marker.name
  });

  const label = MarkerBlueprintUtils.getMarkerLabel(marker);

  const name = dom.div({
    className: "plain waterfall-marker-name",
    style: { transform: `translateX(${level * LEVEL_INDENT}px)` },
    title: label
  }, label);

  return dom.div({
    className: "waterfall-sidebar theme-sidebar",
    style: { width: sidebarWidth + "px" }
  }, bullet, name);
}

function buildMarkerTimebar(blueprint, props) {
  const { marker, startTime, dataScale, arrow } = props;
  const offset = (marker.start - startTime) * dataScale + ARROW_NODE_OFFSET;
  const width = Math.max((marker.end - marker.start) * dataScale,
                         WATERFALL_MARKER_TIMEBAR_WIDTH_MIN);

  const bar = dom.div(
    {
      className: "waterfall-marker-wrap",
      style: { transform: `translateX(${offset}px)` }
    },
    arrow,
    dom.div({
      className: `waterfall-marker-bar marker-color-${blueprint.colorName}`,
      style: { width: `${width}px` },
      "data-type": marker.name
    })
  );

  return dom.div(
    { className: "waterfall-marker waterfall-background-ticks" },
    bar
  );
}

function WaterfallTreeRow(props) {
  const { marker, focused } = props;
  const blueprint = MarkerBlueprintUtils.getBlueprintFor(marker);

  const attrs = {
    className: "waterfall-tree-item" + (focused ? " focused" : ""),
    "data-otmt": marker.isOffMainThread
  };

  // Don't render an expando-arrow for leaf nodes.
  const submarkers = marker.submarkers;
  const hasDescendants = submarkers && submarkers.length > 0;
  if (hasDescendants) {
    attrs["data-expandable"] = "";
  } else {
    attrs["data-invisible"] = "";
  }

  return dom.div(
    attrs,
    buildMarkerSidebar(blueprint, props),
    buildMarkerTimebar(blueprint, props)
  );
}

WaterfallTreeRow.displayName = "WaterfallTreeRow";

WaterfallTreeRow.propTypes = {
  marker: PropTypes.object.isRequired,
  level: PropTypes.number.isRequired,
  arrow: PropTypes.element.isRequired,
  expanded: PropTypes.bool.isRequired,
  focused: PropTypes.bool.isRequired,
  startTime: PropTypes.number.isRequired,
  dataScale: PropTypes.number.isRequired,
  sidebarWidth: PropTypes.number.isRequired,
};

module.exports = WaterfallTreeRow;
