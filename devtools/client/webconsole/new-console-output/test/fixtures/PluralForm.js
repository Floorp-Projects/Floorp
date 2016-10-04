/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

module.exports = {
  PluralForm: {
    get: function (occurence, str) {
      // @TODO Remove when loading the actual strings from webconsole.properties
      // is done in the L10n fixture.
      if (str === "messageRepeats.tooltip2") {
        return `${occurence} repeats`;
      }

      return str;
    }
  }
};
