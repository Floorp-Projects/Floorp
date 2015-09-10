/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

(function() {
  "use strict";

  /**
   * /!\ FIXME: THIS IS A HORRID HACK which fakes both the mozL10n and webL10n
   * objects and makes them returning the string id and serialized vars if any,
   * for any requested string id.
   * @type {Object}
   */
  navigator.mozL10n = document.mozL10n = {
    initialize: function(){},

    getDirection: function(){},

    get: function(stringId, vars) {

      // upcase the first letter
      var readableStringId = stringId.replace(/^./, function(match) {
        return match.toUpperCase();
      }).replace(/_/g, " ");  // and convert _ chars to spaces

      return "" + readableStringId + (vars ? ";" + JSON.stringify(vars) : "");
    },

    /* For timestamp formatting reasons. */
    language: {
      code: "en-US"
    }
  };
})();
