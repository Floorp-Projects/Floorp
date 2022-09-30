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

const UnsupportedBrowserItem = createFactory(
  require("resource://devtools/client/inspector/compatibility/components/UnsupportedBrowserItem.js")
);

const Types = require("resource://devtools/client/inspector/compatibility/types.js");
const FluentReact = require("resource://devtools/client/shared/vendor/fluent-react.js");
const Localized = createFactory(FluentReact.Localized);

class UnsupportedBrowserList extends PureComponent {
  static get propTypes() {
    return {
      browsers: PropTypes.arrayOf(PropTypes.shape(Types.browser)).isRequired,
    };
  }

  render() {
    const { browsers } = this.props;

    const unsupportedBrowserItems = {};
    const browsersList = [];

    for (const { id, name, version, status } of browsers) {
      // Only display one icon per browser
      if (!unsupportedBrowserItems[id]) {
        unsupportedBrowserItems[id] = UnsupportedBrowserItem({
          key: id,
          id,
          name,
        });
      }
      browsersList.push(`${name} ${version}${status ? ` (${status})` : ""}`);
    }
    return Localized(
      {
        id: "compatibility-issue-browsers-list",
        $browsers: browsersList.join("\n"),
        attrs: { title: true },
      },
      dom.ul(
        {
          className: "compatibility-unsupported-browser-list",
        },
        Object.values(unsupportedBrowserItems)
      )
    );
  }
}

module.exports = UnsupportedBrowserList;
