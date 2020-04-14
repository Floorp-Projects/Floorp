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
const Services = require("Services");
const {
  connect,
} = require("devtools/client/shared/redux/visibility-handler-connect");
const { L10N } = require("devtools/client/netmonitor/src/utils/l10n");
const {
  fetchNetworkUpdatePacket,
  getUrlQuery,
  parseQueryString,
  parseFormData,
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
const Accordion = createFactory(
  require("devtools/client/shared/components/Accordion")
);

loader.lazyGetter(this, "SourcePreview", function() {
  return createFactory(
    require("devtools/client/netmonitor/src/components/previews/SourcePreview")
  );
});

const { div } = dom;

const JSON_SCOPE_NAME = L10N.getStr("jsonScopeName");
const PARAMS_EMPTY_TEXT = L10N.getStr("paramsEmptyText");
const PARAMS_FILTER_TEXT = L10N.getStr("paramsFilterText");
const PARAMS_FORM_DATA = L10N.getStr("paramsFormData");
const PARAMS_POST_PAYLOAD = L10N.getStr("paramsPostPayload");
const PARAMS_QUERY_STRING = L10N.getStr("paramsQueryString");
const REQUEST_TRUNCATED = L10N.getStr("requestTruncated");

/**
 * Params panel component
 * Displays the GET parameters and POST data of a request
 */
class ParamsPanel extends Component {
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
    };
  }

  componentDidMount() {
    const { request, connector } = this.props;
    fetchNetworkUpdatePacket(connector.requestData, request, [
      "requestPostData",
    ]);
    updateFormDataSections(this.props);
  }

  componentWillReceiveProps(nextProps) {
    const { request, connector } = nextProps;
    fetchNetworkUpdatePacket(connector.requestData, request, [
      "requestPostData",
    ]);
    updateFormDataSections(nextProps);
  }

  /**
   * Update only if:
   * 1) The rendered object has changed
   * 2) The filter text has changed
   * 3) The user selected another search result target.
   */
  shouldComponentUpdate(nextProps, nextState) {
    return (
      this.props.request !== nextProps.request ||
      this.state.filterText !== nextState.filterText ||
      (this.props.targetSearchResult !== nextProps.targetSearchResult &&
        nextProps.targetSearchResult !== null)
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
   * @param {Object[]} arr - key-value pair array like query or form params
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

  parseJSON(postData) {
    try {
      return JSON.parse(postData);
    } catch (err) {
      // Continue regardless of parsing error
    }
    return null;
  }

  render() {
    const { request, targetSearchResult } = this.props;
    const { filterText } = this.state;
    const { formDataSections, mimeType, requestPostData, url } = request;
    const postData = requestPostData ? requestPostData.postData.text : null;
    const query = getUrlQuery(url);

    if (
      (!formDataSections || formDataSections.length === 0) &&
      !postData &&
      !query
    ) {
      return div({ className: "empty-notice" }, PARAMS_EMPTY_TEXT);
    }

    let error;
    const items = [];

    // Query String section
    if (query) {
      items.push({
        component: PropertiesView,
        componentProps: {
          object: this.getProperties(parseQueryString(query)),
          filterText,
          targetSearchResult,
        },
        header: PARAMS_QUERY_STRING,
        id: "paramsQueryString",
        opened: true,
      });
    }

    // Form Data section
    if (formDataSections && formDataSections.length > 0) {
      const sections = formDataSections.filter(str => /\S/.test(str)).join("&");
      items.push({
        component: PropertiesView,
        componentProps: {
          object: this.getProperties(parseFormData(sections)),
          filterText,
          targetSearchResult,
          defaultSelectFirstNode: false,
        },
        header: PARAMS_FORM_DATA,
        id: "paramsFormData",
        opened: true,
      });
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
        const json = this.parseJSON(postData);

        if (json) {
          items.push({
            component: PropertiesView,
            componentProps: {
              object: sortObjectKeys(json),
              filterText,
              targetSearchResult,
              defaultSelectFirstNode: false,
            },
            header: JSON_SCOPE_NAME,
            id: "jsonScopeName",
            opened: true,
          });
        }
      }
    }

    if (postData) {
      items.push({
        component: SourcePreview,
        componentProps: {
          text: postData,
          mode: mimeType.replace(/;.+/, ""),
          targetSearchResult,
        },
        header: PARAMS_POST_PAYLOAD,
        id: "paramsPostPayload",
        opened: true,
      });
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
          placeholder: PARAMS_FILTER_TEXT,
        })
      ),
      Accordion({ items })
    );
  }
}

module.exports = connect(null, dispatch => ({
  updateRequest: (id, data, batch) =>
    dispatch(Actions.updateRequest(id, data, batch)),
}))(ParamsPanel);
