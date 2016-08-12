/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// @TODO Load the actual strings from webconsole.properties instead.
module.exports = {
  getStr: str => {
    switch (str) {
      case "severity.error":
        return "Error";
    }
    return str;
  }
};
