/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

/**
 * This component displays runtime information.
 */
class RuntimeInfo extends PureComponent {
  static get propTypes() {
    return {
      icon: PropTypes.string.isRequired,
      deviceName: PropTypes.string,
      name: PropTypes.string.isRequired,
      version: PropTypes.string.isRequired,
    };
  }

  render() {
    const { icon, deviceName, name, version } = this.props;

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
      Localized(
        {
          id: deviceName ? "about-debugging-runtime-info-with-model"
                          : "about-debugging-runtime-info",
          $name: name,
          $deviceName: deviceName,
          $version: version,
        },
        dom.label({}, `${ name } on ${ deviceName } (${ version })`)
      )
    );
  }
}

module.exports = RuntimeInfo;
