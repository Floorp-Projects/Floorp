/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const {
  connect,
} = require("devtools/client/shared/redux/visibility-handler-connect");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { L10N } = require("../utils/l10n");
const { fetchNetworkUpdatePacket } = require("../utils/request-utils");
const { sortObjectKeys } = require("../utils/sort-utils");
const {
  setTargetSearchResult,
} = require("devtools/client/netmonitor/src/actions/search");

// Component
const PropertiesView = createFactory(require("./PropertiesView"));

const { div } = dom;

const COOKIES_EMPTY_TEXT = L10N.getStr("cookiesEmptyText");
const COOKIES_FILTER_TEXT = L10N.getStr("cookiesFilterText");
const REQUEST_COOKIES = L10N.getStr("requestCookies");
const RESPONSE_COOKIES = L10N.getStr("responseCookies");
const SECTION_NAMES = [RESPONSE_COOKIES, REQUEST_COOKIES];

/*
 * Cookies panel component
 * This tab lists full details of any cookies sent with the request or response
 */
class CookiesPanel extends Component {
  static get propTypes() {
    return {
      connector: PropTypes.object.isRequired,
      openLink: PropTypes.func,
      request: PropTypes.object.isRequired,
      resetTargetSearchResult: PropTypes.func,
      targetSearchResult: PropTypes.object,
    };
  }

  componentDidMount() {
    const { connector, request } = this.props;
    fetchNetworkUpdatePacket(connector.requestData, request, [
      "requestCookies",
      "responseCookies",
    ]);
  }

  componentWillReceiveProps(nextProps) {
    const { connector, request } = nextProps;
    fetchNetworkUpdatePacket(connector.requestData, request, [
      "requestCookies",
      "responseCookies",
    ]);
  }

  /**
   * Mapping array to dict for TreeView usage.
   * Since TreeView only support Object(dict) format.
   *
   * @param {Object[]} arr - key-value pair array like cookies or params
   * @returns {Object}
   */
  getProperties(arr) {
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

  /**
   * If target cookie (coming from the Search panel) is set, let's
   * scroll the content so the cookie is visible. This is used for
   * search result navigation, which happens when the user clicks
   * on a search result coming from a cookie.
   */
  scrollToCookie() {
    const { targetSearchResult, resetTargetSearchResult } = this.props;
    if (!targetSearchResult) {
      return;
    }

    const path = this.getTargetCookiePath(targetSearchResult);
    const element = document.getElementById(path);
    if (element) {
      element.scrollIntoView({ block: "center" });
    }

    resetTargetSearchResult();
  }

  /**
   * Returns unique cookie path that identifies cookie location
   * within the Tree component. The path is calculated from
   * labels used to render the tree items.
   * This path is used to highlight the cookie and ensure
   * that it's properly scrolled within the visible view-port.
   */
  getTargetCookiePath(searchResult) {
    if (!searchResult) {
      return null;
    }

    if (
      searchResult.type !== "requestCookies" &&
      searchResult.type !== "responseCookies"
    ) {
      return null;
    }

    return (
      "/" +
      (searchResult.type == "requestCookies"
        ? REQUEST_COOKIES
        : RESPONSE_COOKIES) +
      "/" +
      searchResult.label
    );
  }

  render() {
    let {
      request: {
        requestCookies = { cookies: [] },
        responseCookies = { cookies: [] },
      },
      openLink,
      targetSearchResult,
    } = this.props;

    requestCookies = requestCookies.cookies || requestCookies;
    responseCookies = responseCookies.cookies || responseCookies;

    if (!requestCookies.length && !responseCookies.length) {
      return div({ className: "empty-notice" }, COOKIES_EMPTY_TEXT);
    }

    const object = {};

    if (responseCookies.length) {
      object[RESPONSE_COOKIES] = sortObjectKeys(
        this.getProperties(responseCookies)
      );
    }

    if (requestCookies.length) {
      object[REQUEST_COOKIES] = sortObjectKeys(
        this.getProperties(requestCookies)
      );
    }

    return div(
      { className: "panel-container" },
      PropertiesView({
        object,
        ref: () => this.scrollToCookie(),
        selected: this.getTargetCookiePath(targetSearchResult),
        filterPlaceHolder: COOKIES_FILTER_TEXT,
        sectionNames: SECTION_NAMES,
        openLink,
        targetSearchResult,
      })
    );
  }
}

module.exports = connect(
  null,
  dispatch => ({
    resetTargetSearchResult: () => dispatch(setTargetSearchResult(null)),
  })
)(CookiesPanel);
