/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

class ConnectSteps extends PureComponent {
  static get propTypes() {
    return {
      steps: PropTypes.arrayOf(PropTypes.string).isRequired,
    };
  }

  render() {
    return dom.ul(
      {
        className: "connect-page__steps"
      },
      this.props.steps.map(step => dom.li(
        {
          className: "connect-page__step",
          key: step,
        },
        step)
      )
    );
  }
}

module.exports = ConnectSteps;
