/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * The "waterfall ticks" view, a header for the markers displayed in the waterfall.
 */

const { DOM: dom, PropTypes } = require("devtools/client/shared/vendor/react");
const { L10N } = require("../modules/global");
const { TickUtils } = require("../modules/waterfall-ticks");

// ms
const WATERFALL_HEADER_TICKS_MULTIPLE = 5;
// px
const WATERFALL_HEADER_TICKS_SPACING_MIN = 50;
// px
const WATERFALL_HEADER_TEXT_PADDING = 3;

function WaterfallHeader(props) {
  let { startTime, dataScale, sidebarWidth, waterfallWidth } = props;

  let tickInterval = TickUtils.findOptimalTickInterval({
    ticksMultiple: WATERFALL_HEADER_TICKS_MULTIPLE,
    ticksSpacingMin: WATERFALL_HEADER_TICKS_SPACING_MIN,
    dataScale: dataScale
  });

  let ticks = [];
  for (let x = 0; x < waterfallWidth; x += tickInterval) {
    let left = x + WATERFALL_HEADER_TEXT_PADDING;
    let time = Math.round(x / dataScale + startTime);
    let label = L10N.getFormatStr("timeline.tick", time);

    let node = dom.div({
      className: "plain waterfall-header-tick",
      style: { transform: `translateX(${left}px)` }
    }, label);
    ticks.push(node);
  }

  return dom.div(
    { className: "waterfall-header" },
    dom.div(
      {
        className: "waterfall-sidebar theme-sidebar waterfall-header-name",
        style: { width: sidebarWidth + "px" }
      },
      L10N.getStr("timeline.records")
    ),
    dom.div(
      { className: "waterfall-header-ticks waterfall-background-ticks" },
      ticks
    )
  );
}

WaterfallHeader.displayName = "WaterfallHeader";

WaterfallHeader.propTypes = {
  startTime: PropTypes.number.isRequired,
  dataScale: PropTypes.number.isRequired,
  sidebarWidth: PropTypes.number.isRequired,
  waterfallWidth: PropTypes.number.isRequired,
};

module.exports = WaterfallHeader;
