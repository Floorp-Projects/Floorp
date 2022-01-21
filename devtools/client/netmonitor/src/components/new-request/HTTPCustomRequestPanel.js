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
  getClickedRequest,
} = require("devtools/client/netmonitor/src/selectors/index");
const {
  getUrlQuery,
  parseQueryString,
  writeHeaderText,
} = require("devtools/client/netmonitor/src/utils/request-utils");
const { button, div, input, label, textarea, select, option } = dom;

const CUSTOM_HEADERS = L10N.getStr("netmonitor.custom.headers");
const CUSTOM_NEW_REQUEST_URL_LABEL = L10N.getStr(
  "netmonitor.custom.newRequestUrlLabel"
);
const CUSTOM_POSTDATA = L10N.getStr("netmonitor.custom.postData");
const CUSTOM_QUERY = L10N.getStr("netmonitor.custom.query");
const CUSTOM_SEND = L10N.getStr("netmonitor.custom.send");
const CUSTOM_CLEAR = L10N.getStr("netmonitor.custom.clear");

/*
 * HTTP Custom request panel component
 * A network request panel which enables creating and sending new requests
 * or selecting, editing and re-sending current requests.
 */
class HTTPCustomRequestPanel extends Component {
  static get propTypes() {
    return {
      connector: PropTypes.object.isRequired,
      request: PropTypes.object,
      sendCustomRequest: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

    const { request } = props;

    this.state = {
      method: request ? request.method : "",
      url: request ? request.url : "",
      headers: {
        customHeadersValue: "",
        headers: request ? request.requestHeaders.headers : [],
      },
      requestPostData: request
        ? request.requestPostData?.postData.text || ""
        : "",
    };

    this.handleInputChange = this.handleInputChange.bind(this);
    this.handleHeadersChange = this.handleHeadersChange.bind(this);
    this.handleClear = this.handleClear.bind(this);
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

  handleInputChange(event) {
    const target = event.target;
    const value = target.value;
    const name = target.name;

    this.setState({
      [name]: value,
    });
  }

  handleHeadersChange(event) {
    const target = event.target;
    const value = target.value;

    this.setState({
      // Parse text representation of multiple HTTP headers
      headers: {
        customHeadersValue: value || "",
        headers: this.parseRequestText(value, "\\S+?", ":"),
      },
    });
  }

  handleClear() {
    this.setState({
      method: "",
      url: "",
      headers: {
        customHeadersValue: "",
        headers: [],
      },
      requestPostData: "",
    });
  }

  render() {
    const { sendCustomRequest } = this.props;
    const { method, requestPostData, url, headers } = this.state;

    const formattedHeaders = headers.customHeadersValue
      ? headers.customHeadersValue
      : writeHeaderText(headers.headers).trim();

    const queryArray = url ? parseQueryString(getUrlQuery(url)) : [];
    const queryParams = queryArray
      ? queryArray.map(({ name, value }) => name + "=" + value).join("\n")
      : "";

    const methods = [
      "GET",
      "HEAD",
      "POST",
      "DELETE",
      "PUT",
      "CONNECT",
      "OPTIONS",
      "TRACE",
      "PATH",
    ];
    return div(
      { className: "http-custom-request-panel" },
      div(
        { className: "http-custom-request-panel-content" },
        div(
          {
            className: "tabpanel-summary-container http-custom-method-and-url",
            id: "http-custom-method-and-url",
          },
          select(
            {
              className: "http-custom-method-value",
              id: "http-custom-method-value",
              name: "method",
              onChange: this.handleInputChange,
              onBlur: this.handleInputChange,
              value: method,
            },

            methods.map(item =>
              option(
                {
                  value: item,
                  key: item,
                },
                item
              )
            )
          ),
          input({
            className: "http-custom-url-value",
            id: "http-custom-url-value",
            name: "url",
            placeholder: CUSTOM_NEW_REQUEST_URL_LABEL,
            onChange: this.handleInputChange,
            onBlur: this.handleInputChange,
            value: url,
          })
        ),
        // Hide query field when there is no params
        queryParams
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
                name: "queryParams",
                onChange: () => {},
                rows: 4,
                value: queryParams,
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
            name: "headers",
            onChange: this.handleHeadersChange,
            rows: 8,
            value: formattedHeaders,
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
            name: "requestPostData",
            onChange: this.handleInputChange,
            rows: 6,
            value: requestPostData,
            wrap: "off",
          })
        ),
        div(
          { className: "tabpanel-summary-container http-custom-request" },
          div(
            { className: "http-custom-request-button-container" },
            button(
              {
                className: "devtools-button",
                id: "http-custom-request-clear-button",
                onClick: this.handleClear,
              },
              CUSTOM_CLEAR
            ),
            button(
              {
                className: "devtools-button",
                id: "http-custom-request-send-button",
                disabled: !this.state.url,
                onClick: () => sendCustomRequest(this.state),
              },
              CUSTOM_SEND
            )
          )
        )
      )
    );
  }
}

module.exports = connect(
  state => ({ request: getClickedRequest(state) }),
  (dispatch, props) => ({
    sendCustomRequest: request =>
      dispatch(Actions.sendHTTPCustomRequest(props.connector, request)),
  })
)(HTTPCustomRequestPanel);
