/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const ComputedStylePath = require("devtools/client/inspector/animation/components/keyframes-graph/ComputedStylePath");

class DiscretePath extends ComputedStylePath {
  static get propTypes() {
    return {
      name: PropTypes.string.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = this.propToState(props);
  }

  // FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=1774507
  UNSAFE_componentWillReceiveProps(nextProps) {
    this.setState(this.propToState(nextProps));
  }

  getPropertyName() {
    return this.props.name;
  }

  getPropertyValue(keyframe) {
    return keyframe.value;
  }

  propToState({ getComputedStyle, keyframes, name }) {
    const discreteValues = [];

    for (const keyframe of keyframes) {
      const style = getComputedStyle(name, { [name]: keyframe.value });

      if (!discreteValues.includes(style)) {
        discreteValues.push(style);
      }
    }

    return { discreteValues };
  }

  toSegmentValue(computedStyle) {
    const { discreteValues } = this.state;
    return discreteValues.indexOf(computedStyle) / (discreteValues.length - 1);
  }

  render() {
    return dom.g(
      {
        className: "discrete-path",
      },
      super.renderGraph()
    );
  }
}

module.exports = DiscretePath;
