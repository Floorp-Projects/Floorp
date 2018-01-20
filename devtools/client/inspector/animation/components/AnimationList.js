/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const AnimationItem = createFactory(require("./AnimationItem"));

class AnimationList extends PureComponent {
  static get propTypes() {
    return {
      animations: PropTypes.arrayOf(PropTypes.object).isRequired,
      emitEventForTest: PropTypes.func.isRequired,
      getAnimatedPropertyMap: PropTypes.func.isRequired,
      getNodeFromActor: PropTypes.func.isRequired,
      onHideBoxModelHighlighter: PropTypes.func.isRequired,
      onShowBoxModelHighlighterForNode: PropTypes.func.isRequired,
      selectAnimation: PropTypes.func.isRequired,
      setSelectedNode: PropTypes.func.isRequired,
      simulateAnimation: PropTypes.func.isRequired,
      timeScale: PropTypes.object.isRequired,
    };
  }

  render() {
    const {
      animations,
      emitEventForTest,
      getAnimatedPropertyMap,
      getNodeFromActor,
      onHideBoxModelHighlighter,
      onShowBoxModelHighlighterForNode,
      selectAnimation,
      setSelectedNode,
      simulateAnimation,
      timeScale,
    } = this.props;

    return dom.ul(
      {
        className: "animation-list"
      },
      animations.map(animation =>
        AnimationItem(
          {
            animation,
            emitEventForTest,
            getAnimatedPropertyMap,
            getNodeFromActor,
            onHideBoxModelHighlighter,
            onShowBoxModelHighlighterForNode,
            selectAnimation,
            setSelectedNode,
            simulateAnimation,
            timeScale,
          }
        )
      )
    );
  }
}

module.exports = AnimationList;
