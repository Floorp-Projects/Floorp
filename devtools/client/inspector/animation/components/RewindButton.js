/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

const {
  getStr,
} = require("resource://devtools/client/inspector/animation/utils/l10n.js");

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
