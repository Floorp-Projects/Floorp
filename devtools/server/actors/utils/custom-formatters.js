/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyRequireGetter(
  this,
  "makeSideeffectFreeDebugger",
  "resource://devtools/server/actors/webconsole/eval-with-debugger.js",
  true
);

const _invalidCustomFormatterHooks = new WeakSet();

/**
 * Handle a protocol request to get the custom formatter header for an object.
 * This is typically returned into ObjectActor's form if custom formatters are enabled.
 *
 * @param {Object} rawValue
 *        The related ObjectActor raw Javascript object.
 *
 * @returns {Object} Data related to the custom formatter header:
 *          - {boolean} useCustomFormatter, indicating if a custom formatter is used.
 *          - {number} customFormatterIndex Index of the custom formatter in the formatters array.
 *          - {Array} header JsonML of the output header.
 *          - {boolean} hasBody True in case the custom formatter has a body.
 */
function customFormatterHeader(rawValue) {
  const globalWrapper = Cu.getGlobalForObject(rawValue);
  const global = globalWrapper?.wrappedJSObject;
  if (global && Array.isArray(global.devtoolsFormatters)) {
    // We're using the same setup as the eager evaluation to ensure evaluating
    // the custom formatter's functions doesn't have any side effects.
    const dbg = makeSideeffectFreeDebugger();
    const dbgGlobal = dbg.makeGlobalObjectReference(global);

    for (const [index, formatter] of global.devtoolsFormatters.entries()) {
      // If the message for the erroneous formatter already got logged,
      // skip logging it again.
      if (_invalidCustomFormatterHooks.has(formatter)) {
        continue;
      }

      const headerType = typeof formatter?.header;
      if (headerType !== "function") {
        _invalidCustomFormatterHooks.add(formatter);
        logCustomFormatterError(
          globalWrapper,
          `devtoolsFormatters[${index}].header should be a function, got ${headerType}`
        );
        continue;
      }

      // TODO: Any issues regarding the implementation will be covered in https://bugzil.la/1776611.
      try {
        const formatterHeaderDbgValue = dbgGlobal.makeDebuggeeValue(
          formatter.header
        );
        const debuggeeValue = dbgGlobal.makeDebuggeeValue(rawValue);
        const header = formatterHeaderDbgValue.call(dbgGlobal, debuggeeValue);
        let errorMsg = "";
        if (header?.return?.class === "Array") {
          const rawHeader = header.return.unsafeDereference();
          if (rawHeader.length === 0) {
            _invalidCustomFormatterHooks.add(formatter);
            logCustomFormatterError(
              globalWrapper,
              `devtoolsFormatters[${index}].header returned an empty array`,
              formatterHeaderDbgValue?.script
            );
            continue;
          }
          let hasBody = false;
          const hasBodyType = typeof formatter?.hasBody;
          if (hasBodyType === "function") {
            const formatterHasBodyDbgValue = dbgGlobal.makeDebuggeeValue(
              formatter.hasBody
            );
            hasBody = formatterHasBodyDbgValue.call(dbgGlobal, debuggeeValue);

            if (hasBody == null) {
              _invalidCustomFormatterHooks.add(formatter);

              logCustomFormatterError(
                globalWrapper,
                `devtoolsFormatters[${index}].hasBody was not run because it has side effects`,
                formatterHasBodyDbgValue?.script
              );

              continue;
            } else if ("throw" in hasBody) {
              _invalidCustomFormatterHooks.add(formatter);

              logCustomFormatterError(
                globalWrapper,
                `devtoolsFormatters[${index}].hasBody threw: ${
                  hasBody.throw.getProperty("message")?.return
                }`,
                formatterHasBodyDbgValue?.script
              );

              continue;
            }
          } else if (hasBodyType !== "undefined") {
            _invalidCustomFormatterHooks.add(formatter);
            logCustomFormatterError(
              globalWrapper,
              `devtoolsFormatters[${index}].hasBody should be a function, got ${hasBodyType}`
            );
            continue;
          }

          return {
            useCustomFormatter: true,
            customFormatterIndex: index,
            // As the value represents an array coming from the page,
            // we're cloning it to avoid any interferences with the original
            // variable.
            header: global.structuredClone(rawHeader),
            hasBody: !!hasBody?.return,
          };
        }

        // If the header returns null, the custom formatter isn't used for that object
        if (header?.return === null) {
          continue;
        }

        _invalidCustomFormatterHooks.add(formatter);
        if (header == null) {
          errorMsg = `devtoolsFormatters[${index}].header was not run because it has side effects`;
        } else if ("return" in header) {
          let type = typeof header.return;
          if (type === "object") {
            type = header.return?.class;
          }
          errorMsg = `devtoolsFormatters[${index}].header should return an array, got ${type}`;
        } else if ("throw" in header) {
          errorMsg = `devtoolsFormatters[${index}].header threw: ${
            header.throw.getProperty("message")?.return
          }`;
        }

        logCustomFormatterError(
          globalWrapper,
          errorMsg,
          formatterHeaderDbgValue?.script
        );
      } catch (e) {
        logCustomFormatterError(
          globalWrapper,
          `devtoolsFormatters[${index}] couldn't be run: ${e.message}`
        );
      } finally {
        // We need to be absolutely sure that the side-effect-free debugger's
        // debuggees are removed because otherwise we risk them terminating
        // execution of later code in the case of unexpected exceptions.
        dbg.removeAllDebuggees();
      }
    }
  }

  return null;
}
exports.customFormatterHeader = customFormatterHeader;

/**
 * Handle a protocol request to get the custom formatter body for an object
 *
 * @param number customFormatterIndex
 *        Index of the custom formatter used for the object
 * @returns {Object} Data related to the custom formatter body:
 *          - {*} customFormatterBody Data of the custom formatter body.
 */
async function customFormatterBody(rawValue, customFormatterIndex) {
  const globalWrapper = Cu.getGlobalForObject(rawValue);
  const global = globalWrapper?.wrappedJSObject;

  // Use makeSideeffectFreeDebugger (from eval-with-debugger.js) and the debugger
  // object for each formatter and use `call` (https://searchfox.org/mozilla-central/rev/5e15e00fa247cba5b765727496619bf9010ed162/js/src/doc/Debugger/Debugger.Object.md#484)
  const dbg = makeSideeffectFreeDebugger();
  try {
    const dbgGlobal = dbg.makeGlobalObjectReference(global);
    const formatter = global.devtoolsFormatters[customFormatterIndex];

    if (_invalidCustomFormatterHooks.has(formatter)) {
      return {
        customFormatterBody: null,
      };
    }

    const bodyType = typeof formatter?.body;
    if (bodyType !== "function") {
      _invalidCustomFormatterHooks.add(formatter);
      logCustomFormatterError(
        globalWrapper,
        `devtoolsFormatters[${customFormatterIndex}].body should be a function, got ${bodyType}`
      );
      return {
        customFormatterBody: null,
      };
    }

    const formatterBodyDbgValue =
      formatter && dbgGlobal.makeDebuggeeValue(formatter.body);
    const body = formatterBodyDbgValue.call(
      dbgGlobal,
      dbgGlobal.makeDebuggeeValue(rawValue)
    );
    if (body?.return?.class === "Array") {
      const rawBody = body.return.unsafeDereference();
      if (rawBody.length === 0) {
        _invalidCustomFormatterHooks.add(formatter);
        logCustomFormatterError(
          globalWrapper,
          `devtoolsFormatters[${customFormatterIndex}].body returned an empty array`,
          formatterBodyDbgValue?.script
        );
        return {
          customFormatterBody: null,
        };
      }

      return {
        // As the value is represents an array coming from the page,
        // we're cloning it to avoid any interferences with the original
        // variable.
        customFormatterBody: global.structuredClone(rawBody),
      };
    }

    _invalidCustomFormatterHooks.add(formatter);
    let errorMsg = "";
    if (body == null) {
      errorMsg = `devtoolsFormatters[${customFormatterIndex}].body was not run because it has side effects`;
    } else if ("return" in body) {
      let type = body.return === null ? "null" : typeof body.return;
      if (type === "object") {
        type = body.return?.class;
      }
      errorMsg = `devtoolsFormatters[${customFormatterIndex}].body should return an array, got ${type}`;
    } else if ("throw" in body) {
      errorMsg = `devtoolsFormatters[${customFormatterIndex}].body threw: ${
        body.throw.getProperty("message")?.return
      }`;
    }

    logCustomFormatterError(
      globalWrapper,
      errorMsg,
      formatterBodyDbgValue?.script
    );
  } catch (e) {
    logCustomFormatterError(
      globalWrapper,
      `Custom formatter with index ${customFormatterIndex} couldn't be run: ${e.message}`
    );
  } finally {
    // We need to be absolutely sure that the side-effect-free debugger's
    // debuggees are removed because otherwise we risk them terminating
    // execution of later code in the case of unexpected exceptions.
    dbg.removeAllDebuggees();
  }

  return {};
}
exports.customFormatterBody = customFormatterBody;

/**
 * Log an error caused by a fault in a custom formatter to the web console.
 *
 * @param {Window} window The related global where we should log this message.
 *                        This should be the xray wrapper in order to expose windowGlobalChild.
 *                        The unwrapped, unpriviledged won't expose this attribute.
 * @param {string} errorMsg Message to log to the console.
 * @param {DebuggerObject} [script] The script causing the error.
 */
function logCustomFormatterError(window, errorMsg, script) {
  const scriptErrorClass = Cc["@mozilla.org/scripterror;1"];
  const scriptError = scriptErrorClass.createInstance(Ci.nsIScriptError);
  const { url, source, startLine, startColumn } = script ?? {};

  scriptError.initWithWindowID(
    `Custom formatter failed: ${errorMsg}`,
    url,
    source,
    startLine,
    startColumn,
    Ci.nsIScriptError.errorFlag,
    "devtoolsFormatter",
    window.windowGlobalChild.innerWindowId
  );
  Services.console.logMessage(scriptError);
}
