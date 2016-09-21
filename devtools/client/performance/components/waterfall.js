/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * This file contains the "waterfall" view, essentially a detailed list
 * of all the markers in the timeline data.
 */

const { DOM: dom, createFactory, PropTypes } = require("devtools/client/shared/vendor/react");
const WaterfallHeader = createFactory(require("./waterfall-header"));
const WaterfallTree = createFactory(require("./waterfall-tree"));

function Waterfall(props) {
  return dom.div(
    { className: "waterfall-markers" },
    WaterfallHeader(props),
    WaterfallTree(props)
  );
}

Waterfall.displayName = "Waterfall";

Waterfall.propTypes = {
  marker: PropTypes.object.isRequired,
  startTime: PropTypes.number.isRequired,
  endTime: PropTypes.number.isRequired,
  dataScale: PropTypes.number.isRequired,
  sidebarWidth: PropTypes.number.isRequired,
  waterfallWidth: PropTypes.number.isRequired,
  onFocus: PropTypes.func,
  onBlur: PropTypes.func,
};

module.exports = Waterfall;
