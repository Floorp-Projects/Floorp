/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TIMEZONES = {
  IDLW: -12,
  NT: -11,
  HST: -10,
  AKST: -9,
  PST: -8,
  AKDT: -8,
  MST: -7,
  PDT: -7,
  CST: -6,
  MDT: -6,
  EST: -5,
  CDT: -5,
  EDT: -4,
  AST: -4,
  GUY: -3,
  ADT: -3,
  AT: -2,
  UTC: 0,
  GMT: 0,
  Z: 0,
  WET: 0,
  WEST: 1,
  CET: 1,
  BST: 1,
  IST: 1,
  CEST: 2,
  EET: 2,
  EEST: 3,
  MSK: 3,
  MSD: 4,
  ZP4: 4,
  ZP5: 5,
  ZP6: 6,
  WAST: 7,
  AWST: 8,
  WST: 8,
  JST: 9,
  ACST: 9.5,
  ACDT: 10.5,
  AEST: 10,
  AEDT: 11,
  NZST: 12,
  IDLE: 12,
  NZD: 13,
};

const TIME_REGEX = "([0-2]?[0-9])(:([0-5][0-9]))?\\s*([ap]m)?";
const TIMEZONE_REGEX = "\\w+";

// NOTE: This regex need to be localized upon supporting multi locales
//       since it supports en-US input format only.
const QUERY_REGEX = new RegExp(
  `^(${TIME_REGEX}|now)\\s*(${TIMEZONE_REGEX})?(?:\\s+in\\s+|\\s+to\\s+|\\s*=\\s*)(${TIMEZONE_REGEX}|here)`,
  "i"
);

const KEYWORD_HERE = "HERE";
const KEYWORD_NOW = "NOW";

/**
 * This module converts timezone.
 */
export class UnitConverterTimezone {
  /**
   * Convert the given search string.
   *
   * @param {string} searchString
   * @returns {string} conversion result.
   */
  convert(searchString) {
    const regexResult = QUERY_REGEX.exec(searchString);
    if (!regexResult) {
      return null;
    }

    const inputTime = regexResult[1].toUpperCase();
    const inputTimezone = regexResult[6]?.toUpperCase();
    let outputTimezone = regexResult[7].toUpperCase();

    if (
      (inputTimezone &&
        inputTimezone !== KEYWORD_NOW &&
        !(inputTimezone in TIMEZONES)) ||
      (outputTimezone !== KEYWORD_HERE && !(outputTimezone in TIMEZONES))
    ) {
      return null;
    }

    const inputDate = new Date();
    let isMeridiemNeeded = false;
    if (inputTime === KEYWORD_NOW) {
      inputDate.setUTCHours(inputDate.getHours());
      inputDate.setUTCMinutes(inputDate.getMinutes());
    } else {
      // If the input was given as AM/PM, we need to convert it to 24h.
      // 12AM is converted to 00, and for PM times we add 12 to the hour value except for 12PM.
      // If the input is for example 23PM, we use 23 as the hour - we don't add 12 as this would result in a date increment.
      const inputAMPM = regexResult[5]?.toLowerCase() || "";
      const inputHours =
        regexResult[2] === "12" && inputAMPM === "am"
          ? 0
          : Number(regexResult[2]);
      const inputMinutes = regexResult[4] ? Number(regexResult[4]) : 0;
      const inputMeridianHourShift =
        inputAMPM === "pm" && inputHours < 12 ? 12 : 0;
      inputDate.setUTCHours(inputHours + inputMeridianHourShift);
      inputDate.setUTCMinutes(inputMinutes);
      isMeridiemNeeded = !!inputAMPM;
    }

    const inputOffset = inputTimezone
      ? TIMEZONES[inputTimezone] * 60
      : -inputDate.getTimezoneOffset();
    let outputOffset;
    if (outputTimezone === KEYWORD_HERE) {
      outputOffset = -inputDate.getTimezoneOffset();
      const sign = -inputDate.getTimezoneOffset() > 0 ? "+" : "-";
      const hours = parseInt(Math.abs(outputOffset) / 60);
      const minutes = formatMinutes((outputOffset % 60) * 60);
      outputTimezone = `UTC${sign}${hours}${minutes}`;
    } else {
      outputOffset = TIMEZONES[outputTimezone] * 60;
    }

    const outputDate = new Date(inputDate.getTime());
    outputDate.setUTCMinutes(
      outputDate.getUTCMinutes() - inputOffset + outputOffset
    );

    const time = new Intl.DateTimeFormat("en-US", {
      timeStyle: "short",
      hour12: isMeridiemNeeded,
      timeZone: "UTC",
    }).format(outputDate);

    return `${time} ${outputTimezone}`;
  }
}

function formatMinutes(minutes) {
  return minutes.toString().padStart(2, "0");
}
