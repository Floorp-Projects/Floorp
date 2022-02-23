/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const {
  connect,
} = require("devtools/client/shared/redux/visibility-handler-connect");
const { L10N } = require("devtools/client/netmonitor/src/utils/l10n");
const Actions = require("devtools/client/netmonitor/src/actions/index");
const {
  getSelectedRequest,
} = require("devtools/client/netmonitor/src/selectors/index");
const {
  getUrlQuery,
  parseQueryString,
  writeHeaderText,
} = require("devtools/client/netmonitor/src/utils/request-utils");

const { button, div, input, label, textarea } = dom;

const CUSTOM_HEADERS = L10N.getStr("netmonitor.custom.headers");
const CUSTOM_NEW_REQUEST_METHOD_LABEL = L10N.getStr(
  "netmonitor.custom.newRequestMethodLabel"
);
const CUSTOM_NEW_REQUEST_URL_LABEL = L10N.getStr(
  "netmonitor.custom.newRequestUrlLabel"
);
const CUSTOM_POSTDATA = L10N.getStr("netmonitor.custom.postData");
const CUSTOM_QUERY = L10N.getStr("netmonitor.custom.query");
const CUSTOM_SEND = L10N.getStr("netmonitor.custom.send");

/*
 * HTTP Custom request panel component
 * A network request panel which enables creating and sending new requests
 * or selecting, editing and re-sending current requests.
 */
class HTTPCustomRequestPanel extends Component {
  static get propTypes() {
    return {
      connector: PropTypes.object,
      request: PropTypes.object,
      sendCustomRequest: PropTypes.func.isRequired,
    };
  }

  render() {
    const { request = {}, sendCustomRequest } = this.props;
    const {
      method,
      customQueryValue,
      requestHeaders,
      requestPostData,
      url,
    } = request;

    let headers = "";
    if (requestHeaders) {
      headers = requestHeaders.customHeadersValue
        ? requestHeaders.customHeadersValue
        : writeHeaderText(requestHeaders.headers).trim();
    }
    const queryArray = url ? parseQueryString(getUrlQuery(url)) : [];
    let params = customQueryValue;
    if (!params) {
      params = queryArray
        ? queryArray.map(({ name, value }) => name + "=" + value).join("\n")
        : "";
    }
    const postData = requestPostData?.postData.text
      ? requestPostData.postData.text
      : "";

    return div(
      { className: "http-custom-request-panel" },
      div(
        { className: "http-custom-request-panel-content" },
        div(
          { className: "tabpanel-summary-container http-custom-request" },
          div(
            { className: "http-custom-request-button-container" },
            button(
              {
                className: "devtools-button",
                id: "http-custom-request-send-button",
                onClick: sendCustomRequest,
              },
              CUSTOM_SEND
            )
          )
        ),
        div(
          {
            className: "tabpanel-summary-container http-custom-method-and-url",
            id: "http-custom-method-and-url",
          },
          label(
            {
              className:
                "http-custom-method-value-label http-custom-request-label",
              htmlFor: "http-custom-method-value",
            },
            CUSTOM_NEW_REQUEST_METHOD_LABEL
          ),
          input({
            className: "http-custom-method-value",
            id: "http-custom-method-value",
            onChange: evt => {},
            onBlur: () => {},
            value: method,
          }),
          label(
            {
              className:
                "http-custom-url-value-label http-custom-request-label",
              htmlFor: "http-custom-url-value",
            },
            CUSTOM_NEW_REQUEST_URL_LABEL
          ),
          input({
            className: "http-custom-url-value",
            id: "http-custom-url-value",
            onChange: evt => {},
            value: url || "http://",
          })
        ),
        // Hide query field when there is no params
        params
          ? div(
              {
                className: "tabpanel-summary-container http-custom-section",
                id: "http-custom-query",
              },
              label(
                {
                  className: "http-custom-request-label",
                  htmlFor: "http-custom-query-value",
                },
                CUSTOM_QUERY
              ),
              textarea({
                className: "tabpanel-summary-input",
                id: "http-custom-query-value",
                onChange: evt => {},
                rows: 4,
                value: params,
                wrap: "off",
              })
            )
          : null,
        div(
          {
            id: "http-custom-headers",
            className: "tabpanel-summary-container http-custom-section",
          },
          label(
            {
              className: "http-custom-request-label",
              htmlFor: "custom-headers-value",
            },
            CUSTOM_HEADERS
          ),
          textarea({
            className: "tabpanel-summary-input",
            id: "http-custom-headers-value",
            onChange: evt => {},
            rows: 8,
            value: headers,
            wrap: "off",
          })
        ),
        div(
          {
            id: "http-custom-postdata",
            className: "tabpanel-summary-container http-custom-section",
          },
          label(
            {
              className: "http-custom-request-label",
              htmlFor: "http-custom-postdata-value",
            },
            CUSTOM_POSTDATA
          ),
          textarea({
            className: "tabpanel-summary-input",
            id: "http-custom-postdata-value",
            onChange: evt => {},
            rows: 6,
            value: postData,
            wrap: "off",
          })
        )
      )
    );
  }
}

module.exports = connect(
  state => ({ request: getSelectedRequest(state) }),
  (dispatch, props) => ({
    sendCustomRequest: () =>
      dispatch(Actions.sendCustomRequest(props.connector)),
  })
)(HTTPCustomRequestPanel);
