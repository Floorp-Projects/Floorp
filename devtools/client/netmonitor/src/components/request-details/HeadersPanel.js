/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const {
  getFormattedIPAndPort,
  getFormattedSize,
} = require("devtools/client/netmonitor/src/utils/format-utils");
const { L10N } = require("devtools/client/netmonitor/src/utils/l10n");
const {
  getHeadersURL,
  getHTTPStatusCodeURL,
  getTrackingProtectionURL,
} = require("devtools/client/netmonitor/src/utils/mdn-utils");
const {
  fetchNetworkUpdatePacket,
  writeHeaderText,
} = require("devtools/client/netmonitor/src/utils/request-utils");
const {
  HeadersProvider,
  HeaderList,
} = require("devtools/client/netmonitor/src/utils/headers-provider");
const {
  FILTER_SEARCH_DELAY,
} = require("devtools/client/netmonitor/src/constants");
// Components
const PropertiesView = createFactory(
  require("devtools/client/netmonitor/src/components/request-details/PropertiesView")
);
const SearchBox = createFactory(
  require("devtools/client/shared/components/SearchBox")
);
const Accordion = createFactory(
  require("devtools/client/shared/components/Accordion")
);
const StatusCode = createFactory(
  require("devtools/client/netmonitor/src/components/StatusCode")
);

loader.lazyGetter(this, "MDNLink", function() {
  return createFactory(require("devtools/client/shared/components/MdnLink"));
});
loader.lazyGetter(this, "Rep", function() {
  return require("devtools/client/shared/components/reps/reps").REPS.Rep;
});
loader.lazyGetter(this, "MODE", function() {
  return require("devtools/client/shared/components/reps/reps").MODE;
});
loader.lazyGetter(this, "TreeRow", function() {
  return createFactory(
    require("devtools/client/shared/components/tree/TreeRow")
  );
});

const { button, div, input, label, span, textarea, tr, td } = dom;

const EDIT_AND_RESEND = L10N.getStr("netmonitor.summary.editAndResend");
const RAW_HEADERS = L10N.getStr("netmonitor.summary.rawHeaders");
const HEADERS_EMPTY_TEXT = L10N.getStr("headersEmptyText");
const HEADERS_FILTER_TEXT = L10N.getStr("headersFilterText");
const REQUEST_HEADERS = L10N.getStr("requestHeaders");
const REQUEST_HEADERS_FROM_UPLOAD = L10N.getStr("requestHeadersFromUpload");
const RESPONSE_HEADERS = L10N.getStr("responseHeaders");
const SUMMARY_ADDRESS = L10N.getStr("netmonitor.summary.address");
const SUMMARY_METHOD = L10N.getStr("netmonitor.summary.method");
const SUMMARY_URL = L10N.getStr("netmonitor.summary.url");
const SUMMARY_STATUS = L10N.getStr("netmonitor.summary.status");
const SUMMARY_VERSION = L10N.getStr("netmonitor.summary.version");
const SUMMARY_STATUS_LEARN_MORE = L10N.getStr("netmonitor.summary.learnMore");
const SUMMARY_REFERRER_POLICY = L10N.getStr(
  "netmonitor.summary.referrerPolicy"
);

/**
 * Headers panel component
 * Lists basic information about the request
 *
 * In http/2 all response headers are in small case.
 * See: https://developer.mozilla.org/en-US/docs/Tools/Network_Monitor/request_details#Headers
 * RFC: https://tools.ietf.org/html/rfc7540#section-8.1.2
 */
class HeadersPanel extends Component {
  static get propTypes() {
    return {
      connector: PropTypes.object.isRequired,
      cloneSelectedRequest: PropTypes.func.isRequired,
      member: PropTypes.object,
      request: PropTypes.object.isRequired,
      renderValue: PropTypes.func,
      openLink: PropTypes.func,
      targetSearchResult: PropTypes.object,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      rawRequestHeadersOpened: false,
      rawResponseHeadersOpened: false,
      rawUploadHeadersOpened: false,
      lastToggledRawHeader: "",
      filterText: null,
    };

    this.getProperties = this.getProperties.bind(this);
    this.getTargetHeaderPath = this.getTargetHeaderPath.bind(this);
    this.toggleRawResponseHeaders = this.toggleRawResponseHeaders.bind(this);
    this.toggleRawRequestHeaders = this.toggleRawRequestHeaders.bind(this);
    this.toggleRawUploadHeaders = this.toggleRawUploadHeaders.bind(this);
    this.renderSummary = this.renderSummary.bind(this);
    this.renderRow = this.renderRow.bind(this);
    this.renderValue = this.renderValue.bind(this);
    this.renderRawHeadersBtn = this.renderRawHeadersBtn.bind(this);
  }

  componentDidMount() {
    const { request, connector } = this.props;
    fetchNetworkUpdatePacket(connector.requestData, request, [
      "requestHeaders",
      "responseHeaders",
      "requestPostData",
    ]);
  }

  componentWillReceiveProps(nextProps) {
    const { request, connector } = nextProps;
    fetchNetworkUpdatePacket(connector.requestData, request, [
      "requestHeaders",
      "responseHeaders",
      "requestPostData",
    ]);
  }

  getHeadersTitle(headers, title) {
    let result;
    if (headers?.headers.length) {
      result = `${title} (${getFormattedSize(headers.headersSize, 3)})`;
    }

    return result;
  }

  getProperties(headers, title) {
    let propertiesResult;

    if (headers?.headers.length) {
      const headerKey = this.getHeadersTitle(headers, title);
      propertiesResult = {
        [headerKey]: new HeaderList(headers.headers),
      };
      if (
        (title === RESPONSE_HEADERS && this.state.rawResponseHeadersOpened) ||
        (title === REQUEST_HEADERS && this.state.rawRequestHeadersOpened) ||
        (title === REQUEST_HEADERS_FROM_UPLOAD &&
          this.state.rawUploadHeadersOpened)
      ) {
        propertiesResult = {
          [headerKey]: { RAW_HEADERS_ID: headers.rawHeaders },
        };
      }
    }
    return propertiesResult;
  }

  toggleRawResponseHeaders() {
    this.setState({
      rawResponseHeadersOpened: !this.state.rawResponseHeadersOpened,
      lastToggledRawHeader: "response",
    });
  }

  toggleRawRequestHeaders() {
    this.setState({
      rawRequestHeadersOpened: !this.state.rawRequestHeadersOpened,
      lastToggledRawHeader: "request",
    });
  }

  toggleRawUploadHeaders() {
    this.setState({
      rawUploadHeadersOpened: !this.state.rawUploadHeadersOpened,
      lastToggledRawHeader: "upload",
    });
  }

  /**
   * Helper method to identify what kind of raw header this is.
   * Information is in the path variable
   */
  getRawHeaderType(path) {
    if (path.includes(RESPONSE_HEADERS)) {
      return "RESPONSE";
    }
    if (path.includes(REQUEST_HEADERS_FROM_UPLOAD)) {
      return "UPLOAD";
    }
    return "REQUEST";
  }

  /**
   * Renders the top part of the headers detail panel - Summary.
   */
  renderSummary(summaryLabel, value) {
    return div(
      {
        key: summaryLabel,
        className: "tabpanel-summary-container headers-summary",
      },
      div(
        { className: "tabpanel-summary-labelvalue" },
        span(
          { className: "tabpanel-summary-label headers-summary-label" },
          summaryLabel
        ),
        span(
          {
            className:
              "tabpanel-summary-value textbox-input devtools-monospace",
          },
          value
        )
      )
    );
  }

  /**
   * Get path for target header if it's set. It's used to select
   * the header automatically within the tree of headers.
   * Note that the target header is set by the Search panel.
   */
  getTargetHeaderPath(searchResult) {
    if (!searchResult) {
      return null;
    }
    if (
      searchResult.type !== "requestHeaders" &&
      searchResult.type !== "responseHeaders" &&
      searchResult.type !== "requestHeadersFromUploadStream"
    ) {
      return null;
    }
    const {
      request: {
        requestHeaders,
        requestHeadersFromUploadStream: uploadHeaders,
        responseHeaders,
      },
    } = this.props;
    // Using `HeaderList` ensures that we'll get the same
    // header index as it's used in the tree.
    const getPath = (headers, title) => {
      return (
        "/" +
        this.getHeadersTitle(headers, title) +
        "/" +
        new HeaderList(headers.headers).headers.findIndex(
          header => header.name == searchResult.label
        )
      );
    };
    // Calculate target header path according to the header type.
    switch (searchResult.type) {
      case "requestHeaders":
        return getPath(requestHeaders, REQUEST_HEADERS);
      case "responseHeaders":
        return getPath(responseHeaders, RESPONSE_HEADERS);
      case "requestHeadersFromUploadStream":
        return getPath(uploadHeaders, REQUEST_HEADERS_FROM_UPLOAD);
    }
    return null;
  }

  /**
   * Custom rendering method passed to PropertiesView. It's responsible
   * for rendering <textarea> element with raw headers data.
   */
  renderRow(props) {
    const { level, path } = props.member;

    const {
      request: {
        httpVersion,
        requestHeaders,
        requestHeadersFromUploadStream: uploadHeaders,
        responseHeaders,
        status,
        statusText,
      },
    } = this.props;

    let value;

    if (path.includes("RAW_HEADERS_ID")) {
      const rawHeaderType = this.getRawHeaderType(path);
      switch (rawHeaderType) {
        case "REQUEST":
          value = writeHeaderText(requestHeaders.headers);
          break;
        case "RESPONSE":
          // display Status-Line above other response headers
          const statusLine = `${httpVersion} ${status} ${statusText}\n`;
          value = statusLine + writeHeaderText(responseHeaders.headers);
          break;
        case "UPLOAD":
          value = writeHeaderText(uploadHeaders.headers);
          break;
      }

      let rows;
      if (value) {
        // Need to add 1 for the horizontal scrollbar
        // not to cover the last row of raw data
        rows = value.match(/\n/g).length + 1;
      }

      return tr(
        {
          key: path,
          role: "treeitem",
          className: "raw-headers-container",
        },
        td(
          {
            colSpan: 2,
          },
          textarea({
            className: "raw-headers",
            rows: rows,
            value: value,
            readOnly: true,
          })
        )
      );
    }

    if (level !== 1) {
      return null;
    }

    return TreeRow(props);
  }

  renderRawHeadersBtn(key, checked, onChange) {
    return [
      label(
        { key: `${key}RawHeadersBtn`, className: "raw-headers-toggle" },
        span({ className: "headers-summary-label" }, RAW_HEADERS),
        div(
          { className: "raw-headers-toggle-input" },
          input({
            checked,
            className: "devtools-checkbox-toggle",
            onClick: event => {
              // stop the header click event
              event.stopPropagation();
            },
            onChange,
            type: "checkbox",
          })
        )
      ),
    ];
  }

  renderValue(props) {
    const member = props.member;
    const value = props.value;

    if (typeof value !== "string") {
      return null;
    }

    const headerDocURL = getHeadersURL(member.name);

    return div(
      { className: "treeValueCellDivider" },
      Rep(
        Object.assign(props, {
          // FIXME: A workaround for the issue in StringRep
          // Force StringRep to crop the text everytime
          member: Object.assign({}, member, { open: false }),
          mode: MODE.TINY,
          cropLimit: 60,
          noGrip: true,
        })
      ),
      headerDocURL ? MDNLink({ url: headerDocURL }) : null
    );
  }

  getShouldOpen(rawHeader, filterText, targetSearchResult) {
    return (item, opened) => {
      // If closed, open panel for these reasons
      //  1.The raw header is switched on or off
      //  2.The filter text is set
      //  3.The search text is set
      if (
        (!opened && this.state.lastToggledRawHeader === rawHeader) ||
        (!opened && filterText) ||
        (!opened && targetSearchResult)
      ) {
        return true;
      }
      return !!opened;
    };
  }

  render() {
    const {
      cloneSelectedRequest,
      targetSearchResult,
      request: {
        fromCache,
        fromServiceWorker,
        httpVersion,
        method,
        remoteAddress,
        remotePort,
        requestHeaders,
        requestHeadersFromUploadStream: uploadHeaders,
        responseHeaders,
        status,
        statusText,
        urlDetails,
        referrerPolicy,
        isThirdPartyTrackingResource,
      },
    } = this.props;
    const {
      rawResponseHeadersOpened,
      rawRequestHeadersOpened,
      rawUploadHeadersOpened,
      filterText,
    } = this.state;
    const item = { fromCache, fromServiceWorker, status, statusText };

    if (
      (!requestHeaders || !requestHeaders.headers.length) &&
      (!uploadHeaders || !uploadHeaders.headers.length) &&
      (!responseHeaders || !responseHeaders.headers.length)
    ) {
      return div({ className: "empty-notice" }, HEADERS_EMPTY_TEXT);
    }

    const items = [
      {
        component: PropertiesView,
        componentProps: {
          object: this.getProperties(responseHeaders, RESPONSE_HEADERS),
          filterText,
          targetSearchResult,
          renderRow: this.renderRow,
          renderValue: this.renderValue,
          provider: HeadersProvider,
          selectPath: this.getTargetHeaderPath,
          defaultSelectFirstNode: false,
        },
        header: this.getHeadersTitle(responseHeaders, RESPONSE_HEADERS),
        buttons: this.renderRawHeadersBtn(
          "response",
          rawResponseHeadersOpened,
          this.toggleRawResponseHeaders
        ),
        id: "responseHeaders",
        opened: true,
        shouldOpen: this.getShouldOpen(
          "response",
          filterText,
          targetSearchResult
        ),
      },
      {
        component: PropertiesView,
        componentProps: {
          object: this.getProperties(requestHeaders, REQUEST_HEADERS),
          filterText,
          targetSearchResult,
          renderRow: this.renderRow,
          renderValue: this.renderValue,
          provider: HeadersProvider,
          selectPath: this.getTargetHeaderPath,
          defaultSelectFirstNode: false,
        },
        header: this.getHeadersTitle(requestHeaders, REQUEST_HEADERS),
        buttons: this.renderRawHeadersBtn(
          "request",
          rawRequestHeadersOpened,
          this.toggleRawRequestHeaders
        ),
        id: "requestHeaders",
        opened: true,
        shouldOpen: this.getShouldOpen(
          "request",
          filterText,
          targetSearchResult
        ),
      },
    ];

    if (uploadHeaders?.headers.length) {
      items.push({
        component: PropertiesView,
        componentProps: {
          object: this.getProperties(
            uploadHeaders,
            REQUEST_HEADERS_FROM_UPLOAD
          ),
          filterText,
          targetSearchResult,
          renderRow: this.renderRow,
          renderValue: this.renderValue,
          provider: HeadersProvider,
          selectPath: this.getTargetHeaderPath,
          defaultSelectFirstNode: false,
        },
        header: this.getHeadersTitle(
          uploadHeaders,
          REQUEST_HEADERS_FROM_UPLOAD
        ),
        buttons: this.renderRawHeadersBtn(
          "upload",
          rawUploadHeadersOpened,
          this.toggleRawUploadHeaders
        ),
        id: "uploadHeaders",
        opened: true,
        shouldOpen: this.getShouldOpen(
          "upload",
          filterText,
          targetSearchResult
        ),
      });
    }

    // not showing #hash in url
    const summaryUrl = urlDetails.url
      ? this.renderSummary(SUMMARY_URL, urlDetails.url.split("#")[0])
      : null;

    const summaryMethod = method
      ? this.renderSummary(SUMMARY_METHOD, method)
      : null;

    const summaryAddress = remoteAddress
      ? this.renderSummary(
          SUMMARY_ADDRESS,
          getFormattedIPAndPort(remoteAddress, remotePort)
        )
      : null;

    let summaryStatus;

    if (status) {
      const statusCodeDocURL = getHTTPStatusCodeURL(status.toString());
      const inputWidth = statusText.length + 1;

      summaryStatus = div(
        {
          key: "headers-summary",
          className: "tabpanel-summary-container headers-summary",
        },
        div(
          {
            className: "tabpanel-summary-label headers-summary-label",
          },
          SUMMARY_STATUS
        ),
        StatusCode({ item }),
        input({
          className:
            "tabpanel-summary-value textbox-input devtools-monospace" +
            " status-text",
          readOnly: true,
          value: `${statusText}`,
          size: `${inputWidth}`,
        }),
        statusCodeDocURL
          ? MDNLink({
              url: statusCodeDocURL,
              title: SUMMARY_STATUS_LEARN_MORE,
            })
          : span({
              className: "headers-summary learn-more-link",
            })
      );
    }

    let trackingProtectionStatus;

    if (isThirdPartyTrackingResource) {
      const trackingProtectionDocURL = getTrackingProtectionURL();

      trackingProtectionStatus = div(
        {
          key: "tracking-protection",
          className: "tabpanel-summary-container tracking-protection",
        },
        div({
          className: "tracking-resource",
        }),
        L10N.getStr("netmonitor.trackingResource.tooltip"),
        trackingProtectionDocURL
          ? MDNLink({
              url: trackingProtectionDocURL,
              title: SUMMARY_STATUS_LEARN_MORE,
            })
          : span({
              className: "headers-summary learn-more-link",
            })
      );
    }

    const summaryVersion = httpVersion
      ? this.renderSummary(SUMMARY_VERSION, httpVersion)
      : null;

    const summaryReferrerPolicy = referrerPolicy
      ? this.renderSummary(SUMMARY_REFERRER_POLICY, referrerPolicy)
      : null;

    const summaryItems = [
      summaryUrl,
      trackingProtectionStatus,
      summaryMethod,
      summaryAddress,
      summaryStatus,
      summaryVersion,
      summaryReferrerPolicy,
    ].filter(summaryItem => summaryItem !== null);

    const summaryEditAndResendBtn = div(
      {
        className: "summary-edit-and-resend",
      },
      summaryItems.pop(),
      button(
        {
          className: "edit-and-resend-button devtools-button",
          onClick: cloneSelectedRequest,
        },
        EDIT_AND_RESEND
      )
    );

    return div(
      { className: "panel-container" },
      div(
        { className: "headers-overview" },
        summaryItems,
        summaryEditAndResendBtn
      ),
      div(
        { className: "devtools-toolbar devtools-input-toolbar" },
        SearchBox({
          delay: FILTER_SEARCH_DELAY,
          type: "filter",
          onChange: text => this.setState({ filterText: text }),
          placeholder: HEADERS_FILTER_TEXT,
        })
      ),
      Accordion({ items })
    );
  }
}

module.exports = HeadersPanel;
