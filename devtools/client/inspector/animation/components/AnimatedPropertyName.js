/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

class AnimatedPropertyName extends PureComponent {
  static get propTypes() {
    return {
      name: PropTypes.string.isRequired,
      state: PropTypes.oneOfType([null, PropTypes.object]).isRequired,
    };
  }

  render() {
    const { name, state } = this.props;

    return dom.div(
      {
        className:
          "animated-property-name" +
          (state && state.runningOnCompositor ? " compositor" : "") +
          (state && state.warning ? " warning" : ""),
        title: state ? state.warning : "",
      },
      dom.span({}, name)
    );
  }
}

module.exports = AnimatedPropertyName;
