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
const {
  getSelectedRequestCookies,
  getSelectedResponseCookies,
} = require("../../selectors/index");

// Component
const PropertiesView = createFactory(require("./properties-view"));

const { div } = DOM;

const COOKIES_EMPTY_TEXT = L10N.getStr("cookiesEmptyText");
const COOKIES_FILTER_TEXT = L10N.getStr("cookiesFilterText");
const REQUEST_COOKIES = L10N.getStr("requestCookies");
const RESPONSE_COOKIES = L10N.getStr("responseCookies");
const SECTION_NAMES = [
  RESPONSE_COOKIES,
  REQUEST_COOKIES,
];

/*
 * Cookies panel component
 * This tab lists full details of any cookies sent with the request or response
 */
function CookiesPanel({
  request,
  response,
}) {
  if (!response.length && !request.length) {
    return div({ className: "empty-notice" },
      COOKIES_EMPTY_TEXT
    );
  }

  let object = {};
  if (response.length) {
    object[RESPONSE_COOKIES] = getProperties(response);
  }
  if (request.length) {
    object[REQUEST_COOKIES] = getProperties(request);
  }

  return (
    PropertiesView({
      object,
      filterPlaceHolder: COOKIES_FILTER_TEXT,
      sectionNames: SECTION_NAMES,
    })
  );
}

CookiesPanel.displayName = "CookiesPanel";

CookiesPanel.propTypes = {
  request: PropTypes.array.isRequired,
  response: PropTypes.array.isRequired,
};

/**
 * Mapping array to dict for TreeView usage.
 * Since TreeView only support Object(dict) format.
 *
 * @param {Object[]} arr - key-value pair array like cookies or params
 * @returns {Object}
 */
function getProperties(arr) {
  return arr.reduce((map, obj) => {
    // Generally cookies object contains only name and value properties and can
    // be rendered as name: value pair.
    // When there are more properties in cookies object such as extra or path,
    // We will pass the object to display these extra information
    if (Object.keys(obj).length > 2) {
      map[obj.name] = Object.assign({}, obj);
      delete map[obj.name].name;
    } else {
      map[obj.name] = obj.value;
    }
    return map;
  }, {});
}

module.exports = connect(
  state => ({
    request: getSelectedRequestCookies(state),
    response: getSelectedResponseCookies(state),
  })
)(CookiesPanel);
