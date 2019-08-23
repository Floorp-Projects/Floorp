/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

const Message = createFactory(require("./shared/Message"));

const { MESSAGE_LEVEL } = require("../constants");
const {
  COMPATIBILITY_STATUS,
} = require("devtools/client/shared/remote-debugging/version-checker");

const TROUBLESHOOTING_URL =
  "https://developer.mozilla.org/docs/Tools/about:debugging#Troubleshooting";

const Types = require("../types/index");

class CompatibilityWarning extends PureComponent {
  static get propTypes() {
    return {
      compatibilityReport: Types.compatibilityReport.isRequired,
    };
  }

  render() {
    const {
      localID,
      localVersion,
      minVersion,
      runtimeID,
      runtimeVersion,
      status,
    } = this.props.compatibilityReport;

    if (status === COMPATIBILITY_STATUS.COMPATIBLE) {
      return null;
    }

    let localizationId, statusClassName;
    switch (status) {
      case COMPATIBILITY_STATUS.TOO_OLD:
        statusClassName = "qa-compatibility-warning-too-old";
        localizationId = "about-debugging-browser-version-too-old";
        break;
      case COMPATIBILITY_STATUS.TOO_RECENT:
        statusClassName = "qa-compatibility-warning-too-recent";
        localizationId = "about-debugging-browser-version-too-recent";
        break;
      case COMPATIBILITY_STATUS.TOO_OLD_67_DEBUGGER:
        statusClassName = "qa-compatibility-warning-too-old-67-debugger";
        localizationId = "about-debugging-browser-version-too-old-67-debugger";
        break;
    }

    return Message(
      {
        level: MESSAGE_LEVEL.WARNING,
        isCloseable: true,
      },
      Localized(
        {
          id: localizationId,
          a: dom.a({
            href: TROUBLESHOOTING_URL,
            target: "_blank",
          }),
          $localID: localID,
          $localVersion: localVersion,
          $minVersion: minVersion,
          $runtimeID: runtimeID,
          $runtimeVersion: runtimeVersion,
        },
        dom.p(
          {
            className: `qa-compatibility-warning ${statusClassName}`,
          },
          localizationId
        )
      )
    );
  }
}

module.exports = CompatibilityWarning;
