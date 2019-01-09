/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const Types = require("../types");

class Selector extends PureComponent {
  static get propTypes() {
    return {
      selector: PropTypes.shape(Types.selector).isRequired,
    };
  }

  render() {
    const { selectorText } = this.props.selector;

    return (
      dom.span(
        {
          className: "ruleview-selectorcontainer",
          tabIndex: -1,
        },
        selectorText
      )
    );
  }
}

module.exports = Selector;
