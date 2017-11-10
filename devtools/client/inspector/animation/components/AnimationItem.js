/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { DOM: dom, PropTypes, PureComponent } =
  require("devtools/client/shared/vendor/react");

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
