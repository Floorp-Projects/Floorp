/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  DOM,
  PropTypes,
} = require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const { L10N } = require("../../l10n");
const { getSelectedRequest } = require("../../selectors/index");
const { getUrlQuery, parseQueryString } = require("../../request-utils");

// Components
const PropertiesView = createFactory(require("./properties-view"));

const { div } = DOM;

const JSON_SCOPE_NAME = L10N.getStr("jsonScopeName");
const PARAMS_EMPTY_TEXT = L10N.getStr("paramsEmptyText");
const PARAMS_FILTER_TEXT = L10N.getStr("paramsFilterText");
const PARAMS_FORM_DATA = L10N.getStr("paramsFormData");
const PARAMS_POST_PAYLOAD = L10N.getStr("paramsPostPayload");
const PARAMS_QUERY_STRING = L10N.getStr("paramsQueryString");
const SECTION_NAMES = [
  JSON_SCOPE_NAME,
  PARAMS_FORM_DATA,
  PARAMS_POST_PAYLOAD,
  PARAMS_QUERY_STRING,
];

/*
 * Params panel component
 * Displays the GET parameters and POST data of a request
 */
function ParamsPanel({
  formDataSections,
  mimeType,
  postData,
  query,
}) {
  if (!formDataSections && !postData && !query) {
    return div({ className: "empty-notice" },
      PARAMS_EMPTY_TEXT
    );
  }

  let object = {};
  let json;

  // Query String section
  if (query) {
    object[PARAMS_QUERY_STRING] =
      parseQueryString(query)
        .reduce((acc, { name, value }) =>
          name ? Object.assign(acc, { [name]: value }) : acc
        , {});
  }
  // Form Data section
  if (formDataSections && formDataSections.length > 0) {
    let sections = formDataSections.filter((str) => /\S/.test(str)).join("&");
    object[PARAMS_FORM_DATA] =
      parseQueryString(sections)
        .reduce((acc, { name, value }) =>
          name ? Object.assign(acc, { [name]: value }) : acc
        , {});
  }

  // Request payload section
  if (formDataSections && formDataSections.length === 0 && postData) {
    try {
      json = JSON.parse(postData);
    } catch (error) {
      // Continue regardless of parsing error
    }

    if (json) {
      object[JSON_SCOPE_NAME] = json;
    } else {
      object[PARAMS_POST_PAYLOAD] = {
        EDITOR_CONFIG: {
          text: postData,
          mode: mimeType.replace(/;.+/, ""),
        },
      };
    }
  } else {
    postData = "";
  }

  return (
    PropertiesView({
      object,
      filterPlaceHolder: PARAMS_FILTER_TEXT,
      sectionNames: SECTION_NAMES,
    })
  );
}

ParamsPanel.displayName = "ParamsPanel";

ParamsPanel.propTypes = {
  formDataSections: PropTypes.array,
  postData: PropTypes.string,
  query: PropTypes.string,
};

module.exports = connect(
  (state) => {
    const selectedRequest = getSelectedRequest(state);

    if (selectedRequest) {
      const { formDataSections, mimeType, requestPostData, url } = selectedRequest;

      return {
        formDataSections,
        mimeType,
        postData: requestPostData ? requestPostData.postData.text : null,
        query: getUrlQuery(url),
      };
    }

    return {};
  }
)(ParamsPanel);
