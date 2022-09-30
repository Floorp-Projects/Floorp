/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

const AnimationItem = createFactory(
  require("resource://devtools/client/inspector/animation/components/AnimationItem.js")
);

class AnimationList extends PureComponent {
  static get propTypes() {
    return {
      animations: PropTypes.arrayOf(PropTypes.object).isRequired,
      dispatch: PropTypes.func.isRequired,
      displayableRange: PropTypes.object.isRequired,
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
      displayableRange,
      getAnimatedPropertyMap,
      getNodeFromActor,
      selectAnimation,
      setHighlightedNode,
      setSelectedNode,
      simulateAnimation,
      timeScale,
    } = this.props;

    const { startIndex, endIndex } = displayableRange;

    return dom.ul(
      {
        className: "animation-list",
      },
      animations.map((animation, index) =>
        AnimationItem({
          animation,
          dispatch,
          getAnimatedPropertyMap,
          getNodeFromActor,
          isDisplayable: startIndex <= index && index <= endIndex,
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
