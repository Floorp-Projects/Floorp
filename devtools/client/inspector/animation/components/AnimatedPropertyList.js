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
      // Map which is mapped property name and the keyframes.
      animatedPropertyMap: null,
      // Object which is mapped property name and the animation type
      // such the "color", "discrete", "length" and so on.
      animationTypes: null,
      // To avoid rendering while the state is updating
      // since we call an async function in updateState.
      isStateUpdating: false,
    };
  }

  componentDidMount() {
    // No need to set isStateUpdating state since paint sequence is finish here.
    this.updateState(this.props.animation);
  }

  componentWillReceiveProps(nextProps) {
    this.setState({ isStateUpdating: true });
    this.updateState(nextProps.animation);
  }

  shouldComponentUpdate(nextProps, nextState) {
    return !nextState.isStateUpdating;
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

  async updateState(animation) {
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

    this.setState(
      {
        animatedPropertyMap,
        animationTypes,
        isStateUpdating: false
      }
    );

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
      [...animatedPropertyMap.entries()].map(([name, keyframes]) => {
        const state = this.getPropertyState(name);
        const type = animationTypes[name];
        return AnimatedPropertyItem(
          {
            getComputedStyle,
            keyframes,
            name,
            simulateAnimation,
            state,
            type,
          }
        );
      })
    );
  }
}

module.exports = AnimatedPropertyList;
