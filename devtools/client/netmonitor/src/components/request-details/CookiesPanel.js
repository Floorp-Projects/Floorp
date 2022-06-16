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

loader.lazyGetter(this, "TreeRow", function() {
  return createFactory(
    require("devtools/client/shared/components/tree/TreeRow")
  );
});

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

  // FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=1774507
  UNSAFE_componentWillReceiveProps(nextProps) {
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
  getProperties(arr, title) {
    const cookies = arr.reduce((map, obj) => {
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

    // To have different roots for Request and Response cookies
    return { [title]: cookies };
  }

  /**
   * Custom rendering method passed to PropertiesView. It's
   * responsible to filter out level 0 node in the tree
   *
   * @param {Object} props
   */
  renderRow(props) {
    const { level } = props.member;

    if (level === 0) {
      return null;
    }

    return TreeRow(props);
  }

  /**
   * Get the selected cookies path
   * @param {Object} searchResult
   * @returns {string}
   */
  getTargetCookiePath(searchResult) {
    if (!searchResult) {
      return null;
    }

    switch (searchResult.type) {
      case "requestCookies": {
        return `/${REQUEST_COOKIES}/${searchResult.label}`;
      }
      case "responseCookies":
        return `/${RESPONSE_COOKIES}/${searchResult.label}`;
    }

    return null;
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
          object: sortObjectKeys(
            this.getProperties(responseCookies, RESPONSE_COOKIES)
          ),
          filterText,
          targetSearchResult,
          defaultSelectFirstNode: false,
          selectPath: this.getTargetCookiePath,
          renderRow: this.renderRow,
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
          object: sortObjectKeys(
            this.getProperties(requestCookies, REQUEST_COOKIES)
          ),
          filterText,
          targetSearchResult,
          defaultSelectFirstNode: false,
          selectPath: this.getTargetCookiePath,
          renderRow: this.renderRow,
        },
        header: REQUEST_COOKIES,
        id: "requestCookies",
        opened: true,
      });
    }

    return div(
      { className: "panel-container cookies-panel-container" },
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
