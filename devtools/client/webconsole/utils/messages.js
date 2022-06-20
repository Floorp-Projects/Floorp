/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const l10n = require("devtools/client/webconsole/utils/l10n");
const ResourceCommand = require("devtools/shared/commands/resource/resource-command");
const {
  isSupportedByConsoleTable,
} = require("devtools/shared/webconsole/messages");

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

function prepareMessage(resource, idGenerator) {
  if (!resource.source) {
    resource = transformResource(resource);
  }

  resource.id = idGenerator.getNextId(resource);
  return resource;
}

/**
 * Transforms a resource given its type.
 *
 * @param {Object} resource: This can be either a simple RDP packet or an object emitted
 *                           by the Resource API.
 */
function transformResource(resource) {
  switch (resource.resourceType || resource.type) {
    case ResourceCommand.TYPES.CONSOLE_MESSAGE: {
      return transformConsoleAPICallResource(resource);
    }

    case ResourceCommand.TYPES.PLATFORM_MESSAGE: {
      return transformPlatformMessageResource(resource);
    }

    case ResourceCommand.TYPES.ERROR_MESSAGE: {
      return transformPageErrorResource(resource);
    }

    case ResourceCommand.TYPES.CSS_MESSAGE: {
      return transformCSSMessageResource(resource);
    }

    case ResourceCommand.TYPES.NETWORK_EVENT: {
      return transformNetworkEventResource(resource);
    }

    case "will-navigate": {
      return transformNavigationMessagePacket(resource);
    }

    case "evaluationResult":
    default: {
      return transformEvaluationResultPacket(resource);
    }
  }
}

// eslint-disable-next-line complexity
function transformConsoleAPICallResource(consoleMessageResource) {
  const { message, targetFront } = consoleMessageResource;

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
      if (!isSupportedByConsoleTable(parameters)) {
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
    targetFront,
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
    chromeContext: message.chromeContext,
  });
}

function transformNavigationMessagePacket(packet) {
  const { url } = packet;
  return new ConsoleMessage({
    source: MESSAGE_SOURCE.CONSOLE_FRONTEND,
    type: MESSAGE_TYPE.NAVIGATION_MARKER,
    level: MESSAGE_LEVEL.LOG,
    messageText: l10n.getFormatStr("webconsole.navigated", [url]),
    timeStamp: packet.timeStamp,
    allowRepeating: false,
  });
}

function transformPlatformMessageResource(platformMessageResource) {
  const { message, timeStamp, targetFront } = platformMessageResource;
  return new ConsoleMessage({
    targetFront,
    source: MESSAGE_SOURCE.CONSOLE_API,
    type: MESSAGE_TYPE.LOG,
    level: MESSAGE_LEVEL.LOG,
    messageText: message,
    timeStamp,
    chromeContext: true,
  });
}

function transformPageErrorResource(pageErrorResource, override = {}) {
  const { pageError, targetFront } = pageErrorResource;
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

  return new ConsoleMessage(
    Object.assign(
      {
        targetFront,
        innerWindowID: pageError.innerWindowID,
        source: MESSAGE_SOURCE.JAVASCRIPT,
        type: MESSAGE_TYPE.LOG,
        level,
        category: pageError.category,
        messageText: pageError.errorMessage,
        stacktrace: pageError.stacktrace ? pageError.stacktrace : null,
        frame,
        errorMessageName: pageError.errorMessageName,
        exceptionDocURL: pageError.exceptionDocURL,
        hasException: pageError.hasException,
        parameters: pageError.hasException ? [pageError.exception] : null,
        timeStamp: pageError.timeStamp,
        notes: pageError.notes,
        private: pageError.private,
        chromeContext: pageError.chromeContext,
        isPromiseRejection: pageError.isPromiseRejection,
      },
      override
    )
  );
}

function transformCSSMessageResource(cssMessageResource) {
  return transformPageErrorResource(cssMessageResource, {
    cssSelectors: cssMessageResource.cssSelectors,
    source: MESSAGE_SOURCE.CSS,
  });
}

function transformNetworkEventResource(networkEventResource) {
  return new NetworkEventMessage(networkEventResource);
}

function transformEvaluationResultPacket(packet) {
  let {
    exceptionMessage,
    errorMessageName,
    exceptionDocURL,
    exception,
    exceptionStack,
    hasException,
    frame,
    result,
    helperResult,
    timestamp: timeStamp,
    notes,
  } = packet;

  let parameter;

  if (hasException) {
    // If we have an exception, we prefix it, and we reset the exception message, as we're
    // not going to use it.
    parameter = exception;
    exceptionMessage = null;
  } else if (helperResult?.object) {
    parameter = helperResult.object;
  } else if (helperResult?.type === "error") {
    try {
      exceptionMessage = l10n.getFormatStr(
        helperResult.message,
        helperResult.messageArgs || []
      );
    } catch (ex) {
      exceptionMessage = helperResult.message;
    }
  } else {
    parameter = result;
  }

  const level =
    typeof exceptionMessage !== "undefined" && packet.exceptionMessage !== null
      ? MESSAGE_LEVEL.ERROR
      : MESSAGE_LEVEL.LOG;

  return new ConsoleMessage({
    source: MESSAGE_SOURCE.JAVASCRIPT,
    type: MESSAGE_TYPE.RESULT,
    helperType: helperResult ? helperResult.type : null,
    level,
    messageText: exceptionMessage,
    hasException,
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

/**
 * Return if passed messages are similar and can thus be "repeated".
 * ⚠ This function is on a hot path, called for (almost) every message being sent by
 * the server. This should be kept as fast as possible.
 *
 * @param {Message} message1
 * @param {Message} message2
 * @returns {Boolean}
 */
// eslint-disable-next-line complexity
function areMessagesSimilar(message1, message2) {
  if (!message1 || !message2) {
    return false;
  }

  if (!areMessagesParametersSimilar(message1, message2)) {
    return false;
  }

  if (!areMessagesStacktracesSimilar(message1, message2)) {
    return false;
  }

  if (
    !message1.allowRepeating ||
    !message2.allowRepeating ||
    message1.type !== message2.type ||
    message1.level !== message2.level ||
    message1.source !== message2.source ||
    message1.category !== message2.category ||
    message1.frame?.source !== message2.frame?.source ||
    message1.frame?.line !== message2.frame?.line ||
    message1.frame?.column !== message2.frame?.column ||
    message1.messageText !== message2.messageText ||
    message1.private !== message2.private ||
    message1.errorMessageName !== message2.errorMessageName ||
    message1.hasException !== message2.hasException ||
    message1.isPromiseRejection !== message2.isPromiseRejection ||
    message1.userProvidedStyles?.length !==
      message2.userProvidedStyles?.length ||
    `${message1.userProvidedStyles}` !== `${message2.userProvidedStyles}`
  ) {
    return false;
  }

  return true;
}

/**
 * Return if passed messages parameters are similar
 * ⚠ This function is on a hot path, called for (almost) every message being sent by
 * the server. This should be kept as fast as possible.
 *
 * @param {Message} message1
 * @param {Message} message2
 * @returns {Boolean}
 */
function areMessagesParametersSimilar(message1, message2) {
  const message1ParamsLength = message1.parameters?.length;
  if (message1ParamsLength !== message2.parameters?.length) {
    return false;
  }

  if (!message1ParamsLength) {
    return true;
  }

  for (let i = 0; i < message1ParamsLength; i++) {
    const message1Parameter = message1.parameters[i];
    const message2Parameter = message2.parameters[i];
    // exceptions have a grip, but we want to consider 2 messages similar as long as
    // they refer to the same error.
    if (
      message1.hasException &&
      message2.hasException &&
      message1Parameter._grip?.class == message2Parameter._grip?.class &&
      message1Parameter._grip?.preview?.message ==
        message2Parameter._grip?.preview?.message &&
      message1Parameter._grip?.preview?.stack ==
        message2Parameter._grip?.preview?.stack
    ) {
      continue;
    }

    // For object references (grips), that are not exceptions, we don't want to consider
    // messages to be the same as we only have a preview of what they look like, and not
    // some kind of property that would give us the state of a given instance at a given
    // time.
    if (message1Parameter._grip || message2Parameter._grip) {
      return false;
    }

    if (message1Parameter.type !== message2Parameter.type) {
      return false;
    }

    if (message1Parameter.type) {
      if (message1Parameter.text !== message2Parameter.text) {
        return false;
      }
    } else if (message1Parameter !== message2Parameter) {
      return false;
    }
  }
  return true;
}

/**
 * Return if passed messages stacktraces are similar
 *
 * @param {Message} message1
 * @param {Message} message2
 * @returns {Boolean}
 */
function areMessagesStacktracesSimilar(message1, message2) {
  const message1StackLength = message1.stacktrace?.length;
  if (message1StackLength !== message2.stacktrace?.length) {
    return false;
  }

  if (!message1StackLength) {
    return true;
  }

  for (let i = 0; i < message1StackLength; i++) {
    const message1Frame = message1.stacktrace[i];
    const message2Frame = message2.stacktrace[i];

    if (message1Frame.filename !== message2Frame.filename) {
      return false;
    }

    if (message1Frame.columnNumber !== message2Frame.columnNumber) {
      return false;
    }

    if (message1Frame.lineNumber !== message2Frame.lineNumber) {
      return false;
    }
  }
  return true;
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
    allowRepeating: false,
    level: MESSAGE_LEVEL.WARN,
    source: MESSAGE_SOURCE.CONSOLE_FRONTEND,
    type,
    messageText: getWarningGroupLabel(firstMessage),
    timeStamp: firstMessage.timeStamp,
    innerWindowID: firstMessage.innerWindowID,
  });
}

function createSimpleTableMessage(columns, items, timeStamp) {
  return new ConsoleMessage({
    allowRepeating: false,
    level: MESSAGE_LEVEL.LOG,
    source: MESSAGE_SOURCE.CONSOLE_FRONTEND,
    type: MESSAGE_TYPE.SIMPLE_TABLE,
    columns,
    items,
    timeStamp: timeStamp,
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
    isStorageIsolationMessage(firstMessage) ||
    isTrackingProtectionMessage(firstMessage)
  ) {
    return replaceURL(firstMessage.messageText, "<URL>");
  }

  if (isCookieSameSiteMessage(firstMessage)) {
    if (Services.prefs.getBoolPref("network.cookie.sameSite.laxByDefault")) {
      return l10n.getStr("webconsole.group.cookieSameSiteLaxByDefaultEnabled2");
    }
    return l10n.getStr("webconsole.group.cookieSameSiteLaxByDefaultDisabled2");
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
  if (
    message.level !== MESSAGE_LEVEL.WARN &&
    // CookieSameSite messages are not warnings but infos
    message.level !== MESSAGE_LEVEL.INFO
  ) {
    return null;
  }

  if (isContentBlockingMessage(message)) {
    return MESSAGE_TYPE.CONTENT_BLOCKING_GROUP;
  }

  if (isStorageIsolationMessage(message)) {
    return MESSAGE_TYPE.STORAGE_ISOLATION_GROUP;
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
    message.type === MESSAGE_TYPE.STORAGE_ISOLATION_GROUP ||
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
 * Returns true if the message is a storage isolation message.
 * @param {ConsoleMessage} message
 * @returns {Boolean}
 */
function isStorageIsolationMessage(message) {
  const { category } = message;
  return category == "cookiePartitionedForeign";
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

function isMessageNetworkError(message) {
  return (
    message.source === MESSAGE_SOURCE.NETWORK &&
    message?.status &&
    message?.status.toString().match(/^[4,5]\d\d$/)
  );
}

module.exports = {
  areMessagesSimilar,
  createWarningGroupMessage,
  createSimpleTableMessage,
  getDescriptorValue,
  getNaturalOrder,
  getParentWarningGroupMessageId,
  getWarningGroupType,
  isContentBlockingMessage,
  isGroupType,
  isMessageNetworkError,
  isPacketPrivate,
  isWarningGroup,
  l10n,
  prepareMessage,
};
