/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createRef,
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
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
  updateTextareaRows,
} = require("devtools/client/netmonitor/src/utils/request-utils");
const InputMap = createFactory(
  require("devtools/client/netmonitor/src/components/new-request/InputMap")
);
const { button, div, label, textarea, select, option } = dom;

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

  static createQueryParamsListFromURL(url) {
    const queryArray = (url ? parseQueryString(getUrlQuery(url)) : []) || [];
    return queryArray.map(({ name, value }) => {
      return {
        checked: true,
        name,
        value,
      };
    });
  }

  constructor(props) {
    super(props);

    const { request } = props;

    this.URLTextareaRef = createRef();

    this.state = {
      method: request ? request.method : "",
      url: request ? request.url : "",
      urlQueryParams: HTTPCustomRequestPanel.createQueryParamsListFromURL(
        request?.url
      ),
      headers: request
        ? request.requestHeaders.headers.map(({ name, value }) => {
            return {
              name,
              value,
              checked: true,
            };
          })
        : [],
      requestPostData: request
        ? request.requestPostData?.postData.text || ""
        : "",
    };

    this.handleInputChange = this.handleInputChange.bind(this);
    this.handleChangeURL = this.handleChangeURL.bind(this);
    this.updateInputMapItem = this.updateInputMapItem.bind(this);
    this.addInputMapItem = this.addInputMapItem.bind(this);
    this.deleteInputMapItem = this.deleteInputMapItem.bind(this);
    this.handleClear = this.handleClear.bind(this);
  }

  componentDidMount() {
    updateTextareaRows(this.URLTextareaRef.current);
    this.resizeObserver = new ResizeObserver(entries => {
      updateTextareaRows(this.URLTextareaRef.current);
    });

    this.resizeObserver.observe(this.URLTextareaRef.current);
  }

  componentWillUnmount() {
    if (this.resizeObserver) {
      this.resizeObserver.disconnect();
    }
  }

  handleChangeURL(event) {
    const { value } = event.target;

    this.setState({
      url: value,
      urlQueryParams: HTTPCustomRequestPanel.createQueryParamsListFromURL(
        value
      ),
    });
  }

  handleInputChange(event) {
    const { name, value } = event.target;

    this.setState({
      [name]: value,
    });
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
      [stateName]: [...this.state[stateName], { name, value, checked: true }],
    });
  }

  deleteInputMapItem(stateName, index) {
    this.setState({
      [stateName]: this.state[stateName].filter((_, i) => i !== index),
    });
  }

  handleClear() {
    this.setState(
      {
        method: "",
        url: "",
        urlQueryParams: [],
        headers: [],
        requestPostData: "",
      },
      () => updateTextareaRows(this.URLTextareaRef.current)
    );
  }

  render() {
    const { sendCustomRequest } = this.props;
    const {
      method,
      urlQueryParams,
      requestPostData,
      url,
      headers,
    } = this.state;

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
          textarea({
            className: "http-custom-url-value",
            id: "http-custom-url-value",
            name: "url",
            placeholder: CUSTOM_NEW_REQUEST_URL_LABEL,
            ref: this.URLTextareaRef,
            onChange: event => {
              this.handleChangeURL(event);
              updateTextareaRows(event.target);
            },
            onBlur: this.handleTextareaChange,
            value: url,
            rows: 1,
          })
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
          InputMap({
            list: urlQueryParams,
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
          InputMap({
            ref: this.headersListRef,
            resizeable: true,
            list: headers,
            onUpdate: event => {
              this.updateInputMapItem("headers", event);
            },
            onAdd: (name, value) =>
              this.addInputMapItem("headers", name, value),
            onDelete: index => this.deleteInputMapItem("headers", index),
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
            placeholder: CUSTOM_POSTDATA_PLACEHOLDER,
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
