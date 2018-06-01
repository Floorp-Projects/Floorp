/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const l10n = require("devtools/client/webconsole/webconsole-l10n");
const { getUrlDetails } = require("devtools/client/netmonitor/src/utils/request-utils");

const {
  MESSAGE_SOURCE,
  MESSAGE_TYPE,
  MESSAGE_LEVEL,
} = require("../constants");
const {
  ConsoleMessage,
  NetworkEventMessage,
} = require("../types");

function prepareMessage(packet, idGenerator) {
  if (!packet.source) {
    packet = transformPacket(packet);
  }

  if (packet.allowRepeating) {
    packet.repeatId = getRepeatId(packet);
  }
  packet.id = idGenerator.getNextId(packet);
  return packet;
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
      return transformConsoleAPICallPacket(packet);
    }

    case "navigationMessage": {
      return transformNavigationMessagePacket(packet);
    }

    case "logMessage": {
      return transformLogMessagePacket(packet);
    }

    case "pageError": {
      return transformPageErrorPacket(packet);
    }

    case "networkEvent": {
      return transformNetworkEventPacket(packet);
    }

    case "evaluationResult":
    default: {
      return transformEvaluationResultPacket(packet);
    }
  }
}

function transformConsoleAPICallPacket(packet) {
  const { message } = packet;

  let parameters = message.arguments;
  let type = message.level;
  let level = getLevelFromType(type);
  let messageText = null;
  const timer = message.timer;

  // Special per-type conversion.
  switch (type) {
    case "clear":
      // We show a message to users when calls console.clear() is called.
      parameters = [l10n.getStr("consoleCleared")];
      break;
    case "count":
    case "countReset":
      // Chrome RDP doesn't have a special type for count.
      type = MESSAGE_TYPE.LOG;
      const {counter} = message;

      if (!counter) {
        // We don't show anything if we don't have counter data.
        type = MESSAGE_TYPE.NULL_MESSAGE;
      } else if (counter.error) {
        messageText = l10n.getFormatStr(counter.error, [counter.label]);
        level = MESSAGE_LEVEL.WARN;
        parameters = null;
      } else {
        const label = counter.label ? counter.label : l10n.getStr("noCounterLabel");
        messageText = `${label}: ${counter.count}`;
        parameters = null;
      }
      break;
    case "time":
      parameters = null;
      if (timer && timer.error) {
        messageText = l10n.getFormatStr(timer.error, [timer.name]);
        level = MESSAGE_LEVEL.WARN;
      } else {
        // We don't show anything for console.time calls to match Chrome's behaviour.
        type = MESSAGE_TYPE.NULL_MESSAGE;
      }
      break;
    case "timeLog":
    case "timeEnd":
      if (timer && timer.error) {
        parameters = null;
        messageText = l10n.getFormatStr(timer.error, [timer.name]);
        level = MESSAGE_LEVEL.WARN;
      } else if (timer) {
        // We show the duration to users when calls console.timeLog/timeEnd is called,
        // if corresponding console.time() was called before.
        const duration = Math.round(timer.duration * 100) / 100;
        if (type === "timeEnd") {
          messageText = l10n.getFormatStr("timeEnd", [timer.name, duration]);
          parameters = null;
        } else if (type === "timeLog") {
          const [, ...rest] = parameters;
          parameters = [
            l10n.getFormatStr("timeLog", [timer.name, duration]),
            ...rest,
          ];
        }
      } else {
        // If the `timer` property does not exists, we don't output anything.
        type = MESSAGE_TYPE.NULL_MESSAGE;
      }
      break;
    case "table":
      const supportedClasses = [
        "Array", "Object", "Map", "Set", "WeakMap", "WeakSet"];
      if (
        !Array.isArray(parameters) ||
        parameters.length === 0 ||
        !supportedClasses.includes(parameters[0].class)
      ) {
        // If the class of the first parameter is not supported,
        // we handle the call as a simple console.log
        type = "log";
      }
      break;
    case "group":
      type = MESSAGE_TYPE.START_GROUP;
      if (parameters.length === 0) {
        parameters = [l10n.getStr("noGroupLabel")];
      }
      break;
    case "groupCollapsed":
      type = MESSAGE_TYPE.START_GROUP_COLLAPSED;
      if (parameters.length === 0) {
        parameters = [l10n.getStr("noGroupLabel")];
      }
      break;
    case "groupEnd":
      type = MESSAGE_TYPE.END_GROUP;
      parameters = null;
      break;
    case "dirxml":
      // Handle console.dirxml calls as simple console.log
      type = "log";
      break;
  }

  const frame = message.filename ? {
    source: message.filename,
    line: message.lineNumber,
    column: message.columnNumber,
  } : null;

  return new ConsoleMessage({
    source: MESSAGE_SOURCE.CONSOLE_API,
    type,
    level,
    parameters,
    messageText,
    stacktrace: message.stacktrace ? message.stacktrace : null,
    frame,
    timeStamp: message.timeStamp,
    userProvidedStyles: message.styles,
    prefix: message.prefix,
    private: message.private,
  });
}

function transformNavigationMessagePacket(packet) {
  const { message } = packet;
  return new ConsoleMessage({
    source: MESSAGE_SOURCE.CONSOLE_API,
    type: MESSAGE_TYPE.LOG,
    level: MESSAGE_LEVEL.LOG,
    messageText: l10n.getFormatStr("webconsole.navigated", [message.url]),
    timeStamp: message.timeStamp,
    private: message.private,
  });
}

function transformLogMessagePacket(packet) {
  const {
    message,
    timeStamp,
  } = packet;

  return new ConsoleMessage({
    source: MESSAGE_SOURCE.CONSOLE_API,
    type: MESSAGE_TYPE.LOG,
    level: MESSAGE_LEVEL.LOG,
    messageText: message,
    timeStamp,
    private: message.private,
  });
}

function transformPageErrorPacket(packet) {
  const { pageError } = packet;
  let level = MESSAGE_LEVEL.ERROR;
  if (pageError.warning || pageError.strict) {
    level = MESSAGE_LEVEL.WARN;
  } else if (pageError.info) {
    level = MESSAGE_LEVEL.INFO;
  }

  const frame = pageError.sourceName ? {
    source: pageError.sourceName,
    line: pageError.lineNumber,
    column: pageError.columnNumber
  } : null;

  const matchesCSS = /^(?:CSS|Layout)\b/.test(pageError.category);
  const messageSource = matchesCSS ? MESSAGE_SOURCE.CSS
                                  : MESSAGE_SOURCE.JAVASCRIPT;
  return new ConsoleMessage({
    source: messageSource,
    type: MESSAGE_TYPE.LOG,
    level,
    messageText: pageError.errorMessage,
    stacktrace: pageError.stacktrace ? pageError.stacktrace : null,
    frame,
    exceptionDocURL: pageError.exceptionDocURL,
    timeStamp: pageError.timeStamp,
    notes: pageError.notes,
    private: pageError.private,
  });
}

function transformNetworkEventPacket(packet) {
  const { networkEvent } = packet;

  return new NetworkEventMessage({
    actor: networkEvent.actor,
    isXHR: networkEvent.isXHR,
    request: networkEvent.request,
    response: networkEvent.response,
    timeStamp: networkEvent.timeStamp,
    totalTime: networkEvent.totalTime,
    url: networkEvent.request.url,
    urlDetails: getUrlDetails(networkEvent.request.url),
    method: networkEvent.request.method,
    updates: networkEvent.updates,
    cause: networkEvent.cause,
    private: networkEvent.private,
  });
}

function transformEvaluationResultPacket(packet) {
  let {
    exceptionMessage,
    exceptionDocURL,
    exception,
    frame,
    result,
    helperResult,
    timestamp: timeStamp,
    notes,
  } = packet;

  const parameter = helperResult && helperResult.object
    ? helperResult.object
    : result;

  if (helperResult && helperResult.type === "error") {
    try {
      exceptionMessage = l10n.getStr(helperResult.message);
    } catch (ex) {
      exceptionMessage = helperResult.message;
    }
  } else if (typeof exception === "string") {
    // Wrap thrown strings in Error objects, so `throw "foo"` outputs "Error: foo"
    exceptionMessage = new Error(exceptionMessage).toString();
  }

  const level = typeof exceptionMessage !== "undefined" && exceptionMessage !== null
    ? MESSAGE_LEVEL.ERROR
    : MESSAGE_LEVEL.LOG;

  return new ConsoleMessage({
    source: MESSAGE_SOURCE.JAVASCRIPT,
    type: MESSAGE_TYPE.RESULT,
    helperType: helperResult ? helperResult.type : null,
    level,
    messageText: exceptionMessage,
    parameters: [parameter],
    exceptionDocURL,
    frame,
    timeStamp,
    notes,
    private: packet.private,
  });
}

// Helpers
function getRepeatId(message) {
  return JSON.stringify({
    frame: message.frame,
    groupId: message.groupId,
    indent: message.indent,
    level: message.level,
    messageText: message.messageText,
    parameters: message.parameters,
    source: message.source,
    type: message.type,
    userProvidedStyles: message.userProvidedStyles,
    private: message.private,
  });
}

function convertCachedPacket(packet) {
  // The devtools server provides cached message packets in a different shape, so we
  // transform them here.
  let convertPacket = {};
  if (packet._type === "ConsoleAPI") {
    convertPacket.message = packet;
    convertPacket.type = "consoleAPICall";
  } else if (packet._type === "PageError") {
    convertPacket.pageError = packet;
    convertPacket.type = "pageError";
  } else if ("_navPayload" in packet) {
    convertPacket.type = "navigationMessage";
    convertPacket.message = packet;
  } else if (packet._type === "NetworkEvent") {
    convertPacket.networkEvent = packet;
    convertPacket.type = "networkEvent";
  } else if (packet._type === "LogMessage") {
    convertPacket = {
      ...packet,
      type: "logMessage"
    };
  } else {
    throw new Error("Unexpected packet type: " + packet._type);
  }
  return convertPacket;
}

/**
 * Maps a Firefox RDP type to its corresponding level.
 */
function getLevelFromType(type) {
  const levels = {
    LEVEL_ERROR: "error",
    LEVEL_WARNING: "warn",
    LEVEL_INFO: "info",
    LEVEL_LOG: "log",
    LEVEL_DEBUG: "debug",
  };

  // A mapping from the console API log event levels to the Web Console levels.
  const levelMap = {
    error: levels.LEVEL_ERROR,
    exception: levels.LEVEL_ERROR,
    assert: levels.LEVEL_ERROR,
    warn: levels.LEVEL_WARNING,
    info: levels.LEVEL_INFO,
    log: levels.LEVEL_LOG,
    clear: levels.LEVEL_LOG,
    trace: levels.LEVEL_LOG,
    table: levels.LEVEL_LOG,
    debug: levels.LEVEL_DEBUG,
    dir: levels.LEVEL_LOG,
    dirxml: levels.LEVEL_LOG,
    group: levels.LEVEL_LOG,
    groupCollapsed: levels.LEVEL_LOG,
    groupEnd: levels.LEVEL_LOG,
    time: levels.LEVEL_LOG,
    timeEnd: levels.LEVEL_LOG,
    count: levels.LEVEL_LOG,
  };

  return levelMap[type] || MESSAGE_TYPE.LOG;
}

function isGroupType(type) {
  return [
    MESSAGE_TYPE.START_GROUP,
    MESSAGE_TYPE.START_GROUP_COLLAPSED
  ].includes(type);
}

function getInitialMessageCountForViewport(win) {
  const minMessageHeight = 20;
  return Math.ceil(win.innerHeight / minMessageHeight);
}

function isPacketPrivate(packet) {
  return (
    packet.private === true ||
    (packet.message && packet.message.private === true) ||
    (packet.pageError && packet.pageError.private === true) ||
    (packet.networkEvent && packet.networkEvent.private === true)
  );
}

module.exports = {
  getInitialMessageCountForViewport,
  isGroupType,
  isPacketPrivate,
  l10n,
  prepareMessage,
  // Export for use in testing.
  getRepeatId,
};
