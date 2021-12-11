/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const KeyframeMarkerList = createFactory(
  require("devtools/client/inspector/animation/components/keyframes-graph/KeyframeMarkerList")
);
const KeyframesGraphPath = createFactory(
  require("devtools/client/inspector/animation/components/keyframes-graph/KeyframesGraphPath")
);

class KeyframesGraph extends PureComponent {
  static get propTypes() {
    return {
      getComputedStyle: PropTypes.func.isRequired,
      keyframes: PropTypes.array.isRequired,
      name: PropTypes.string.isRequired,
      simulateAnimation: PropTypes.func.isRequired,
      type: PropTypes.string.isRequired,
    };
  }

  render() {
    const {
      getComputedStyle,
      keyframes,
      name,
      simulateAnimation,
      type,
    } = this.props;

    return dom.div(
      {
        className: `keyframes-graph ${name}`,
      },
      KeyframesGraphPath({
        getComputedStyle,
        keyframes,
        name,
        simulateAnimation,
        type,
      }),
      KeyframeMarkerList({ keyframes })
    );
  }
}

module.exports = KeyframesGraph;
