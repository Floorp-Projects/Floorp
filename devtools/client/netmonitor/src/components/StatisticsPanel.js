/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const ReactDOM = require("devtools/client/shared/vendor/react-dom");
const { FILTER_TAGS } = require("../constants");
const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/redux/visibility-handler-connect");
const { Chart } = require("devtools/client/shared/widgets/Chart");
const { PluralForm } = require("devtools/shared/plural-form");
const Actions = require("../actions/index");
const { Filters } = require("../utils/filter-predicates");
const { getSizeWithDecimals, getTimeWithDecimals } = require("../utils/format-utils");
const { L10N } = require("../utils/l10n");
const { getPerformanceAnalysisURL } = require("../utils/mdn-utils");
const { fetchNetworkUpdatePacket } = require("../utils/request-utils");

// Components
const MDNLink = createFactory(require("devtools/client/shared/components/MdnLink"));

const { button, div } = dom;
const MediaQueryList = window.matchMedia("(min-width: 700px)");

const NETWORK_ANALYSIS_PIE_CHART_DIAMETER = 200;
const BACK_BUTTON = L10N.getStr("netmonitor.backButton");
const CHARTS_CACHE_ENABLED = L10N.getStr("charts.cacheEnabled");
const CHARTS_CACHE_DISABLED = L10N.getStr("charts.cacheDisabled");

/*
 * Statistics panel component
 * Performance analysis tool which shows you how long the browser takes to
 * download the different parts of your site.
 */
class StatisticsPanel extends Component {
  static get propTypes() {
    return {
      connector: PropTypes.object.isRequired,
      closeStatistics: PropTypes.func.isRequired,
      enableRequestFilterTypeOnly: PropTypes.func.isRequired,
      requests: PropTypes.array,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      isVerticalSpliter: MediaQueryList.matches,
    };

    this.createMDNLink = this.createMDNLink.bind(this);
    this.unmountMDNLinkContainers = this.unmountMDNLinkContainers.bind(this);
    this.createChart = this.createChart.bind(this);
    this.sanitizeChartDataSource = this.sanitizeChartDataSource.bind(this);
    this.responseIsFresh = this.responseIsFresh.bind(this);
    this.onLayoutChange = this.onLayoutChange.bind(this);
  }

  componentWillMount() {
    this.mdnLinkContainerNodes = new Map();
  }

  componentDidMount() {
    const { requests, connector } = this.props;
    requests.forEach((request) => {
      fetchNetworkUpdatePacket(connector.requestData, request, [
        "eventTimings",
        "responseHeaders",
      ]);
    });
  }

  componentWillReceiveProps(nextProps) {
    const { requests, connector } = nextProps;
    requests.forEach((request) => {
      fetchNetworkUpdatePacket(connector.requestData, request, [
        "eventTimings",
        "responseHeaders",
      ]);
    });
  }

  componentDidUpdate(prevProps) {
    MediaQueryList.addListener(this.onLayoutChange);

    const { requests } = this.props;
    const ready = requests && requests.length && requests.every((req) =>
      req.contentSize !== undefined && req.mimeType && req.responseHeaders &&
      req.status !== undefined && req.totalTime !== undefined
    );

    this.createChart({
      id: "primedCacheChart",
      title: CHARTS_CACHE_ENABLED,
      data: ready ? this.sanitizeChartDataSource(requests, false) : null,
    });

    this.createChart({
      id: "emptyCacheChart",
      title: CHARTS_CACHE_DISABLED,
      data: ready ? this.sanitizeChartDataSource(requests, true) : null,
    });

    this.createMDNLink("primedCacheChart", getPerformanceAnalysisURL());
    this.createMDNLink("emptyCacheChart", getPerformanceAnalysisURL());
  }

  componentWillUnmount() {
    MediaQueryList.removeListener(this.onLayoutChange);
    this.unmountMDNLinkContainers();
  }

  createMDNLink(chartId, url) {
    if (this.mdnLinkContainerNodes.has(chartId)) {
      ReactDOM.unmountComponentAtNode(this.mdnLinkContainerNodes.get(chartId));
    }

    // MDNLink is a React component but Chart isn't.  To get the link
    // into the chart we mount a new ReactDOM at the appropriate
    // location after the chart has been created.
    const title = this.refs[chartId].querySelector(".table-chart-title");
    const containerNode = document.createElement("span");
    title.appendChild(containerNode);
    ReactDOM.render(MDNLink({ url }), containerNode);
    this.mdnLinkContainerNodes.set(chartId, containerNode);
  }

  unmountMDNLinkContainers() {
    for (const [, node] of this.mdnLinkContainerNodes) {
      ReactDOM.unmountComponentAtNode(node);
    }
  }

  createChart({ id, title, data }) {
    // Create a new chart.
    const chart = Chart.PieTable(document, {
      diameter: NETWORK_ANALYSIS_PIE_CHART_DIAMETER,
      title,
      header: {
        cached: "",
        count: "",
        label: L10N.getStr("charts.type"),
        size: L10N.getStr("charts.size"),
        transferredSize: L10N.getStr("charts.transferred"),
        time: L10N.getStr("charts.time"),
        nonBlockingTime: L10N.getStr("charts.nonBlockingTime"),
      },
      data,
      strings: {
        size: (value) =>
          L10N.getFormatStr("charts.sizeKB", getSizeWithDecimals(value / 1024)),
        transferredSize: (value) =>
          L10N.getFormatStr("charts.transferredSizeKB",
            getSizeWithDecimals(value / 1024)),
        time: (value) =>
          L10N.getFormatStr("charts.totalS", getTimeWithDecimals(value / 1000)),
        nonBlockingTime: (value) =>
          L10N.getFormatStr("charts.totalS", getTimeWithDecimals(value / 1000)),
      },
      totals: {
        cached: (total) => L10N.getFormatStr("charts.totalCached", total),
        count: (total) => L10N.getFormatStr("charts.totalCount", total),
        size: (total) =>
          L10N.getFormatStr("charts.totalSize", getSizeWithDecimals(total / 1024)),
        transferredSize: total =>
          L10N.getFormatStr("charts.totalTransferredSize",
            getSizeWithDecimals(total / 1024)),
        time: (total) => {
          const seconds = total / 1000;
          const string = getTimeWithDecimals(seconds);
          return PluralForm.get(seconds,
            L10N.getStr("charts.totalSeconds")).replace("#1", string);
        },
        nonBlockingTime: (total) => {
          const seconds = total / 1000;
          const string = getTimeWithDecimals(seconds);
          return PluralForm.get(seconds,
            L10N.getStr("charts.totalSecondsNonBlocking")).replace("#1", string);
        },
      },
      sorted: true,
    });

    chart.on("click", ({ label }) => {
      // Reset FilterButtons and enable one filter exclusively
      this.props.closeStatistics();
      this.props.enableRequestFilterTypeOnly(label);
    });

    const container = this.refs[id];

    // Nuke all existing charts of the specified type.
    while (container.hasChildNodes()) {
      container.firstChild.remove();
    }

    container.appendChild(chart.node);
  }

  sanitizeChartDataSource(requests, emptyCache) {
    const data = FILTER_TAGS.map((type) => ({
      cached: 0,
      count: 0,
      label: type,
      size: 0,
      transferredSize: 0,
      time: 0,
      nonBlockingTime: 0,
    }));

    for (const request of requests) {
      let type;

      if (Filters.html(request)) {
        // "html"
        type = 0;
      } else if (Filters.css(request)) {
        // "css"
        type = 1;
      } else if (Filters.js(request)) {
        // "js"
        type = 2;
      } else if (Filters.fonts(request)) {
        // "fonts"
        type = 4;
      } else if (Filters.images(request)) {
        // "images"
        type = 5;
      } else if (Filters.media(request)) {
        // "media"
        type = 6;
      } else if (Filters.ws(request)) {
        // "ws"
        type = 7;
      } else if (Filters.xhr(request)) {
        // Verify XHR last, to categorize other mime types in their own blobs.
        // "xhr"
        type = 3;
      } else {
        // "other"
        type = 8;
      }

      if (emptyCache || !this.responseIsFresh(request)) {
        data[type].time += request.totalTime || 0;
        data[type].size += request.contentSize || 0;
        data[type].transferredSize += request.transferredSize || 0;
        const nonBlockingTime =
           request.eventTimings.totalTime - request.eventTimings.timings.blocked;
        data[type].nonBlockingTime += nonBlockingTime || 0;
      } else {
        data[type].cached++;
      }
      data[type].count++;
    }

    return data.filter(e => e.count > 0);
  }

  /**
   * Checks if the "Expiration Calculations" defined in section 13.2.4 of the
   * "HTTP/1.1: Caching in HTTP" spec holds true for a collection of headers.
   *
   * @param object
   *        An object containing the { responseHeaders, status } properties.
   * @return boolean
   *         True if the response is fresh and loaded from cache.
   */
  responseIsFresh({ responseHeaders, status }) {
    // Check for a "304 Not Modified" status and response headers availability.
    if (status != 304 || !responseHeaders) {
      return false;
    }

    const list = responseHeaders.headers;
    const cacheControl = list.find(e => e.name.toLowerCase() === "cache-control");
    const expires = list.find(e => e.name.toLowerCase() === "expires");

    // Check the "Cache-Control" header for a maximum age value.
    if (cacheControl) {
      const maxAgeMatch =
        cacheControl.value.match(/s-maxage\s*=\s*(\d+)/) ||
        cacheControl.value.match(/max-age\s*=\s*(\d+)/);

      if (maxAgeMatch && maxAgeMatch.pop() > 0) {
        return true;
      }
    }

    // Check the "Expires" header for a valid date.
    if (expires && Date.parse(expires.value)) {
      return true;
    }

    return false;
  }

  onLayoutChange() {
    this.setState({
      isVerticalSpliter: MediaQueryList.matches,
    });
  }

  render() {
    const { closeStatistics } = this.props;
    const splitterClassName = ["splitter"];

    if (this.state.isVerticalSpliter) {
      splitterClassName.push("devtools-side-splitter");
    } else {
      splitterClassName.push("devtools-horizontal-splitter");
    }

    return (
      div({ className: "statistics-panel" },
        button({
          className: "back-button devtools-button",
          "data-text-only": "true",
          title: BACK_BUTTON,
          onClick: closeStatistics,
        }, BACK_BUTTON),
        div({ className: "charts-container" },
          div({ ref: "primedCacheChart", className: "charts primed-cache-chart" }),
          div({ className: splitterClassName.join(" ") }),
          div({ ref: "emptyCacheChart", className: "charts empty-cache-chart" }),
        ),
      )
    );
  }
}

module.exports = connect(
  (state) => ({
    requests: [...state.requests.requests.values()],
  }),
  (dispatch, props) => ({
    closeStatistics: () => dispatch(Actions.openStatistics(props.connector, false)),
    enableRequestFilterTypeOnly: (label) =>
      dispatch(Actions.enableRequestFilterTypeOnly(label)),
  })
)(StatisticsPanel);
