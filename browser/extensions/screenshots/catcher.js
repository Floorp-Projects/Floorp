/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// eslint-disable-next-line no-var
var global = this;

this.catcher = (function() {
  const exports = {};

  let handler;

  let queue = [];

  const log = global.log;

  exports.unhandled = function(error, info) {
    if (!error.noReport) {
      log.error("Unhandled error:", error, info);
    }
    const e = makeError(error, info);
    if (!handler) {
      queue.push(e);
    } else {
      handler(e);
    }
  };

  /** Turn an exception into an error object */
  function makeError(exc, info) {
    let result;
    if (exc.fromMakeError) {
      result = exc;
    } else {
      result = {
        fromMakeError: true,
        name: exc.name || "ERROR",
        message: String(exc),
        stack: exc.stack,
      };
      for (const attr in exc) {
        result[attr] = exc[attr];
      }
    }
    if (info) {
      for (const attr of Object.keys(info)) {
        result[attr] = info[attr];
      }
    }
    return result;
  }

  /** Wrap the function, and if it raises any exceptions then call unhandled() */
  exports.watchFunction = function watchFunction(func, quiet) {
    return function() {
      try {
        return func.apply(this, arguments);
      } catch (e) {
        if (!quiet) {
          exports.unhandled(e);
        }
        throw e;
      }
    };
  };

  exports.watchPromise = function watchPromise(promise, quiet) {
    return promise.catch((e) => {
      if (quiet) {
        if (!e.noReport) {
          log.debug("------Error in promise:", e);
          log.debug(e.stack);
        }
      } else {
        if (!e.noReport) {
          log.error("------Error in promise:", e);
          log.error(e.stack);
        }
        exports.unhandled(makeError(e));
      }
      throw e;
    });
  };

  exports.registerHandler = function(h) {
    if (handler) {
      log.error("registerHandler called after handler was already registered");
      return;
    }
    handler = h;
    for (const error of queue) {
      handler(error);
    }
    queue = [];
  };

  return exports;
})();
null;
