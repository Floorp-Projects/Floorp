/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const AnimatedPropertyItem = createFactory(require("./AnimatedPropertyItem"));

class AnimatedPropertyList extends PureComponent {
  static get propTypes() {
    return {
      animation: PropTypes.object.isRequired,
      emitEventForTest: PropTypes.func.isRequired,
      getAnimatedPropertyMap: PropTypes.func.isRequired,
      getComputedStyle: PropTypes.func.isRequired,
      simulateAnimation: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      animatedPropertyMap: null
    };
  }

  componentDidMount() {
    this.updateKeyframesList(this.props.animation);
  }

  componentWillReceiveProps(nextProps) {
    this.updateKeyframesList(nextProps.animation);
  }

  getPropertyState(property) {
    const { animation } = this.props;

    for (const propState of animation.state.propertyState) {
      if (propState.property === property) {
        return propState;
      }
    }

    return null;
  }

  async updateKeyframesList(animation) {
    const {
      getAnimatedPropertyMap,
      emitEventForTest,
    } = this.props;

    let animatedPropertyMap = null;
    let animationTypes = null;

    try {
      animatedPropertyMap = await getAnimatedPropertyMap(animation);
      animationTypes = await animation.getAnimationTypes(animatedPropertyMap.keys());
    } catch (e) {
      // Expected if we've already been destroyed or other node have been selected
      // in the meantime.
      console.error(e);
      return;
    }

    this.setState({ animatedPropertyMap, animationTypes });
    emitEventForTest("animation-keyframes-rendered");
  }

  render() {
    const {
      getComputedStyle,
      simulateAnimation,
    } = this.props;
    const {
      animatedPropertyMap,
      animationTypes,
    } = this.state;

    if (!animatedPropertyMap) {
      return null;
    }

    return dom.ul(
      {
        className: "animated-property-list"
      },
      [...animatedPropertyMap.entries()].map(([property, values]) => {
        const state = this.getPropertyState(property);
        const type = animationTypes[property];
        return AnimatedPropertyItem(
          {
            getComputedStyle,
            property,
            simulateAnimation,
            state,
            type,
            values,
          }
        );
      })
    );
  }
}

module.exports = AnimatedPropertyList;
