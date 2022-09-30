/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const {
  div,
} = require("resource://devtools/client/shared/vendor/react-dom-factories.js");

const Types = require("resource://devtools/client/application/src/types/index.js");
const ManifestItem = createFactory(
  require("resource://devtools/client/application/src/components/manifest/ManifestItem.js")
);

/**
 * This component displays a Manifest member which holds a color value
 */
class ManifestColorItem extends PureComponent {
  static get propTypes() {
    return {
      ...Types.manifestItemColor, // { label, value }
    };
  }

  renderColor() {
    let { value } = this.props;
    if (!value) {
      return null;
    }

    // Truncate colors in #rrggbbaa format to #rrggbb
    if (value.length === 9 && value.toLowerCase().endsWith("ff")) {
      value = value.slice(0, 7);
    }

    /* div instead of span because CSS `direction` works with block elements */
    return div(
      {
        className: "manifest-item__color",
        style: { "--color-value": value },
      },
      value
    );
  }

  render() {
    const { label } = this.props;
    return ManifestItem({ label }, this.renderColor());
  }
}

module.exports = ManifestColorItem;
