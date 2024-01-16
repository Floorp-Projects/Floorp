/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { DevToolsServer } = require("resource://devtools/server/devtools-server.js");
const DevToolsUtils = require("resource://devtools/shared/DevToolsUtils.js");
loader.lazyRequireGetter(
  this,
  "ObjectUtils",
  "resource://devtools/server/actors/object/utils.js"
);
loader.lazyRequireGetter(
  this,
  "PropertyIterators",
  "resource://devtools/server/actors/object/property-iterator.js"
);

// Number of items to preview in objects, arrays, maps, sets, lists,
// collections, etc.
const OBJECT_PREVIEW_MAX_ITEMS = 10;

const ERROR_CLASSNAMES = new Set([
  "Error",
  "EvalError",
  "RangeError",
  "ReferenceError",
  "SyntaxError",
  "TypeError",
  "URIError",
  "InternalError",
  "AggregateError",
  "CompileError",
  "DebuggeeWouldRun",
  "LinkError",
  "RuntimeError",
  "Exception", // This related to Components.Exception()
]);
const ARRAY_LIKE_CLASSNAMES = new Set([
  "DOMStringList",
  "DOMTokenList",
  "CSSRuleList",
  "MediaList",
  "StyleSheetList",
  "NamedNodeMap",
  "FileList",
  "NodeList",
]);
const OBJECT_WITH_URL_CLASSNAMES = new Set([
  "CSSImportRule",
  "CSSStyleSheet",
  "Location",
]);

/**
 * Functions for adding information to ObjectActor grips for the purpose of
 * having customized output. This object holds arrays mapped by
 * Debugger.Object.prototype.class.
 *
 * In each array you can add functions that take three
 * arguments:
 *   - the ObjectActor instance and its hooks to make a preview for,
 *   - the grip object being prepared for the client,
 *   - the raw JS object after calling Debugger.Object.unsafeDereference(). This
 *   argument is only provided if the object is safe for reading properties and
 *   executing methods. See DevToolsUtils.isSafeJSObject().
 *   - the object class (result of objectActor.obj.class). This is passed so we don't have
 *   to access it on each previewer, which can add some overhead.
 *
 * Functions must return false if they cannot provide preview
 * information for the debugger object, or true otherwise.
 */
const previewers = {
  String: [
    function(objectActor, grip, rawObj) {
      return wrappedPrimitivePreviewer(
        "String",
        String,
        objectActor,
        grip,
        rawObj
      );
    },
  ],

  Boolean: [
    function(objectActor, grip, rawObj) {
      return wrappedPrimitivePreviewer(
        "Boolean",
        Boolean,
        objectActor,
        grip,
        rawObj
      );
    },
  ],

  Number: [
    function(objectActor, grip, rawObj) {
      return wrappedPrimitivePreviewer(
        "Number",
        Number,
        objectActor,
        grip,
        rawObj
      );
    },
  ],

  Symbol: [
    function(objectActor, grip, rawObj) {
      return wrappedPrimitivePreviewer(
        "Symbol",
        Symbol,
        objectActor,
        grip,
        rawObj
      );
    },
  ],

  Function: [
    function({ obj, hooks }, grip) {
      if (obj.name) {
        grip.name = obj.name;
      }

      if (obj.displayName) {
        grip.displayName = obj.displayName.substr(0, 500);
      }

      if (obj.parameterNames) {
        grip.parameterNames = obj.parameterNames;
      }

      // Check if the developer has added a de-facto standard displayName
      // property for us to use.
      let userDisplayName;
      try {
        userDisplayName = obj.getOwnPropertyDescriptor("displayName");
      } catch (e) {
        // The above can throw "permission denied" errors when the debuggee
        // does not subsume the function's compartment.
      }

      if (
        userDisplayName &&
        typeof userDisplayName.value == "string" &&
        userDisplayName.value
      ) {
        grip.userDisplayName = hooks.createValueGrip(userDisplayName.value);
      }

      grip.isAsync = obj.isAsyncFunction;
      grip.isGenerator = obj.isGeneratorFunction;

      if (obj.script) {
        // NOTE: Debugger.Script.prototype.startColumn is 1-based.
        //       Convert to 0-based, while keeping the wasm's column (1) as is.
        //       (bug 1863878)
        const columnBase = obj.script.format === "wasm" ? 0 : 1;
        grip.location = {
          url: obj.script.url,
          line: obj.script.startLine,
          column: obj.script.startColumn - columnBase,
        };
      }

      return true;
    },
  ],

  RegExp: [
    function({ obj, hooks }, grip) {
      const str = DevToolsUtils.callPropertyOnObject(obj, "toString");
      if (typeof str != "string") {
        return false;
      }

      grip.displayString = hooks.createValueGrip(str);
      return true;
    },
  ],

  Date: [
    function({ obj, hooks }, grip) {
      const time = DevToolsUtils.callPropertyOnObject(obj, "getTime");
      if (typeof time != "number") {
        return false;
      }

      grip.preview = {
        timestamp: hooks.createValueGrip(time),
      };
      return true;
    },
  ],

  Array: [
    function({ obj, hooks }, grip) {
      const length = ObjectUtils.getArrayLength(obj);

      grip.preview = {
        kind: "ArrayLike",
        length: length,
      };

      if (hooks.getGripDepth() > 1) {
        return true;
      }

      const raw = obj.unsafeDereference();
      const items = (grip.preview.items = []);

      for (let i = 0; i < length; ++i) {
        if (raw && !isWorker) {
          // Array Xrays filter out various possibly-unsafe properties (like
          // functions, and claim that the value is undefined instead. This
          // is generally the right thing for privileged code accessing untrusted
          // objects, but quite confusing for Object previews. So we manually
          // override this protection by waiving Xrays on the array, and re-applying
          // Xrays on any indexed value props that we pull off of it.
          const desc = Object.getOwnPropertyDescriptor(Cu.waiveXrays(raw), i);
          if (desc && !desc.get && !desc.set) {
            let value = Cu.unwaiveXrays(desc.value);
            value = ObjectUtils.makeDebuggeeValueIfNeeded(obj, value);
            items.push(hooks.createValueGrip(value));
          } else if (!desc) {
            items.push(null);
          } else {
            const item = {};
            if (desc.get) {
              let getter = Cu.unwaiveXrays(desc.get);
              getter = ObjectUtils.makeDebuggeeValueIfNeeded(obj, getter);
              item.get = hooks.createValueGrip(getter);
            }
            if (desc.set) {
              let setter = Cu.unwaiveXrays(desc.set);
              setter = ObjectUtils.makeDebuggeeValueIfNeeded(obj, setter);
              item.set = hooks.createValueGrip(setter);
            }
            items.push(item);
          }
        } else if (raw && !obj.getOwnPropertyDescriptor(i)) {
          items.push(null);
        } else {
          // Workers do not have access to Cu.
          const value = DevToolsUtils.getProperty(obj, i);
          items.push(hooks.createValueGrip(value));
        }

        if (items.length == OBJECT_PREVIEW_MAX_ITEMS) {
          break;
        }
      }

      return true;
    },
  ],

  Set: [
    function(objectActor, grip) {
      const size = DevToolsUtils.getProperty(objectActor.obj, "size");
      if (typeof size != "number") {
        return false;
      }

      grip.preview = {
        kind: "ArrayLike",
        length: size,
      };

      // Avoid recursive object grips.
      if (objectActor.hooks.getGripDepth() > 1) {
        return true;
      }

      const items = (grip.preview.items = []);
      for (const item of PropertyIterators.enumSetEntries(objectActor)) {
        items.push(item);
        if (items.length == OBJECT_PREVIEW_MAX_ITEMS) {
          break;
        }
      }

      return true;
    },
  ],

  WeakSet: [
    function(objectActor, grip) {
      const enumEntries = PropertyIterators.enumWeakSetEntries(objectActor);

      grip.preview = {
        kind: "ArrayLike",
        length: enumEntries.size,
      };

      // Avoid recursive object grips.
      if (objectActor.hooks.getGripDepth() > 1) {
        return true;
      }

      const items = (grip.preview.items = []);
      for (const item of enumEntries) {
        items.push(item);
        if (items.length == OBJECT_PREVIEW_MAX_ITEMS) {
          break;
        }
      }

      return true;
    },
  ],

  Map: [
    function(objectActor, grip) {
      const size = DevToolsUtils.getProperty(objectActor.obj, "size");
      if (typeof size != "number") {
        return false;
      }

      grip.preview = {
        kind: "MapLike",
        size: size,
      };

      if (objectActor.hooks.getGripDepth() > 1) {
        return true;
      }

      const entries = (grip.preview.entries = []);
      for (const entry of PropertyIterators.enumMapEntries(objectActor)) {
        entries.push(entry);
        if (entries.length == OBJECT_PREVIEW_MAX_ITEMS) {
          break;
        }
      }

      return true;
    },
  ],

  WeakMap: [
    function(objectActor, grip) {
      const enumEntries = PropertyIterators.enumWeakMapEntries(objectActor);

      grip.preview = {
        kind: "MapLike",
        size: enumEntries.size,
      };

      if (objectActor.hooks.getGripDepth() > 1) {
        return true;
      }

      const entries = (grip.preview.entries = []);
      for (const entry of enumEntries) {
        entries.push(entry);
        if (entries.length == OBJECT_PREVIEW_MAX_ITEMS) {
          break;
        }
      }

      return true;
    },
  ],

  URLSearchParams: [
    function(objectActor, grip) {
      const enumEntries = PropertyIterators.enumURLSearchParamsEntries(objectActor);

      grip.preview = {
        kind: "MapLike",
        size: enumEntries.size,
      };

      if (objectActor.hooks.getGripDepth() > 1) {
        return true;
      }

      const entries = (grip.preview.entries = []);
      for (const entry of enumEntries) {
        entries.push(entry);
        if (entries.length == OBJECT_PREVIEW_MAX_ITEMS) {
          break;
        }
      }

      return true;
    },
  ],

  FormData: [
    function(objectActor, grip) {
      const enumEntries = PropertyIterators.enumFormDataEntries(objectActor);

      grip.preview = {
        kind: "MapLike",
        size: enumEntries.size,
      };

      if (objectActor.hooks.getGripDepth() > 1) {
        return true;
      }

      const entries = (grip.preview.entries = []);
      for (const entry of enumEntries) {
        entries.push(entry);
        if (entries.length == OBJECT_PREVIEW_MAX_ITEMS) {
          break;
        }
      }

      return true;
    },
  ],

  Headers: [
    function(objectActor, grip) {
      // Bug 1863776: Headers can't be yet previewed from workers
      if (isWorker) {
        return false;
      }
      const enumEntries = PropertyIterators.enumHeadersEntries(objectActor);

      grip.preview = {
        kind: "MapLike",
        size: enumEntries.size,
      };

      if (objectActor.hooks.getGripDepth() > 1) {
        return true;
      }

      const entries = (grip.preview.entries = []);
      for (const entry of enumEntries) {
        entries.push(entry);
        if (entries.length == OBJECT_PREVIEW_MAX_ITEMS) {
          break;
        }
      }

      return true;
    },
  ],


  HighlightRegistry: [
    function(objectActor, grip) {
      const enumEntries = PropertyIterators.enumHighlightRegistryEntries(objectActor);

      grip.preview = {
        kind: "MapLike",
        size: enumEntries.size,
      };

      if (objectActor.hooks.getGripDepth() > 1) {
        return true;
      }

      const entries = (grip.preview.entries = []);
      for (const entry of enumEntries) {
        entries.push(entry);
        if (entries.length == OBJECT_PREVIEW_MAX_ITEMS) {
          break;
        }
      }

      return true;
    },
  ],

  MIDIInputMap: [
    function(objectActor, grip) {
      const enumEntries = PropertyIterators.enumMidiInputMapEntries(
        objectActor
      );

      grip.preview = {
        kind: "MapLike",
        size: enumEntries.size,
      };

      if (objectActor.hooks.getGripDepth() > 1) {
        return true;
      }

      const entries = (grip.preview.entries = []);
      for (const entry of enumEntries) {
        entries.push(entry);
        if (entries.length == OBJECT_PREVIEW_MAX_ITEMS) {
          break;
        }
      }

      return true;
    },
  ],

  MIDIOutputMap: [
    function(objectActor, grip) {
      const enumEntries = PropertyIterators.enumMidiOutputMapEntries(
        objectActor
      );

      grip.preview = {
        kind: "MapLike",
        size: enumEntries.size,
      };

      if (objectActor.hooks.getGripDepth() > 1) {
        return true;
      }

      const entries = (grip.preview.entries = []);
      for (const entry of enumEntries) {
        entries.push(entry);
        if (entries.length == OBJECT_PREVIEW_MAX_ITEMS) {
          break;
        }
      }

      return true;
    },
  ],

  DOMStringMap: [
    function({ obj, hooks }, grip, rawObj) {
      if (!rawObj) {
        return false;
      }

      const keys = obj.getOwnPropertyNames();
      grip.preview = {
        kind: "MapLike",
        size: keys.length,
      };

      if (hooks.getGripDepth() > 1) {
        return true;
      }

      const entries = (grip.preview.entries = []);
      for (const key of keys) {
        const value = ObjectUtils.makeDebuggeeValueIfNeeded(obj, rawObj[key]);
        entries.push([key, hooks.createValueGrip(value)]);
        if (entries.length == OBJECT_PREVIEW_MAX_ITEMS) {
          break;
        }
      }

      return true;
    },
  ],

  Promise: [
    function({ obj, hooks }, grip, rawObj) {
      const { state, value, reason } = ObjectUtils.getPromiseState(obj);
      const ownProperties = Object.create(null);
      ownProperties["<state>"] = { value: state };
      let ownPropertiesLength = 1;

      // Only expose <value> or <reason> in top-level promises, to avoid recursion.
      // <state> is not problematic because it's a string.
      if (hooks.getGripDepth() === 1) {
        if (state == "fulfilled") {
          ownProperties["<value>"] = { value: hooks.createValueGrip(value) };
          ++ownPropertiesLength;
        } else if (state == "rejected") {
          ownProperties["<reason>"] = { value: hooks.createValueGrip(reason) };
          ++ownPropertiesLength;
        }
      }

      grip.preview = {
        kind: "Object",
        ownProperties,
        ownPropertiesLength,
      };

      return true;
    },
  ],

  Proxy: [
    function({ obj, hooks }, grip, rawObj) {
      // Only preview top-level proxies, avoiding recursion. Otherwise, since both the
      // target and handler can also be proxies, we could get an exponential behavior.
      if (hooks.getGripDepth() > 1) {
        return true;
      }

      // The `isProxy` getter of the debuggee object only detects proxies without
      // security wrappers. If false, the target and handler are not available.
      const hasTargetAndHandler = obj.isProxy;

      grip.preview = {
        kind: "Object",
        ownProperties: Object.create(null),
        ownPropertiesLength: 2 * hasTargetAndHandler,
      };

      if (hasTargetAndHandler) {
        Object.assign(grip.preview.ownProperties, {
          "<target>": { value: hooks.createValueGrip(obj.proxyTarget) },
          "<handler>": { value: hooks.createValueGrip(obj.proxyHandler) },
        });
      }

      return true;
    },
  ],
};

/**
 * Generic previewer for classes wrapping primitives, like String,
 * Number and Boolean.
 *
 * @param string className
 *        Class name to expect.
 * @param object classObj
 *        The class to expect, eg. String. The valueOf() method of the class is
 *        invoked on the given object.
 * @param ObjectActor objectActor
 *        The object actor
 * @param Object grip
 *        The result grip to fill in
 * @return Booolean true if the object was handled, false otherwise
 */
function wrappedPrimitivePreviewer(
  className,
  classObj,
  objectActor,
  grip,
  rawObj
) {
  let v = null;
  try {
    v = classObj.prototype.valueOf.call(rawObj);
  } catch (ex) {
    // valueOf() can throw if the raw JS object is "misbehaved".
    return false;
  }

  if (v === null) {
    return false;
  }

  const { obj, hooks } = objectActor;

  const canHandle = GenericObject(objectActor, grip, rawObj, className);
  if (!canHandle) {
    return false;
  }

  grip.preview.wrappedValue = hooks.createValueGrip(
    ObjectUtils.makeDebuggeeValueIfNeeded(obj, v)
  );
  return true;
}

/**
 * @param {ObjectActor} objectActor
 * @param {Object} grip: The grip built by the objectActor, for which we need to populate
 *                       the `preview` property.
 * @param {*} rawObj: The native js object
 * @param {String} className: objectActor.obj.class
 * @returns
 */
function GenericObject(objectActor, grip, rawObj, className) {
  const { obj, hooks } = objectActor;
  if (grip.preview || grip.displayString || hooks.getGripDepth() > 1) {
    return false;
  }

  const preview = (grip.preview = {
    kind: "Object",
    ownProperties: Object.create(null),
  });

  const names = ObjectUtils.getPropNamesFromObject(obj, rawObj);
  preview.ownPropertiesLength = names.length;

  let length,
    i = 0;
  let specialStringBehavior = className === "String";
  if (specialStringBehavior) {
    length = DevToolsUtils.getProperty(obj, "length");
    if (typeof length != "number") {
      specialStringBehavior = false;
    }
  }

  for (const name of names) {
    if (specialStringBehavior && /^[0-9]+$/.test(name)) {
      const num = parseInt(name, 10);
      if (num.toString() === name && num >= 0 && num < length) {
        continue;
      }
    }

    const desc = objectActor._propertyDescriptor(name, true);
    if (!desc) {
      continue;
    }

    preview.ownProperties[name] = desc;
    if (++i == OBJECT_PREVIEW_MAX_ITEMS) {
      break;
    }
  }

  if (i === OBJECT_PREVIEW_MAX_ITEMS) {
    return true;
  }

  const privatePropertiesSymbols = ObjectUtils.getSafePrivatePropertiesSymbols(
    obj
  );
  if (privatePropertiesSymbols.length > 0) {
    preview.privatePropertiesLength = privatePropertiesSymbols.length;
    preview.privateProperties = [];

    // Retrieve private properties, which are represented as non-enumerable Symbols
    for (const privateProperty of privatePropertiesSymbols) {
      if (
        !privateProperty.description ||
        !privateProperty.description.startsWith("#")
      ) {
        continue;
      }
      const descriptor = objectActor._propertyDescriptor(privateProperty);
      if (!descriptor) {
        continue;
      }

      preview.privateProperties.push(
        Object.assign(
          {
            descriptor,
          },
          hooks.createValueGrip(privateProperty)
        )
      );

      if (++i == OBJECT_PREVIEW_MAX_ITEMS) {
        break;
      }
    }
  }

  if (i === OBJECT_PREVIEW_MAX_ITEMS) {
    return true;
  }

  const symbols = ObjectUtils.getSafeOwnPropertySymbols(obj);
  if (symbols.length > 0) {
    preview.ownSymbolsLength = symbols.length;
    preview.ownSymbols = [];

    for (const symbol of symbols) {
      const descriptor = objectActor._propertyDescriptor(symbol, true);
      if (!descriptor) {
        continue;
      }

      preview.ownSymbols.push(
        Object.assign(
          {
            descriptor,
          },
          hooks.createValueGrip(symbol)
        )
      );

      if (++i == OBJECT_PREVIEW_MAX_ITEMS) {
        break;
      }
    }
  }

  if (i === OBJECT_PREVIEW_MAX_ITEMS) {
    return true;
  }

  const safeGetterValues = objectActor._findSafeGetterValues(
    Object.keys(preview.ownProperties),
    OBJECT_PREVIEW_MAX_ITEMS - i
  );
  if (Object.keys(safeGetterValues).length) {
    preview.safeGetterValues = safeGetterValues;
  }

  return true;
}

// Preview functions that do not rely on the object class.
previewers.Object = [
  function TypedArray({ obj, hooks }, grip) {
    if (!ObjectUtils.isTypedArray(obj)) {
      return false;
    }

    grip.preview = {
      kind: "ArrayLike",
      length: ObjectUtils.getArrayLength(obj),
    };

    if (hooks.getGripDepth() > 1) {
      return true;
    }

    const previewLength = Math.min(
      OBJECT_PREVIEW_MAX_ITEMS,
      grip.preview.length
    );
    grip.preview.items = [];
    for (let i = 0; i < previewLength; i++) {
      const desc = obj.getOwnPropertyDescriptor(i);
      if (!desc) {
        break;
      }
      grip.preview.items.push(desc.value);
    }

    return true;
  },

  function Error(objectActor, grip, rawObj, className) {
    if (!ERROR_CLASSNAMES.has(className)) {
      return false;
    }

    const { hooks, obj } = objectActor;

    // The name and/or message could be getters, and even if it's unsafe, we do want
    // to show it to the user (See Bug 1710694).
    const name = DevToolsUtils.getProperty(obj, "name", true);
    const msg = DevToolsUtils.getProperty(obj, "message", true);
    const stack = DevToolsUtils.getProperty(obj, "stack");
    const fileName = DevToolsUtils.getProperty(obj, "fileName");
    const lineNumber = DevToolsUtils.getProperty(obj, "lineNumber");
    const columnNumber = DevToolsUtils.getProperty(obj, "columnNumber");

    grip.preview = {
      kind: "Error",
      name: hooks.createValueGrip(name),
      message: hooks.createValueGrip(msg),
      stack: hooks.createValueGrip(stack),
      fileName: hooks.createValueGrip(fileName),
      lineNumber: hooks.createValueGrip(lineNumber),
      columnNumber: hooks.createValueGrip(columnNumber),
    };

    const errorHasCause = obj.getOwnPropertyNames().includes("cause");
    if (errorHasCause) {
      grip.preview.cause = hooks.createValueGrip(
        DevToolsUtils.getProperty(obj, "cause", true)
      );
    }

    return true;
  },

  function CSSMediaRule(objectActor, grip, rawObj, className) {
    if (!rawObj || className != "CSSMediaRule" || isWorker) {
      return false;
    }
    const { hooks } = objectActor;
    grip.preview = {
      kind: "ObjectWithText",
      text: hooks.createValueGrip(rawObj.conditionText),
    };
    return true;
  },

  function CSSStyleRule(objectActor, grip, rawObj, className) {
    if (!rawObj || className != "CSSStyleRule" || isWorker) {
      return false;
    }
    const { hooks } = objectActor;
    grip.preview = {
      kind: "ObjectWithText",
      text: hooks.createValueGrip(rawObj.selectorText),
    };
    return true;
  },

  function ObjectWithURL(objectActor, grip, rawObj, className) {
    if (isWorker || !rawObj) {
      return false;
    }

    const isWindow = Window.isInstance(rawObj);
    if (!OBJECT_WITH_URL_CLASSNAMES.has(className) && !isWindow) {
      return false;
    }

    const { hooks } = objectActor;

    let url;
    if (isWindow && rawObj.location) {
      try {
        url = rawObj.location.href;
      } catch(e) {
        // This can happen when we have a cross-process window.
        // In such case, let's retrieve the url from the iframe.
        // For window.top from a remote iframe, there's no way we can't retrieve the URL,
        // so return a label that help user know what's going on.
        url = rawObj.browsingContext?.embedderElement?.src || "Restricted";
      }
    } else if (rawObj.href) {
      url = rawObj.href;
    } else {
      return false;
    }

    grip.preview = {
      kind: "ObjectWithURL",
      url: hooks.createValueGrip(url),
    };

    return true;
  },

  function ArrayLike(objectActor, grip, rawObj, className) {
    if (
      !rawObj ||
      !ARRAY_LIKE_CLASSNAMES.has(className) ||
      typeof rawObj.length != "number" ||
      isWorker
    ) {
      return false;
    }

    const { obj, hooks } = objectActor;
    grip.preview = {
      kind: "ArrayLike",
      length: rawObj.length,
    };

    if (hooks.getGripDepth() > 1) {
      return true;
    }

    const items = (grip.preview.items = []);

    for (
      let i = 0;
      i < rawObj.length && items.length < OBJECT_PREVIEW_MAX_ITEMS;
      i++
    ) {
      const value = ObjectUtils.makeDebuggeeValueIfNeeded(obj, rawObj[i]);
      items.push(hooks.createValueGrip(value));
    }

    return true;
  },

  function CSSStyleDeclaration(objectActor, grip, rawObj, className) {
    if (
      !rawObj ||
      (className != "CSSStyleDeclaration" && className != "CSS2Properties") ||
      isWorker
    ) {
      return false;
    }

    const { hooks } = objectActor;
    grip.preview = {
      kind: "MapLike",
      size: rawObj.length,
    };

    const entries = (grip.preview.entries = []);

    for (let i = 0; i < OBJECT_PREVIEW_MAX_ITEMS && i < rawObj.length; i++) {
      const prop = rawObj[i];
      const value = rawObj.getPropertyValue(prop);
      entries.push([prop, hooks.createValueGrip(value)]);
    }

    return true;
  },

  function DOMNode(objectActor, grip, rawObj, className) {
    if (
      className == "Object" ||
      !rawObj ||
      !Node.isInstance(rawObj) ||
      isWorker
    ) {
      return false;
    }

    const { obj, hooks } = objectActor;

    const preview = (grip.preview = {
      kind: "DOMNode",
      nodeType: rawObj.nodeType,
      nodeName: rawObj.nodeName,
      isConnected: rawObj.isConnected === true,
    });

    if (rawObj.nodeType == rawObj.DOCUMENT_NODE && rawObj.location) {
      preview.location = hooks.createValueGrip(rawObj.location.href);
    } else if (obj.class == "DocumentFragment") {
      preview.childNodesLength = rawObj.childNodes.length;

      if (hooks.getGripDepth() < 2) {
        preview.childNodes = [];
        for (const node of rawObj.childNodes) {
          const actor = hooks.createValueGrip(obj.makeDebuggeeValue(node));
          preview.childNodes.push(actor);
          if (preview.childNodes.length == OBJECT_PREVIEW_MAX_ITEMS) {
            break;
          }
        }
      }
    } else if (Element.isInstance(rawObj)) {
      // For HTML elements (in an HTML document, at least), the nodeName is an
      // uppercased version of the actual element name.  Check for HTML
      // elements, that is elements in the HTML namespace, and lowercase the
      // nodeName in that case.
      if (rawObj.namespaceURI == "http://www.w3.org/1999/xhtml") {
        preview.nodeName = preview.nodeName.toLowerCase();
      }

      // Add preview for DOM element attributes.
      preview.attributes = {};
      preview.attributesLength = rawObj.attributes.length;
      for (const attr of rawObj.attributes) {
        preview.attributes[attr.nodeName] = hooks.createValueGrip(attr.value);
      }
    } else if (obj.class == "Attr") {
      preview.value = hooks.createValueGrip(rawObj.value);
    } else if (
      obj.class == "Text" ||
      obj.class == "CDATASection" ||
      obj.class == "Comment"
    ) {
      preview.textContent = hooks.createValueGrip(rawObj.textContent);
    }

    return true;
  },

  function DOMEvent(objectActor, grip, rawObj) {
    if (!rawObj || !Event.isInstance(rawObj) || isWorker) {
      return false;
    }

    const { obj, hooks } = objectActor;
    const preview = (grip.preview = {
      kind: "DOMEvent",
      type: rawObj.type,
      properties: Object.create(null),
    });

    if (hooks.getGripDepth() < 2) {
      const target = obj.makeDebuggeeValue(rawObj.target);
      preview.target = hooks.createValueGrip(target);
    }

    if (obj.class == "KeyboardEvent") {
      preview.eventKind = "key";
      preview.modifiers = ObjectUtils.getModifiersForEvent(rawObj);
    }

    const props = ObjectUtils.getPropsForEvent(obj.class);

    // Add event-specific properties.
    for (const prop of props) {
      let value = rawObj[prop];
      if (ObjectUtils.isObjectOrFunction(value)) {
        // Skip properties pointing to objects.
        if (hooks.getGripDepth() > 1) {
          continue;
        }
        value = obj.makeDebuggeeValue(value);
      }
      preview.properties[prop] = hooks.createValueGrip(value);
    }

    // Add any properties we find on the event object.
    if (!props.length) {
      let i = 0;
      for (const prop in rawObj) {
        let value = rawObj[prop];
        if (
          prop == "target" ||
          prop == "type" ||
          value === null ||
          typeof value == "function"
        ) {
          continue;
        }
        if (value && typeof value == "object") {
          if (hooks.getGripDepth() > 1) {
            continue;
          }
          value = obj.makeDebuggeeValue(value);
        }
        preview.properties[prop] = hooks.createValueGrip(value);
        if (++i == OBJECT_PREVIEW_MAX_ITEMS) {
          break;
        }
      }
    }

    return true;
  },

  function DOMException(objectActor, grip, rawObj, className) {
    if (!rawObj || className !== "DOMException" || isWorker) {
      return false;
    }

    const { hooks } = objectActor;
    grip.preview = {
      kind: "DOMException",
      name: hooks.createValueGrip(rawObj.name),
      message: hooks.createValueGrip(rawObj.message),
      code: hooks.createValueGrip(rawObj.code),
      result: hooks.createValueGrip(rawObj.result),
      filename: hooks.createValueGrip(rawObj.filename),
      lineNumber: hooks.createValueGrip(rawObj.lineNumber),
      columnNumber: hooks.createValueGrip(rawObj.columnNumber),
      stack: hooks.createValueGrip(rawObj.stack),
    };

    return true;
  },

  function Object(objectActor, grip, rawObj, className) {
    return GenericObject(objectActor, grip, rawObj, className);
  },
];

module.exports = previewers;
