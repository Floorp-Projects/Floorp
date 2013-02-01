/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "deprecated"
};

function logToConsole(e) {
  console.exception(e);
}

var catchAndLog = exports.catchAndLog = function(callback,
                                                 defaultResponse,
                                                 logException) {
  if (!logException)
    logException = logToConsole;

  return function() {
    try {
      return callback.apply(this, arguments);
    } catch (e) {
      logException(e);
      return defaultResponse;
    }
  };
};

exports.catchAndLogProps = function catchAndLogProps(object,
                                                     props,
                                                     defaultResponse,
                                                     logException) {
  if (typeof(props) == "string")
    props = [props];
  props.forEach(
    function(property) {
      object[property] = catchAndLog(object[property],
                                     defaultResponse,
                                     logException);
    });
};

/**
 * Catch and return an exception while calling the callback.  If the callback
 * doesn't throw, return the return value of the callback in a way that makes it
 * possible to distinguish between a return value and an exception.
 *
 * This function is useful when you need to pass the result of a call across
 * a process boundary (across which exceptions don't propagate).  It probably
 * doesn't need to be factored out into this module, since it is only used by
 * a single caller, but putting it here works around bug 625560.
 */
exports.catchAndReturn = function(callback) {
  return function() {
    try {
      return { returnValue: callback.apply(this, arguments) };
    }
    catch (exception) {
      return { exception: exception };
    }
  };
};
