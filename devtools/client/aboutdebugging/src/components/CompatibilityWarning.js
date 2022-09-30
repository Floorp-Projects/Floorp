/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");

const FluentReact = require("resource://devtools/client/shared/vendor/fluent-react.js");
const Localized = createFactory(FluentReact.Localized);

const Message = createFactory(
  require("resource://devtools/client/aboutdebugging/src/components/shared/Message.js")
);

const {
  MESSAGE_LEVEL,
} = require("resource://devtools/client/aboutdebugging/src/constants.js");
const {
  COMPATIBILITY_STATUS,
} = require("resource://devtools/client/shared/remote-debugging/version-checker.js");

const TROUBLESHOOTING_URL =
  "https://firefox-source-docs.mozilla.org/devtools-user/about_colon_debugging/";
const FENNEC_TROUBLESHOOTING_URL =
  "https://firefox-source-docs.mozilla.org/devtools-user/about_colon_debugging/index.html#connection-to-firefox-for-android-68";

const Types = require("resource://devtools/client/aboutdebugging/src/types/index.js");

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
