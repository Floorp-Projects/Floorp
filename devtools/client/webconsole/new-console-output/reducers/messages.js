/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const Immutable = require("devtools/client/shared/vendor/immutable");
const constants = require("devtools/client/webconsole/new-console-output/constants");

const MessageState = Immutable.Record({
  messagesById: Immutable.List(),
  messagesUiById: Immutable.List(),
});

function messages(state = new MessageState(), action) {
  const messagesById = state.messagesById;
  const messagesUiById = state.messagesUiById;

  switch (action.type) {
    case constants.MESSAGE_ADD:
      let newMessage = action.message;

      if (newMessage.type === "clear") {
        return state.set("messagesById", Immutable.List([newMessage]));
      }

      if (newMessage.allowRepeating && messagesById.size > 0) {
        let lastMessage = messagesById.last();
        if (lastMessage.repeatId === newMessage.repeatId) {
          return state.withMutations(function (record) {
            record.set("messagesById", messagesById.pop().push(
              newMessage.set("repeat", lastMessage.repeat + 1)
            ));
          });
        }
      }

      return state.withMutations(function (record) {
        record.set("messagesById", messagesById.push(newMessage));
        if (newMessage.type === "trace") {
          record.set("messagesUiById", messagesUiById.push(newMessage.id));
        }
      });
    case constants.MESSAGES_CLEAR:
      return state.set("messagesById", Immutable.List());
    case constants.MESSAGE_OPEN:
      return state.set("messagesUiById", messagesUiById.push(action.id));
    case constants.MESSAGE_CLOSE:
      let index = state.messagesUiById.indexOf(action.id);
      return state.deleteIn(["messagesUiById", index]);
  }

  return state;
}

exports.messages = messages;
