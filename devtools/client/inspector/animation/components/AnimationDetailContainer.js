/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  connect,
} = require("resource://devtools/client/shared/vendor/react-redux.js");
const {
  createFactory,
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

const AnimationDetailHeader = createFactory(
  require("resource://devtools/client/inspector/animation/components/AnimationDetailHeader.js")
);
const AnimatedPropertyListContainer = createFactory(
  require("resource://devtools/client/inspector/animation/components/AnimatedPropertyListContainer.js")
);

class AnimationDetailContainer extends PureComponent {
  static get propTypes() {
    return {
      addAnimationsCurrentTimeListener: PropTypes.func.isRequired,
      animation: PropTypes.object.isRequired,
      emitEventForTest: PropTypes.func.isRequired,
      getAnimatedPropertyMap: PropTypes.func.isRequired,
      getAnimationsCurrentTime: PropTypes.func.isRequired,
      getComputedStyle: PropTypes.func.isRequired,
      removeAnimationsCurrentTimeListener: PropTypes.func.isRequired,
      setDetailVisibility: PropTypes.func.isRequired,
      simulateAnimation: PropTypes.func.isRequired,
      simulateAnimationForKeyframesProgressBar: PropTypes.func.isRequired,
      timeScale: PropTypes.object.isRequired,
    };
  }

  render() {
    const {
      addAnimationsCurrentTimeListener,
      animation,
      emitEventForTest,
      getAnimatedPropertyMap,
      getAnimationsCurrentTime,
      getComputedStyle,
      removeAnimationsCurrentTimeListener,
      setDetailVisibility,
      simulateAnimation,
      simulateAnimationForKeyframesProgressBar,
      timeScale,
    } = this.props;

    return dom.div(
      {
        className: "animation-detail-container",
      },
      animation
        ? AnimationDetailHeader({
            animation,
            setDetailVisibility,
          })
        : null,
      animation
        ? AnimatedPropertyListContainer({
            addAnimationsCurrentTimeListener,
            animation,
            emitEventForTest,
            getAnimatedPropertyMap,
            getAnimationsCurrentTime,
            getComputedStyle,
            removeAnimationsCurrentTimeListener,
            simulateAnimation,
            simulateAnimationForKeyframesProgressBar,
            timeScale,
          })
        : null
    );
  }
}

const mapStateToProps = state => {
  return {
    animation: state.animations.selectedAnimation,
  };
};

module.exports = connect(mapStateToProps)(AnimationDetailContainer);
