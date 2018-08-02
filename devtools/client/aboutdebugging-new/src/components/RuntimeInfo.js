/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

/**
 * This component displays runtime information.
 */
class RuntimeInfo extends PureComponent {
  static get propTypes() {
    return {
      icon: PropTypes.string.isRequired,
      name: PropTypes.string.isRequired,
      version: PropTypes.string.isRequired,
    };
  }

  render() {
    const { icon, name, version } = this.props;

    return dom.h1(
      {
        className: "runtime-info",
      },
      dom.img(
        {
          className: "runtime-info__icon",
          src: icon,
        }
      ),
      `${ name } (${ version })`
    );
  }
}

module.exports = RuntimeInfo;
