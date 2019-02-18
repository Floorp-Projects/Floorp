/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

const Message = createFactory(require("./shared/Message"));

const { MESSAGE_LEVEL } = require("../constants");
const { COMPATIBILITY_STATUS } = require("devtools/client/shared/remote-debugging/version-checker");

const TROUBLESHOOTING_URL = "https://developer.mozilla.org/docs/Tools/WebIDE/Troubleshooting";

const Types = require("../types/index");

class CompatibilityWarning extends PureComponent {
  static get propTypes() {
    return {
      compatibilityReport: Types.compatibilityReport.isRequired,
    };
  }

  render() {
    const { localID, localVersion, minVersion, runtimeID, runtimeVersion, status } =
      this.props.compatibilityReport;

    if (status === COMPATIBILITY_STATUS.COMPATIBLE) {
      return null;
    }

    const isTooOld = status === COMPATIBILITY_STATUS.TOO_OLD;
    const localizationId = isTooOld ?
      "about-debugging-runtime-version-too-old" :
      "about-debugging-runtime-version-too-recent";

    const statusClassName = isTooOld ?
      "js-compatibility-warning-too-old" :
      "js-compatibility-warning-too-recent";

    return Message(
      {
        level: MESSAGE_LEVEL.WARNING,
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
            className: `js-compatibility-warning ${statusClassName}`,
          },
          localizationId,
        ),
      )
    );
  }
}

module.exports = CompatibilityWarning;
