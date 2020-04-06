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
const {
  fetchNetworkUpdatePacket,
} = require("devtools/client/netmonitor/src/utils/request-utils");
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

const CUSTOM_CANCEL = L10N.getStr("netmonitor.custom.cancel");
const CUSTOM_HEADERS = L10N.getStr("netmonitor.custom.headers");
const CUSTOM_NEW_REQUEST = L10N.getStr("netmonitor.custom.newRequest");
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
 * Custom request panel component
 * A network request editor which simply provide edit and resend interface
 * for network development.
 */
class CustomRequestPanel extends Component {
  static get propTypes() {
    return {
      connector: PropTypes.object.isRequired,
      removeSelectedCustomRequest: PropTypes.func.isRequired,
      request: PropTypes.object,
      sendCustomRequest: PropTypes.func.isRequired,
      updateRequest: PropTypes.func.isRequired,
    };
  }

  componentDidMount() {
    const { request, connector } = this.props;
    this.initialRequestMethod = request.method;
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

  /**
   * Parse a text representation of a name[divider]value list with
   * the given name regex and divider character.
   *
   * @param {string} text - Text of list
   * @return {array} array of headers info {name, value}
   */
  parseRequestText(text, namereg, divider) {
    const regex = new RegExp(`(${namereg})\\${divider}\\s*(\\S.*)`);
    const pairs = [];

    for (const line of text.split("\n")) {
      const matches = regex.exec(line);
      if (matches) {
        const [, name, value] = matches;
        pairs.push({ name, value });
      }
    }
    return pairs;
  }

  /**
   * Update Custom Request Fields
   *
   * @param {Object} evt click event
   * @param {Object} request current request
   * @param {updateRequest} updateRequest action
   */
  updateCustomRequestFields(evt, request, updateRequest) {
    const val = evt.target.value;
    let data;

    switch (evt.target.id) {
      case "custom-headers-value":
        data = {
          requestHeaders: {
            customHeadersValue: val || "",
            // Parse text representation of multiple HTTP headers
            headers: this.parseRequestText(val, "\\S+?", ":"),
          },
        };
        break;
      case "custom-method-value":
        // If val is empty when leaving the "method" field, set the method to
        // its original value
        data =
          evt.type === "blur" && val === ""
            ? { method: this.initialRequestMethod }
            : { method: val.trim() };
        break;
      case "custom-postdata-value":
        // Update "content-length" header value to reflect change
        // in post data field.
        const { requestHeaders } = request;
        const newHeaders = requestHeaders.headers.map(header => {
          if (header.name.toLowerCase() == "content-length") {
            return {
              name: header.name,
              value: val.length,
            };
          }
          return header;
        });

        data = {
          requestPostData: {
            postData: { text: val },
          },
          requestHeaders: {
            headers: newHeaders,
          },
        };
        break;
      case "custom-query-value":
        let customQueryValue = val || "";
        // Parse readable text list of a query string
        const queryArray = customQueryValue
          ? this.parseRequestText(customQueryValue, ".+?", "=")
          : [];
        // Write out a list of query params into a query string
        const queryString = queryArray
          .map(({ name, value }) => name + "=" + value)
          .join("&");
        const url = queryString
          ? [request.url.split("?")[0], queryString].join("?")
          : request.url.split("?")[0];
        // Remove temp customQueryValue while query string is parsable
        if (
          customQueryValue === "" ||
          queryArray.length === customQueryValue.split("\n").length
        ) {
          customQueryValue = null;
        }
        data = {
          customQueryValue,
          url,
        };
        break;
      case "custom-url-value":
        data = {
          customQueryValue: null,
          url: val,
        };
        break;
      default:
        break;
    }
    if (data) {
      // All updateRequest batch mode should be disabled to make UI editing in sync
      updateRequest(request.id, data, false);
    }
  }

  render() {
    const {
      removeSelectedCustomRequest,
      request = {},
      sendCustomRequest,
      updateRequest,
    } = this.props;
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
        : writeHeaderText(requestHeaders.headers);
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
      { className: "custom-request-panel" },
      div(
        { className: "custom-request-label custom-header" },
        CUSTOM_NEW_REQUEST
      ),
      div(
        { className: "custom-request-panel-content" },
        div(
          { className: "tabpanel-summary-container custom-request" },
          div(
            { className: "custom-request-button-container" },
            button(
              {
                className: "devtools-button",
                id: "custom-request-close-button",
                onClick: removeSelectedCustomRequest,
              },
              CUSTOM_CANCEL
            ),
            button(
              {
                className: "devtools-button",
                id: "custom-request-send-button",
                onClick: sendCustomRequest,
              },
              CUSTOM_SEND
            )
          )
        ),
        div(
          {
            className: "tabpanel-summary-container custom-method-and-url",
            id: "custom-method-and-url",
          },
          label(
            {
              className: "custom-method-value-label custom-request-label",
              htmlFor: "custom-method-value",
            },
            CUSTOM_NEW_REQUEST_METHOD_LABEL
          ),
          input({
            className: "custom-method-value",
            id: "custom-method-value",
            onChange: evt =>
              this.updateCustomRequestFields(evt, request, updateRequest),
            onBlur: evt =>
              this.updateCustomRequestFields(evt, request, updateRequest),
            value: method,
          }),
          label(
            {
              className: "custom-url-value-label custom-request-label",
              htmlFor: "custom-url-value",
            },
            CUSTOM_NEW_REQUEST_URL_LABEL
          ),
          input({
            className: "custom-url-value",
            id: "custom-url-value",
            onChange: evt =>
              this.updateCustomRequestFields(evt, request, updateRequest),
            value: url || "http://",
          })
        ),
        // Hide query field when there is no params
        params
          ? div(
              {
                className: "tabpanel-summary-container custom-section",
                id: "custom-query",
              },
              label(
                {
                  className: "custom-request-label",
                  htmlFor: "custom-query-value",
                },
                CUSTOM_QUERY
              ),
              textarea({
                className: "tabpanel-summary-input",
                id: "custom-query-value",
                onChange: evt =>
                  this.updateCustomRequestFields(evt, request, updateRequest),
                rows: 4,
                value: params,
                wrap: "off",
              })
            )
          : null,
        div(
          {
            id: "custom-headers",
            className: "tabpanel-summary-container custom-section",
          },
          label(
            {
              className: "custom-request-label",
              htmlFor: "custom-headers-value",
            },
            CUSTOM_HEADERS
          ),
          textarea({
            className: "tabpanel-summary-input",
            id: "custom-headers-value",
            onChange: evt =>
              this.updateCustomRequestFields(evt, request, updateRequest),
            rows: 8,
            value: headers,
            wrap: "off",
          })
        ),
        div(
          {
            id: "custom-postdata",
            className: "tabpanel-summary-container custom-section",
          },
          label(
            {
              className: "custom-request-label",
              htmlFor: "custom-postdata-value",
            },
            CUSTOM_POSTDATA
          ),
          textarea({
            className: "tabpanel-summary-input",
            id: "custom-postdata-value",
            onChange: evt =>
              this.updateCustomRequestFields(evt, request, updateRequest),
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
    removeSelectedCustomRequest: () =>
      dispatch(Actions.removeSelectedCustomRequest()),
    sendCustomRequest: () =>
      dispatch(Actions.sendCustomRequest(props.connector)),
    updateRequest: (id, data, batch) =>
      dispatch(Actions.updateRequest(id, data, batch)),
  })
)(CustomRequestPanel);
