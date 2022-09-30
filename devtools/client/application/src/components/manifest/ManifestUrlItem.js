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
 * This component displays a Manifest member which holds a URL
 */
class ManifestUrlItem extends PureComponent {
  static get propTypes() {
    return {
      ...Types.manifestItemUrl, // { label, value }
    };
  }

  render() {
    const { label, value } = this.props;
    return ManifestItem(
      { label },
      div({ className: "manifest-item__url" }, value)
    );
  }
}

module.exports = ManifestUrlItem;
