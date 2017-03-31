/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  DOM,
  PropTypes,
} = require("devtools/client/shared/vendor/react");
const { L10N } = require("../utils/l10n");
const { getUrlQuery, parseQueryString, parseFormData } = require("../utils/request-utils");

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
function ParamsPanel({ request }) {
  let {
    formDataSections,
    mimeType,
    requestPostData,
    url,
  } = request;
  let postData = requestPostData ? requestPostData.postData.text : null;
  let query = getUrlQuery(url);

  if (!formDataSections && !postData && !query) {
    return div({ className: "empty-notice" },
      PARAMS_EMPTY_TEXT
    );
  }

  let object = {};
  let json;

  // Query String section
  if (query) {
    object[PARAMS_QUERY_STRING] = getProperties(parseQueryString(query));
  }

  // Form Data section
  if (formDataSections && formDataSections.length > 0) {
    let sections = formDataSections.filter((str) => /\S/.test(str)).join("&");
    object[PARAMS_FORM_DATA] = getProperties(parseFormData(sections));
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
    div({ className: "panel-container" },
      PropertiesView({
        object,
        filterPlaceHolder: PARAMS_FILTER_TEXT,
        sectionNames: SECTION_NAMES,
      })
    )
  );
}

ParamsPanel.displayName = "ParamsPanel";

ParamsPanel.propTypes = {
  request: PropTypes.object.isRequired,
};

/**
 * Mapping array to dict for TreeView usage.
 * Since TreeView only support Object(dict) format.
 * This function also deal with duplicate key case
 * (for multiple selection and query params with same keys)
 *
 * @param {Object[]} arr - key-value pair array like query or form params
 * @returns {Object} Rep compatible object
 */
function getProperties(arr) {
  return arr.reduce((map, obj) => {
    let value = map[obj.name];
    if (value) {
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

module.exports = ParamsPanel;
