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

class UnsupportedBrowserList extends PureComponent {
  static get propTypes() {
    return {
      browsers: PropTypes.arrayOf(PropTypes.shape(Types.browser)).isRequired,
    };
  }

  render() {
    const { browsers } = this.props;

    const unsupportedBrowserItems = {};

    const unsupportedVersionsListByBrowser = new Map();

    for (const { name, version, status } of browsers) {
      if (!unsupportedVersionsListByBrowser.has(name)) {
        unsupportedVersionsListByBrowser.set(name, []);
      }
      unsupportedVersionsListByBrowser.get(name).push({ version, status });
    }

    for (const { id, name, version, status } of browsers) {
      // Only display one icon per browser
      if (!unsupportedBrowserItems[id]) {
        if (status === "esr") {
          // The data is ordered by version number, so we'll show the first unsupported
          // browser version. This might be confusing for Firefox as we'll show ESR
          // version first, and so the user wouldn't be able to tell if there's an issue
          // only on ESR, or also on release.
          // So only show ESR if there's no newer unsupported version
          const newerVersionIsUnsupported = browsers.find(
            browser => browser.id == id && browser.status !== status
          );
          if (newerVersionIsUnsupported) {
            continue;
          }
        }

        unsupportedBrowserItems[id] = UnsupportedBrowserItem({
          key: id,
          id,
          name,
          version,
          unsupportedVersions: unsupportedVersionsListByBrowser.get(name),
        });
      }
    }
    return dom.ul(
      {
        className: "compatibility-unsupported-browser-list",
      },
      Object.values(unsupportedBrowserItems)
    );
  }
}

module.exports = UnsupportedBrowserList;
