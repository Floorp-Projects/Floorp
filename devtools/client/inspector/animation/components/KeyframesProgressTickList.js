/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

const KeyframesProgressTickItem = createFactory(require("./KeyframesProgressTickItem"));
const { getFormatStr } = require("../utils/l10n");

class KeyframesProgressTickList extends PureComponent {
  render() {
    return dom.div(
      {
        className: "keyframes-progress-tick-list"
      },
      [0, 50, 100].map(progress => {
        const direction = progress === 100 ? "right" : "left";
        const position = progress === 100 ? 0 : progress;
        const progressTickLabel =
          getFormatStr("detail.propertiesHeader.percentage", progress);

        return KeyframesProgressTickItem(
          {
            direction,
            position,
            progressTickLabel,
          }
        );
      })
    );
  }
}

module.exports = KeyframesProgressTickList;
