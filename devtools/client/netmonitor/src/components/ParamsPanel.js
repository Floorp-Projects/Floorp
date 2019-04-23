/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const Services = require("Services");
const { connect } = require("devtools/client/shared/redux/visibility-handler-connect");
const { L10N } = require("../utils/l10n");
const {
  fetchNetworkUpdatePacket,
  getUrlQuery,
  parseQueryString,
  parseFormData,
} = require("../utils/request-utils");
const { sortObjectKeys } = require("../utils/sort-utils");
const { updateFormDataSections } = require("../utils/request-utils");
const Actions = require("../actions/index");

// Components
const PropertiesView = createFactory(require("./PropertiesView"));

const { div } = dom;

const JSON_SCOPE_NAME = L10N.getStr("jsonScopeName");
const PARAMS_EMPTY_TEXT = L10N.getStr("paramsEmptyText");
const PARAMS_FILTER_TEXT = L10N.getStr("paramsFilterText");
const PARAMS_FORM_DATA = L10N.getStr("paramsFormData");
const PARAMS_POST_PAYLOAD = L10N.getStr("paramsPostPayload");
const PARAMS_QUERY_STRING = L10N.getStr("paramsQueryString");
const REQUEST_TRUNCATED = L10N.getStr("requestTruncated");
const SECTION_NAMES = [
  JSON_SCOPE_NAME,
  PARAMS_FORM_DATA,
  PARAMS_POST_PAYLOAD,
  PARAMS_QUERY_STRING,
];

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
    };
  }

  constructor(props) {
    super(props);
  }

  componentDidMount() {
    const { request, connector } = this.props;
    fetchNetworkUpdatePacket(connector.requestData, request, ["requestPostData"]);
    updateFormDataSections(this.props);
  }

  componentWillReceiveProps(nextProps) {
    const { request, connector } = nextProps;
    fetchNetworkUpdatePacket(connector.requestData, request, ["requestPostData"]);
    updateFormDataSections(nextProps);
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

  render() {
    const {
      openLink,
      request,
    } = this.props;
    const {
      formDataSections,
      mimeType,
      requestPostData,
      url,
    } = request;
    let postData = requestPostData ? requestPostData.postData.text : null;
    const query = getUrlQuery(url);

    if ((!formDataSections || formDataSections.length === 0) && !postData && !query) {
      return div({ className: "empty-notice" },
        PARAMS_EMPTY_TEXT
      );
    }

    const object = {};
    let json, error;

    // Query String section
    if (query) {
      object[PARAMS_QUERY_STRING] = this.getProperties(parseQueryString(query));
    }

    // Form Data section
    if (formDataSections && formDataSections.length > 0) {
      const sections = formDataSections.filter((str) => /\S/.test(str)).join("&");
      object[PARAMS_FORM_DATA] = this.getProperties(parseFormData(sections));
    }

    // Request payload section

    const limit = Services.prefs.getIntPref("devtools.netmonitor.requestBodyLimit");
    // Check if the request post data has been truncated, in which case no parse should
    // be attempted.
    if (postData && limit <= postData.length) {
      error = REQUEST_TRUNCATED;
    }

    if (formDataSections && formDataSections.length === 0 && postData) {
      if (!error) {
        try {
          json = JSON.parse(postData);
        } catch (err) {
          // Continue regardless of parsing error
        }

        if (json) {
          object[JSON_SCOPE_NAME] = sortObjectKeys(json);
        }
      }

      object[PARAMS_POST_PAYLOAD] = {
        EDITOR_CONFIG: {
          text: postData,
          mode: mimeType.replace(/;.+/, ""),
        },
      };
    } else {
      postData = "";
    }

    return (
      div({ className: "panel-container" },
        error && div({ className: "request-error-header", title: error },
          error
        ),
        PropertiesView({
          object,
          filterPlaceHolder: PARAMS_FILTER_TEXT,
          sectionNames: SECTION_NAMES,
          openLink,
        })
      )
    );
  }
}

module.exports = connect(null,
  (dispatch) => ({
    updateRequest: (id, data, batch) => dispatch(Actions.updateRequest(id, data, batch)),
  }),
)(ParamsPanel);
