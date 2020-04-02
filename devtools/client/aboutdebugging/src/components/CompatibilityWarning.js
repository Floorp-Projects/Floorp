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

const Message = createFactory(
  require("devtools/client/aboutdebugging/src/components/shared/Message")
);

const {
  MESSAGE_LEVEL,
} = require("devtools/client/aboutdebugging/src/constants");
const {
  COMPATIBILITY_STATUS,
} = require("devtools/client/shared/remote-debugging/version-checker");

const TROUBLESHOOTING_URL =
  "https://developer.mozilla.org/docs/Tools/about:debugging#Troubleshooting";
const FENNEC_TROUBLESHOOTING_URL =
  "https://developer.mozilla.org/docs/Tools/about:debugging#Connection_to_Firefox_for_Android_68";

const Types = require("devtools/client/aboutdebugging/src/types/index");

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
      case COMPATIBILITY_STATUS.TOO_OLD_FENNEC:
        statusClassName = "qa-compatibility-warning-too-old-fennec";
        localizationId = "about-debugging-browser-version-too-old-fennec";
        break;
    }

    const troubleshootingUrl =
      status === COMPATIBILITY_STATUS.TOO_OLD_FENNEC
        ? FENNEC_TROUBLESHOOTING_URL
        : TROUBLESHOOTING_URL;

    const messageLevel =
      status === COMPATIBILITY_STATUS.TOO_OLD_FENNEC
        ? MESSAGE_LEVEL.ERROR
        : MESSAGE_LEVEL.WARNING;

    return Message(
      {
        level: messageLevel,
        isCloseable: true,
      },
      Localized(
        {
          id: localizationId,
          a: dom.a({
            href: troubleshootingUrl,
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
