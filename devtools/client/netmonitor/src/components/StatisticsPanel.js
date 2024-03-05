/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const ReactDOM = require("resource://devtools/client/shared/vendor/react-dom.js");
const {
  FILTER_TAGS,
} = require("resource://devtools/client/netmonitor/src/constants.js");
const {
  Component,
  createFactory,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const {
  connect,
} = require("resource://devtools/client/shared/redux/visibility-handler-connect.js");
const { Chart } = require("resource://devtools/client/shared/widgets/Chart.js");
const { PluralForm } = require("resource://devtools/shared/plural-form.js");
const Actions = require("resource://devtools/client/netmonitor/src/actions/index.js");
const {
  Filters,
} = require("resource://devtools/client/netmonitor/src/utils/filter-predicates.js");
const {
  getSizeWithDecimals,
  getTimeWithDecimals,
} = require("resource://devtools/client/netmonitor/src/utils/format-utils.js");
const {
  L10N,
} = require("resource://devtools/client/netmonitor/src/utils/l10n.js");
const {
  getPerformanceAnalysisURL,
} = require("resource://devtools/client/netmonitor/src/utils/doc-utils.js");
const {
  fetchNetworkUpdatePacket,
} = require("resource://devtools/client/netmonitor/src/utils/request-utils.js");

// Components
const MDNLink = createFactory(
  require("resource://devtools/client/shared/components/MdnLink.js")
);

const { button, div } = dom;
const MediaQueryList = window.matchMedia("(min-width: 700px)");

const NETWORK_ANALYSIS_PIE_CHART_DIAMETER = 200;
const BACK_BUTTON = L10N.getStr("netmonitor.backButton");
const CHARTS_CACHE_ENABLED = L10N.getStr("charts.cacheEnabled");
const CHARTS_CACHE_DISABLED = L10N.getStr("charts.cacheDisabled");
const CHARTS_LEARN_MORE = L10N.getStr("charts.learnMore");

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
      hasLoad: PropTypes.bool,
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

  // FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=1774507
  UNSAFE_componentWillMount() {
    this.mdnLinkContainerNodes = new Map();
  }

  componentDidMount() {
    const { requests, connector } = this.props;
    requests.forEach(request => {
      fetchNetworkUpdatePacket(connector.requestData, request, [
        "eventTimings",
        "responseHeaders",
      ]);
    });
  }

  // FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=1774507
  UNSAFE_componentWillReceiveProps(nextProps) {
    const { requests, connector } = nextProps;
    requests.forEach(request => {
      fetchNetworkUpdatePacket(connector.requestData, request, [
        "eventTimings",
        "responseHeaders",
      ]);
    });
  }

  componentDidUpdate() {
    MediaQueryList.addListener(this.onLayoutChange);

    const { hasLoad, requests } = this.props;

    // Display statistics about all requests for which we received enough data,
    // as soon as the page is considered to be loaded
    const ready = requests.length && hasLoad;

    // Ignore requests which are missing data expected by this component:
    // - pending/incomplete requests
    // - blocked/errored requests
    const validRequests = requests.filter(
      req =>
        req.contentSize !== undefined &&
        req.mimeType &&
        req.responseHeaders &&
        req.status !== undefined &&
        req.totalTime !== undefined
    );

    this.createChart({
      id: "primedCacheChart",
      title: CHARTS_CACHE_ENABLED,
      data: ready ? this.sanitizeChartDataSource(validRequests, false) : null,
    });

    this.createChart({
      id: "emptyCacheChart",
      title: CHARTS_CACHE_DISABLED,
      data: ready ? this.sanitizeChartDataSource(validRequests, true) : null,
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

    ReactDOM.render(
      MDNLink({
        url,
        title: CHARTS_LEARN_MORE,
      }),
      containerNode
    );
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
        count: L10N.getStr("charts.requestsNumber"),
        label: L10N.getStr("charts.type"),
        size: L10N.getStr("charts.size"),
        transferredSize: L10N.getStr("charts.transferred"),
        time: L10N.getStr("charts.time"),
        nonBlockingTime: L10N.getStr("charts.nonBlockingTime"),
      },
      data,
      strings: {
        size: value =>
          L10N.getFormatStr(
            "charts.size.kB",
            getSizeWithDecimals(value / 1000)
          ),
        transferredSize: value =>
          L10N.getFormatStr(
            "charts.transferredSize.kB",
            getSizeWithDecimals(value / 1000)
          ),
        time: value =>
          L10N.getFormatStr("charts.totalS", getTimeWithDecimals(value / 1000)),
        nonBlockingTime: value =>
          L10N.getFormatStr("charts.totalS", getTimeWithDecimals(value / 1000)),
      },
      totals: {
        cached: total => L10N.getFormatStr("charts.totalCached", total),
        count: total => L10N.getFormatStr("charts.totalCount", total),
        size: total =>
          L10N.getFormatStr(
            "charts.totalSize.kB",
            getSizeWithDecimals(total / 1000)
          ),
        transferredSize: total =>
          L10N.getFormatStr(
            "charts.totalTransferredSize.kB",
            getSizeWithDecimals(total / 1000)
          ),
        time: total => {
          const seconds = total / 1000;
          const string = getTimeWithDecimals(seconds);
          return PluralForm.get(
            seconds,
            L10N.getStr("charts.totalSeconds")
          ).replace("#1", string);
        },
        nonBlockingTime: total => {
          const seconds = total / 1000;
          const string = getTimeWithDecimals(seconds);
          return PluralForm.get(
            seconds,
            L10N.getStr("charts.totalSecondsNonBlocking")
          ).replace("#1", string);
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
    const data = FILTER_TAGS.map(type => ({
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
    const cacheControl = list.find(
      e => e.name.toLowerCase() === "cache-control"
    );
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

    return div(
      { className: "statistics-panel" },
      button(
        {
          className: "back-button devtools-button",
          "data-text-only": "true",
          title: BACK_BUTTON,
          onClick: closeStatistics,
        },
        BACK_BUTTON
      ),
      div(
        { className: "charts-container" },
        div({
          ref: "primedCacheChart",
          className: "charts primed-cache-chart",
        }),
        div({ className: splitterClassName.join(" ") }),
        div({ ref: "emptyCacheChart", className: "charts empty-cache-chart" })
      )
    );
  }
}

module.exports = connect(
  state => ({
    // `firstDocumentLoadTimestamp` is set on timing markers when we receive
    // DOCUMENT_EVENT's dom-complete, which is equivalent to page `load` event.
    hasLoad: state.timingMarkers.firstDocumentLoadTimestamp != -1,
    requests: [...state.requests.requests],
  }),
  (dispatch, props) => ({
    closeStatistics: () =>
      dispatch(Actions.openStatistics(props.connector, false)),
    enableRequestFilterTypeOnly: label =>
      dispatch(Actions.enableRequestFilterTypeOnly(label)),
  })
)(StatisticsPanel);
