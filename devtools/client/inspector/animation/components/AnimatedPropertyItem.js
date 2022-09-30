/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

const AnimatedPropertyName = createFactory(
  require("resource://devtools/client/inspector/animation/components/AnimatedPropertyName.js")
);
const KeyframesGraph = createFactory(
  require("resource://devtools/client/inspector/animation/components/keyframes-graph/KeyframesGraph.js")
);

class AnimatedPropertyItem extends PureComponent {
  static get propTypes() {
    return {
      getComputedStyle: PropTypes.func.isRequired,
      isUnchanged: PropTypes.bool.isRequired,
      keyframes: PropTypes.array.isRequired,
      name: PropTypes.string.isRequired,
      simulateAnimation: PropTypes.func.isRequired,
      state: PropTypes.object.isRequired,
      type: PropTypes.string.isRequired,
    };
  }

  render() {
    const {
      getComputedStyle,
      isUnchanged,
      keyframes,
      name,
      simulateAnimation,
      state,
      type,
    } = this.props;

    return dom.li(
      {
        className: "animated-property-item" + (isUnchanged ? " unchanged" : ""),
      },
      AnimatedPropertyName({
        name,
        state,
      }),
      KeyframesGraph({
        getComputedStyle,
        keyframes,
        name,
        simulateAnimation,
        type,
      })
    );
  }
}

module.exports = AnimatedPropertyItem;
