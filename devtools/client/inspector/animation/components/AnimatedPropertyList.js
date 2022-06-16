/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const AnimatedPropertyItem = createFactory(
  require("devtools/client/inspector/animation/components/AnimatedPropertyItem")
);

class AnimatedPropertyList extends Component {
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
      // Array of object which has the property name, the keyframes, its aniamtion type
      // and unchanged flag.
      animatedProperties: null,
      // To avoid rendering while the state is updating
      // since we call an async function in updateState.
      isStateUpdating: false,
    };
  }

  componentDidMount() {
    // No need to set isStateUpdating state since paint sequence is finish here.
    this.updateState(this.props.animation);
  }

  // FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=1774507
  UNSAFE_componentWillReceiveProps(nextProps) {
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
    const { getAnimatedPropertyMap, emitEventForTest } = this.props;

    let propertyMap = null;
    let propertyNames = null;
    let types = null;

    try {
      propertyMap = getAnimatedPropertyMap(animation);
      propertyNames = [...propertyMap.keys()];
      types = await animation.getAnimationTypes(propertyNames);
    } catch (e) {
      // Expected if we've already been destroyed or other node have been selected
      // in the meantime.
      console.error(e);
      return;
    }

    const animatedProperties = propertyNames.map(name => {
      const keyframes = propertyMap.get(name);
      const type = types[name];
      const isUnchanged = keyframes.every(
        keyframe => keyframe.value === keyframes[0].value
      );
      return { isUnchanged, keyframes, name, type };
    });

    animatedProperties.sort((a, b) => {
      if (a.isUnchanged === b.isUnchanged) {
        return a.name > b.name ? 1 : -1;
      }

      return a.isUnchanged ? 1 : -1;
    });

    this.setState({
      animatedProperties,
      isStateUpdating: false,
    });

    emitEventForTest("animation-keyframes-rendered");
  }

  render() {
    const { getComputedStyle, simulateAnimation } = this.props;
    const { animatedProperties } = this.state;

    if (!animatedProperties) {
      return null;
    }

    return dom.ul(
      {
        className: "animated-property-list",
      },
      animatedProperties.map(({ isUnchanged, keyframes, name, type }) => {
        const state = this.getPropertyState(name);
        return AnimatedPropertyItem({
          getComputedStyle,
          isUnchanged,
          keyframes,
          name,
          simulateAnimation,
          state,
          type,
        });
      })
    );
  }
}

module.exports = AnimatedPropertyList;
