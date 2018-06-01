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
      case "webconsole.clearButton.tooltip":
        return "Clear the Web Console output";
      case "webconsole.toggleFilterButton.tooltip":
        return "Toggle filter bar";
      case "webconsole.filterInput.placeholder":
        return "Filter output";
      case "webconsole.errorsFilterButton.label":
        return "Errors";
      case "webconsole.warningsFilterButton.label":
        return "Warnings";
      case "webconsole.logsFilterButton.label":
        return "Logs";
      case "webconsole.infoFilterButton.label":
        return "Info";
      case "webconsole.debugFilterButton.label":
        return "Debug";
      case "webconsole.cssFilterButton.label":
        return "CSS";
      case "webconsole.xhrFilterButton.label":
        return "XHR";
      case "webconsole.requestsFilterButton.label":
        return "Requests";
      case "messageRepeats.tooltip2":
        return "#1 repeat;#1 repeats";
      case "webconsole.filteredMessages.label":
        return "#1 item hidden by filters;#1 items hidden by filters";
      default:
        return str;
    }
  }

  getFormatStr(str) {
    return this.getStr(str);
  }

  timestampString(milliseconds) {
    const d = new Date(milliseconds ? milliseconds : null);
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
