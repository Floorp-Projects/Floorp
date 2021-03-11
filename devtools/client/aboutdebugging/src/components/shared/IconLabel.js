/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const {
  ICON_LABEL_LEVEL,
} = require("devtools/client/aboutdebugging/src/constants");

const ICONS = {
  [ICON_LABEL_LEVEL.INFO]: "chrome://global/skin/icons/info.svg",
  [ICON_LABEL_LEVEL.OK]: "chrome://global/skin/icons/success.svg",
};

/* This component displays an icon accompanied by some content. It's similar
   to a message, but it's not interactive */
class IconLabel extends PureComponent {
  static get propTypes() {
    return {
      children: PropTypes.node.isRequired,
      className: PropTypes.string,
      level: PropTypes.oneOf(Object.values(ICON_LABEL_LEVEL)).isRequired,
    };
  }

  render() {
    const { children, className, level } = this.props;
    return dom.span(
      {
        className: `icon-label icon-label--${level} ${className || ""}`,
      },
      dom.img({
        className: "icon-label__icon",
        src: ICONS[level],
      }),
      children
    );
  }
}

module.exports = IconLabel;
