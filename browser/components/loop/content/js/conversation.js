/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*global loop*/
// XXX This file needs unit testing

var loop = loop || {};
loop.conversation = (function(_, __) {
  "use strict";

  // XXX: baseApiUrl should be configurable (browser pref)
  var baseApiUrl = "http://localhost:5000";

  /**
   * Panel initialisation.
   */
  function init() {
    // Send a message to the server to get the call info
    this.client = new loop.Client({
      baseApiUrl: baseApiUrl
    });

    // Get the call information
    this.client.requestCallsInfo(function(err, calls) {
      if (err) {
        console.error("Error getting call data: ", err);
        return;
      }

      console.log("Received Calls Data: ", calls);
    });
  }

  return {
    init: init
  };
})(_);
