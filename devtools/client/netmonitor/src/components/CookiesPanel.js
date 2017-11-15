/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { L10N } = require("../utils/l10n");
const { sortObjectKeys } = require("../utils/sort-utils");

// Component
const PropertiesView = createFactory(require("./PropertiesView"));

const { div } = dom;

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
  openLink,
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
    object[RESPONSE_COOKIES] = sortObjectKeys(getProperties(responseCookies));
  }

  if (requestCookies.length) {
    object[REQUEST_COOKIES] = sortObjectKeys(getProperties(requestCookies));
  }

  return (
    div({ className: "panel-container" },
      PropertiesView({
        object,
        filterPlaceHolder: COOKIES_FILTER_TEXT,
        sectionNames: SECTION_NAMES,
        openLink,
      })
    )
  );
}

CookiesPanel.displayName = "CookiesPanel";

CookiesPanel.propTypes = {
  request: PropTypes.object.isRequired,
  openLink: PropTypes.func,
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
