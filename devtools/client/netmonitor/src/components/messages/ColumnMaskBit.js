/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

/**
 * Renders the "MaskBit" column of a message.
 */
class ColumnMaskBit extends Component {
  static get propTypes() {
    return {
      item: PropTypes.object.isRequired,
    };
  }

  shouldComponentUpdate(nextProps) {
    return this.props.item.maskBit !== nextProps.item.maskBit;
  }

  render() {
    const { maskBit } = this.props.item;

    return dom.td(
      {
        className: "message-list-column message-list-maskBit",
        title: maskBit.toString(),
      },
      maskBit.toString()
    );
  }
}

module.exports = ColumnMaskBit;
