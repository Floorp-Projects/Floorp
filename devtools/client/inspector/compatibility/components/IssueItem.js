/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

const FluentReact = require("resource://devtools/client/shared/vendor/fluent-react.js");
const Localized = createFactory(FluentReact.Localized);

loader.lazyRequireGetter(
  this,
  "openDocLink",
  "resource://devtools/client/shared/link.js",
  true
);

const UnsupportedBrowserList = createFactory(
  require("resource://devtools/client/inspector/compatibility/components/UnsupportedBrowserList.js")
);

const Types = require("resource://devtools/client/inspector/compatibility/types.js");

const NodePane = createFactory(
  require("resource://devtools/client/inspector/compatibility/components/NodePane.js")
);

// For test
loader.lazyRequireGetter(
  this,
  "toSnakeCase",
  "resource://devtools/client/inspector/compatibility/utils/cases.js",
  true
);

const MDN_LINK_PARAMS = new URLSearchParams({
  utm_source: "devtools",
  utm_medium: "inspector-compatibility",
  utm_campaign: "default",
});

class IssueItem extends PureComponent {
  static get propTypes() {
    return {
      ...Types.issue,
      dispatch: PropTypes.func.isRequired,
      setSelectedNode: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);
    this._onLinkClicked = this._onLinkClicked.bind(this);
  }

  _onLinkClicked(e) {
    e.preventDefault();
    e.stopPropagation();

    const isMacOS = Services.appinfo.OS === "Darwin";

    openDocLink(e.target.href, {
      relatedToCurrent: true,
      inBackground: isMacOS ? e.metaKey : e.ctrlKey,
    });
  }

  _getTestDataAttributes() {
    const testDataSet = {};

    if (Services.prefs.getBoolPref("devtools.testing", false)) {
      for (const [key, value] of Object.entries(this.props)) {
        if (key === "nodes") {
          continue;
        }
        const datasetKey = `data-qa-${toSnakeCase(key)}`;
        testDataSet[datasetKey] = JSON.stringify(value);
      }
    }

    return testDataSet;
  }

  _renderAliases() {
    const { property } = this.props;
    let { aliases } = this.props;

    if (!aliases) {
      return null;
    }

    aliases = aliases.filter(alias => alias !== property);

    if (aliases.length === 0) {
      return null;
    }

    return dom.ul(
      {
        className: "compatibility-issue-item__aliases",
      },
      aliases.map(alias =>
        dom.li(
          {
            key: alias,
            className: "compatibility-issue-item__alias",
          },
          alias
        )
      )
    );
  }

  _renderCauses() {
    const { deprecated, experimental, prefixNeeded } = this.props;

    if (!deprecated && !experimental && !prefixNeeded) {
      return null;
    }

    let localizationId = "";

    if (deprecated && experimental && prefixNeeded) {
      localizationId =
        "compatibility-issue-deprecated-experimental-prefixneeded";
    } else if (deprecated && experimental) {
      localizationId = "compatibility-issue-deprecated-experimental";
    } else if (deprecated && prefixNeeded) {
      localizationId = "compatibility-issue-deprecated-prefixneeded";
    } else if (experimental && prefixNeeded) {
      localizationId = "compatibility-issue-experimental-prefixneeded";
    } else if (deprecated) {
      localizationId = "compatibility-issue-deprecated";
    } else if (experimental) {
      localizationId = "compatibility-issue-experimental";
    } else if (prefixNeeded) {
      localizationId = "compatibility-issue-prefixneeded";
    }

    return Localized(
      {
        id: localizationId,
      },
      dom.span(
        { className: "compatibility-issue-item__causes" },
        localizationId
      )
    );
  }

  _renderPropertyEl() {
    const { property, url, specUrl } = this.props;
    const baseCls = "compatibility-issue-item__property devtools-monospace";
    if (!url && !specUrl) {
      return dom.span({ className: baseCls }, property);
    }

    const href = url ? `${url}?${MDN_LINK_PARAMS}` : specUrl;

    return dom.a(
      {
        className: `${baseCls} ${
          url
            ? "compatibility-issue-item__mdn-link"
            : "compatibility-issue-item__spec-link"
        }`,
        href,
        title: href,
        onClick: e => this._onLinkClicked(e),
      },
      property
    );
  }

  _renderDescription() {
    return dom.div(
      {
        className: "compatibility-issue-item__description",
      },
      this._renderPropertyEl(),
      this._renderCauses(),
      this._renderUnsupportedBrowserList()
    );
  }

  _renderNodeList() {
    const { dispatch, nodes, setSelectedNode } = this.props;

    if (!nodes) {
      return null;
    }

    return NodePane({
      dispatch,
      nodes,
      setSelectedNode,
    });
  }

  _renderUnsupportedBrowserList() {
    const { unsupportedBrowsers } = this.props;

    return unsupportedBrowsers.length
      ? UnsupportedBrowserList({ browsers: unsupportedBrowsers })
      : null;
  }

  render() {
    const { deprecated, experimental, property, unsupportedBrowsers } =
      this.props;

    const classes = ["compatibility-issue-item"];

    if (deprecated) {
      classes.push("compatibility-issue-item--deprecated");
    }

    if (experimental) {
      classes.push("compatibility-issue-item--experimental");
    }

    if (unsupportedBrowsers.length) {
      classes.push("compatibility-issue-item--unsupported");
    }

    return dom.li(
      {
        className: classes.join(" "),
        key: property,
        ...this._getTestDataAttributes(),
      },
      this._renderDescription(),
      this._renderAliases(),
      this._renderNodeList()
    );
  }
}

module.exports = IssueItem;
