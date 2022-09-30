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

const AnimatedPropertyList = createFactory(
  require("resource://devtools/client/inspector/animation/components/AnimatedPropertyList.js")
);
const KeyframesProgressBar = createFactory(
  require("resource://devtools/client/inspector/animation/components/KeyframesProgressBar.js")
);
const ProgressInspectionPanel = createFactory(
  require("resource://devtools/client/inspector/animation/components/ProgressInspectionPanel.js")
);

const {
  getFormatStr,
} = require("resource://devtools/client/inspector/animation/utils/l10n.js");

class AnimatedPropertyListContainer extends PureComponent {
  static get propTypes() {
    return {
      addAnimationsCurrentTimeListener: PropTypes.func.isRequired,
      animation: PropTypes.object.isRequired,
      emitEventForTest: PropTypes.func.isRequired,
      getAnimatedPropertyMap: PropTypes.func.isRequired,
      getAnimationsCurrentTime: PropTypes.func.isRequired,
      getComputedStyle: PropTypes.func.isRequired,
      removeAnimationsCurrentTimeListener: PropTypes.func.isRequired,
      simulateAnimation: PropTypes.func.isRequired,
      simulateAnimationForKeyframesProgressBar: PropTypes.func.isRequired,
      timeScale: PropTypes.object.isRequired,
    };
  }

  render() {
    const {
      addAnimationsCurrentTimeListener,
      animation,
      emitEventForTest,
      getAnimatedPropertyMap,
      getAnimationsCurrentTime,
      getComputedStyle,
      removeAnimationsCurrentTimeListener,
      simulateAnimation,
      simulateAnimationForKeyframesProgressBar,
      timeScale,
    } = this.props;

    return dom.div(
      {
        className: `animated-property-list-container ${animation.state.type}`,
      },
      ProgressInspectionPanel({
        indicator: KeyframesProgressBar({
          addAnimationsCurrentTimeListener,
          animation,
          getAnimationsCurrentTime,
          removeAnimationsCurrentTimeListener,
          simulateAnimationForKeyframesProgressBar,
          timeScale,
        }),
        list: AnimatedPropertyList({
          animation,
          emitEventForTest,
          getAnimatedPropertyMap,
          getComputedStyle,
          simulateAnimation,
        }),
        ticks: [0, 50, 100].map(position => {
          const label = getFormatStr(
            "detail.propertiesHeader.percentage",
            position
          );
          return { position, label };
        }),
      })
    );
  }
}

module.exports = AnimatedPropertyListContainer;
