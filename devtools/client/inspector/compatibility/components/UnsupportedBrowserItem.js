/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const BrowserIcon = createFactory(
  require("devtools/client/inspector/compatibility/components/BrowserIcon")
);

const Types = require("devtools/client/inspector/compatibility/types");

class UnsupportedBrowserItem extends PureComponent {
  static get propTypes() {
    return {
      id: Types.browser.id,
      name: Types.browser.name,
      versions: PropTypes.arrayOf(
        PropTypes.shape({
          alias: Types.browser.status,
          version: Types.browser.version,
        })
      ),
    };
  }

  render() {
    const { id, name, versions } = this.props;

    const versionsTitle = versions
      .map(({ alias, version }) => version + (alias ? `(${alias})` : ""))
      .join(", ");
    const title = `${name} ${versionsTitle}`;

    return dom.li({}, BrowserIcon({ id, name, title }));
  }
}

module.exports = UnsupportedBrowserItem;
