/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyRequireGetter(
  this,
  "createValueGripForTarget",
  "resource://devtools/server/actors/object/utils.js",
  true
);

loader.lazyRequireGetter(
  this,
  "ObjectUtils",
  "resource://devtools/server/actors/object/utils.js"
);

const _invalidCustomFormatterHooks = new WeakSet();
function addInvalidCustomFormatterHooks(hook) {
  if (!hook) {
    return;
  }

  try {
    _invalidCustomFormatterHooks.add(hook);
  } catch (e) {
    console.error("Couldn't add hook to the WeakSet", hook);
  }
}

// Custom exception used between customFormatterHeader and processFormatterForHeader
class FormatterError extends Error {
  constructor(message, script) {
    super(message);
    this.script = script;
  }
}

/**
 * Handle a protocol request to get the custom formatter header for an object.
 * This is typically returned into ObjectActor's form if custom formatters are enabled.
 *
 * @param {ObjectActor} objectActor
 *
 * @returns {Object} Data related to the custom formatter header:
 *          - {boolean} useCustomFormatter, indicating if a custom formatter is used.
 *          - {Array} header JsonML of the output header.
 *          - {boolean} hasBody True in case the custom formatter has a body.
 *          - {Object} formatter The devtoolsFormatters item that was being used to format
 *                               the object.
 */
function customFormatterHeader(objectActor) {
  const rawValue = objectActor.rawValue();
  const globalWrapper = Cu.getGlobalForObject(rawValue);
  const global = globalWrapper?.wrappedJSObject;

  // We expect a `devtoolsFormatters` global attribute and it to be an array
  if (!global || !Array.isArray(global.devtoolsFormatters)) {
    return null;
  }

  const customFormatterTooDeep =
    (objectActor.hooks.customFormatterObjectTagDepth || 0) > 20;
  if (customFormatterTooDeep) {
    logCustomFormatterError(
      globalWrapper,
      `Too deep hierarchy of inlined custom previews`
    );
    return null;
  }

  const targetActor = objectActor.thread._parent;

  const {
    customFormatterConfigDbgObj: configDbgObj,
    customFormatterObjectTagDepth,
  } = objectActor.hooks;

  const valueDbgObj = objectActor.obj;

  for (const [
    customFormatterIndex,
    formatter,
  ] of global.devtoolsFormatters.entries()) {
    // If the message for the erroneous formatter already got logged,
    // skip logging it again.
    if (_invalidCustomFormatterHooks.has(formatter)) {
      continue;
    }

    // TODO: Any issues regarding the implementation will be covered in https://bugzil.la/1776611.
    try {
      const rv = processFormatterForHeader({
        configDbgObj,
        customFormatterObjectTagDepth,
        formatter,
        targetActor,
        valueDbgObj,
      });
      // Return the first valid formatter value
      if (rv) {
        return rv;
      }
    } catch (e) {
      logCustomFormatterError(
        globalWrapper,
        e instanceof FormatterError
          ? `devtoolsFormatters[${customFormatterIndex}].${e.message}`
          : `devtoolsFormatters[${customFormatterIndex}] couldn't be run: ${e.message}`,
        // If the exception is FormatterError, this comes with a script attribute
        e.script
      );
      addInvalidCustomFormatterHooks(formatter);
    }
  }

  return null;
}
exports.customFormatterHeader = customFormatterHeader;

/**
 * Handle one precise custom formatter.
 * i.e. one element of the window.customFormatters Array.
 *
 * @param {Object} options
 * @param {Debugger.Object} options.configDbgObj
 *        The Debugger.Object of the config object.
 * @param {Number} options.customFormatterObjectTagDepth
 *        See buildJsonMlFromCustomFormatterHookResult JSDoc.
 * @param {Object} options.formatter
 *        The raw formatter object (coming from "customFormatter" array).
 * @param {BrowsingContextTargetActor} options.targetActor
 *        See buildJsonMlFromCustomFormatterHookResult JSDoc.
 * @param {Debugger.Object} options.valueDbgObj
 *        The Debugger.Object of rawValue.
 *
 * @returns {Object} See customFormatterHeader jsdoc, it returns the same object.
 */
function processFormatterForHeader({
  configDbgObj,
  customFormatterObjectTagDepth,
  formatter,
  targetActor,
  valueDbgObj,
}) {
  const headerType = typeof formatter?.header;
  if (headerType !== "function") {
    throw new FormatterError(`header should be a function, got ${headerType}`);
  }

  // Call the formatter's header attribute, which should be a function.
  const formatterHeaderDbgValue = ObjectUtils.makeDebuggeeValueIfNeeded(
    valueDbgObj,
    formatter.header
  );
  const header = formatterHeaderDbgValue.call(
    formatterHeaderDbgValue.boundThis,
    valueDbgObj,
    configDbgObj
  );

  // If the header returns null, the custom formatter isn't used for that object
  if (header?.return === null) {
    return null;
  }

  // The header has to be an Array, all other cases are errors
  if (header?.return?.class !== "Array") {
    let errorMsg = "";
    if (header == null) {
      errorMsg = `header was not run because it has side effects`;
    } else if ("return" in header) {
      let type = typeof header.return;
      if (type === "object") {
        type = header.return?.class;
      }
      errorMsg = `header should return an array, got ${type}`;
    } else if ("throw" in header) {
      errorMsg = `header threw: ${header.throw.getProperty("message")?.return}`;
    }

    throw new FormatterError(errorMsg, formatterHeaderDbgValue?.script);
  }

  const rawHeader = header.return.unsafeDereference();
  if (rawHeader.length === 0) {
    throw new FormatterError(
      `header returned an empty array`,
      formatterHeaderDbgValue?.script
    );
  }

  const sanitizedHeader = buildJsonMlFromCustomFormatterHookResult(
    header.return,
    customFormatterObjectTagDepth,
    targetActor
  );

  let hasBody = false;
  const hasBodyType = typeof formatter?.hasBody;
  if (hasBodyType === "function") {
    const formatterHasBodyDbgValue = ObjectUtils.makeDebuggeeValueIfNeeded(
      valueDbgObj,
      formatter.hasBody
    );
    hasBody = formatterHasBodyDbgValue.call(
      formatterHasBodyDbgValue.boundThis,
      valueDbgObj,
      configDbgObj
    );

    if (hasBody == null) {
      throw new FormatterError(
        `hasBody was not run because it has side effects`,
        formatterHasBodyDbgValue?.script
      );
    } else if ("throw" in hasBody) {
      throw new FormatterError(
        `hasBody threw: ${hasBody.throw.getProperty("message")?.return}`,
        formatterHasBodyDbgValue?.script
      );
    }
  } else if (hasBodyType !== "undefined") {
    throw new FormatterError(
      `hasBody should be a function, got ${hasBodyType}`
    );
  }

  return {
    useCustomFormatter: true,
    header: sanitizedHeader,
    hasBody: !!hasBody?.return,
    formatter,
  };
}

/**
 * Handle a protocol request to get the custom formatter body for an object
 *
 * @param {ObjectActor} objectActor
 * @param {Object} formatter: The global.devtoolsFormatters entry that was used in customFormatterHeader
 *                            for this object.
 *
 * @returns {Object} Data related to the custom formatter body:
 *          - {*} customFormatterBody Data of the custom formatter body.
 */
async function customFormatterBody(objectActor, formatter) {
  const rawValue = objectActor.rawValue();
  const globalWrapper = Cu.getGlobalForObject(rawValue);
  const global = globalWrapper?.wrappedJSObject;

  const customFormatterIndex = global.devtoolsFormatters.indexOf(formatter);

  const targetActor = objectActor.thread._parent;
  try {
    const { customFormatterConfigDbgObj, customFormatterObjectTagDepth } =
      objectActor.hooks;

    if (_invalidCustomFormatterHooks.has(formatter)) {
      return {
        customFormatterBody: null,
      };
    }

    const bodyType = typeof formatter.body;
    if (bodyType !== "function") {
      logCustomFormatterError(
        globalWrapper,
        `devtoolsFormatters[${customFormatterIndex}].body should be a function, got ${bodyType}`
      );
      addInvalidCustomFormatterHooks(formatter);
      return {
        customFormatterBody: null,
      };
    }

    const formatterBodyDbgValue = ObjectUtils.makeDebuggeeValueIfNeeded(
      objectActor.obj,
      formatter.body
    );
    const body = formatterBodyDbgValue.call(
      formatterBodyDbgValue.boundThis,
      objectActor.obj,
      customFormatterConfigDbgObj
    );
    if (body?.return?.class === "Array") {
      const rawBody = body.return.unsafeDereference();
      if (rawBody.length === 0) {
        logCustomFormatterError(
          globalWrapper,
          `devtoolsFormatters[${customFormatterIndex}].body returned an empty array`,
          formatterBodyDbgValue?.script
        );
        addInvalidCustomFormatterHooks(formatter);
        return {
          customFormatterBody: null,
        };
      }

      const customFormatterBodyJsonMl =
        buildJsonMlFromCustomFormatterHookResult(
          body.return,
          customFormatterObjectTagDepth,
          targetActor
        );

      return {
        customFormatterBody: customFormatterBodyJsonMl,
      };
    }

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
    addInvalidCustomFormatterHooks(formatter);
  } catch (e) {
    logCustomFormatterError(
      globalWrapper,
      `Custom formatter with index ${customFormatterIndex} couldn't be run: ${e.message}`
    );
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

/**
 * Return a ready to use JsonMl object, safe to be sent to the client.
 * This will replace JsonMl items with object reference, e.g `[ "object", { config: ..., object: ... } ]`
 * with objectActor grip or "regular" JsonMl items (e.g. `["span", {style: "color: red"}, "this is", "an object"]`)
 * if the referenced object gets custom formatted as well.
 *
 * @param {DebuggerObject} jsonMlDbgObj: The debugger object representing a jsonMl object returned
 *                         by a custom formatter hook.
 * @param {Number} customFormatterObjectTagDepth: See `processObjectTag`.
 * @param {BrowsingContextTargetActor} targetActor: The actor that will be managing any
 *                                     created ObjectActor.
 * @returns {Array|null} Returns null if the passed object is a not DebuggerObject representing an Array
 */
function buildJsonMlFromCustomFormatterHookResult(
  jsonMlDbgObj,
  customFormatterObjectTagDepth,
  targetActor
) {
  const tagName = jsonMlDbgObj.getProperty(0)?.return;
  if (typeof tagName !== "string") {
    const tagNameType =
      tagName?.class || (tagName === null ? "null" : typeof tagName);
    throw new Error(`tagName should be a string, got ${tagNameType}`);
  }

  // Fetch the other items of the jsonMl
  const rest = [];
  const dbgObjLength = jsonMlDbgObj.getProperty("length")?.return || 0;
  for (let i = 1; i < dbgObjLength; i++) {
    rest.push(jsonMlDbgObj.getProperty(i)?.return);
  }

  // The second item of the array can either be an object holding the attributes
  // for the element or the first child element.
  const attributesDbgObj =
    rest[0] && rest[0].class === "Object" ? rest[0] : null;
  const childrenDbgObj = attributesDbgObj ? rest.slice(1) : rest;

  // If the tagName is "object", we need to replace the entry with the grip representing
  // this object (that may or may not be custom formatted).
  if (tagName == "object") {
    if (!attributesDbgObj) {
      throw new Error(`"object" tag should have attributes`);
    }

    // TODO: We could emit a warning if `childrenDbgObj` isn't empty as we're going to
    // ignore them here.
    return processObjectTag(
      attributesDbgObj,
      customFormatterObjectTagDepth,
      targetActor
    );
  }

  const jsonMl = [tagName, {}];
  if (attributesDbgObj) {
    // For non "object" tags, we only care about the style property
    jsonMl[1].style = attributesDbgObj.getProperty("style")?.return;
  }

  // Handle children, which could be simple primitives or JsonML objects
  for (const childDbgObj of childrenDbgObj) {
    const childDbgObjType = typeof childDbgObj;
    if (childDbgObj?.class === "Array") {
      // `childDbgObj` probably holds a JsonMl item, sanitize it.
      jsonMl.push(
        buildJsonMlFromCustomFormatterHookResult(
          childDbgObj,
          customFormatterObjectTagDepth,
          targetActor
        )
      );
    } else if (childDbgObjType == "object" && childDbgObj !== null) {
      // If we don't have an array, match Chrome implementation.
      jsonMl.push("[object Object]");
    } else {
      // Here `childDbgObj` is a primitive. Create a grip so we can handle all the types
      // we can stringify easily (e.g. `undefined`, `bigint`, …).
      const grip = createValueGripForTarget(targetActor, childDbgObj);
      if (grip !== null) {
        jsonMl.push(grip);
      }
    }
  }
  return jsonMl;
}

/**
 * Return a ready to use JsonMl object, safe to be sent to the client.
 * This will replace JsonMl items with object reference, e.g `[ "object", { config: ..., object: ... } ]`
 * with objectActor grip or "regular" JsonMl items (e.g. `["span", {style: "color: red"}, "this is", "an object"]`)
 * if the referenced object gets custom formatted as well.
 *
 * @param {DebuggerObject} attributesDbgObj: The debugger object representing the "attributes"
 *                         of a jsonMl item (e.g. the second item in the array).
 * @param {Number} customFormatterObjectTagDepth: As "object" tag can reference custom
 *                 formatted data, we track the number of time we go through this function
 *                 from the "root" object so we don't have an infinite loop.
 * @param {BrowsingContextTargetActor} targetActor: The actor that will be managin any
 *                                     created ObjectActor.
 * @returns {Object} Returns a grip representing the underlying object
 */
function processObjectTag(
  attributesDbgObj,
  customFormatterObjectTagDepth,
  targetActor
) {
  const objectDbgObj = attributesDbgObj.getProperty("object")?.return;
  if (typeof objectDbgObj == "undefined") {
    throw new Error(
      `attribute of "object" tag should have an "object" property`
    );
  }

  // We need to replace the "object" tag with the actual `attribute.object` object,
  // which might be also custom formatted.
  // We create the grip so the custom formatter hooks can be called on this object, or
  // we'd get an object grip that we can consume to display an ObjectInspector on the client.
  const configRv = attributesDbgObj.getProperty("config");
  const grip = createValueGripForTarget(targetActor, objectDbgObj, 0, {
    // Store the config so we can pass it when calling custom formatter hooks for this object.
    customFormatterConfigDbgObj: configRv?.return,
    customFormatterObjectTagDepth: (customFormatterObjectTagDepth || 0) + 1,
  });

  return grip;
}
