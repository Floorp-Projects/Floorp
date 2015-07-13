/* jshint esnext: true */
/* global DataView */

"use strict";

this.EXPORTED_SYMBOLS = ["LogParser"];

/**
 * Parse an array read from a /dev/log/ file. Format taken from
 * kernel/drivers/staging/android/logger.h and system/core/logcat/logcat.cpp
 *
 * @param array {Uint8Array} Array read from /dev/log/ file
 * @return {Array} List of log messages
 */
function parseLogArray(array) {
  let data = new DataView(array.buffer);
  let byteString = String.fromCharCode.apply(null, array);

  let logMessages = [];
  let pos = 0;

  while (pos < byteString.length) {
    // Parse a single log entry

    // Track current offset from global position
    let offset = 0;

    // Length of the entry, discarded
    let length = data.getUint32(pos + offset, true);
    offset += 4;
    // Id of the process which generated the message
    let processId = data.getUint32(pos + offset, true);
    offset += 4;
    // Id of the thread which generated the message
    let threadId = data.getUint32(pos + offset, true);
    offset += 4;
    // Seconds since epoch when this message was logged
    let seconds = data.getUint32(pos + offset, true);
    offset += 4;
    // Nanoseconds since the last second
    let nanoseconds = data.getUint32(pos + offset, true);
    offset += 4;

    // Priority in terms of the ANDROID_LOG_* constants (see below)
    // This is where the length field begins counting
    let priority = data.getUint8(pos + offset);

    // Reset pos and offset to count from here
    pos += offset;
    offset = 0;
    offset += 1;

    // Read the tag and message, represented as null-terminated c-style strings
    let tag = "";
    while (byteString[pos + offset] != "\0") {
      tag += byteString[pos + offset];
      offset ++;
    }
    offset ++;

    let message = "";
    // The kernel log driver may have cut off the null byte (logprint.c)
    while (byteString[pos + offset] != "\0" && offset < length) {
      message += byteString[pos + offset];
      offset ++;
    }

    // Un-skip the missing null terminator
    if (offset === length) {
      offset --;
    }

    offset ++;

    pos += offset;

    // Log messages are occasionally delimited by newlines, but are also
    // sometimes followed by newlines as well
    if (message.charAt(message.length - 1) === "\n") {
      message = message.substring(0, message.length - 1);
    }

    // Add an aditional time property to mimic the milliseconds since UTC
    // expected by Date
    let time = seconds * 1000.0 + nanoseconds/1000000.0;

    // Log messages with interleaved newlines are considered to be separate log
    // messages by logcat
    for (let lineMessage of message.split("\n")) {
      logMessages.push({
        processId: processId,
        threadId: threadId,
        seconds: seconds,
        nanoseconds: nanoseconds,
        time: time,
        priority: priority,
        tag: tag,
        message: lineMessage + "\n"
      });
    }
  }

  return logMessages;
}

/**
 * Get a thread-time style formatted string from time
 * @param time {Number} Milliseconds since epoch
 * @return {String} Formatted time string
 */
function getTimeString(time) {
  let date = new Date(time);
  function pad(number) {
    if ( number < 10 ) {
      return "0" + number;
    }
    return number;
  }
  return pad( date.getMonth() + 1 ) +
         "-" + pad( date.getDate() ) +
         " " + pad( date.getHours() ) +
         ":" + pad( date.getMinutes() ) +
         ":" + pad( date.getSeconds() ) +
         "." + (date.getMilliseconds() / 1000).toFixed(3).slice(2, 5);
}

/**
 * Pad a string using spaces on the left
 * @param str   {String} String to pad
 * @param width {Number} Desired string length
 */
function padLeft(str, width) {
  while (str.length < width) {
    str = " " + str;
  }
  return str;
}

/**
 * Pad a string using spaces on the right
 * @param str   {String} String to pad
 * @param width {Number} Desired string length
 */
function padRight(str, width) {
  while (str.length < width) {
    str = str + " ";
  }
  return str;
}

/** Constant values taken from system/core/liblog */
const ANDROID_LOG_UNKNOWN = 0;
const ANDROID_LOG_DEFAULT = 1;
const ANDROID_LOG_VERBOSE = 2;
const ANDROID_LOG_DEBUG   = 3;
const ANDROID_LOG_INFO    = 4;
const ANDROID_LOG_WARN    = 5;
const ANDROID_LOG_ERROR   = 6;
const ANDROID_LOG_FATAL   = 7;
const ANDROID_LOG_SILENT  = 8;

/**
 * Map a priority number to its abbreviated string equivalent
 * @param priorityNumber {Number} Log-provided priority number
 * @return {String} Priority number's abbreviation
 */
function getPriorityString(priorityNumber) {
  switch (priorityNumber) {
  case ANDROID_LOG_VERBOSE:
    return "V";
  case ANDROID_LOG_DEBUG:
    return "D";
  case ANDROID_LOG_INFO:
    return "I";
  case ANDROID_LOG_WARN:
    return "W";
  case ANDROID_LOG_ERROR:
    return "E";
  case ANDROID_LOG_FATAL:
    return "F";
  case ANDROID_LOG_SILENT:
    return "S";
  default:
    return "?";
  }
}


/**
 * Mimic the logcat "threadtime" format, generating a formatted string from a
 * log message object.
 * @param logMessage {Object} A log message from the list returned by parseLogArray
 * @return {String} threadtime formatted summary of the message
 */
function formatLogMessage(logMessage) {
  // MM-DD HH:MM:SS.ms pid tid priority tag: message
  // from system/core/liblog/logprint.c:
  return getTimeString(logMessage.time) +
         " " + padLeft(""+logMessage.processId, 5) +
         " " + padLeft(""+logMessage.threadId, 5) +
         " " + getPriorityString(logMessage.priority) +
         " " + padRight(logMessage.tag, 8) +
         ": " + logMessage.message;
}

/**
 * Convert a string to a utf-8 Uint8Array
 * @param {String} str
 * @return {Uint8Array}
 */
function textEncode(str) {
  return new TextEncoder("utf-8").encode(str);
}

/**
 * Pretty-print an array of bytes read from a log file by parsing then
 * threadtime formatting its entries.
 * @param array {Uint8Array} Array of a log file's bytes
 * @return {Uint8Array} Pretty-printed log
 */
function prettyPrintLogArray(array) {
  let logMessages = parseLogArray(array);
  return textEncode(logMessages.map(formatLogMessage).join(""));
}

/**
 * Pretty-print an array read from the list of propreties.
 * @param {Object} Object representing the properties
 * @return {Uint8Array} Human-readable string of property name: property value
 */
function prettyPrintPropertiesArray(properties) {
  let propertiesString = "";
  for(let propName in properties) {
    propertiesString += "[" + propName + "]: [" + properties[propName] + "]\n";
  }
  return textEncode(propertiesString);
}

/**
 * Pretty-print a normal array. Does nothing.
 * @param array {Uint8Array} Input array
 * @return {Uint8Array} The same array
 */
function prettyPrintArray(array) {
  return array;
}

this.LogParser = {
  parseLogArray: parseLogArray,
  prettyPrintArray: prettyPrintArray,
  prettyPrintLogArray: prettyPrintLogArray,
  prettyPrintPropertiesArray: prettyPrintPropertiesArray
};
