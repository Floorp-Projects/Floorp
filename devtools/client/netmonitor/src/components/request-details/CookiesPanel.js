/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { L10N } = require("devtools/client/netmonitor/src/utils/l10n");
const {
  fetchNetworkUpdatePacket,
} = require("devtools/client/netmonitor/src/utils/request-utils");
const {
  sortObjectKeys,
} = require("devtools/client/netmonitor/src/utils/sort-utils");
const {
  FILTER_SEARCH_DELAY,
} = require("devtools/client/netmonitor/src/constants");

// Component
const PropertiesView = createFactory(
  require("devtools/client/netmonitor/src/components/request-details/PropertiesView")
);
const SearchBox = createFactory(
  require("devtools/client/shared/components/SearchBox")
);
const Accordion = createFactory(
  require("devtools/client/shared/components/Accordion")
);

const { div } = dom;

const COOKIES_EMPTY_TEXT = L10N.getStr("cookiesEmptyText");
const COOKIES_FILTER_TEXT = L10N.getStr("cookiesFilterText");
const REQUEST_COOKIES = L10N.getStr("requestCookies");
const RESPONSE_COOKIES = L10N.getStr("responseCookies");

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

  render() {
    let {
      request: {
        requestCookies = { cookies: [] },
        responseCookies = { cookies: [] },
      },
      targetSearchResult,
    } = this.props;

    const { filterText } = this.state;

    requestCookies = requestCookies.cookies || requestCookies;
    responseCookies = responseCookies.cookies || responseCookies;

    if (!requestCookies.length && !responseCookies.length) {
      return div({ className: "empty-notice" }, COOKIES_EMPTY_TEXT);
    }

    const items = [];

    if (responseCookies.length) {
      items.push({
        component: PropertiesView,
        componentProps: {
          object: sortObjectKeys(this.getProperties(responseCookies)),
          filterText,
          targetSearchResult,
          defaultSelectFirstNode: false,
        },
        header: RESPONSE_COOKIES,
        id: "responseCookies",
        opened: true,
      });
    }

    if (requestCookies.length) {
      items.push({
        component: PropertiesView,
        componentProps: {
          object: sortObjectKeys(this.getProperties(requestCookies)),
          filterText,
          targetSearchResult,
          defaultSelectFirstNode: false,
        },
        header: REQUEST_COOKIES,
        id: "requestCookies",
        opened: true,
      });
    }

    return div(
      { className: "panel-container" },
      div(
        { className: "devtools-toolbar devtools-input-toolbar" },
        SearchBox({
          delay: FILTER_SEARCH_DELAY,
          type: "filter",
          onChange: text => this.setState({ filterText: text }),
          placeholder: COOKIES_FILTER_TEXT,
        })
      ),
      Accordion({ items })
    );
  }
}

module.exports = CookiesPanel;
