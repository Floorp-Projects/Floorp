/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const KeyframesProgressBar = createFactory(require("./KeyframesProgressBar"));
const KeyframesProgressTickList = createFactory(require("./KeyframesProgressTickList"));

class AnimatedPropertyListHeader extends PureComponent {
  static get propTypes() {
    return {
      addAnimationsCurrentTimeListener: PropTypes.func.isRequired,
      animation: PropTypes.object.isRequired,
      getAnimationsCurrentTime: PropTypes.func.isRequired,
      removeAnimationsCurrentTimeListener: PropTypes.func.isRequired,
      simulateAnimationForKeyframesProgressBar: PropTypes.func.isRequired,
      timeScale: PropTypes.object.isRequired,
    };
  }

  render() {
    const {
      addAnimationsCurrentTimeListener,
      animation,
      getAnimationsCurrentTime,
      removeAnimationsCurrentTimeListener,
      simulateAnimationForKeyframesProgressBar,
      timeScale,
    } = this.props;

    return dom.div(
      {
        className: "animated-property-list-header devtools-toolbar"
      },
      KeyframesProgressTickList(),
      KeyframesProgressBar(
        {
          addAnimationsCurrentTimeListener,
          animation,
          getAnimationsCurrentTime,
          removeAnimationsCurrentTimeListener,
          simulateAnimationForKeyframesProgressBar,
          timeScale,
        }
      )
    );
  }
}

module.exports = AnimatedPropertyListHeader;
