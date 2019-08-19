/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { PluralForm } = require("devtools/shared/plural-form");
const { l10n } = require("devtools/client/webconsole/utils/messages");
const messageRepeatsTooltip = l10n.getStr("messageRepeats.tooltip2");

MessageRepeat.displayName = "MessageRepeat";

MessageRepeat.propTypes = {
  repeat: PropTypes.number.isRequired,
};

function MessageRepeat(props) {
  const { repeat } = props;
  return dom.span(
    {
      className: "message-repeats",
      title: PluralForm.get(repeat, messageRepeatsTooltip).replace(
        "#1",
        repeat
      ),
    },
    repeat
  );
}

module.exports = MessageRepeat;
