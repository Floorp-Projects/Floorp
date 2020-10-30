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

const AnimationItem = createFactory(
  require("devtools/client/inspector/animation/components/AnimationItem")
);

class AnimationList extends PureComponent {
  static get propTypes() {
    return {
      animations: PropTypes.arrayOf(PropTypes.object).isRequired,
      dispatch: PropTypes.func.isRequired,
      emitEventForTest: PropTypes.func.isRequired,
      getAnimatedPropertyMap: PropTypes.func.isRequired,
      getNodeFromActor: PropTypes.func.isRequired,
      selectAnimation: PropTypes.func.isRequired,
      setHighlightedNode: PropTypes.func.isRequired,
      setSelectedNode: PropTypes.func.isRequired,
      simulateAnimation: PropTypes.func.isRequired,
      timeScale: PropTypes.object.isRequired,
    };
  }

  render() {
    const {
      animations,
      dispatch,
      emitEventForTest,
      getAnimatedPropertyMap,
      getNodeFromActor,
      selectAnimation,
      setHighlightedNode,
      setSelectedNode,
      simulateAnimation,
      timeScale,
    } = this.props;

    return dom.ul(
      {
        className: "animation-list",
      },
      animations.map(animation =>
        AnimationItem({
          animation,
          dispatch,
          emitEventForTest,
          getAnimatedPropertyMap,
          getNodeFromActor,
          selectAnimation,
          setHighlightedNode,
          setSelectedNode,
          simulateAnimation,
          timeScale,
        })
      )
    );
  }
}

module.exports = AnimationList;
