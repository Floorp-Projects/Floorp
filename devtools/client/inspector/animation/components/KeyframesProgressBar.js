/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const ReactDOM = require("devtools/client/shared/vendor/react-dom");

class KeyframesProgressBar extends PureComponent {
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

  constructor(props) {
    super(props);

    this.onCurrentTimeUpdated = this.onCurrentTimeUpdated.bind(this);

    this.state = {
      // offset of the position for the progress bar
      offset: 0,
    };
  }

  componentDidMount() {
    const { addAnimationsCurrentTimeListener } = this.props;

    this.element = ReactDOM.findDOMNode(this);
    this.setupAnimation(this.props);
    addAnimationsCurrentTimeListener(this.onCurrentTimeUpdated);
  }

  componentWillReceiveProps(nextProps) {
    const { getAnimationsCurrentTime } = nextProps;

    this.setupAnimation(nextProps);
    this.onCurrentTimeUpdated(getAnimationsCurrentTime());
  }

  componentWillUnmount() {
    const { removeAnimationsCurrentTimeListener } = this.props;

    removeAnimationsCurrentTimeListener(this.onCurrentTimeUpdated);
    this.element = null;
    this.simulatedAnimation = null;
  }

  onCurrentTimeUpdated(currentTime) {
    const {
      animation,
      timeScale,
    } = this.props;
    const {
      playbackRate,
      previousStartTime = 0,
    } = animation.state;

    this.simulatedAnimation.currentTime =
      (timeScale.minStartTime + currentTime - previousStartTime) * playbackRate;
    const offset = this.element.offsetWidth *
                   this.simulatedAnimation.effect.getComputedTiming().progress;

    this.setState({ offset });
  }

  setupAnimation(props) {
    const {
      animation,
      simulateAnimationForKeyframesProgressBar,
    } = props;

    if (this.simulatedAnimation) {
      this.simulatedAnimation.cancel();
    }

    const timing = Object.assign({}, animation.state, {
      iterations: animation.state.iterationCount || Infinity
    });

    this.simulatedAnimation = simulateAnimationForKeyframesProgressBar(timing);
  }

  render() {
    const { offset } = this.state;

    return dom.div(
      {
        className: "keyframes-progress-bar-area devtools-toolbar",
      },
      dom.div(
        {
          className: "keyframes-progress-bar",
          style: {
            transform: `translateX(${ offset }px)`,
          },
        }
      )
    );
  }
}

module.exports = KeyframesProgressBar;
