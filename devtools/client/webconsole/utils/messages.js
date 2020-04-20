/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const l10n = require("devtools/client/webconsole/utils/l10n");
const {
  getUrlDetails,
} = require("devtools/client/netmonitor/src/utils/request-utils");

// URL Regex, common idioms:
//
// Lead-in (URL):
// (                     Capture because we need to know if there was a lead-in
//                       character so we can include it as part of the text
//                       preceding the match. We lack look-behind matching.
//  ^|                   The URL can start at the beginning of the string.
//  [\s(,;'"`“]          Or whitespace or some punctuation that does not imply
//                       a context which would preclude a URL.
// )
//
// We do not need a trailing look-ahead because our regex's will terminate
// because they run out of characters they can eat.

// What we do not attempt to have the regexp do:
// - Avoid trailing '.' and ')' characters.  We let our greedy match absorb
//   these, but have a separate regex for extra characters to leave off at the
//   end.
//
// The Regex (apart from lead-in/lead-out):
// (                     Begin capture of the URL
//  (?:                  (potential detect beginnings)
//   https?:\/\/|        Start with "http" or "https"
//   www\d{0,3}[.][a-z0-9.\-]{2,249}|
//                      Start with "www", up to 3 numbers, then "." then
//                       something that looks domain-namey.  We differ from the
//                       next case in that we do not constrain the top-level
//                       domain as tightly and do not require a trailing path
//                       indicator of "/".  This is IDN root compatible.
//   [a-z0-9.\-]{2,250}[.][a-z]{2,4}\/
//                       Detect a non-www domain, but requiring a trailing "/"
//                       to indicate a path.  This only detects IDN domains
//                       with a non-IDN root.  This is reasonable in cases where
//                       there is no explicit http/https start us out, but
//                       unreasonable where there is.  Our real fix is the bug
//                       to port the Thunderbird/gecko linkification logic.
//
//                       Domain names can be up to 253 characters long, and are
//                       limited to a-zA-Z0-9 and '-'.  The roots don't have
//                       hyphens unless they are IDN roots.  Root zones can be
//                       found here: http://www.iana.org/domains/root/db
//  )
//  [-\w.!~*'();,/?:@&=+$#%]*
//                       path onwards. We allow the set of characters that
//                       encodeURI does not escape plus the result of escaping
//                       (so also '%')
// )
// eslint-disable-next-line max-len
const urlRegex = /(^|[\s(,;'"`“])((?:https?:\/\/|www\d{0,3}[.][a-z0-9.\-]{2,249}|[a-z0-9.\-]{2,250}[.][a-z]{2,4}\/)[-\w.!~*'();,/?:@&=+$#%]*)/im;

// Set of terminators that are likely to have been part of the context rather
// than part of the URL and so should be uneaten. This is '(', ',', ';', plus
// quotes and question end-ing punctuation and the potential permutations with
// parentheses (english-specific).
const uneatLastUrlCharsRegex = /(?:[),;.!?`'"]|[.!?]\)|\)[.!?])$/;

const {
  MESSAGE_SOURCE,
  MESSAGE_TYPE,
  MESSAGE_LEVEL,
} = require("devtools/client/webconsole/constants");
const {
  ConsoleMessage,
  NetworkEventMessage,
} = require("devtools/client/webconsole/types");

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

    case "will-navigate": {
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

// eslint-disable-next-line complexity
function transformConsoleAPICallPacket(packet) {
  const { message } = packet;

  let parameters = message.arguments;
  let type = message.level;
  let level = getLevelFromType(type);
  let messageText = null;
  const { timer } = message;

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
      const { counter } = message;

      if (!counter) {
        // We don't show anything if we don't have counter data.
        type = MESSAGE_TYPE.NULL_MESSAGE;
      } else if (counter.error) {
        messageText = l10n.getFormatStr(counter.error, [counter.label]);
        level = MESSAGE_LEVEL.WARN;
        parameters = null;
      } else {
        const label = counter.label
          ? counter.label
          : l10n.getStr("noCounterLabel");
        messageText = `${label}: ${counter.count}`;
        parameters = null;
      }
      break;
    case "timeStamp":
      type = MESSAGE_TYPE.NULL_MESSAGE;
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
          messageText = l10n.getFormatStr("console.timeEnd", [
            timer.name,
            duration,
          ]);
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
        "Object",
        "Map",
        "Set",
        "WeakMap",
        "WeakSet",
      ].concat(getArrayTypeNames());

      if (
        !Array.isArray(parameters) ||
        parameters.length === 0 ||
        !parameters[0] ||
        !parameters[0].getGrip ||
        !supportedClasses.includes(parameters[0].getGrip().class)
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

  const frame = message.filename
    ? {
        source: message.filename,
        sourceId: message.sourceId,
        line: message.lineNumber,
        column: message.columnNumber,
      }
    : null;

  if (frame && (type === "logPointError" || type === "logPoint")) {
    frame.options = { logPoint: true };
  }

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
    logpointId: message.logpointId,
    chromeContext: message.chromeContext,
  });
}

function transformNavigationMessagePacket(packet) {
  const { url } = packet;
  return new ConsoleMessage({
    source: MESSAGE_SOURCE.CONSOLE_API,
    type: MESSAGE_TYPE.NAVIGATION_MARKER,
    level: MESSAGE_LEVEL.LOG,
    messageText: l10n.getFormatStr("webconsole.navigated", [url]),
    timeStamp: packet.timeStamp,
    allowRepeating: false,
  });
}

function transformLogMessagePacket(packet) {
  const { message, timeStamp } = packet;

  return new ConsoleMessage({
    source: MESSAGE_SOURCE.CONSOLE_API,
    type: MESSAGE_TYPE.LOG,
    level: MESSAGE_LEVEL.LOG,
    messageText: message,
    timeStamp,
    private: message.private,
    chromeContext: message.chromeContext,
  });
}

function transformPageErrorPacket(packet) {
  const { pageError } = packet;
  let level = MESSAGE_LEVEL.ERROR;
  if (pageError.warning) {
    level = MESSAGE_LEVEL.WARN;
  } else if (pageError.info) {
    level = MESSAGE_LEVEL.INFO;
  }

  const frame = pageError.sourceName
    ? {
        source: pageError.sourceName,
        sourceId: pageError.sourceId,
        line: pageError.lineNumber,
        column: pageError.columnNumber,
      }
    : null;

  const matchesCSS = pageError.category == "CSS Parser";
  const messageSource = matchesCSS
    ? MESSAGE_SOURCE.CSS
    : MESSAGE_SOURCE.JAVASCRIPT;
  return new ConsoleMessage({
    innerWindowID: pageError.innerWindowID,
    source: messageSource,
    type: MESSAGE_TYPE.LOG,
    level,
    category: pageError.category,
    messageText: pageError.errorMessage,
    stacktrace: pageError.stacktrace ? pageError.stacktrace : null,
    frame,
    errorMessageName: pageError.errorMessageName,
    exceptionDocURL: pageError.exceptionDocURL,
    timeStamp: pageError.timeStamp,
    notes: pageError.notes,
    private: pageError.private,
    chromeContext: pageError.chromeContext,
    cssSelectors: pageError.cssSelectors,
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
    securityState: networkEvent.securityState,
    chromeContext: networkEvent.chromeContext,
  });
}

function transformEvaluationResultPacket(packet) {
  let {
    exceptionMessage,
    errorMessageName,
    exceptionDocURL,
    exception,
    exceptionStack,
    frame,
    result,
    helperResult,
    timestamp: timeStamp,
    notes,
  } = packet;

  const parameter =
    helperResult && helperResult.object ? helperResult.object : result;

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

  const level =
    typeof exceptionMessage !== "undefined" && exceptionMessage !== null
      ? MESSAGE_LEVEL.ERROR
      : MESSAGE_LEVEL.LOG;

  return new ConsoleMessage({
    source: MESSAGE_SOURCE.JAVASCRIPT,
    type: MESSAGE_TYPE.RESULT,
    helperType: helperResult ? helperResult.type : null,
    level,
    messageText: exceptionMessage,
    parameters: [parameter],
    errorMessageName,
    exceptionDocURL,
    stacktrace: exceptionStack,
    frame,
    timeStamp,
    notes,
    private: packet.private,
    allowRepeating: false,
  });
}

// Helpers
function getRepeatId(message) {
  return JSON.stringify(
    {
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
      stacktrace: message.stacktrace,
    },
    function(_, value) {
      if (typeof value === "bigint") {
        return value.toString() + "n";
      }

      if (value && value._grip) {
        return value._grip;
      }

      return value;
    }
  );
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
  } else if (packet._type === "NetworkEvent") {
    convertPacket.networkEvent = packet;
    convertPacket.type = "networkEvent";
  } else if (packet._type === "LogMessage") {
    convertPacket = {
      ...packet,
      type: "logMessage",
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
    logPointError: levels.LEVEL_ERROR,
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
    MESSAGE_TYPE.START_GROUP_COLLAPSED,
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

function createWarningGroupMessage(id, type, firstMessage) {
  return new ConsoleMessage({
    id,
    level: MESSAGE_LEVEL.WARN,
    source: MESSAGE_SOURCE.CONSOLE_FRONTEND,
    type,
    messageText: getWarningGroupLabel(firstMessage),
    timeStamp: firstMessage.timeStamp,
    innerWindowID: firstMessage.innerWindowID,
  });
}

/**
 * Given the a regular warning message, compute the label of the warning group the message
 * could be in.
 * For example, if the message text is:
 * The resource at “http://evil.com” was blocked because content blocking is enabled
 *
 * it may be turned into
 *
 * The resource at “<URL>” was blocked because content blocking is enabled
 *
 * @param {ConsoleMessage} firstMessage
 * @returns {String} The computed label
 */
function getWarningGroupLabel(firstMessage) {
  if (
    isContentBlockingMessage(firstMessage) ||
    isTrackingProtectionMessage(firstMessage)
  ) {
    return replaceURL(firstMessage.messageText, "<URL>");
  }

  if (isCookieSameSiteMessage(firstMessage)) {
    if (Services.prefs.getBoolPref("network.cookie.sameSite.laxByDefault")) {
      return l10n.getStr("webconsole.group.cookieSameSiteLaxByDefaultEnabled");
    }
    return l10n.getStr("webconsole.group.cookieSameSiteLaxByDefaultDisabled");
  }

  return "";
}

/**
 * Replace any URL in the provided text by the provided replacement text, or an empty
 * string.
 *
 * @param {String} text
 * @param {String} replacementText
 * @returns {String}
 */
function replaceURL(text, replacementText = "") {
  let result = "";
  let currentIndex = 0;
  let contentStart;
  while (true) {
    const url = urlRegex.exec(text);
    // Pick the regexp with the earlier content; index will always be zero.
    if (!url) {
      break;
    }
    contentStart = url.index + url[1].length;
    if (contentStart > 0) {
      const nonUrlText = text.substring(0, contentStart);
      result += nonUrlText;
    }

    // There are some final characters for a URL that are much more likely
    // to have been part of the enclosing text rather than the end of the
    // URL.
    let useUrl = url[2];
    const uneat = uneatLastUrlCharsRegex.exec(useUrl);
    if (uneat) {
      useUrl = useUrl.substring(0, uneat.index);
    }

    if (useUrl) {
      result += replacementText;
    }

    currentIndex = currentIndex + contentStart;

    currentIndex = currentIndex + useUrl.length;
    text = text.substring(url.index + url[1].length + useUrl.length);
  }

  return result + text;
}

/**
 * Get the warningGroup type in which the message could be in.
 * @param {ConsoleMessage} message
 * @returns {String|null} null if the message can't be part of a warningGroup.
 */
function getWarningGroupType(message) {
  if (isContentBlockingMessage(message)) {
    return MESSAGE_TYPE.CONTENT_BLOCKING_GROUP;
  }

  if (isTrackingProtectionMessage(message)) {
    return MESSAGE_TYPE.TRACKING_PROTECTION_GROUP;
  }

  if (isCookieSameSiteMessage(message)) {
    return MESSAGE_TYPE.COOKIE_SAMESITE_GROUP;
  }

  return null;
}

/**
 * Returns a computed id given a message
 *
 * @param {ConsoleMessage} type: the message type, from MESSAGE_TYPE.
 * @param {Integer} innerWindowID: the message innerWindowID.
 * @returns {String}
 */
function getParentWarningGroupMessageId(message) {
  const warningGroupType = getWarningGroupType(message);
  if (!warningGroupType) {
    return null;
  }

  return `${warningGroupType}-${message.innerWindowID}`;
}

/**
 * Returns true if the message is a warningGroup message (i.e. the "Header").
 * @param {ConsoleMessage} message
 * @returns {Boolean}
 */
function isWarningGroup(message) {
  return (
    message.type === MESSAGE_TYPE.CONTENT_BLOCKING_GROUP ||
    message.type === MESSAGE_TYPE.TRACKING_PROTECTION_GROUP ||
    message.type === MESSAGE_TYPE.COOKIE_SAMESITE_GROUP ||
    message.type === MESSAGE_TYPE.CORS_GROUP ||
    message.type === MESSAGE_TYPE.CSP_GROUP
  );
}

/**
 * Returns true if the message is a content blocking message.
 * @param {ConsoleMessage} message
 * @returns {Boolean}
 */
function isContentBlockingMessage(message) {
  const { category } = message;
  return (
    category == "cookieBlockedPermission" ||
    category == "cookieBlockedTracker" ||
    category == "cookieBlockedAll" ||
    category == "cookieBlockedForeign"
  );
}

/**
 * Returns true if the message is a tracking protection message.
 * @param {ConsoleMessage} message
 * @returns {Boolean}
 */
function isTrackingProtectionMessage(message) {
  const { category } = message;
  return category == "Tracking Protection";
}

/**
 * Returns true if the message is a cookie message.
 * @param {ConsoleMessage} message
 * @returns {Boolean}
 */
function isCookieSameSiteMessage(message) {
  const { category } = message;
  return category == "cookieSameSite";
}

function getArrayTypeNames() {
  return [
    "Array",
    "Int8Array",
    "Uint8Array",
    "Int16Array",
    "Uint16Array",
    "Int32Array",
    "Uint32Array",
    "Float32Array",
    "Float64Array",
    "Uint8ClampedArray",
    "BigInt64Array",
    "BigUint64Array",
  ];
}

function getDescriptorValue(descriptor) {
  if (!descriptor) {
    return descriptor;
  }

  if (Object.prototype.hasOwnProperty.call(descriptor, "safeGetterValues")) {
    return descriptor.safeGetterValues;
  }

  if (Object.prototype.hasOwnProperty.call(descriptor, "getterValue")) {
    return descriptor.getterValue;
  }

  if (Object.prototype.hasOwnProperty.call(descriptor, "value")) {
    return descriptor.value;
  }
  return descriptor;
}

function getNaturalOrder(messageA, messageB) {
  const aFirst = -1;
  const bFirst = 1;

  // It can happen that messages are emitted in the same microsecond, making their
  // timestamp similar. In such case, we rely on which message came first through
  // the console API service, checking their id, except for expression result, which we'll
  // always insert after because console API messages emitted from the expression need to
  // be rendered before.
  if (messageA.timeStamp === messageB.timeStamp) {
    if (messageA.type === "result") {
      return bFirst;
    }

    if (messageB.type === "result") {
      return aFirst;
    }

    if (
      !Number.isNaN(parseInt(messageA.id, 10)) &&
      !Number.isNaN(parseInt(messageB.id, 10))
    ) {
      return parseInt(messageA.id, 10) < parseInt(messageB.id, 10)
        ? aFirst
        : bFirst;
    }
  }
  return messageA.timeStamp < messageB.timeStamp ? aFirst : bFirst;
}

module.exports = {
  createWarningGroupMessage,
  getArrayTypeNames,
  getDescriptorValue,
  getInitialMessageCountForViewport,
  getNaturalOrder,
  getParentWarningGroupMessageId,
  getWarningGroupType,
  isContentBlockingMessage,
  isGroupType,
  isPacketPrivate,
  isWarningGroup,
  l10n,
  prepareMessage,
  // Export for use in testing.
  getRepeatId,
};
