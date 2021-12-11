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
      className: PropTypes.string,
      property: PropTypes.string.isRequired,
      value: PropTypes.string.isRequired,
    };
  }

  static get defaultProps() {
    return {
      className: "",
    };
  }

  render() {
    const { className, property, value } = this.props;

    return dom.div(
      { className: `changes__declaration ${className}` },
      dom.span(
        { className: "changes__declaration-name theme-fg-color3" },
        property
      ),
      ": ",
      dom.span(
        { className: "changes__declaration-value theme-fg-color1" },
        value
      ),
      ";"
    );
  }
}

module.exports = CSSDeclaration;
