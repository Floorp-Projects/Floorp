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
        className: "main-heading runtime-info",
      },
      dom.img(
        {
          className: "main-heading__icon runtime-info__icon",
          src: icon,
        }
      ),
      Localized(
        {
          id: "about-debugging-runtime-name",
          $name: name,
          $version: version,
        },
        dom.label(
          {
            className: "js-runtime-name runtime-info__title",
          },
          `${ name } (${ version })`
        )
      ),
      deviceName ?
        dom.label(
          {
            className: "runtime-info__subtitle",
          },
          deviceName
        ) : null,
      dom.button(
        {
          className: "default-button runtime-info__action",
          // Implementation ongoing in Bug 1505128.
          disabled: true,
        },
        "Disconnect"
      )
    );
  }
}

module.exports = RuntimeInfo;
