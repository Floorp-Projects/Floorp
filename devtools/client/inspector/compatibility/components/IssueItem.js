/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

loader.lazyRequireGetter(
  this,
  "openDocLink",
  "devtools/client/shared/link",
  true
);

const Types = require("../types");

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

  render() {
    const { property, url } = this.props;

    return dom.li(
      {
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
      )
    );
  }
}

module.exports = IssueItem;
