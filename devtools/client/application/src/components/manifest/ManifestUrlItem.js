/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const { div } = require("devtools/client/shared/vendor/react-dom-factories");

const Types = require("devtools/client/application/src/types/index");
const ManifestItem = createFactory(
  require("devtools/client/application/src/components/manifest/ManifestItem")
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
