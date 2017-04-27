/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createClass,
  createFactory,
  DOM,
  PropTypes,
} = require("devtools/client/shared/vendor/react");
const { NetMonitorController } = require("../netmonitor-controller");
const {
  getFormattedIPAndPort,
  getFormattedSize,
} = require("../utils/format-utils");
const { L10N } = require("../utils/l10n");
const {
  getHeadersURL,
  getHTTPStatusCodeURL,
} = require("../utils/mdn-utils");
const { writeHeaderText } = require("../utils/request-utils");

// Components
const { REPS, MODE } = require("devtools/client/shared/components/reps/reps");
const MDNLink = createFactory(require("./mdn-link"));
const PropertiesView = createFactory(require("./properties-view"));

const { Rep } = REPS;
const { button, div, input, textarea } = DOM;

const EDIT_AND_RESEND = L10N.getStr("netmonitor.summary.editAndResend");
const RAW_HEADERS = L10N.getStr("netmonitor.summary.rawHeaders");
const RAW_HEADERS_REQUEST = L10N.getStr("netmonitor.summary.rawHeaders.requestHeaders");
const RAW_HEADERS_RESPONSE = L10N.getStr("netmonitor.summary.rawHeaders.responseHeaders");
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

/*
 * Headers panel component
 * Lists basic information about the request
 */
const HeadersPanel = createClass({
  displayName: "HeadersPanel",

  propTypes: {
    cloneSelectedRequest: PropTypes.func.isRequired,
    request: PropTypes.object.isRequired,
    renderValue: PropTypes.func
  },

  getInitialState() {
    return {
      rawHeadersOpened: false,
    };
  },

  getProperties(headers, title) {
    if (headers && headers.headers.length) {
      return {
        [`${title} (${getFormattedSize(headers.headersSize, 3)})`]:
          headers.headers.reduce((acc, { name, value }) =>
            name ? Object.assign(acc, { [name]: value }) : acc
          , {})
      };
    }

    return null;
  },

  toggleRawHeaders() {
    this.setState({
      rawHeadersOpened: !this.state.rawHeadersOpened,
    });
  },

  renderSummary(label, value) {
    return (
      div({ className: "tabpanel-summary-container headers-summary" },
        div({
          className: "tabpanel-summary-label headers-summary-label",
        }, label),
        input({
          className: "tabpanel-summary-value textbox-input devtools-monospace",
          readOnly: true,
          value,
        }),
      )
    );
  },

  renderValue(props) {
    const member = props.member;
    const value = props.value;

    if (typeof value !== "string") {
      return null;
    }

    let headerDocURL = getHeadersURL(member.name);

    return (
      div({ className: "treeValueCellDivider" },
        Rep(Object.assign(props, {
          // FIXME: A workaround for the issue in StringRep
          // Force StringRep to crop the text everytime
          member: Object.assign({}, member, { open: false }),
          mode: MODE.TINY,
          cropLimit: 60,
        })),
        headerDocURL ? MDNLink({
          url: headerDocURL,
        }) : null
      )
    );
  },

  render() {
    const {
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
      },
    } = this.props;

    if ((!requestHeaders || !requestHeaders.headers.length) &&
        (!uploadHeaders || !uploadHeaders.headers.length) &&
        (!responseHeaders || !responseHeaders.headers.length)) {
      return div({ className: "empty-notice" },
        HEADERS_EMPTY_TEXT
      );
    }

    let object = Object.assign({},
      this.getProperties(responseHeaders, RESPONSE_HEADERS),
      this.getProperties(requestHeaders, REQUEST_HEADERS),
      this.getProperties(uploadHeaders, REQUEST_HEADERS_FROM_UPLOAD),
    );

    let summaryUrl = urlDetails.unicodeUrl ?
      this.renderSummary(SUMMARY_URL, urlDetails.unicodeUrl) : null;

    let summaryMethod = method ?
      this.renderSummary(SUMMARY_METHOD, method) : null;

    let summaryAddress = remoteAddress ?
      this.renderSummary(SUMMARY_ADDRESS,
        getFormattedIPAndPort(remoteAddress, remotePort)) : null;

    let summaryStatus;

    if (status) {
      let code;
      if (fromCache) {
        code = "cached";
      } else if (fromServiceWorker) {
        code = "service worker";
      } else {
        code = status;
      }

      let statusCodeDocURL = getHTTPStatusCodeURL(code);
      let inputWidth = status.length + statusText.length + 1;

      summaryStatus = (
        div({ className: "tabpanel-summary-container headers-summary" },
          div({
            className: "tabpanel-summary-label headers-summary-label",
          }, SUMMARY_STATUS),
          div({
            className: "requests-list-status-icon",
            "data-code": code,
          }),
          input({
            className: "tabpanel-summary-value textbox-input devtools-monospace"
              + " status-text",
            readOnly: true,
            value: `${status} ${statusText}`,
            size: `${inputWidth}`,
          }),
          statusCodeDocURL ? MDNLink({
            url: statusCodeDocURL,
          }) : null,
          NetMonitorController.supportsCustomRequest && button({
            className: "devtools-button",
            onClick: cloneSelectedRequest,
          }, EDIT_AND_RESEND),
          button({
            className: "devtools-button",
            onClick: this.toggleRawHeaders,
          }, RAW_HEADERS),
        )
      );
    }

    let summaryVersion = httpVersion ?
      this.renderSummary(SUMMARY_VERSION, httpVersion) : null;

    let summaryRawHeaders;
    if (this.state.rawHeadersOpened) {
      summaryRawHeaders = (
        div({ className: "tabpanel-summary-container headers-summary" },
          div({ className: "raw-headers-container" },
            div({ className: "raw-headers" },
              div({ className: "tabpanel-summary-label" }, RAW_HEADERS_REQUEST),
              textarea({
                value: writeHeaderText(requestHeaders.headers),
                readOnly: true,
              }),
            ),
            div({ className: "raw-headers" },
              div({ className: "tabpanel-summary-label" }, RAW_HEADERS_RESPONSE),
              textarea({
                value: writeHeaderText(responseHeaders.headers),
                readOnly: true,
              }),
            ),
          )
        )
      );
    }

    return (
      div({ className: "panel-container" },
        div({ className: "headers-overview" },
          summaryUrl,
          summaryMethod,
          summaryAddress,
          summaryStatus,
          summaryVersion,
          summaryRawHeaders,
        ),
        PropertiesView({
          object,
          filterPlaceHolder: HEADERS_FILTER_TEXT,
          sectionNames: Object.keys(object),
          renderValue: this.renderValue,
        }),
      )
    );
  }
});

module.exports = HeadersPanel;
