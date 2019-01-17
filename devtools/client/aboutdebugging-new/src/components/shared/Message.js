/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const { MESSAGE_LEVEL } = require("../../constants");

const ICONS = {
  // Reuse the warning icon for errors. Waiting for the proper icon in Bug 1520191.
  [MESSAGE_LEVEL.ERROR]: "chrome://global/skin/icons/warning.svg",
  [MESSAGE_LEVEL.WARNING]: "chrome://global/skin/icons/warning.svg",
};

/**
 * This component is designed to display a photon-style message bar. The component is
 * responsible for displaying the message container with the appropriate icon. The content
 * of the message should be passed as the children of this component.
 */
class Message extends PureComponent {
  static get propTypes() {
    return {
      children: PropTypes.node.isRequired,
      level: PropTypes.oneOf(Object.values(MESSAGE_LEVEL)).isRequired,
    };
  }

  render() {
    const { level } = this.props;
    const iconSrc = ICONS[level];

    return dom.aside(
      {
        className: `message message--level-${level} js-message`,
      },
      dom.img(
        {
          className: "message__icon",
          src: iconSrc,
        }
      ),
      this.props.children
    );
  }
}

module.exports = Message;
