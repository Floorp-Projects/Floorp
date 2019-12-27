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
const ReactDOM = require("devtools/client/shared/vendor/react-dom");

const ColorPath = createFactory(require("./ColorPath"));
const DiscretePath = createFactory(require("./DiscretePath"));
const DistancePath = createFactory(require("./DistancePath"));

const {
  DEFAULT_EASING_HINT_STROKE_WIDTH,
  DEFAULT_GRAPH_HEIGHT,
  DEFAULT_KEYFRAMES_GRAPH_DURATION,
} = require("../../utils/graph-helper");

class KeyframesGraphPath extends PureComponent {
  static get propTypes() {
    return {
      getComputedStyle: PropTypes.func.isRequired,
      keyframes: PropTypes.array.isRequired,
      name: PropTypes.string.isRequired,
      simulateAnimation: PropTypes.func.isRequired,
      type: PropTypes.string.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      componentHeight: 0,
      componentWidth: 0,
    };
  }

  componentDidMount() {
    this.updateState();
  }

  getPathComponent(type) {
    switch (type) {
      case "color":
        return ColorPath;
      case "discrete":
        return DiscretePath;
      default:
        return DistancePath;
    }
  }

  updateState() {
    const thisEl = ReactDOM.findDOMNode(this);
    this.setState({
      componentHeight: thisEl.parentNode.clientHeight,
      componentWidth: thisEl.parentNode.clientWidth,
    });
  }

  render() {
    const {
      getComputedStyle,
      keyframes,
      name,
      simulateAnimation,
      type,
    } = this.props;
    const { componentHeight, componentWidth } = this.state;

    if (!componentWidth) {
      return dom.svg();
    }

    const pathComponent = this.getPathComponent(type);
    const strokeWidthInViewBox =
      (DEFAULT_EASING_HINT_STROKE_WIDTH / 2 / componentHeight) *
      DEFAULT_GRAPH_HEIGHT;

    return dom.svg(
      {
        className: "keyframes-graph-path",
        preserveAspectRatio: "none",
        viewBox:
          `0 -${DEFAULT_GRAPH_HEIGHT + strokeWidthInViewBox} ` +
          `${DEFAULT_KEYFRAMES_GRAPH_DURATION} ` +
          `${DEFAULT_GRAPH_HEIGHT + strokeWidthInViewBox * 2}`,
      },
      pathComponent({
        componentWidth,
        easingHintStrokeWidth: DEFAULT_EASING_HINT_STROKE_WIDTH,
        getComputedStyle,
        graphHeight: DEFAULT_GRAPH_HEIGHT,
        keyframes,
        name,
        simulateAnimation,
        totalDuration: DEFAULT_KEYFRAMES_GRAPH_DURATION,
      })
    );
  }
}

module.exports = KeyframesGraphPath;
