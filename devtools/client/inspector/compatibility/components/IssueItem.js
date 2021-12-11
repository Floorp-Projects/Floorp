/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const {
  createFactory,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

loader.lazyRequireGetter(
  this,
  "openDocLink",
  "devtools/client/shared/link",
  true
);

const UnsupportedBrowserList = createFactory(
  require("devtools/client/inspector/compatibility/components/UnsupportedBrowserList")
);

const Types = require("devtools/client/inspector/compatibility/types");

const NodePane = createFactory(
  require("devtools/client/inspector/compatibility/components/NodePane")
);

// For test
loader.lazyRequireGetter(
  this,
  "toSnakeCase",
  "devtools/client/inspector/compatibility/utils/cases",
  true
);

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
    const { url } = this.props;

    e.preventDefault();
    e.stopPropagation();

    openDocLink(
      url +
        "?utm_source=devtools&utm_medium=inspector-compatibility&utm_campaign=default"
    );
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

  _renderDescription() {
    const { property, url } = this.props;

    return dom.div(
      {
        className: "compatibility-issue-item__description",
      },
      dom.a(
        {
          className: "compatibility-issue-item__mdn-link devtools-monospace",
          href: url,
          title: url,
          onClick: e => this._onLinkClicked(e),
        },
        property
      ),
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
    const {
      deprecated,
      experimental,
      property,
      unsupportedBrowsers,
    } = this.props;

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
