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
      children: PropTypes.any.isRequired,
      className: PropTypes.string,
      icon: PropTypes.string.isRequired,
      title: PropTypes.string.isRequired,
    };
  }

  render() {
    return dom.section(
      {
        className: `page__section ${this.props.className || ""}`,
      },
      dom.h2(
        {
          className: "page__section__title",
        },
        dom.img(
          {
            className: "page__section__icon",
            src: this.props.icon,
          }
        ),
        this.props.title
      ),
      this.props.children
    );
  }
}

module.exports = ConnectSection;
