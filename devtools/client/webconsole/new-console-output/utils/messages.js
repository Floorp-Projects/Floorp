/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let l10n;
try {
  const WebConsoleUtils = require("devtools/shared/webconsole/utils").Utils;
  const STRINGS_URI = "chrome://devtools/locale/webconsole.properties";
  l10n = new WebConsoleUtils.L10n(STRINGS_URI);
} catch (e) {
  l10n = require("devtools/client/webconsole/new-console-output/test/fixtures/l10n");
}

const {
  MESSAGE_SOURCE,
  MESSAGE_TYPE,
  MESSAGE_LEVEL,
  // Legacy
  CATEGORY_JS,
  CATEGORY_OUTPUT,
  CATEGORY_WEBDEV,
  LEVELS,
  SEVERITY_LOG,
} = require("../constants");
const { ConsoleMessage } = require("../types");

function prepareMessage(packet, idGenerator) {
  // This packet is already in the expected packet structure. Simply return.
  if (!packet.source) {
    packet = transformPacket(packet);
  }

  if (packet.allowRepeating) {
    packet = packet.set("repeatId", getRepeatId(packet));
  }
  return packet.set("id", idGenerator.getNextId());
}

/**
 * Transforms a packet from Firefox RDP structure to Chrome RDP structure.
 */
function transformPacket(packet) {
  if (packet._type) {
    packet = convertCachedPacket(packet);
  }

  switch (packet.type) {
    case "consoleAPICall": {
      let { message } = packet;

      let parameters = message.arguments;
      let type = message.level;
      let level = LEVELS[type] || MESSAGE_TYPE.LOG;
      let messageText = null;

      // Special per-type conversion.
      switch (type) {
        case "clear":
          // We show a message to users when calls console.clear() is called.
          parameters = [l10n.getStr("consoleCleared")];
          break;
        case "count":
          // Chrome RDP doesn't have a special type for count.
          type = MESSAGE_TYPE.LOG;
          level = MESSAGE_LEVEL.DEBUG;
          let {counter} = message;
          let label = counter.label ? counter.label : l10n.getStr("noCounterLabel");
          messageText = `${label}: ${counter.count}`;
          parameters = null;
          break;
      }

      return new ConsoleMessage({
        source: MESSAGE_SOURCE.CONSOLE_API,
        type,
        level,
        parameters,
        messageText,
        category: CATEGORY_WEBDEV,
        severity: level,
      });
    }

    case "pageError": {
      let { pageError } = packet;
      let level = MESSAGE_LEVEL.ERROR;
      if (pageError.warning || pageError.strict) {
        level = MESSAGE_LEVEL.WARN;
      } else if (pageError.info) {
        level = MESSAGE_LEVEL.INFO;
      }

      return new ConsoleMessage({
        source: MESSAGE_SOURCE.JAVASCRIPT,
        type: MESSAGE_TYPE.LOG,
        messageText: pageError.errorMessage,
        category: CATEGORY_JS,
        severity: level,
      });
    }

    case "evaluationResult":
    default: {
      let { result } = packet;

      return new ConsoleMessage({
        source: MESSAGE_SOURCE.JAVASCRIPT,
        type: MESSAGE_TYPE.RESULT,
        level: MESSAGE_LEVEL.LOG,
        parameters: result,
        category: CATEGORY_OUTPUT,
        severity: SEVERITY_LOG,
      });
    }
  }
}

// Helpers
function getRepeatId(message) {
  message = message.toJS();
  delete message.repeat;
  return JSON.stringify(message);
}

function convertCachedPacket(packet) {
  // The devtools server provides cached message packets in a different shape
  // from those of consoleApiCalls, so we prepare them for preparation here.
  let convertPacket = {};
  if (packet._type === "ConsoleAPI") {
    convertPacket.message = packet;
    convertPacket.type = "consoleAPICall";
  } else if (packet._type === "PageError") {
    convertPacket.pageError = packet;
    convertPacket.type = "pageError";
  } else {
    throw new Error("Unexpected packet type");
  }
  return convertPacket;
}

exports.prepareMessage = prepareMessage;
// Export for use in testing.
exports.getRepeatId = getRepeatId;

exports.l10n = l10n;
