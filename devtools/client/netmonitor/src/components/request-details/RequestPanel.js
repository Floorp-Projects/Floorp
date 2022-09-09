/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const {
  connect,
} = require("devtools/client/shared/redux/visibility-handler-connect");
const { L10N } = require("devtools/client/netmonitor/src/utils/l10n");
const {
  fetchNetworkUpdatePacket,
  parseFormData,
  parseJSON,
} = require("devtools/client/netmonitor/src/utils/request-utils");
const {
  sortObjectKeys,
} = require("devtools/client/netmonitor/src/utils/sort-utils");
const {
  FILTER_SEARCH_DELAY,
} = require("devtools/client/netmonitor/src/constants");
const {
  updateFormDataSections,
} = require("devtools/client/netmonitor/src/utils/request-utils");
const Actions = require("devtools/client/netmonitor/src/actions/index");

// Components
const PropertiesView = createFactory(
  require("devtools/client/netmonitor/src/components/request-details/PropertiesView")
);
const SearchBox = createFactory(
  require("devtools/client/shared/components/SearchBox")
);

loader.lazyGetter(this, "SourcePreview", function() {
  return createFactory(
    require("devtools/client/netmonitor/src/components/previews/SourcePreview")
  );
});

const { div, input, label, span, h2 } = dom;

const JSON_SCOPE_NAME = L10N.getStr("jsonScopeName");
const REQUEST_EMPTY_TEXT = L10N.getStr("paramsNoPayloadText");
const REQUEST_FILTER_TEXT = L10N.getStr("paramsFilterText");
const REQUEST_FORM_DATA = L10N.getStr("paramsFormData");
const REQUEST_POST_PAYLOAD = L10N.getStr("paramsPostPayload");
const RAW_REQUEST_PAYLOAD = L10N.getStr("netmonitor.request.raw");
const REQUEST_TRUNCATED = L10N.getStr("requestTruncated");

/**
 * Params panel component
 * Displays the GET parameters and POST data of a request
 */
class RequestPanel extends Component {
  static get propTypes() {
    return {
      connector: PropTypes.object.isRequired,
      openLink: PropTypes.func,
      request: PropTypes.object.isRequired,
      updateRequest: PropTypes.func.isRequired,
      targetSearchResult: PropTypes.object,
    };
  }

  constructor(props) {
    super(props);
    this.state = {
      filterText: "",
      rawRequestPayloadDisplayed: !!props.targetSearchResult,
    };

    this.toggleRawRequestPayload = this.toggleRawRequestPayload.bind(this);
    this.renderRawRequestPayloadBtn = this.renderRawRequestPayloadBtn.bind(
      this
    );
  }

  componentDidMount() {
    const { request, connector } = this.props;
    fetchNetworkUpdatePacket(connector.requestData, request, [
      "requestPostData",
    ]);
    updateFormDataSections(this.props);
  }

  // FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=1774507
  UNSAFE_componentWillReceiveProps(nextProps) {
    const { request, connector } = nextProps;
    fetchNetworkUpdatePacket(connector.requestData, request, [
      "requestPostData",
    ]);
    updateFormDataSections(nextProps);

    if (nextProps.targetSearchResult !== null) {
      this.setState({
        rawRequestPayloadDisplayed: !!nextProps.targetSearchResult,
      });
    }
  }

  /**
   * Update only if:
   * 1) The rendered object has changed
   * 2) The filter text has changed
   * 2) The display got toggled between formatted and raw data
   * 3) The user selected another search result target.
   */
  shouldComponentUpdate(nextProps, nextState) {
    return (
      this.props.request !== nextProps.request ||
      this.state.filterText !== nextState.filterText ||
      this.state.rawRequestPayloadDisplayed !==
        nextState.rawRequestPayloadDisplayed ||
      this.props.targetSearchResult !== nextProps.targetSearchResult
    );
  }

  /**
   * Mapping array to dict for TreeView usage.
   * Since TreeView only support Object(dict) format.
   * This function also deal with duplicate key case
   * (for multiple selection and query params with same keys)
   *
   * This function is not sorting result properties since it can
   * results in unexpected order of params. See bug 1469533
   *
   * @param {Object[]} arr - key-value pair array or form params
   * @returns {Object} Rep compatible object
   */
  getProperties(arr) {
    return arr.reduce((map, obj) => {
      const value = map[obj.name];
      if (value || value === "") {
        if (typeof value !== "object") {
          map[obj.name] = [value];
        }
        map[obj.name].push(obj.value);
      } else {
        map[obj.name] = obj.value;
      }
      return map;
    }, {});
  }

  toggleRawRequestPayload() {
    this.setState({
      rawRequestPayloadDisplayed: !this.state.rawRequestPayloadDisplayed,
    });
  }

  renderRawRequestPayloadBtn(key, checked, onChange) {
    return [
      label(
        {
          key: `${key}RawRequestPayloadBtn`,
          className: "raw-data-toggle",
          htmlFor: `raw-${key}-checkbox`,
          onClick: event => {
            // stop the header click event
            event.stopPropagation();
          },
        },
        span({ className: "raw-data-toggle-label" }, RAW_REQUEST_PAYLOAD),
        span(
          { className: "raw-data-toggle-input" },
          input({
            id: `raw-${key}-checkbox`,
            checked,
            className: "devtools-checkbox-toggle",
            onChange,
            type: "checkbox",
          })
        )
      ),
    ];
  }

  renderRequestPayload(component, componentProps) {
    return component(componentProps);
  }

  render() {
    const { request, targetSearchResult } = this.props;
    const { filterText, rawRequestPayloadDisplayed } = this.state;
    const { formDataSections, mimeType, requestPostData } = request;
    const postData = requestPostData ? requestPostData.postData?.text : null;

    if ((!formDataSections || formDataSections.length === 0) && !postData) {
      return div({ className: "empty-notice" }, REQUEST_EMPTY_TEXT);
    }

    let component;
    let componentProps;
    let requestPayloadLabel = REQUEST_POST_PAYLOAD;
    let hasFormattedDisplay = false;

    let error;

    // Form Data section
    if (formDataSections && formDataSections.length) {
      const sections = formDataSections.filter(str => /\S/.test(str)).join("&");
      component = PropertiesView;
      componentProps = {
        object: this.getProperties(parseFormData(sections)),
        filterText,
        targetSearchResult,
        defaultSelectFirstNode: false,
      };
      requestPayloadLabel = REQUEST_FORM_DATA;
      hasFormattedDisplay = true;
    }

    // Request payload section
    const limit = Services.prefs.getIntPref(
      "devtools.netmonitor.requestBodyLimit"
    );

    // Check if the request post data has been truncated from the backend,
    // in which case no parse should be attempted.
    if (postData && limit <= postData.length) {
      error = REQUEST_TRUNCATED;
    }
    if (formDataSections && formDataSections.length === 0 && postData) {
      if (!error) {
        const jsonParsedPostData = parseJSON(postData);
        const { json, strippedChars } = jsonParsedPostData;
        // If XSSI characters were present in the request just display the raw
        // data because a request should never have XSSI escape characters
        if (strippedChars) {
          hasFormattedDisplay = false;
        } else if (json) {
          component = PropertiesView;
          componentProps = {
            object: sortObjectKeys(json),
            filterText,
            targetSearchResult,
            defaultSelectFirstNode: false,
          };
          requestPayloadLabel = JSON_SCOPE_NAME;
          hasFormattedDisplay = true;
        }
      }
    }

    if (
      (!hasFormattedDisplay || this.state.rawRequestPayloadDisplayed) &&
      postData
    ) {
      component = SourcePreview;
      componentProps = {
        text: postData,
        mode: mimeType?.replace(/;.+/, ""),
        targetSearchResult,
      };
      requestPayloadLabel = REQUEST_POST_PAYLOAD;
    }

    return div(
      { className: "panel-container" },
      error && div({ className: "request-error-header", title: error }, error),
      div(
        { className: "devtools-toolbar devtools-input-toolbar" },
        SearchBox({
          delay: FILTER_SEARCH_DELAY,
          type: "filter",
          onChange: text => this.setState({ filterText: text }),
          placeholder: REQUEST_FILTER_TEXT,
        })
      ),
      h2({ className: "data-header", role: "heading" }, [
        span(
          {
            key: "data-label",
            className: "data-label",
          },
          requestPayloadLabel
        ),
        hasFormattedDisplay &&
          this.renderRawRequestPayloadBtn(
            "request",
            rawRequestPayloadDisplayed,
            this.toggleRawRequestPayload
          ),
      ]),
      this.renderRequestPayload(component, componentProps)
    );
  }
}

module.exports = connect(null, dispatch => ({
  updateRequest: (id, data, batch) =>
    dispatch(Actions.updateRequest(id, data, batch)),
}))(RequestPanel);
