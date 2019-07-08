/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const { getStr } = require("../utils/l10n");

class RewindButton extends PureComponent {
  static get propTypes() {
    return {
      rewindAnimationsCurrentTime: PropTypes.func.isRequired,
    };
  }

  render() {
    const { rewindAnimationsCurrentTime } = this.props;

    return dom.button({
      className: "rewind-button devtools-button",
      onClick: event => {
        event.stopPropagation();
        rewindAnimationsCurrentTime();
      },
      title: getStr("timeline.rewindButtonTooltip"),
    });
  }
}

module.exports = RewindButton;
