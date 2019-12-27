/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

const { MESSAGE_LEVEL } = require("../../constants");

const ICONS = {
  [MESSAGE_LEVEL.ERROR]:
    "chrome://devtools/skin/images/aboutdebugging-error.svg",
  [MESSAGE_LEVEL.INFO]:
    "chrome://devtools/skin/images/aboutdebugging-information.svg",
  [MESSAGE_LEVEL.WARNING]: "chrome://global/skin/icons/warning.svg",
};
const CLOSE_ICON_SRC = "chrome://devtools/skin/images/close.svg";

/**
 * This component is designed to display a photon-style message bar. The component is
 * responsible for displaying the message container with the appropriate icon. The content
 * of the message should be passed as the children of this component.
 */
class Message extends PureComponent {
  static get propTypes() {
    return {
      children: PropTypes.node.isRequired,
      className: PropTypes.string,
      isCloseable: PropTypes.bool,
      level: PropTypes.oneOf(Object.values(MESSAGE_LEVEL)).isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.state = {
      isClosed: false,
    };
  }

  closeMessage() {
    this.setState({ isClosed: true });
  }

  renderButton(level) {
    return dom.button(
      {
        className:
          `ghost-button message__button message__button--${level} ` +
          `qa-message-button-close-button`,
        onClick: () => this.closeMessage(),
      },
      Localized(
        {
          id: "about-debugging-message-close-icon",
          attrs: {
            alt: true,
          },
        },
        dom.img({
          className: "qa-message-button-close-icon",
          src: CLOSE_ICON_SRC,
        })
      )
    );
  }

  render() {
    const { children, className, level, isCloseable } = this.props;
    const { isClosed } = this.state;

    if (isClosed) {
      return null;
    }

    return dom.aside(
      {
        className:
          `message message--level-${level}  qa-message` +
          (className ? ` ${className}` : ""),
      },
      dom.img({
        className: "message__icon",
        src: ICONS[level],
      }),
      dom.div(
        {
          className: "message__body",
        },
        children
      ),
      // if the message is closeable, render a closing button
      isCloseable ? this.renderButton(level) : null
    );
  }
}

module.exports = Message;
