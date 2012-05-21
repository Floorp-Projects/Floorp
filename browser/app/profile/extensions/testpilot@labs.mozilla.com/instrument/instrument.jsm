/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let EXPORTED_SYMBOLS = ["Instrument"];

let data = {};
/**
 * Track how many times an object's member function is called
 *
 * @param obj
 *        Object containing the method to track
 * @param func
 *        Property name of the object that is the function to track
 * @param name
 *        "Pretty" name to log the usage counts
 */
let track = function(obj, func, name) {
  // Initialize count data
  data[name] = 0;

  // Save the original function to call
  let orig = obj[func];
  obj[func] = function() {
    // Increment our counter right away (in-case there's an exception)
    data[name]++;

    // Make the call just like it was originally was called
    return orig.apply(this, arguments);
  };
}

/**
 * Instrument a browser window for various behavior
 *
 * @param window
 *        Browser window to instrument
 */
function Instrument(window) {
  let $ = function(id) window.document.getElementById(id);

  track(window.gURLBar, "showHistoryPopup", "dropdown");
  track($("back-button"), "_handleClick", "back");
  track($("forward-button"), "_handleClick", "forward");
}

// Provide a way to get at the collected data (e.g., from Error Console)
Instrument.report = function() JSON.stringify(data);
