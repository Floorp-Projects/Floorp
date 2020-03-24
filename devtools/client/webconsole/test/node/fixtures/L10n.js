/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const strings = {
  "level.error": "Error",
  consoleCleared: "Console was cleared.",
  webConsoleXhrIndicator: "XHR",
  webConsoleMoreInfoLabel: "Learn More",
  "webconsole.clearButton.tooltip": "Clear the Web Console output",
  "webconsole.toggleFilterButton.tooltip": "Toggle filter bar",
  "webconsole.filterInput.placeholder": "Filter output",
  "webconsole.errorsFilterButton.label": "Errors",
  "webconsole.warningsFilterButton.label": "Warnings",
  "webconsole.logsFilterButton.label": "Logs",
  "webconsole.infoFilterButton.label": "Info",
  "webconsole.debugFilterButton.label": "Debug",
  "webconsole.cssFilterButton.label": "CSS",
  "webconsole.xhrFilterButton.label": "XHR",
  "webconsole.requestsFilterButton.label": "Requests",
  "messageRepeats.tooltip2": "#1 repeat;#1 repeats",
  "webconsole.filteredMessagesByText.label": "#1 hidden;#1 hidden",
  "webconsole.filteredMessagesByText.tooltip":
    "#1 item hidden by text filter;#1 items hidden by text filter",
  cdFunctionInvalidArgument:
    "Cannot cd() to the given window. Invalid argument.",
  "webconsole.group.cookieSameSiteLaxByDefaultEnabled":
    "Some cookies are misusing the sameSite attribute, so it won't work as expected",
  "webconsole.group.cookieSameSiteLaxByDefaultDisabled":
    "Some cookies are misusing the recommended sameSite attribute",
};

// @TODO Load the actual strings from webconsole.properties instead.
class L10n {
  getStr(str) {
    return strings[str] || str;
  }

  getFormatStr(str, ...rest) {
    switch (str) {
      case "counterDoesntExist":
        return `Counter “${rest[0]}” doesn’t exist.`;
      case "timerDoesntExist":
        return `Timer “${rest[0]}” doesn’t exist.`;
      case "timerAlreadyExists":
        return `Timer “${rest[0]}” already exists.`;
      case "timeLog":
        return `${rest[0]}: ${rest[1]}ms`;
      case "console.timeEnd":
        return `${rest[0]}: ${rest[1]}ms - timer ended`;
      default:
        return this.getStr(str);
    }
  }

  timestampString(milliseconds) {
    const d = new Date(milliseconds ? milliseconds : null);
    let hours = d.getHours(),
      minutes = d.getMinutes();
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
