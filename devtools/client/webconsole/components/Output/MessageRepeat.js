/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const { PluralForm } = require("resource://devtools/shared/plural-form.js");
const {
  l10n,
} = require("resource://devtools/client/webconsole/utils/messages.js");
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
