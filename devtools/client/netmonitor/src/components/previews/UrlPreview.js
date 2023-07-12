/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
const {
  Component,
  createFactory,
} = require("resource://devtools/client/shared/vendor/react.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

const PropertiesView = createFactory(
  require("resource://devtools/client/netmonitor/src/components/request-details/PropertiesView.js")
);
const {
  L10N,
} = require("resource://devtools/client/netmonitor/src/utils/l10n.js");
const {
  parseQueryString,
} = require("resource://devtools/client/netmonitor/src/utils/request-utils.js");

const TreeRow = createFactory(
  require("resource://devtools/client/shared/components/tree/TreeRow.js")
);

loader.lazyGetter(this, "MODE", function () {
  return require("resource://devtools/client/shared/components/reps/index.js")
    .MODE;
});
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");

const { div, span, tr, td } = dom;

/**
 * Url Preview Component
 * This component is used to render urls. Its show both compact and destructured views
 * of the url. Its takes a url and the http method as properties.
 *
 * Example Url:
 * https://foo.com/bla?x=123&y=456&z=789&a=foo&a=bar
 *
 * Structure:
 * {
 *   GET : {
 *    "scheme" : "https",
 *    "host" : "foo.com",
 *    "filename" : "bla",
 *    "query" : {
 *      "x": "123",
 *      "y": "456",
 *      "z": "789",
 *      "a": {
 *         "0": foo,
 *         "1": bar
 *       }
 *     },
 *     "remote" :  {
 *        "address" : "127.0.0.1:8080"
 *     }
 *   }
 * }
 */
class UrlPreview extends Component {
  static get propTypes() {
    return {
      url: PropTypes.string,
      method: PropTypes.string,
      address: PropTypes.string,
      proxyStatus: PropTypes.string,
      shouldExpandPreview: PropTypes.bool,
      onTogglePreview: PropTypes.func,
    };
  }

  constructor(props) {
    super(props);
    this.parseUrl = this.parseUrl.bind(this);
    this.renderValue = this.renderValue.bind(this);
  }

  shouldComponentUpdate(nextProps) {
    return (
      nextProps.url !== this.props.url ||
      nextProps.method !== this.props.method ||
      nextProps.address !== this.props.address
    );
  }

  renderRow(props) {
    const {
      member: { name, level },
    } = props;
    if ((name == "query" || name == "remote") && level == 1) {
      return tr(
        { key: name, className: "treeRow stringRow" },
        td(
          { colSpan: 2, className: "splitter" },
          div({ className: "horizontal-splitter" })
        )
      );
    }

    const customProps = { ...props };
    customProps.member.selected = false;
    return TreeRow(customProps);
  }

  renderValue(props) {
    const {
      member: { level, open },
      value,
    } = props;
    if (level == 0) {
      if (open) {
        return "";
      }
      const { scheme, host, filename, query } = value;
      const queryParamNames = query ? Object.keys(query) : [];
      // render collapsed url
      return div(
        { key: "url", className: "url" },
        span({ key: "url-scheme", className: "url-scheme" }, `${scheme}://`),
        span({ key: "url-host", className: "url-host" }, `${host}`),
        span({ key: "url-filename", className: "url-filename" }, `${filename}`),
        !!queryParamNames.length &&
          span({ key: "url-ques", className: "url-chars" }, "?"),

        queryParamNames.map((name, index) => {
          if (Array.isArray(query[name])) {
            return query[name].map((item, queryIndex) => {
              return span(
                {
                  key: `url-params-${name}${queryIndex}`,
                  className: "url-params",
                },
                span(
                  {
                    key: `url-params${name}${queryIndex}-name`,
                    className: "url-params-name",
                  },
                  `${name}`
                ),
                span(
                  {
                    key: `url-chars-${name}${queryIndex}-equals`,
                    className: "url-chars",
                  },
                  "="
                ),
                span(
                  {
                    key: `url-params-${name}${queryIndex}-value`,
                    className: "url-params-value",
                  },
                  `${item}`
                ),
                (query[name].length - 1 !== queryIndex ||
                  queryParamNames.length - 1 !== index) &&
                  span({ key: "url-amp", className: "url-chars" }, "&")
              );
            });
          }

          return span(
            { key: `url-params-${name}`, className: "url-params" },
            span(
              { key: "url-params-name", className: "url-params-name" },
              `${name}`
            ),
            span({ key: "url-chars-equals", className: "url-chars" }, "="),
            span(
              { key: "url-params-value", className: "url-params-value" },
              `${query[name]}`
            ),
            queryParamNames.length - 1 !== index &&
              span({ key: "url-amp", className: "url-chars" }, "&")
          );
        })
      );
    }
    if (typeof value !== "string") {
      // the query node would be an object
      if (level == 0) {
        return "";
      }
      // for arrays (multival)
      return "[...]";
    }

    return value;
  }

  parseUrl(url) {
    const { method, address, proxyStatus } = this.props;
    const { host, protocol, pathname, search } = new URL(url);

    const urlObject = {
      [method]: {
        scheme: protocol.replace(":", ""),
        host,
        filename: pathname,
      },
    };

    const expandedNodes = new Set();

    // check and add query parameters
    if (search.length) {
      const params = parseQueryString(search);
      // make sure the query node is always expanded
      expandedNodes.add(`/${method}/query`);
      urlObject[method].query = params.reduce((map, obj) => {
        const value = map[obj.name];
        if (value || value === "") {
          if (typeof value !== "object") {
            expandedNodes.add(`/${method}/query/${obj.name}`);
            map[obj.name] = [value];
          }
          map[obj.name].push(obj.value);
        } else {
          map[obj.name] = obj.value;
        }
        return map;
      }, Object.create(null));
    }

    if (address) {
      // makes sure the remote address section is expanded
      expandedNodes.add(`/${method}/remote`);
      urlObject[method].remote = {
        [L10N.getStr(
          proxyStatus
            ? "netmonitor.headers.proxyAddress"
            : "netmonitor.headers.address"
        )]: address,
      };
    }

    return {
      urlObject,
      expandedNodes,
    };
  }

  render() {
    const {
      url,
      method,
      shouldExpandPreview = false,
      onTogglePreview,
    } = this.props;

    const { urlObject, expandedNodes } = this.parseUrl(url);

    if (shouldExpandPreview) {
      expandedNodes.add(`/${method}`);
    }

    return div(
      { className: "url-preview" },
      PropertiesView({
        object: urlObject,
        useQuotes: true,
        defaultSelectFirstNode: false,
        mode: MODE.TINY,
        expandedNodes,
        renderRow: this.renderRow,
        renderValue: this.renderValue,
        enableInput: false,
        onClickRow: (path, evt, member) => {
          // Only track when the root is toggled
          // as all the others are always expanded by
          // default.
          if (path == `/${method}`) {
            onTogglePreview(!member.open);
          }
        },
        contextMenuFormatters: {
          copyFormatter: (member, baseCopyFormatter) => {
            const { value, level, hasChildren } = member;
            if (hasChildren && level == 0) {
              const { scheme, filename, host, query } = value;
              return `${scheme}://${host}${filename}${
                query ? "?" + new URLSearchParams(query).toString() : ""
              }`;
            }
            return baseCopyFormatter(member);
          },
        },
      })
    );
  }
}

module.exports = UrlPreview;
