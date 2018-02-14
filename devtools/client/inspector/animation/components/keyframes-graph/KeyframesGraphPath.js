/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const ReactDOM = require("devtools/client/shared/vendor/react-dom");

const ColorPath = createFactory(require("./ColorPath"));
const DistancePath = createFactory(require("./DistancePath"));

const {
  DEFAULT_GRAPH_HEIGHT,
  DEFAULT_KEYFRAMES_GRAPH_DURATION,
} = require("../../utils/graph-helper");

class KeyframesGraphPath extends PureComponent {
  static get propTypes() {
    return {
      simulateAnimation: PropTypes.func.isRequired,
      type: PropTypes.string.isRequired,
      values: PropTypes.array.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      componentWidth: 0,
    };
  }

  componentDidMount() {
    this.updateState();
  }

  getPathComponent(type) {
    switch (type) {
      case "color" :
        return ColorPath;
      default :
        return DistancePath;
    }
  }

  updateState() {
    const thisEl = ReactDOM.findDOMNode(this);
    this.setState({ componentWidth: thisEl.parentNode.clientWidth });
  }

  render() {
    const {
      simulateAnimation,
      type,
      values,
    } = this.props;
    const { componentWidth } = this.state;

    if (!componentWidth) {
      return dom.svg();
    }

    const pathComponent = this.getPathComponent(type);

    return dom.svg(
      {
        className: "keyframes-graph-path",
        preserveAspectRatio: "none",
        viewBox: `0 -${ DEFAULT_GRAPH_HEIGHT } `
                 + `${ DEFAULT_KEYFRAMES_GRAPH_DURATION } ${ DEFAULT_GRAPH_HEIGHT }`,
      },
      pathComponent(
        {
          componentWidth,
          graphHeight: DEFAULT_GRAPH_HEIGHT,
          simulateAnimation,
          totalDuration: DEFAULT_KEYFRAMES_GRAPH_DURATION,
          values,
        }
      )
    );
  }
}

module.exports = KeyframesGraphPath;
