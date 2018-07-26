/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

/**
 * This component shows the runtime as item in RuntimesPane.
 */
class RuntimeItem extends PureComponent {
  static get propTypes() {
    return {
      icon: PropTypes.string.isRequired,
      isSelected: PropTypes.bool.isRequired,
      name: PropTypes.string.isRequired,
    };
  }

  render() {
    const { icon, isSelected, name } = this.props;

    return dom.li(
      {
        className: "runtime-item" + (isSelected ? " runtime-item--selected" : ""),
      },
      dom.img({
        className: "runtime-item__icon" +
                   (isSelected ? " runtime-item__icon--selected" : ""),
        src: icon,
      }),
      name
    );
  }
}

module.exports = RuntimeItem;
