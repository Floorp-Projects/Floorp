/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const DelaySign = createFactory(require("./DelaySign"));
const SummaryGraphPath = createFactory(require("./SummaryGraphPath"));

class SummaryGraph extends PureComponent {
  static get propTypes() {
    return {
      animation: PropTypes.object.isRequired,
      simulateAnimation: PropTypes.func.isRequired,
      timeScale: PropTypes.object.isRequired,
    };
  }

  render() {
    const {
      animation,
      simulateAnimation,
      timeScale,
    } = this.props;

    return dom.div(
      {
        className: "animation-summary-graph",
      },
      SummaryGraphPath(
        {
          animation,
          simulateAnimation,
          timeScale,
        }
      ),
      animation.state.delay ?
        DelaySign(
          {
            animation,
            timeScale,
          }
        )
      :
      null
    );
  }
}

module.exports = SummaryGraph;
