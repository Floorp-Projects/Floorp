/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// @TODO Load the actual strings from netmonitor.properties instead.
class L10n {
  getStr(str) {
    switch (str) {
      default:
        return str;
    }
  }

  getFormatStr(str) {
    return this.getStr(str);
  }
}

module.exports = L10n;
