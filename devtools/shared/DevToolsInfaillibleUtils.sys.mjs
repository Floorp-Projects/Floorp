/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * The 3 methods here are duplicated from ThreadSafeDevToolsUtils.js
 * The ones defined here are used from other sys.mjs files, mostly from the
 * NetworkObserver codebase, while the ones remaining in ThreadSafeDevToolsUtils.js
 * are used in commonjs modules, including modules which can be loaded in workers.
 *
 * sys.mjs modules are currently not supported in workers, see Bug 1247687.
 */

/**
 * Report that |who| threw an exception, |exception|.
 */
function reportException(who, exception) {
  const msg = `${who} threw an exception: ${safeErrorString(exception)}`;
  dump(msg + "\n");

  if (typeof console !== "undefined" && console && console.error) {
    console.error(exception);
  }
}

/**
 * Given a handler function that may throw, return an infallible handler
 * function that calls the fallible handler, and logs any exceptions it
 * throws.
 *
 * @param handler function
 *      A handler function, which may throw.
 * @param aName string
 *      A name for handler, for use in error messages. If omitted, we use
 *      handler.name.
 *
 * (SpiderMonkey does generate good names for anonymous functions, but we
 * don't have a way to get at them from JavaScript at the moment.)
 */
function makeInfallible(handler, name = handler.name) {
  return function () {
    try {
      return handler.apply(this, arguments);
    } catch (ex) {
      let who = "Handler function";
      if (name) {
        who += " " + name;
      }
      reportException(who, ex);
      return undefined;
    }
  };
}

/**
 * Turn the |error| into a string, without fail.
 *
 * @param {Error|any} error
 */
function safeErrorString(error) {
  try {
    let errorString = error.toString();
    if (typeof errorString == "string") {
      // Attempt to attach a stack to |errorString|. If it throws an error, or
      // isn't a string, don't use it.
      try {
        if (error.stack) {
          const stack = error.stack.toString();
          if (typeof stack == "string") {
            errorString += "\nStack: " + stack;
          }
        }
      } catch (ee) {
        // Ignore.
      }

      // Append additional line and column number information to the output,
      // since it might not be part of the stringified error.
      if (
        typeof error.lineNumber == "number" &&
        typeof error.columnNumber == "number"
      ) {
        errorString +=
          "Line: " + error.lineNumber + ", column: " + error.columnNumber;
      }

      return errorString;
    }
  } catch (ee) {
    // Ignore.
  }

  // We failed to find a good error description, so do the next best thing.
  return Object.prototype.toString.call(error);
}

export const DevToolsInfaillibleUtils = {
  makeInfallible,
  reportException,
  safeErrorString,
};
