/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

module.exports = {
  PluralForm: {
    get: function(occurence, str) {
      if (str.includes(";")) {
        const [singular, plural] = str.split(";");
        return occurence > 1 ? plural : singular;
      }

      return str;
    },
  },
};
