/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// @TODO Load the actual strings from webconsole.properties instead.
class L10n {
  getStr(str) {
    switch (str) {
      case "level.error":
        return "Error";
      case "consoleCleared":
        return "Console was cleared.";
      case "webConsoleXhrIndicator":
        return "XHR";
      case "webConsoleMoreInfoLabel":
        return "Learn More";
    }
    return str;
  }

  getFormatStr(str) {
    return this.getStr(str);
  }

  timestampString(milliseconds) {
    let d = new Date(milliseconds ? milliseconds : null);
    let hours = d.getHours(), minutes = d.getMinutes();
    let seconds = d.getSeconds();
    milliseconds = d.getMilliseconds();

    // String.prototype.padStart isn't supported in node
    function padZeros(str, len) {
      str = new String(str);
      while (str.len < len) {
        str = "0" + str;
      }
      return str;
    }

    hours = padZeros(hours, 2);
    minutes = padZeros(minutes, 2);
    seconds = padZeros(seconds, 2);
    milliseconds = padZeros(milliseconds, 3);

    return `${hours}:${minutes}:${seconds}.${milliseconds}`;
  }
}

module.exports = L10n;
