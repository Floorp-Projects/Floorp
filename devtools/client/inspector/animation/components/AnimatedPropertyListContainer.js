/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const AnimatedPropertyList = createFactory(require("./AnimatedPropertyList"));
const KeyframesProgressBar = createFactory(require("./KeyframesProgressBar"));
const ProgressInspectionPanel = createFactory(require("./ProgressInspectionPanel"));

const { getFormatStr } = require("../utils/l10n");

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
        className: `animated-property-list-container ${ animation.state.type }`
      },
      ProgressInspectionPanel(
        {
          indicator: KeyframesProgressBar(
            {
              addAnimationsCurrentTimeListener,
              animation,
              getAnimationsCurrentTime,
              removeAnimationsCurrentTimeListener,
              simulateAnimationForKeyframesProgressBar,
              timeScale,
            }
          ),
          list: AnimatedPropertyList(
            {
              animation,
              emitEventForTest,
              getAnimatedPropertyMap,
              getComputedStyle,
              simulateAnimation,
            }
          ),
          ticks: [0, 50, 100].map(position => {
            const label = getFormatStr("detail.propertiesHeader.percentage", position);
            return { position, label };
          })
        }
      )
    );
  }
}

module.exports = AnimatedPropertyListContainer;
