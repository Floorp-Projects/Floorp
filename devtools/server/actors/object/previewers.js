/* -*- indent-tabs-mode: nil; js-indent-level: 2; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cu, Ci } = require("chrome");
const { DebuggerServer } = require("devtools/server/main");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");
loader.lazyRequireGetter(this, "ObjectUtils", "devtools/server/actors/object/utils");
loader.lazyRequireGetter(this, "PropertyIterators", "devtools/server/actors/object/property-iterator");

// Number of items to preview in objects, arrays, maps, sets, lists,
// collections, etc.
const OBJECT_PREVIEW_MAX_ITEMS = 10;

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
 *
 * Functions must return false if they cannot provide preview
 * information for the debugger object, or true otherwise.
 */
const previewers = {
  String: [function(objectActor, grip, rawObj) {
    return wrappedPrimitivePreviewer("String", String, objectActor, grip, rawObj);
  }],

  Boolean: [function(objectActor, grip, rawObj) {
    return wrappedPrimitivePreviewer("Boolean", Boolean, objectActor, grip, rawObj);
  }],

  Number: [function(objectActor, grip, rawObj) {
    return wrappedPrimitivePreviewer("Number", Number, objectActor, grip, rawObj);
  }],

  Symbol: [function(objectActor, grip, rawObj) {
    return wrappedPrimitivePreviewer("Symbol", Symbol, objectActor, grip, rawObj);
  }],

  Function: [function({obj, hooks}, grip) {
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

    if (userDisplayName && typeof userDisplayName.value == "string" &&
        userDisplayName.value) {
      grip.userDisplayName = hooks.createValueGrip(userDisplayName.value);
    }

    const dbgGlobal = hooks.getGlobalDebugObject();
    if (dbgGlobal) {
      const script = dbgGlobal.makeDebuggeeValue(obj.unsafeDereference()).script;
      if (script) {
        grip.location = {
          url: script.url,
          line: script.startLine,
        };
      }
    }

    return true;
  }],

  RegExp: [function({obj, hooks}, grip) {
    const str = DevToolsUtils.callPropertyOnObject(obj, "toString");
    if (typeof str != "string") {
      return false;
    }

    grip.displayString = hooks.createValueGrip(str);
    return true;
  }],

  Date: [function({obj, hooks}, grip) {
    const time = DevToolsUtils.callPropertyOnObject(obj, "getTime");
    if (typeof time != "number") {
      return false;
    }

    grip.preview = {
      timestamp: hooks.createValueGrip(time),
    };
    return true;
  }],

  Array: [function({obj, hooks}, grip) {
    const length = ObjectUtils.getArrayLength(obj);

    grip.preview = {
      kind: "ArrayLike",
      length: length,
    };

    if (hooks.getGripDepth() > 1) {
      return true;
    }

    const raw = obj.unsafeDereference();
    const items = grip.preview.items = [];

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
        } else {
          items.push(null);
        }
      } else {
        // Workers do not have access to Cu, and when recording/replaying we
        // don't have a raw object. In either case we do not need to deal with
        // xray wrappers.
        const value = DevToolsUtils.getProperty(obj, i);
        items.push(hooks.createValueGrip(value));
      }

      if (items.length == OBJECT_PREVIEW_MAX_ITEMS) {
        break;
      }
    }

    return true;
  }],

  Set: [function(objectActor, grip) {
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

    const items = grip.preview.items = [];
    for (const item of PropertyIterators.enumSetEntries(objectActor)) {
      items.push(item);
      if (items.length == OBJECT_PREVIEW_MAX_ITEMS) {
        break;
      }
    }

    return true;
  }],

  WeakSet: [function(objectActor, grip) {
    const enumEntries = PropertyIterators.enumWeakSetEntries(objectActor);

    grip.preview = {
      kind: "ArrayLike",
      length: enumEntries.size,
    };

    // Avoid recursive object grips.
    if (objectActor.hooks.getGripDepth() > 1) {
      return true;
    }

    const items = grip.preview.items = [];
    for (const item of enumEntries) {
      items.push(item);
      if (items.length == OBJECT_PREVIEW_MAX_ITEMS) {
        break;
      }
    }

    return true;
  }],

  Map: [function(objectActor, grip) {
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

    const entries = grip.preview.entries = [];
    for (const entry of PropertyIterators.enumMapEntries(objectActor)) {
      entries.push(entry);
      if (entries.length == OBJECT_PREVIEW_MAX_ITEMS) {
        break;
      }
    }

    return true;
  }],

  WeakMap: [function(objectActor, grip) {
    const enumEntries = PropertyIterators.enumWeakMapEntries(objectActor);

    grip.preview = {
      kind: "MapLike",
      size: enumEntries.size,
    };

    if (objectActor.hooks.getGripDepth() > 1) {
      return true;
    }

    const entries = grip.preview.entries = [];
    for (const entry of enumEntries) {
      entries.push(entry);
      if (entries.length == OBJECT_PREVIEW_MAX_ITEMS) {
        break;
      }
    }

    return true;
  }],

  DOMStringMap: [function({obj, hooks}, grip, rawObj) {
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

    const entries = grip.preview.entries = [];
    for (const key of keys) {
      const value = ObjectUtils.makeDebuggeeValueIfNeeded(obj, rawObj[key]);
      entries.push([key, hooks.createValueGrip(value)]);
      if (entries.length == OBJECT_PREVIEW_MAX_ITEMS) {
        break;
      }
    }

    return true;
  }],

  Proxy: [function({obj, hooks}, grip, rawObj) {
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
        "<target>": {value: hooks.createValueGrip(obj.proxyTarget)},
        "<handler>": {value: hooks.createValueGrip(obj.proxyHandler)},
      });
    }

    return true;
  }],
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
function wrappedPrimitivePreviewer(className, classObj, objectActor, grip, rawObj) {
  const {obj, hooks} = objectActor;

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

  const canHandle = GenericObject(objectActor, grip, rawObj, className === "String");
  if (!canHandle) {
    return false;
  }

  grip.preview.wrappedValue =
    hooks.createValueGrip(ObjectUtils.makeDebuggeeValueIfNeeded(obj, v));
  return true;
}

function GenericObject(objectActor, grip, rawObj, specialStringBehavior = false) {
  const {obj, hooks} = objectActor;
  if (grip.preview || grip.displayString || hooks.getGripDepth() > 1) {
    return false;
  }

  let i = 0, names = [], symbols = [];
  const preview = grip.preview = {
    kind: "Object",
    ownProperties: Object.create(null),
    ownSymbols: [],
  };

  try {
    if (ObjectUtils.isStorage(obj)) {
      // local and session storage cannot be iterated over using
      // Object.getOwnPropertyNames() because it skips keys that are duplicated
      // on the prototype e.g. "key", "getKeys" so we need to gather the real
      // keys using the storage.key() function.
      for (let j = 0; j < rawObj.length; j++) {
        names.push(rawObj.key(j));
      }
    } else if (isReplaying) {
      // When replaying we can access a batch of properties for use in generating
      // the preview. This avoids needing to enumerate all properties.
      names = obj.getEnumerableOwnPropertyNamesForPreview();
    } else {
      names = obj.getOwnPropertyNames();
    }
    symbols = obj.getOwnPropertySymbols();
  } catch (ex) {
    // Calling getOwnPropertyNames() on some wrapped native prototypes is not
    // allowed: "cannot modify properties of a WrappedNative". See bug 952093.
  }
  preview.ownPropertiesLength = names.length;
  preview.ownSymbolsLength = symbols.length;

  let length;
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

  for (const symbol of symbols) {
    const descriptor = objectActor._propertyDescriptor(symbol, true);
    if (!descriptor) {
      continue;
    }

    preview.ownSymbols.push(Object.assign({
      descriptor,
    }, hooks.createValueGrip(symbol)));

    if (++i == OBJECT_PREVIEW_MAX_ITEMS) {
      break;
    }
  }

  if (i < OBJECT_PREVIEW_MAX_ITEMS) {
    preview.safeGetterValues = objectActor._findSafeGetterValues(
      Object.keys(preview.ownProperties),
      OBJECT_PREVIEW_MAX_ITEMS - i);
  }

  return true;
}

// Preview functions that do not rely on the object class.
previewers.Object = [
  function TypedArray({obj, hooks}, grip) {
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

    const raw = obj.unsafeDereference();

    // The raw object will be null/unavailable when interacting with a
    // replaying execution, and Cu is unavailable in workers. In either case we
    // do not need to worry about xrays.
    if (raw && !isWorker) {
      const global = Cu.getGlobalForObject(DebuggerServer);
      const classProto = global[obj.class].prototype;
      // The Xray machinery for TypedArrays denies indexed access on the grounds
      // that it's slow, and advises callers to do a structured clone instead.
      const safeView = Cu.cloneInto(classProto.subarray.call(raw, 0,
        OBJECT_PREVIEW_MAX_ITEMS), global);
      const items = grip.preview.items = [];
      for (let i = 0; i < safeView.length; i++) {
        items.push(safeView[i]);
      }
    }

    return true;
  },

  function Error({obj, hooks}, grip) {
    switch (obj.class) {
      case "Error":
      case "EvalError":
      case "RangeError":
      case "ReferenceError":
      case "SyntaxError":
      case "TypeError":
      case "URIError":
        const name = DevToolsUtils.getProperty(obj, "name");
        const msg = DevToolsUtils.getProperty(obj, "message");
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
        return true;
      default:
        return false;
    }
  },

  function CSSMediaRule({obj, hooks}, grip, rawObj) {
    if (isWorker || !rawObj || obj.class != "CSSMediaRule") {
      return false;
    }
    grip.preview = {
      kind: "ObjectWithText",
      text: hooks.createValueGrip(rawObj.conditionText),
    };
    return true;
  },

  function CSSStyleRule({obj, hooks}, grip, rawObj) {
    if (isWorker || !rawObj || obj.class != "CSSStyleRule") {
      return false;
    }
    grip.preview = {
      kind: "ObjectWithText",
      text: hooks.createValueGrip(rawObj.selectorText),
    };
    return true;
  },

  function ObjectWithURL({obj, hooks}, grip, rawObj) {
    if (isWorker || !rawObj || !(obj.class == "CSSImportRule" ||
                                 obj.class == "CSSStyleSheet" ||
                                 obj.class == "Location" ||
                                 rawObj instanceof Ci.nsIDOMWindow)) {
      return false;
    }

    let url;
    if (rawObj instanceof Ci.nsIDOMWindow && rawObj.location) {
      url = rawObj.location.href;
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

  function ArrayLike({obj, hooks}, grip, rawObj) {
    if (isWorker || !rawObj ||
        obj.class != "DOMStringList" &&
        obj.class != "DOMTokenList" &&
        obj.class != "CSSRuleList" &&
        obj.class != "MediaList" &&
        obj.class != "StyleSheetList" &&
        obj.class != "NamedNodeMap" &&
        obj.class != "FileList" &&
        obj.class != "NodeList") {
      return false;
    }

    if (typeof rawObj.length != "number") {
      return false;
    }

    grip.preview = {
      kind: "ArrayLike",
      length: rawObj.length,
    };

    if (hooks.getGripDepth() > 1) {
      return true;
    }

    const items = grip.preview.items = [];

    for (let i = 0; i < rawObj.length &&
                    items.length < OBJECT_PREVIEW_MAX_ITEMS; i++) {
      const value = ObjectUtils.makeDebuggeeValueIfNeeded(obj, rawObj[i]);
      items.push(hooks.createValueGrip(value));
    }

    return true;
  },

  function CSSStyleDeclaration({obj, hooks}, grip, rawObj) {
    if (isWorker || !rawObj ||
        (obj.class != "CSSStyleDeclaration" &&
         obj.class != "CSS2Properties")) {
      return false;
    }

    grip.preview = {
      kind: "MapLike",
      size: rawObj.length,
    };

    const entries = grip.preview.entries = [];

    for (let i = 0; i < OBJECT_PREVIEW_MAX_ITEMS &&
                    i < rawObj.length; i++) {
      const prop = rawObj[i];
      const value = rawObj.getPropertyValue(prop);
      entries.push([prop, hooks.createValueGrip(value)]);
    }

    return true;
  },

  function DOMNode({obj, hooks}, grip, rawObj) {
    if (isWorker || obj.class == "Object" || !rawObj ||
        !Node.isInstance(rawObj)) {
      return false;
    }

    const preview = grip.preview = {
      kind: "DOMNode",
      nodeType: rawObj.nodeType,
      nodeName: rawObj.nodeName,
      isConnected: rawObj.isConnected === true,
    };

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
    } else if (obj.class == "Text" ||
               obj.class == "CDATASection" ||
               obj.class == "Comment") {
      preview.textContent = hooks.createValueGrip(rawObj.textContent);
    }

    return true;
  },

  function DOMEvent({obj, hooks}, grip, rawObj) {
    if (isWorker || !rawObj || !Event.isInstance(rawObj)) {
      return false;
    }

    const preview = grip.preview = {
      kind: "DOMEvent",
      type: rawObj.type,
      properties: Object.create(null),
    };

    if (hooks.getGripDepth() < 2) {
      const target = obj.makeDebuggeeValue(rawObj.target);
      preview.target = hooks.createValueGrip(target);
    }

    const props = [];
    if (obj.class == "MouseEvent" ||
        obj.class == "DragEvent" ||
        obj.class == "PointerEvent" ||
        obj.class == "SimpleGestureEvent" ||
        obj.class == "WheelEvent") {
      props.push("buttons", "clientX", "clientY", "layerX", "layerY");
    } else if (obj.class == "KeyboardEvent") {
      const modifiers = [];
      if (rawObj.altKey) {
        modifiers.push("Alt");
      }
      if (rawObj.ctrlKey) {
        modifiers.push("Control");
      }
      if (rawObj.metaKey) {
        modifiers.push("Meta");
      }
      if (rawObj.shiftKey) {
        modifiers.push("Shift");
      }
      preview.eventKind = "key";
      preview.modifiers = modifiers;

      props.push("key", "charCode", "keyCode");
    } else if (obj.class == "TransitionEvent") {
      props.push("propertyName", "pseudoElement");
    } else if (obj.class == "AnimationEvent") {
      props.push("animationName", "pseudoElement");
    } else if (obj.class == "ClipboardEvent") {
      props.push("clipboardData");
    }

    // Add event-specific properties.
    for (const prop of props) {
      let value = rawObj[prop];
      if (value && (typeof value == "object" || typeof value == "function")) {
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
        if (prop == "target" || prop == "type" || value === null ||
            typeof value == "function") {
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

  function DOMException({obj, hooks}, grip, rawObj) {
    if (isWorker || !rawObj || obj.class !== "DOMException") {
      return false;
    }

    grip.preview = {
      kind: "DOMException",
      name: hooks.createValueGrip(rawObj.name),
      message: hooks.createValueGrip(rawObj.message),
      code: hooks.createValueGrip(rawObj.code),
      result: hooks.createValueGrip(rawObj.result),
      filename: hooks.createValueGrip(rawObj.filename),
      lineNumber: hooks.createValueGrip(rawObj.lineNumber),
      columnNumber: hooks.createValueGrip(rawObj.columnNumber),
    };

    return true;
  },

  function PseudoArray({obj, hooks}, grip, rawObj) {
    // An object is considered a pseudo-array if all the following apply:
    // - All its properties are array indices except, optionally, a "length" property.
    // - At least it has the "0" array index.
    // - The array indices are consecutive.
    // - The value of "length", if present, is the number of array indices.

    // Don't generate pseudo array previews when replaying. We don't want to
    // have to enumerate all the properties in order to determine this.
    if (isReplaying) {
      return false;
    }

    let keys;
    try {
      keys = obj.getOwnPropertyNames();
    } catch (err) {
      // The above can throw when the debuggee does not subsume the object's
      // compartment, or for some WrappedNatives like Cu.Sandbox.
      return false;
    }
    let {length} = keys;
    if (length === 0) {
      return false;
    }

    // Array indices should be sorted at the beginning, from smallest to largest.
    // Other properties should be at the end, so check if the last one is "length".
    if (keys[length - 1] === "length") {
      --length;
      if (length === 0 || length !== DevToolsUtils.getProperty(obj, "length")) {
        return false;
      }
    }

    // Check that the last key is the array index expected at that position.
    const lastKey = keys[length - 1];
    if (!ObjectUtils.isArrayIndex(lastKey) || +lastKey !== length - 1) {
      return false;
    }

    grip.preview = {
      kind: "ArrayLike",
      length: length,
    };

    // Avoid recursive object grips.
    if (hooks.getGripDepth() > 1) {
      return true;
    }

    const items = grip.preview.items = [];
    const numItems = Math.min(OBJECT_PREVIEW_MAX_ITEMS, length);

    for (let i = 0; i < numItems; ++i) {
      const desc = obj.getOwnPropertyDescriptor(i);
      if (desc && "value" in desc) {
        items.push(hooks.createValueGrip(desc.value));
      } else {
        items.push(null);
      }
    }

    return true;
  },

  function Object(objectActor, grip, rawObj) {
    return GenericObject(objectActor, grip, rawObj, /* specialStringBehavior = */ false);
  },
];

module.exports = previewers;
