/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const {
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const {
  tr,
  td,
  th,
} = require("resource://devtools/client/shared/vendor/react-dom-factories.js");

/**
 * This component displays a key-value data pair from a manifest
 */
class ManifestItem extends PureComponent {
  static get propTypes() {
    return {
      label: PropTypes.node.isRequired,
      children: PropTypes.node,
    };
  }

  render() {
    const { children, label } = this.props;
    return tr(
      {
        className: "manifest-item js-manifest-item",
      },
      th(
        {
          className: "manifest-item__label js-manifest-item-label",
          scope: "row",
        },
        label
      ),
      td(
        { className: "manifest-item__value js-manifest-item-content" },
        children
      )
    );
  }
}

// Exports
module.exports = ManifestItem;
