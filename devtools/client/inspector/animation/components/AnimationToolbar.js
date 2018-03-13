/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const PauseResumeButton = createFactory(require("./PauseResumeButton"));

class AnimationToolbar extends PureComponent {
  static get propTypes() {
    return {
      animations: PropTypes.arrayOf(PropTypes.object).isRequired,
      setAnimationsPlayState: PropTypes.func.isRequired,
    };
  }

  render() {
    const {
      animations,
      setAnimationsPlayState,
    } = this.props;

    return dom.div(
      {
        className: "animation-toolbar devtools-toolbar",
      },
      PauseResumeButton(
        {
          animations,
          setAnimationsPlayState,
        }
      )
    );
  }
}

module.exports = AnimationToolbar;
