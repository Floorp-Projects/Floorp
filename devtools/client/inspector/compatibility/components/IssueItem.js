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
      url,
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
}

module.exports = IssueItem;
