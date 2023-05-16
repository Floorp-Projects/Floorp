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

const BrowserIcon = createFactory(
  require("resource://devtools/client/inspector/compatibility/components/BrowserIcon.js")
);
const FluentReact = require("resource://devtools/client/shared/vendor/fluent-react.js");
const Localized = createFactory(FluentReact.Localized);

const Types = require("resource://devtools/client/inspector/compatibility/types.js");

class UnsupportedBrowserItem extends PureComponent {
  static get propTypes() {
    return {
      id: Types.browser.id,
      name: Types.browser.name,
      unsupportedVersions: PropTypes.array.isRequired,
      version: Types.browser.version,
    };
  }

  render() {
    const { unsupportedVersions, id, name, version } = this.props;

    return Localized(
      {
        id: "compatibility-issue-browsers-list",
        $browsers: unsupportedVersions
          .map(
            ({ version: v, status }) =>
              `${name} ${v}${status ? ` (${status})` : ""}`
          )
          .join("\n"),
        attrs: { title: true },
      },
      dom.li(
        { className: "compatibility-browser", "data-browser-id": id },
        BrowserIcon({ id, name }),
        dom.span(
          {
            className: "compatibility-browser-version",
          },
          version
        )
      )
    );
  }
}

module.exports = UnsupportedBrowserItem;
