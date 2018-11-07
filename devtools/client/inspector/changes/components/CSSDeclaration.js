/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

class CSSDeclaration extends PureComponent {
  static get propTypes() {
    return {
      property: PropTypes.string.isRequired,
      value: PropTypes.string.isRequired,
      className: PropTypes.string,
    };
  }

  static get defaultProps() {
    return {
      className: "",
    };
  }

  render() {
    const { property, value, className } = this.props;

    return dom.div({ className: `declaration ${className}` },
      dom.span({ className: "declaration-name theme-fg-color5"}, property),
      ":",
      dom.span({ className: "declaration-value theme-fg-color1"}, value),
      ";"
    );
  }
}

module.exports = CSSDeclaration;
