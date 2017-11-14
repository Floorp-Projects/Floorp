/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

class AnimationItem extends PureComponent {
  static get propTypes() {
    return {
      animation: PropTypes.object.isRequired,
    };
  }

  render() {
    return dom.li(
      {
        className: "animation-item"
      }
    );
  }
}

module.exports = AnimationItem;
