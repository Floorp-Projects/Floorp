/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  DOM,
  PropTypes,
} = require("devtools/client/shared/vendor/react");
const { L10N } = require("../../l10n");

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
}) {
  let {
    requestCookies = { cookies: [] },
    responseCookies = { cookies: [] },
  } = request;

  requestCookies = requestCookies.cookies || requestCookies;
  responseCookies = responseCookies.cookies || responseCookies;

  if (!requestCookies.length && !responseCookies.length) {
    return div({ className: "empty-notice" },
      COOKIES_EMPTY_TEXT
    );
  }

  let object = {};

  if (responseCookies.length) {
    object[RESPONSE_COOKIES] = getProperties(responseCookies);
  }

  if (requestCookies.length) {
    object[REQUEST_COOKIES] = getProperties(requestCookies);
  }

  return (
    div({ className: "panel-container" },
      PropertiesView({
        object,
        filterPlaceHolder: COOKIES_FILTER_TEXT,
        sectionNames: SECTION_NAMES,
      })
    )
  );
}

CookiesPanel.displayName = "CookiesPanel";

CookiesPanel.propTypes = {
  request: PropTypes.object.isRequired,
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

module.exports = CookiesPanel;
