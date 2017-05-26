"use strict";

var global = this;

this.catcher = (function() {
  let exports = {};

  let handler;

  let queue = [];

  let log = global.log;

  exports.unhandled = function(error, info) {
    log.error("Unhandled error:", error, info);
    let e = makeError(error, info);
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
        stack: exc.stack
      };
      for (let attr in exc) {
        result[attr] = exc[attr];
      }
    }
    if (info) {
      for (let attr of Object.keys(info)) {
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
        log.debug("------Error in promise:", e);
        log.debug(e.stack);
      } else {
        log.error("------Error in promise:", e);
        log.error(e.stack);
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
    for (let error of queue) {
      handler(error);
    }
    queue = [];
  };

  return exports;
})();
null;
