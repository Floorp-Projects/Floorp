/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  connect,
} = require("devtools/client/shared/redux/visibility-handler-connect");
const { Component } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { L10N } = require("devtools/client/netmonitor/src/utils/l10n");
const {
  getNetMonitorTimingsURL,
} = require("devtools/client/netmonitor/src/utils/doc-utils");
const {
  fetchNetworkUpdatePacket,
} = require("devtools/client/netmonitor/src/utils/request-utils");
const {
  getFormattedTime,
} = require("devtools/client/netmonitor/src/utils/format-utils");
const { TIMING_KEYS } = require("devtools/client/netmonitor/src/constants");

// Components
const MDNLink = require("devtools/client/shared/components/MdnLink");

const { div, span } = dom;

const TIMINGS_END_PADDING = "80px";

/**
 * Timings panel component
 * Display timeline bars that shows the total wait time for various stages
 */
class TimingsPanel extends Component {
  static get propTypes() {
    return {
      connector: PropTypes.object.isRequired,
      request: PropTypes.object.isRequired,
      firstRequestStartedMs: PropTypes.number.isRequired,
    };
  }

  componentDidMount() {
    const { connector, request } = this.props;
    fetchNetworkUpdatePacket(connector.requestData, request, ["eventTimings"]);
  }

  // FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=1774507
  UNSAFE_componentWillReceiveProps(nextProps) {
    const { connector, request } = nextProps;
    fetchNetworkUpdatePacket(connector.requestData, request, ["eventTimings"]);
  }

  renderServerTimings() {
    const { serverTimings, totalTime } = this.props.request.eventTimings;

    if (!serverTimings?.length) {
      return null;
    }

    return div(
      {},
      div(
        { className: "label-separator" },
        L10N.getStr("netmonitor.timings.serverTiming")
      ),
      ...serverTimings.map(({ name, duration, description }, index) => {
        const color = name === "total" ? "total" : (index % 3) + 1;

        return div(
          {
            key: index,
            className: "tabpanel-summary-container timings-container server",
          },
          span(
            { className: "tabpanel-summary-label timings-label" },
            description || name
          ),
          div(
            { className: "requests-list-timings-container" },
            span({
              className: "requests-list-timings-offset",
              style: {
                width: `calc(${(totalTime - duration) /
                  totalTime} * (100% - ${TIMINGS_END_PADDING})`,
              },
            }),
            span({
              className: `requests-list-timings-box server-timings-color-${color}`,
              style: {
                width: `calc(${duration /
                  totalTime} * (100% - ${TIMINGS_END_PADDING}))`,
              },
            }),
            span(
              { className: "requests-list-timings-total" },
              getFormattedTime(duration)
            )
          )
        );
      })
    );
  }

  render() {
    const { eventTimings, totalTime, startedMs } = this.props.request;
    const { firstRequestStartedMs } = this.props;

    if (!eventTimings) {
      return div(
        {
          className:
            "tabpanel-summary-container timings-container empty-notice",
        },
        L10N.getStr("netmonitor.timings.noTimings")
      );
    }

    const { timings, offsets } = eventTimings;
    let queuedAt, startedAt, downloadedAt;
    const isFirstRequestStartedAvailable = firstRequestStartedMs !== null;

    if (isFirstRequestStartedAvailable) {
      queuedAt = startedMs - firstRequestStartedMs;
      startedAt = queuedAt + timings.blocked;
      downloadedAt = queuedAt + totalTime;
    }

    const timelines = TIMING_KEYS.map((type, idx) => {
      // Determine the relative offset for each timings box. For example, the
      // offset of third timings box will be 0 + blocked offset + dns offset
      // If offsets sent from the backend aren't available calculate it
      // from the timing info.
      const offset = offsets
        ? offsets[type]
        : TIMING_KEYS.slice(0, idx).reduce(
            (acc, cur) => acc + timings[cur] || 0,
            0
          );

      const offsetScale = offset / totalTime || 0;
      const timelineScale = timings[type] / totalTime || 0;

      return div(
        {
          key: type,
          id: `timings-summary-${type}`,
          className: "tabpanel-summary-container timings-container request",
        },
        span(
          { className: "tabpanel-summary-label timings-label" },
          L10N.getStr(`netmonitor.timings.${type}`)
        ),
        div(
          { className: "requests-list-timings-container" },
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
          span(
            { className: "requests-list-timings-total" },
            getFormattedTime(timings[type])
          )
        )
      );
    });

    return div(
      { className: "panel-container" },
      isFirstRequestStartedAvailable &&
        div(
          { className: "timings-overview" },
          span(
            { className: "timings-overview-item" },
            L10N.getFormatStr(
              "netmonitor.timings.queuedAt",
              getFormattedTime(queuedAt)
            )
          ),
          span(
            { className: "timings-overview-item" },
            L10N.getFormatStr(
              "netmonitor.timings.startedAt",
              getFormattedTime(startedAt)
            )
          ),
          span(
            { className: "timings-overview-item" },
            L10N.getFormatStr(
              "netmonitor.timings.downloadedAt",
              getFormattedTime(downloadedAt)
            )
          )
        ),
      div(
        { className: "label-separator" },
        L10N.getStr("netmonitor.timings.requestTiming")
      ),
      timelines,
      this.renderServerTimings(),
      MDNLink({
        url: getNetMonitorTimingsURL(),
        title: L10N.getStr("netmonitor.timings.learnMore"),
      })
    );
  }
}

module.exports = connect(state => ({
  firstRequestStartedMs: state.requests ? state.requests.firstStartedMs : null,
}))(TimingsPanel);
