/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * The "waterfall ticks" view, a header for the markers displayed in the waterfall.
 */

const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { L10N } = require("../modules/global");
const { TickUtils } = require("../modules/waterfall-ticks");

const WATERFALL_HEADER_TICKS_MULTIPLE = 5; // ms
const WATERFALL_HEADER_TICKS_SPACING_MIN = 50; // px
const WATERFALL_HEADER_TEXT_PADDING = 3; // px

function WaterfallHeader(props) {
  const { startTime, dataScale, sidebarWidth, waterfallWidth } = props;

  const tickInterval = TickUtils.findOptimalTickInterval({
    ticksMultiple: WATERFALL_HEADER_TICKS_MULTIPLE,
    ticksSpacingMin: WATERFALL_HEADER_TICKS_SPACING_MIN,
    dataScale: dataScale
  });

  const ticks = [];
  for (let x = 0; x < waterfallWidth; x += tickInterval) {
    const left = x + WATERFALL_HEADER_TEXT_PADDING;
    const time = Math.round(x / dataScale + startTime);
    const label = L10N.getFormatStr("timeline.tick", time);

    const node = dom.div({
      key: x,
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
