/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

class AnimationName extends PureComponent {
  static get propTypes() {
    return {
      animation: PropTypes.object.isRequired,
    };
  }

  render() {
    const {
      animation,
    } = this.props;

    return dom.svg(
      {
        className: "animation-name",
      },
      dom.text(
        {
          y: "50%",
          x: "100%",
        },
        animation.state.name
      )
    );
  }
}

module.exports = AnimationName;
