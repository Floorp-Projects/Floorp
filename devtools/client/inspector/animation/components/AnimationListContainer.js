/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } =
  require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

const AnimationList = createFactory(require("./AnimationList"));
const AnimationListHeader = createFactory(require("./AnimationListHeader"));

const TimeScale = require("../utils/timescale");

class AnimationListContainer extends PureComponent {
  static get propTypes() {
    return {
      animations: PropTypes.arrayOf(PropTypes.object).isRequired,
      emitEventForTest: PropTypes.func.isRequired,
      getAnimatedPropertyMap: PropTypes.func.isRequired,
      getNodeFromActor: PropTypes.func.isRequired,
      onHideBoxModelHighlighter: PropTypes.func.isRequired,
      onShowBoxModelHighlighterForNode: PropTypes.func.isRequired,
      setSelectedNode: PropTypes.func.isRequired,
      simulateAnimation: PropTypes.func.isRequired,
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
      setSelectedNode,
      simulateAnimation,
    } = this.props;
    const timeScale = new TimeScale(animations);

    return dom.div(
      {
        className: "animation-list-container"
      },
      AnimationListHeader(
        {
          timeScale,
        }
      ),
      AnimationList(
        {
          animations,
          emitEventForTest,
          getAnimatedPropertyMap,
          getNodeFromActor,
          onHideBoxModelHighlighter,
          onShowBoxModelHighlighterForNode,
          setSelectedNode,
          simulateAnimation,
          timeScale,
        }
      )
    );
  }
}

module.exports = AnimationListContainer;
