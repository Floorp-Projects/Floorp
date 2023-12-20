/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  connect,
} = require("resource://devtools/client/shared/redux/visibility-handler-connect.js");
const {
  Component,
} = require("resource://devtools/client/shared/vendor/react.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const {
  L10N,
} = require("resource://devtools/client/netmonitor/src/utils/l10n.js");
const {
  getNetMonitorTimingsURL,
} = require("resource://devtools/client/netmonitor/src/utils/doc-utils.js");
const {
  fetchNetworkUpdatePacket,
} = require("resource://devtools/client/netmonitor/src/utils/request-utils.js");
const {
  getFormattedTime,
} = require("resource://devtools/client/netmonitor/src/utils/format-utils.js");
const {
  TIMING_KEYS,
} = require("resource://devtools/client/netmonitor/src/constants.js");

// Components
const MDNLink = require("resource://devtools/client/shared/components/MdnLink.js");

const { div, span } = dom;

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

  renderServiceWorkerTimings() {
    const { serviceWorkerTimings } = this.props.request.eventTimings;

    if (!serviceWorkerTimings) {
      return null;
    }

    const totalTime = Object.values(serviceWorkerTimings).reduce(
      (acc, value) => acc + value,
      0
    );

    let offset = 0;
    let preValue = 0;

    return div(
      {},
      div(
        { className: "label-separator" },
        L10N.getStr("netmonitor.timings.serviceWorkerTiming")
      ),
      Object.entries(serviceWorkerTimings).map(([key, value], index) => {
        if (preValue > 0) {
          offset += preValue / totalTime;
        }
        preValue = value;
        return div(
          {
            key,
            className:
              "tabpanel-summary-container timings-container service-worker",
          },
          span(
            { className: "tabpanel-summary-label timings-label" },
            L10N.getStr(`netmonitor.timings.${key}`)
          ),
          div(
            { className: "requests-list-timings-container" },
            span({
              className: `requests-list-timings-box serviceworker-timings-color-${key.replace(
                "ServiceWorker",
                ""
              )}`,
              style: {
                "--current-timing-offset": offset > 0 ? offset : 0,
                "--current-timing-width": value / totalTime,
              },
            }),
            span(
              { className: "requests-list-timings-total" },
              getFormattedTime(value)
            )
          )
        );
      })
    );
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
              className: `requests-list-timings-box server-timings-color-${color}`,
              style: {
                "--current-timing-offset": (totalTime - duration) / totalTime,
                "--current-timing-width": duration / totalTime,
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
            className: `requests-list-timings-box ${type}`,
            style: {
              "--current-timing-offset": offsetScale,
              "--current-timing-width": timelineScale,
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
      this.renderServiceWorkerTimings(),
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
