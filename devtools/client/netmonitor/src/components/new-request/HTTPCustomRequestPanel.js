/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const asyncStorage = require("devtools/shared/async-storage");
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
} = require("devtools/client/netmonitor/src/utils/request-utils");
const InputMap = createFactory(
  require("devtools/client/netmonitor/src/components/new-request/InputMap")
);
const { button, div, footer, label, textarea, select, option } = dom;

const CUSTOM_HEADERS = L10N.getStr("netmonitor.custom.newRequestHeaders");
const CUSTOM_NEW_REQUEST_URL_LABEL = L10N.getStr(
  "netmonitor.custom.newRequestUrlLabel"
);
const CUSTOM_POSTDATA = L10N.getStr("netmonitor.custom.postBody");
const CUSTOM_POSTDATA_PLACEHOLDER = L10N.getStr(
  "netmonitor.custom.postBody.placeholder"
);
const CUSTOM_QUERY = L10N.getStr("netmonitor.custom.urlParameters");
const CUSTOM_SEND = L10N.getStr("netmonitor.custom.send");
const CUSTOM_CLEAR = L10N.getStr("netmonitor.custom.clear");

const FIREFOX_DEFAULT_HEADERS = [
  "Accept-Charset",
  "Accept-Encoding",
  "Access-Control-Request-Headers",
  "Access-Control-Request-Method",
  "Connection",
  "Content-Length",
  "Cookie",
  "Cookie2",
  "Date",
  "DNT",
  "Expect",
  "Feature-Policy",
  "Host",
  "Keep-Alive",
  "Origin",
  "Proxy-",
  "Sec-",
  "Referer",
  "TE",
  "Trailer",
  "Transfer-Encoding",
  "Upgrade",
  "Via",
];
// This does not include the CONNECT method as it is restricted and special.
// See https://bugzilla.mozilla.org/show_bug.cgi?id=1769572#c2 for details
const HTTP_METHODS = [
  "GET",
  "HEAD",
  "POST",
  "DELETE",
  "PUT",
  "OPTIONS",
  "TRACE",
  "PATCH",
];

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

    this.state = {
      method: HTTP_METHODS[0],
      url: "",
      urlQueryParams: [],
      headers: [],
      postBody: "",
      // Flag to know the data from either the request or the async storage has
      // been loaded in componentDidMount
      _isStateDataReady: false,
    };

    this.handleInputChange = this.handleInputChange.bind(this);
    this.handleChangeURL = this.handleChangeURL.bind(this);
    this.updateInputMapItem = this.updateInputMapItem.bind(this);
    this.addInputMapItem = this.addInputMapItem.bind(this);
    this.deleteInputMapItem = this.deleteInputMapItem.bind(this);
    this.checkInputMapItem = this.checkInputMapItem.bind(this);
    this.handleClear = this.handleClear.bind(this);
    this.createQueryParamsListFromURL = this.createQueryParamsListFromURL.bind(
      this
    );
    this.onUpdateQueryParams = this.onUpdateQueryParams.bind(this);
  }

  async componentDidMount() {
    let { connector, request } = this.props;
    const persistedCustomRequest = await asyncStorage.getItem(
      "devtools.netmonitor.customRequest"
    );
    request = request || persistedCustomRequest;

    if (!request) {
      this.setState({ _isStateDataReady: true });
      return;
    }

    // We need this part because in the asyncStorage we are saving the request in one format
    // and from the edit and resend it comes in a different form with different properties,
    // so we need this to nomalize the request.
    if (request.requestHeaders) {
      request.headers = request.requestHeaders.headers;
    }

    if (request.requestPostData?.postData?.text) {
      request.postBody = request.requestPostData.postData.text;
    }

    const headers = request.headers
      .map(({ name, value }) => {
        return {
          name,
          value,
          checked: true,
          disabled: FIREFOX_DEFAULT_HEADERS.some(i => name.startsWith(i)),
        };
      })
      .sort((a, b) => {
        if (a.disabled && !b.disabled) {
          return -1;
        }
        if (!a.disabled && b.disabled) {
          return 1;
        }
        return 0;
      });

    if (request.requestPostDataAvailable && !request.postBody) {
      const requestData = await connector.requestData(
        request.id,
        "requestPostData"
      );
      request.postBody = requestData.postData.text;
    }

    this.setState({
      method: request.method,
      url: request.url,
      urlQueryParams: this.createQueryParamsListFromURL(request.url),
      headers,
      postBody: request.postBody,
      _isStateDataReady: true,
    });
  }

  componentDidUpdate(prevProps, prevState) {
    // This is when the query params change in the url params input map
    if (
      prevState.urlQueryParams !== this.state.urlQueryParams &&
      prevState.url === this.state.url
    ) {
      this.onUpdateQueryParams();
    }
  }

  componentWillUnmount() {
    asyncStorage.setItem("devtools.netmonitor.customRequest", this.state);
  }

  handleChangeURL(event) {
    const { value } = event.target;

    this.setState({
      url: value,
      urlQueryParams: this.createQueryParamsListFromURL(value),
    });
  }

  handleInputChange(event) {
    const { name, value } = event.target;
    const newState = {
      [name]: value,
    };

    // If the message body changes lets make sure we
    // keep the content-length up to date.
    if (name == "postBody") {
      newState.headers = this.state.headers.map(header => {
        if (header.name == "Content-Length") {
          header.value = value.length;
        }
        return header;
      });
    }

    this.setState(newState);
  }

  updateInputMapItem(stateName, event) {
    const { name, value } = event.target;
    const [prop, index] = name.split("-");
    const updatedList = [...this.state[stateName]];
    updatedList[Number(index)][prop] = value;

    this.setState({
      [stateName]: updatedList,
    });
  }

  addInputMapItem(stateName, name, value) {
    this.setState({
      [stateName]: [
        ...this.state[stateName],
        { name, value, checked: true, disabled: false },
      ],
    });
  }

  deleteInputMapItem(stateName, index) {
    this.setState({
      [stateName]: this.state[stateName].filter((_, i) => i !== index),
    });
  }

  checkInputMapItem(stateName, index, checked) {
    this.setState({
      [stateName]: this.state[stateName].map((item, i) => {
        if (index === i) {
          return {
            ...item,
            checked,
          };
        }
        return item;
      }),
    });
  }

  onUpdateQueryParams() {
    const { urlQueryParams, url } = this.state;
    let queryString = "";
    for (const { name, value, checked } of urlQueryParams) {
      if (checked) {
        queryString += `${encodeURIComponent(name)}=${encodeURIComponent(
          value
        )}&`;
      }
    }

    let finalURL = url.split("?")[0];

    if (queryString.length) {
      finalURL += `?${queryString.substring(0, queryString.length - 1)}`;
    }
    this.setState({
      url: finalURL,
    });
  }

  createQueryParamsListFromURL(url = "") {
    const parsedQuery = parseQueryString(getUrlQuery(url) || url.split("?")[1]);
    const queryArray = parsedQuery || [];
    return queryArray.map(({ name, value }) => {
      return {
        checked: true,
        name,
        value,
      };
    });
  }

  handleClear() {
    this.setState({
      method: HTTP_METHODS[0],
      url: "",
      urlQueryParams: [],
      headers: [],
      postBody: "",
    });
  }

  render() {
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
              value: this.state.method,
            },

            HTTP_METHODS.map(item =>
              option(
                {
                  value: item,
                  key: item,
                },
                item
              )
            )
          ),
          div(
            {
              className: "auto-growing-textarea",
              "data-replicated-value": this.state.url,
              title: this.state.url,
            },
            textarea({
              className: "http-custom-url-value",
              id: "http-custom-url-value",
              name: "url",
              placeholder: CUSTOM_NEW_REQUEST_URL_LABEL,
              onChange: event => {
                this.handleChangeURL(event);
              },
              onBlur: this.handleTextareaChange,
              value: this.state.url,
              rows: 1,
            })
          )
        ),
        div(
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
          // This is the input map for the Url Parameters Component
          InputMap({
            list: this.state.urlQueryParams,
            onUpdate: event => {
              this.updateInputMapItem(
                "urlQueryParams",
                event,
                this.onUpdateQueryParams
              );
            },
            onAdd: (name, value) =>
              this.addInputMapItem(
                "urlQueryParams",
                name,
                value,
                this.onUpdateQueryParams
              ),
            onDelete: index =>
              this.deleteInputMapItem(
                "urlQueryParams",
                index,
                this.onUpdateQueryParams
              ),
            onChecked: (index, checked) => {
              this.checkInputMapItem(
                "urlQueryParams",
                index,
                checked,
                this.onUpdateQueryParams
              );
            },
          })
        ),
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
          // This is the input map for the Headers Component
          InputMap({
            ref: this.headersListRef,
            list: this.state.headers,
            onUpdate: event => {
              this.updateInputMapItem("headers", event);
            },
            onAdd: (name, value) =>
              this.addInputMapItem("headers", name, value),
            onDelete: index => this.deleteInputMapItem("headers", index),
            onChecked: (index, checked) => {
              this.checkInputMapItem("headers", index, checked);
            },
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
            name: "postBody",
            placeholder: CUSTOM_POSTDATA_PLACEHOLDER,
            onChange: this.handleInputChange,
            rows: 6,
            value: this.state.postBody,
            wrap: "off",
          })
        )
      ),
      footer(
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
            disabled:
              !this.state._isStateDataReady ||
              !this.state.url ||
              !this.state.method,
            onClick: () => {
              const newRequest = {
                method: this.state.method,
                url: this.state.url,
                cause: this.props.request?.cause,
                urlQueryParams: this.state.urlQueryParams.map(
                  ({ checked, ...params }) => params
                ),
                requestHeaders: {
                  headers: this.state.headers
                    .filter(({ checked }) => checked)
                    .map(({ checked, ...headersValues }) => headersValues),
                },
              };

              if (this.state.postBody) {
                newRequest.requestPostData = {
                  postData: {
                    text: this.state.postBody,
                  },
                };
              }
              this.props.sendCustomRequest(newRequest);
            },
          },
          CUSTOM_SEND
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
