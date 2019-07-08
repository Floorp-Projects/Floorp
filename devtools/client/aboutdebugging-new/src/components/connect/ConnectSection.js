/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

class ConnectSection extends PureComponent {
  static get propTypes() {
    return {
      children: PropTypes.node,
      className: PropTypes.string,
      extraContent: PropTypes.node,
      icon: PropTypes.string.isRequired,
      title: PropTypes.node.isRequired,
    };
  }

  renderExtraContent() {
    const { extraContent } = this.props;
    return dom.section(
      {
        className: "connect-section__extra",
      },
      extraContent
    );
  }

  render() {
    const { extraContent } = this.props;

    return dom.section(
      {
        className: `card connect-section ${this.props.className || ""}`,
      },
      dom.header(
        {
          className: "connect-section__header",
        },
        dom.img({
          className: "connect-section__header__icon",
          src: this.props.icon,
        }),
        dom.h1(
          {
            className: "card__heading connect-section__header__title",
          },
          this.props.title
        )
      ),
      this.props.children
        ? dom.div(
            {
              className: "connect-section__content",
            },
            this.props.children
          )
        : null,
      extraContent ? this.renderExtraContent() : null
    );
  }
}

module.exports = ConnectSection;
