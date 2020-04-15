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
      hideBoxModelHighlighter: PropTypes.func.isRequired,
      setSelectedNode: PropTypes.func.isRequired,
      showBoxModelHighlighterForNode: PropTypes.func.isRequired,
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
    openDocLink(url);
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

  _renderCauses() {
    const { deprecated, experimental } = this.props;

    const causes = [];

    if (deprecated) {
      causes.push("deprecated");
    }

    if (experimental) {
      causes.push("experimental");
    }

    return causes.length
      ? dom.span(
          { className: "compatibility-issue-item__causes" },
          `(${causes.join(",")})`
        )
      : null;
  }

  _renderDescription() {
    const {
      deprecated,
      experimental,
      property,
      unsupportedBrowsers,
      url,
    } = this.props;

    const classes = ["compatibility-issue-item__description"];

    if (deprecated) {
      classes.push("compatibility-issue-item__description--deprecated");
    }

    if (experimental) {
      classes.push("compatibility-issue-item__description--experimental");
    }

    if (unsupportedBrowsers.length) {
      classes.push("compatibility-issue-item__description--unsupported");
    }

    return dom.div(
      {
        className: classes.join(" "),
      },
      dom.a(
        {
          className: "compatibility-issue-item__mdn-link devtools-monospace",
          href:
            url +
            "?utm_source=devtools&utm_medium=inspector-compatibility&utm_campaign=default",
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
    const { nodes } = this.props;

    if (!nodes) {
      return null;
    }

    const {
      hideBoxModelHighlighter,
      setSelectedNode,
      showBoxModelHighlighterForNode,
    } = this.props;

    return NodePane({
      nodes,
      hideBoxModelHighlighter,
      setSelectedNode,
      showBoxModelHighlighterForNode,
    });
  }

  _renderUnsupportedBrowserList() {
    const { unsupportedBrowsers } = this.props;

    return unsupportedBrowsers.length
      ? UnsupportedBrowserList({ browsers: unsupportedBrowsers })
      : null;
  }

  render() {
    const { property } = this.props;

    return dom.li(
      {
        className: "compatibility-issue-item",
        key: property,
        ...this._getTestDataAttributes(),
      },
      this._renderDescription(),
      this._renderNodeList()
    );
  }
}

module.exports = IssueItem;
