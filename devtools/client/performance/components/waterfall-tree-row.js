/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * A single row (node) in the waterfall tree
 */

const { DOM: dom, PropTypes } = require("devtools/client/shared/vendor/react");
const { MarkerBlueprintUtils } = require("../modules/marker-blueprint-utils");

// px
const LEVEL_INDENT = 10;
// px
const ARROW_NODE_OFFSET = -14;
// px
const WATERFALL_MARKER_TIMEBAR_WIDTH_MIN = 5;

function buildMarkerSidebar(blueprint, props) {
  const { marker, level, sidebarWidth } = props;

  let bullet = dom.div({
    className: `waterfall-marker-bullet marker-color-${blueprint.colorName}`,
    style: { transform: `translateX(${level * LEVEL_INDENT}px)` },
    "data-type": marker.name
  });

  let label = MarkerBlueprintUtils.getMarkerLabel(marker);

  let name = dom.div({
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

  let bar = dom.div(
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

  let attrs = {
    className: "waterfall-tree-item" + (focused ? " focused" : ""),
    "data-otmt": marker.isOffMainThread
  };

  // Don't render an expando-arrow for leaf nodes.
  let submarkers = marker.submarkers;
  let hasDescendants = submarkers && submarkers.length > 0;
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
