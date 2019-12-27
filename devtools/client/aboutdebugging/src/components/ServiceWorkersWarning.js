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
const DOC_URL =
  "https://developer.mozilla.org/docs/Tools/about:debugging#Service_workers_not_compatible";

class ServiceWorkersWarning extends PureComponent {
  render() {
    return Message(
      {
        level: MESSAGE_LEVEL.WARNING,
        isCloseable: true,
      },
      Localized(
        {
          id: "about-debugging-runtime-service-workers-not-compatible",
          a: dom.a({
            href: DOC_URL,
            target: "_blank",
          }),
        },
        dom.p(
          {
            className: "qa-service-workers-warning",
          },
          "about-debugging-runtime-service-workers-not-compatible"
        )
      )
    );
  }
}

module.exports = ServiceWorkersWarning;
