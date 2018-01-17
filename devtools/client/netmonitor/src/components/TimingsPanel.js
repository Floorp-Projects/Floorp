/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { L10N } = require("../utils/l10n");
const { getNetMonitorTimingsURL } = require("../utils/mdn-utils");
const { fetchNetworkUpdatePacket } = require("../utils/request-utils");

// Components
const MDNLink = require("./MdnLink");

const { div, span } = dom;
const types = ["blocked", "dns", "connect", "ssl", "send", "wait", "receive"];
const TIMINGS_END_PADDING = "80px";

/*
 * Timings panel component
 * Display timeline bars that shows the total wait time for various stages
 */
class TimingsPanel extends Component {
  static get propTypes() {
    return {
      connector: PropTypes.object.isRequired,
      request: PropTypes.object.isRequired,
    };
  }

  componentDidMount() {
    let { connector, request } = this.props;
    fetchNetworkUpdatePacket(connector.requestData, request, ["eventTimings"]);
  }

  componentWillReceiveProps(nextProps) {
    let { connector, request } = nextProps;
    fetchNetworkUpdatePacket(connector.requestData, request, ["eventTimings"]);
  }

  render() {
    const { eventTimings } = this.props.request;
    if (!eventTimings) {
      return null;
    }

    const { timings, totalTime, offsets } = eventTimings;
    const timelines = types.map((type, idx) => {
      // Determine the relative offset for each timings box. For example, the
      // offset of third timings box will be 0 + blocked offset + dns offset

      const offsetScale = offsets[type] / totalTime || 0;
      const timelineScale = timings[type] / totalTime || 0;

      return div({
        key: type,
        id: `timings-summary-${type}`,
        className: "tabpanel-summary-container timings-container",
      },
        span({ className: "tabpanel-summary-label timings-label" },
          L10N.getStr(`netmonitor.timings.${type}`)
        ),
        div({ className: "requests-list-timings-container" },
          span({
            className: "requests-list-timings-offset",
            style: {
              width: `calc(${offsetScale} * (100% - ${TIMINGS_END_PADDING})`,
            },
          }),
          span({
            className: `requests-list-timings-box ${type}`,
            style: {
              width: `calc(${timelineScale} * (100% - ${TIMINGS_END_PADDING}))`,
            },
          }),
          span({ className: "requests-list-timings-total" },
            L10N.getFormatStr("networkMenu.totalMS", timings[type])
          )
        ),
      );
    });

    return (
      div({ className: "panel-container" },
        timelines,
        MDNLink({
          url: getNetMonitorTimingsURL(),
        }),
      )
    );
  }
}

module.exports = TimingsPanel;
