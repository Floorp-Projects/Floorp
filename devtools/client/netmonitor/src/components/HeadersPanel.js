/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const {
  getFormattedIPAndPort,
  getFormattedSize,
} = require("../utils/format-utils");
const { L10N } = require("../utils/l10n");
const {
  getHeadersURL,
  getHTTPStatusCodeURL,
  getTrackingProtectionURL,
} = require("../utils/mdn-utils");
const {
  fetchNetworkUpdatePacket,
  writeHeaderText,
} = require("../utils/request-utils");
const {
  HeadersProvider,
  HeaderList,
} = require("../utils/headers-provider");

// Components
const PropertiesView = createFactory(require("./PropertiesView"));
const StatusCode = createFactory(require("./StatusCode"));

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
  return createFactory(require("devtools/client/shared/components/tree/TreeRow"));
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
const SUMMARY_REFERRER_POLICY = L10N.getStr("netmonitor.summary.referrerPolicy");

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
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      rawRequestHeadersOpened: false,
      rawResponseHeadersOpened: false,
      rawUploadHeadersOpened: false,
    };

    this.getProperties = this.getProperties.bind(this);
    this.toggleRawResponseHeaders = this.toggleRawResponseHeaders.bind(this);
    this.toggleRawRequestHeaders = this.toggleRawRequestHeaders.bind(this);
    this.toggleRawUploadHeaders = this.toggleRawUploadHeaders.bind(this);
    this.renderSummary = this.renderSummary.bind(this);
    this.renderRow = this.renderRow.bind(this);
    this.renderValue = this.renderValue.bind(this);
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

  getProperties(headers, title) {
    let propertiesResult;

    if (headers && headers.headers.length) {
      const headerKey = `${title} (${getFormattedSize(headers.headersSize, 3)})`;

      propertiesResult = {
        [headerKey]: new HeaderList(headers.headers),
      };

      if ((title === RESPONSE_HEADERS && this.state.rawResponseHeadersOpened) ||
          (title === REQUEST_HEADERS && this.state.rawRequestHeadersOpened) ||
          (title === REQUEST_HEADERS_FROM_UPLOAD && this.state.rawUploadHeadersOpened)) {
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
    });
  }

  toggleRawRequestHeaders() {
    this.setState({
      rawRequestHeadersOpened: !this.state.rawRequestHeadersOpened,
    });
  }

  toggleRawUploadHeaders() {
    this.setState({
      rawUploadHeadersOpened: !this.state.rawUploadHeadersOpened,
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
    return (
      div({ className: "tabpanel-summary-container headers-summary" },
        div({ className: "tabpanel-summary-labelvalue"},
          span({ className: "tabpanel-summary-label headers-summary-label"},
          summaryLabel
          ),
          span({ className: "tabpanel-summary-value textbox-input devtools-monospace"},
            value
          ),
        ),
      )
    );
  }

  /**
   * Custom rendering method passed to PropertiesView. It's responsible
   * for rendering <textarea> element with raw headers data.
   */
  renderRow(props) {
    const {
      level,
      path,
    } = props.member;

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

    if (level === 1 && path.includes("RAW_HEADERS_ID")) {
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

      return (
        tr({
          key: path,
          role: "treeitem",
          className: "raw-headers-container",
        },
          td({
            colSpan: 2,
          },
            textarea({
              className: "raw-headers",
              rows: rows,
              value: value,
              readOnly: true,
            })
          )
        )
      );
    }

    return TreeRow(props);
  }

  /**
   * Rendering toggle buttons for switching between formated and raw
   * headers data.
   */
  renderInput(onChange, checked) {
    return (
     input({
       checked,
       className: "devtools-checkbox-toggle",
       onChange,
       type: "checkbox",
     })
    );
  }

  renderToggleRawHeadersBtn(path) {
    let inputElement;

    const rawHeaderType = this.getRawHeaderType(path);
    switch (rawHeaderType) {
      case "REQUEST":
        // Render toggle button for REQUEST header
        inputElement =
        this.renderInput(
          this.toggleRawRequestHeaders, this.state.rawRequestHeadersOpened);
        break;
      case "RESPONSE":
        // Render toggle button for RESPONSE header
        inputElement = this.renderInput(
          this.toggleRawResponseHeaders, this.state.rawResponseHeadersOpened);
        break;
      case "UPLOAD":
        // Render toggle button for UPLOAD header
        inputElement =
        this.renderInput(
          this.toggleRawUploadHeaders, this.state.rawUploadHeadersOpened);
        break;
    }

    return (
      label({ className: "raw-headers-toggle" },
        span({ className: "headers-summary-label"},
          RAW_HEADERS
        ),
        div({ className: "raw-headers-toggle-input" },
          inputElement
        )
      )
    );
  }

  renderValue(props) {
    const member = props.member;
    const value = props.value;
    const path = member.path;
    let toggleRawHeadersBtn;

    // When member.level === 0, it is a section label
    // Request/Response header
    if (member.level === 0) {
      toggleRawHeadersBtn = this.renderToggleRawHeadersBtn(path);

      // Return label and toggle button
      return toggleRawHeadersBtn;
    }

    if (typeof value !== "string") {
      return null;
    }

    const headerDocURL = getHeadersURL(member.name);

    return (
      div({ className: "treeValueCellDivider" },
        Rep(Object.assign(props, {
          // FIXME: A workaround for the issue in StringRep
          // Force StringRep to crop the text everytime
          member: Object.assign({}, member, { open: false }),
          mode: MODE.TINY,
          cropLimit: 60,
          noGrip: true,
        })),
        headerDocURL ? MDNLink({
          url: headerDocURL,
        }) : null
      )
    );
  }

  render() {
    const {
      openLink,
      cloneSelectedRequest,
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
    const item = { fromCache, fromServiceWorker, status, statusText };

    if ((!requestHeaders || !requestHeaders.headers.length) &&
        (!uploadHeaders || !uploadHeaders.headers.length) &&
        (!responseHeaders || !responseHeaders.headers.length)) {
      return div({ className: "empty-notice" },
        HEADERS_EMPTY_TEXT
      );
    }

    const object = Object.assign({},
      this.getProperties(responseHeaders, RESPONSE_HEADERS),
      this.getProperties(requestHeaders, REQUEST_HEADERS),
      this.getProperties(uploadHeaders, REQUEST_HEADERS_FROM_UPLOAD),
    );

    // not showing #hash in url
    const summaryUrl = urlDetails.url ?
      this.renderSummary(SUMMARY_URL, urlDetails.url.split("#")[0]) : null;

    const summaryMethod = method ?
      this.renderSummary(SUMMARY_METHOD, method) : null;

    const summaryAddress = remoteAddress ?
      this.renderSummary(SUMMARY_ADDRESS,
        getFormattedIPAndPort(remoteAddress, remotePort)) : null;

    let summaryStatus;

    if (status) {
      const statusCodeDocURL = getHTTPStatusCodeURL(status.toString());
      const inputWidth = statusText.length + 1;

      summaryStatus = (
        div({ className: "tabpanel-summary-container headers-summary" },
          div({
            className: "tabpanel-summary-label headers-summary-label",
          }, SUMMARY_STATUS),
          StatusCode({ item }),
          input({
            className: "tabpanel-summary-value textbox-input devtools-monospace"
              + " status-text",
            readOnly: true,
            value: `${statusText}`,
            size: `${inputWidth}`,
          }),
          statusCodeDocURL ? MDNLink({
            url: statusCodeDocURL,
            title: SUMMARY_STATUS_LEARN_MORE,
          }) : span({
            className: "headers-summary learn-more-link",
          }),
        )
      );
    }

    let trackingProtectionStatus;

    if (isThirdPartyTrackingResource) {
      const trackingProtectionDocURL = getTrackingProtectionURL();

      trackingProtectionStatus = (
        div({ className: "tabpanel-summary-container tracking-protection" },
          div({
            className: "tracking-resource",
          }),
          L10N.getStr("netmonitor.trackingResource.tooltip"),
          trackingProtectionDocURL ? MDNLink({
            url: trackingProtectionDocURL,
            title: SUMMARY_STATUS_LEARN_MORE,
          }) : span({
            className: "headers-summary learn-more-link",
          }),
        )
      );
    }

    const summaryVersion = httpVersion ?
      this.renderSummary(SUMMARY_VERSION, httpVersion) : null;

    const summaryReferrerPolicy = referrerPolicy ?
      this.renderSummary(SUMMARY_REFERRER_POLICY, referrerPolicy) : null;

    const summaryEditAndResendBtn = (
      div({
        className: "summary-edit-and-resend" },
        summaryReferrerPolicy,
        button({
          className: "edit-and-resend-button devtools-button",
          onClick: cloneSelectedRequest,
        }, EDIT_AND_RESEND)
      )
    );

    return (
      div({ className: "panel-container" },
        div({ className: "headers-overview" },
          summaryUrl,
          trackingProtectionStatus,
          summaryMethod,
          summaryAddress,
          summaryStatus,
          summaryVersion,
          summaryEditAndResendBtn,
        ),
        PropertiesView({
          object,
          provider: HeadersProvider,
          filterPlaceHolder: HEADERS_FILTER_TEXT,
          sectionNames: Object.keys(object),
          renderRow: this.renderRow,
          renderValue: this.renderValue,
          openLink,
        }),
      )
    );
  }
}

module.exports = HeadersPanel;
