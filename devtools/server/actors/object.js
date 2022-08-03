/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cu } = require("chrome");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const { assert } = DevToolsUtils;

const protocol = require("devtools/shared/protocol");
const { objectSpec } = require("devtools/shared/specs/object");

loader.lazyRequireGetter(
  this,
  "PropertyIteratorActor",
  "devtools/server/actors/object/property-iterator",
  true
);
loader.lazyRequireGetter(
  this,
  "SymbolIteratorActor",
  "devtools/server/actors/object/symbol-iterator",
  true
);
loader.lazyRequireGetter(
  this,
  "PrivatePropertiesIteratorActor",
  "devtools/server/actors/object/private-properties-iterator",
  true
);
loader.lazyRequireGetter(
  this,
  "previewers",
  "devtools/server/actors/object/previewers"
);

loader.lazyRequireGetter(
  this,
  "makeSideeffectFreeDebugger",
  "devtools/server/actors/webconsole/eval-with-debugger",
  true
);

// ContentDOMReference requires ChromeUtils, which isn't available in worker context.
if (!isWorker) {
  loader.lazyRequireGetter(
    this,
    "ContentDOMReference",
    "resource://gre/modules/ContentDOMReference.jsm",
    true
  );
}

const {
  getArrayLength,
  getPromiseState,
  getStorageLength,
  isArray,
  isStorage,
  isTypedArray,
} = require("devtools/server/actors/object/utils");

const proto = {
  /**
   * Creates an actor for the specified object.
   *
   * @param obj Debugger.Object
   *        The debuggee object.
   * @param Object
   *        A collection of abstract methods that are implemented by the caller.
   *        ObjectActor requires the following functions to be implemented by
   *        the caller:
   *          - createValueGrip
   *              Creates a value grip for the given object
   *          - createEnvironmentActor
   *              Creates and return an environment actor
   *          - getGripDepth
   *              An actor's grip depth getter
   *          - incrementGripDepth
   *              Increment the actor's grip depth
   *          - decrementGripDepth
   *              Decrement the actor's grip depth
   */
  initialize(
    obj,
    {
      thread,
      createValueGrip: createValueGripHook,
      createEnvironmentActor,
      getGripDepth,
      incrementGripDepth,
      decrementGripDepth,
    },
    conn
  ) {
    assert(
      !obj.optimizedOut,
      "Should not create object actors for optimized out values!"
    );
    protocol.Actor.prototype.initialize.call(this, conn);

    this.conn = conn;
    this.obj = obj;
    this.thread = thread;
    this.hooks = {
      createValueGrip: createValueGripHook,
      createEnvironmentActor,
      getGripDepth,
      incrementGripDepth,
      decrementGripDepth,
    };
  },

  rawValue() {
    return this.obj.unsafeDereference();
  },

  addWatchpoint(property, label, watchpointType) {
    this.thread.addWatchpoint(this, { property, label, watchpointType });
  },

  removeWatchpoint(property) {
    this.thread.removeWatchpoint(this, property);
  },

  removeWatchpoints() {
    this.thread.removeWatchpoint(this);
  },

  /**
   * Returns a grip for this actor for returning in a protocol message.
   */
  form() {
    const g = {
      type: "object",
      actor: this.actorID,
    };

    const unwrapped = DevToolsUtils.unwrap(this.obj);
    if (unwrapped === undefined) {
      // Objects belonging to an invisible-to-debugger compartment might be proxies,
      // so just in case they shouldn't be accessed.
      g.class = "InvisibleToDebugger: " + this.obj.class;
      return g;
    }

    if (unwrapped?.isProxy) {
      // Proxy objects can run traps when accessed, so just create a preview with
      // the target and the handler.
      g.class = "Proxy";
      this.hooks.incrementGripDepth();
      previewers.Proxy[0](this, g, null);
      this.hooks.decrementGripDepth();
      return g;
    }

    if (this.thread?._parent?.customFormatters) {
      const header = this.customFormatterHeader();
      if (header) {
        return {
          ...g,
          ...header,
        };
      }
    }

    const ownPropertyLength = this._getOwnPropertyLength();

    Object.assign(g, {
      // If the debuggee does not subsume the object's compartment, most properties won't
      // be accessible. Cross-orgin Window and Location objects might expose some, though.
      // Change the displayed class, but when creating the preview use the original one.
      class: unwrapped === null ? "Restricted" : this.obj.class,
      ownPropertyLength: Number.isFinite(ownPropertyLength)
        ? ownPropertyLength
        : undefined,
      extensible: this.obj.isExtensible(),
      frozen: this.obj.isFrozen(),
      sealed: this.obj.isSealed(),
      isError: this.obj.isError,
    });

    this.hooks.incrementGripDepth();

    if (g.class == "Function") {
      g.isClassConstructor = this.obj.isClassConstructor;
    }

    const raw = this.getRawObject();
    this._populateGripPreview(g, raw);
    this.hooks.decrementGripDepth();

    if (raw && Node.isInstance(raw) && ContentDOMReference) {
      // ContentDOMReference.get takes a DOM element and returns an object with
      // its browsing context id, as well as a unique identifier. We are putting it in
      // the grip here in order to be able to retrieve the node later, potentially from a
      // different DevToolsServer running in the same process.
      // If ContentDOMReference.get throws, we simply don't add the property to the grip.
      try {
        g.contentDomReference = ContentDOMReference.get(raw);
      } catch (e) {}
    }

    return g;
  },

  _getOwnPropertyLength() {
    if (isTypedArray(this.obj)) {
      // Bug 1348761: getOwnPropertyNames is unnecessary slow on TypedArrays
      return getArrayLength(this.obj);
    }

    if (isStorage(this.obj)) {
      return getStorageLength(this.obj);
    }

    try {
      return this.obj.getOwnPropertyNamesLength();
    } catch (err) {
      // The above can throw when the debuggee does not subsume the object's
      // compartment, or for some WrappedNatives like Cu.Sandbox.
    }

    return null;
  },

  getRawObject() {
    let raw = this.obj.unsafeDereference();

    // If Cu is not defined, we are running on a worker thread, where xrays
    // don't exist.
    if (raw && Cu) {
      raw = Cu.unwaiveXrays(raw);
    }

    if (raw && !DevToolsUtils.isSafeJSObject(raw)) {
      raw = null;
    }

    return raw;
  },

  /**
   * Populate the `preview` property on `grip` given its type.
   */
  _populateGripPreview(grip, raw) {
    // Cache obj.class as it can be costly if this is in a hot path (e.g. logging objects
    // within a for loop).
    const className = this.obj.class;
    for (const previewer of previewers[className] || previewers.Object) {
      try {
        const previewerResult = previewer(this, grip, raw, className);
        if (previewerResult) {
          return;
        }
      } catch (e) {
        const msg =
          "ObjectActor.prototype._populateGripPreview previewer function";
        DevToolsUtils.reportException(msg, e);
      }
    }
  },

  /**
   * Returns an object exposing the internal Promise state.
   */
  promiseState() {
    const { state, value, reason } = getPromiseState(this.obj);
    const promiseState = { state };

    if (state == "fulfilled") {
      promiseState.value = this.hooks.createValueGrip(value);
    } else if (state == "rejected") {
      promiseState.reason = this.hooks.createValueGrip(reason);
    }

    promiseState.creationTimestamp = Date.now() - this.obj.promiseLifetime;

    // Only add the timeToSettle property if the Promise isn't pending.
    if (state !== "pending") {
      promiseState.timeToSettle = this.obj.promiseTimeToResolution;
    }

    return { promiseState };
  },

  /**
   * Creates an actor to iterate over an object property names and values.
   * See PropertyIteratorActor constructor for more info about options param.
   *
   * @param options object
   */
  enumProperties(options) {
    return PropertyIteratorActor(this, options, this.conn);
  },

  /**
   * Creates an actor to iterate over entries of a Map/Set-like object.
   */
  enumEntries() {
    return PropertyIteratorActor(this, { enumEntries: true }, this.conn);
  },

  /**
   * Creates an actor to iterate over an object symbols properties.
   */
  enumSymbols() {
    return SymbolIteratorActor(this, this.conn);
  },

  /**
   * Creates an actor to iterate over an object private properties.
   */
  enumPrivateProperties() {
    return PrivatePropertiesIteratorActor(this, this.conn);
  },

  /**
   * Handle a protocol request to provide the prototype and own properties of
   * the object.
   *
   * @returns {Object} An object containing the data of this.obj, of the following form:
   *          - {Object} prototype: The descriptor of this.obj's prototype.
   *          - {Object} ownProperties: an object where the keys are the names of the
   *                     this.obj's ownProperties, and the values the descriptors of
   *                     the properties.
   *          - {Array} ownSymbols: An array containing all descriptors of this.obj's
   *                    ownSymbols. Here we have an array, and not an object like for
   *                    ownProperties, because we can have multiple symbols with the same
   *                    name in this.obj, e.g. `{[Symbol()]: "a", [Symbol()]: "b"}`.
   *          - {Object} safeGetterValues: an object that maps this.obj's property names
   *                     with safe getters descriptors.
   */
  prototypeAndProperties() {
    let objProto = null;
    let names = [];
    let symbols = [];
    if (DevToolsUtils.isSafeDebuggerObject(this.obj)) {
      try {
        objProto = this.obj.proto;
        names = this.obj.getOwnPropertyNames();
        symbols = this.obj.getOwnPropertySymbols();
      } catch (err) {
        // The above can throw when the debuggee does not subsume the object's
        // compartment, or for some WrappedNatives like Cu.Sandbox.
      }
    }

    const ownProperties = Object.create(null);
    const ownSymbols = [];

    for (const name of names) {
      ownProperties[name] = this._propertyDescriptor(name);
    }

    for (const sym of symbols) {
      ownSymbols.push({
        name: sym.toString(),
        descriptor: this._propertyDescriptor(sym),
      });
    }

    return {
      prototype: this.hooks.createValueGrip(objProto),
      ownProperties,
      ownSymbols,
      safeGetterValues: this._findSafeGetterValues(names),
    };
  },

  /**
   * Find the safe getter values for the current Debugger.Object, |this.obj|.
   *
   * @private
   * @param array ownProperties
   *        The array that holds the list of known ownProperties names for
   *        |this.obj|.
   * @param number [limit=Infinity]
   *        Optional limit of getter values to find.
   * @return object
   *         An object that maps property names to safe getter descriptors as
   *         defined by the remote debugging protocol.
   */
  _findSafeGetterValues(ownProperties, limit = Infinity) {
    const safeGetterValues = Object.create(null);
    let obj = this.obj;
    let level = 0,
      currentGetterValuesCount = 0;

    // Do not search safe getters in unsafe objects.
    if (!DevToolsUtils.isSafeDebuggerObject(obj)) {
      return safeGetterValues;
    }

    // Most objects don't have any safe getters but inherit some from their
    // prototype. Avoid calling getOwnPropertyNames on objects that may have
    // many properties like Array, strings or js objects. That to avoid
    // freezing firefox when doing so.
    if (isArray(this.obj) || ["Object", "String"].includes(this.obj.class)) {
      obj = obj.proto;
      level++;
    }

    while (obj && DevToolsUtils.isSafeDebuggerObject(obj)) {
      for (const name of this._findSafeGetters(obj)) {
        // Avoid overwriting properties from prototypes closer to this.obj. Also
        // avoid providing safeGetterValues from prototypes if property |name|
        // is already defined as an own property.
        if (
          name in safeGetterValues ||
          (obj != this.obj && ownProperties.includes(name))
        ) {
          continue;
        }

        // Ignore __proto__ on Object.prototye.
        if (!obj.proto && name == "__proto__") {
          continue;
        }

        const desc = safeGetOwnPropertyDescriptor(obj, name);
        if (!desc?.get) {
          // If no getter matches the name, the cache is stale and should be cleaned up.
          obj._safeGetters = null;
          continue;
        }

        const getterValue = this._evaluateGetter(desc.get);
        if (getterValue === undefined) {
          continue;
        }

        // Treat an already-rejected Promise as we would a thrown exception
        // by not including it as a safe getter value (see Bug 1477765).
        if (isRejectedPromise(getterValue)) {
          // Until we have a good way to handle Promise rejections through the
          // debugger API (Bug 1478076), call `catch` when it's safe to do so.
          const raw = getterValue.unsafeDereference();
          if (DevToolsUtils.isSafeJSObject(raw)) {
            raw.catch(e => e);
          }
          continue;
        }

        // WebIDL attributes specified with the LenientThis extended attribute
        // return undefined and should be ignored.
        safeGetterValues[name] = {
          getterValue: this.hooks.createValueGrip(getterValue),
          getterPrototypeLevel: level,
          enumerable: desc.enumerable,
          writable: level == 0 ? desc.writable : true,
        };

        ++currentGetterValuesCount;
        if (currentGetterValuesCount == limit) {
          return safeGetterValues;
        }
      }

      obj = obj.proto;
      level++;
    }

    return safeGetterValues;
  },

  /**
   * Evaluate the getter function |desc.get|.
   * @param {Object} getter
   */
  _evaluateGetter(getter) {
    const result = getter.call(this.obj);
    if (!result || "throw" in result) {
      return undefined;
    }

    let getterValue = undefined;
    if ("return" in result) {
      getterValue = result.return;
    } else if ("yield" in result) {
      getterValue = result.yield;
    }

    return getterValue;
  },

  /**
   * Find the safe getters for a given Debugger.Object. Safe getters are native
   * getters which are safe to execute.
   *
   * @private
   * @param Debugger.Object object
   *        The Debugger.Object where you want to find safe getters.
   * @return Set
   *         A Set of names of safe getters. This result is cached for each
   *         Debugger.Object.
   */
  _findSafeGetters(object) {
    if (object._safeGetters) {
      return object._safeGetters;
    }

    const getters = new Set();

    if (!DevToolsUtils.isSafeDebuggerObject(object)) {
      object._safeGetters = getters;
      return getters;
    }

    let names = [];
    try {
      names = object.getOwnPropertyNames();
    } catch (ex) {
      // Calling getOwnPropertyNames() on some wrapped native prototypes is not
      // allowed: "cannot modify properties of a WrappedNative". See bug 952093.
    }

    for (const name of names) {
      let desc = null;
      try {
        desc = object.getOwnPropertyDescriptor(name);
      } catch (e) {
        // Calling getOwnPropertyDescriptor on wrapped native prototypes is not
        // allowed (bug 560072).
      }
      if (!desc || desc.value !== undefined || !("get" in desc)) {
        continue;
      }

      if (DevToolsUtils.hasSafeGetter(desc)) {
        getters.add(name);
      }
    }

    object._safeGetters = getters;
    return getters;
  },

  /**
   * Handle a protocol request to provide the prototype of the object.
   */
  prototype() {
    let objProto = null;
    if (DevToolsUtils.isSafeDebuggerObject(this.obj)) {
      objProto = this.obj.proto;
    }
    return { prototype: this.hooks.createValueGrip(objProto) };
  },

  /**
   * Handle a protocol request to provide the property descriptor of the
   * object's specified property.
   *
   * @param name string
   *        The property we want the description of.
   */
  property(name) {
    if (!name) {
      return this.throwError(
        "missingParameter",
        "no property name was specified"
      );
    }

    return { descriptor: this._propertyDescriptor(name) };
  },

  /**
   * Handle a protocol request to provide the value of the object's
   * specified property.
   *
   * Note: Since this will evaluate getters, it can trigger execution of
   * content code and may cause side effects. This endpoint should only be used
   * when you are confident that the side-effects will be safe, or the user
   * is expecting the effects.
   *
   * @param {string} name
   *        The property we want the value of.
   * @param {string|null} receiverId
   *        The actorId of the receiver to be used if the property is a getter.
   *        If null or invalid, the receiver will be the referent.
   */
  propertyValue(name, receiverId) {
    if (!name) {
      return this.throwError(
        "missingParameter",
        "no property name was specified"
      );
    }

    let receiver;
    if (receiverId) {
      const receiverActor = this.conn.getActor(receiverId);
      if (receiverActor) {
        receiver = receiverActor.obj;
      }
    }

    const value = receiver
      ? this.obj.getProperty(name, receiver)
      : this.obj.getProperty(name);

    return { value: this._buildCompletion(value) };
  },

  /**
   * Handle a protocol request to evaluate a function and provide the value of
   * the result.
   *
   * Note: Since this will evaluate the function, it can trigger execution of
   * content code and may cause side effects. This endpoint should only be used
   * when you are confident that the side-effects will be safe, or the user
   * is expecting the effects.
   *
   * @param {any} context
   *        The 'this' value to call the function with.
   * @param {Array<any>} args
   *        The array of un-decoded actor objects, or primitives.
   */
  apply(context, args) {
    if (!this.obj.callable) {
      return this.throwError("notCallable", "debugee object is not callable");
    }

    const debugeeContext = this._getValueFromGrip(context);
    const debugeeArgs = args && args.map(this._getValueFromGrip, this);

    const value = this.obj.apply(debugeeContext, debugeeArgs);

    return { value: this._buildCompletion(value) };
  },

  _getValueFromGrip(grip) {
    if (typeof grip !== "object" || !grip) {
      return grip;
    }

    if (typeof grip.actor !== "string") {
      return this.throwError(
        "invalidGrip",
        "grip argument did not include actor ID"
      );
    }

    const actor = this.conn.getActor(grip.actor);

    if (!actor) {
      return this.throwError(
        "unknownActor",
        "grip actor did not match a known object"
      );
    }

    return actor.obj;
  },

  /**
   * Converts a Debugger API completion value record into an equivalent
   * object grip for use by the API.
   *
   * See https://firefox-source-docs.mozilla.org/devtools-user/debugger-api/
   * for more specifics on the expected behavior.
   */
  _buildCompletion(value) {
    let completionGrip = null;

    // .apply result will be falsy if the script being executed is terminated
    // via the "slow script" dialog.
    if (value) {
      completionGrip = {};
      if ("return" in value) {
        completionGrip.return = this.hooks.createValueGrip(value.return);
      }
      if ("throw" in value) {
        completionGrip.throw = this.hooks.createValueGrip(value.throw);
      }
    }

    return completionGrip;
  },

  /**
   * A helper method that creates a property descriptor for the provided object,
   * properly formatted for sending in a protocol response.
   *
   * @private
   * @param string name
   *        The property that the descriptor is generated for.
   * @param boolean [onlyEnumerable]
   *        Optional: true if you want a descriptor only for an enumerable
   *        property, false otherwise.
   * @return object|undefined
   *         The property descriptor, or undefined if this is not an enumerable
   *         property and onlyEnumerable=true.
   */
  _propertyDescriptor(name, onlyEnumerable) {
    if (!DevToolsUtils.isSafeDebuggerObject(this.obj)) {
      return undefined;
    }

    let desc;
    try {
      desc = this.obj.getOwnPropertyDescriptor(name);
    } catch (e) {
      // Calling getOwnPropertyDescriptor on wrapped native prototypes is not
      // allowed (bug 560072). Inform the user with a bogus, but hopefully
      // explanatory, descriptor.
      return {
        configurable: false,
        writable: false,
        enumerable: false,
        value: e.name,
      };
    }

    if (isStorage(this.obj)) {
      if (name === "length") {
        return undefined;
      }
      return desc;
    }

    if (!desc || (onlyEnumerable && !desc.enumerable)) {
      return undefined;
    }

    const retval = {
      configurable: desc.configurable,
      enumerable: desc.enumerable,
    };
    const obj = this.rawValue();

    if ("value" in desc) {
      retval.writable = desc.writable;
      retval.value = this.hooks.createValueGrip(desc.value);
    } else if (this.thread.getWatchpoint(obj, name.toString())) {
      const watchpoint = this.thread.getWatchpoint(obj, name.toString());
      retval.value = this.hooks.createValueGrip(watchpoint.desc.value);
      retval.watchpoint = watchpoint.watchpointType;
    } else {
      if ("get" in desc) {
        retval.get = this.hooks.createValueGrip(desc.get);
      }

      if ("set" in desc) {
        retval.set = this.hooks.createValueGrip(desc.set);
      }
    }
    return retval;
  },

  /**
   * Handle a protocol request to get the target and handler internal slots of a proxy.
   */
  proxySlots() {
    // There could be transparent security wrappers, unwrap to check if it's a proxy.
    // However, retrieve proxyTarget and proxyHandler from `this.obj` to avoid exposing
    // the unwrapped target and handler.
    const unwrapped = DevToolsUtils.unwrap(this.obj);
    if (!unwrapped || !unwrapped.isProxy) {
      return this.throwError(
        "objectNotProxy",
        "'proxySlots' request is only valid for grips with a 'Proxy' class."
      );
    }
    return {
      proxyTarget: this.hooks.createValueGrip(this.obj.proxyTarget),
      proxyHandler: this.hooks.createValueGrip(this.obj.proxyHandler),
    };
  },

  customFormatterHeader() {
    const rawValue = this.rawValue();
    const globalWrapper = Cu.getGlobalForObject(rawValue);
    const global = globalWrapper?.wrappedJSObject;
    if (global && Array.isArray(global.devtoolsFormatters)) {
      // We're using the same setup as the eager evaluation to ensure evaluating
      // the custom formatter's functions doesn't have any side effects.
      const dbg = makeSideeffectFreeDebugger();
      const dbgGlobal = dbg.makeGlobalObjectReference(global);

      for (const [index, formatter] of global.devtoolsFormatters.entries()) {
        if (typeof formatter?.header !== "function") {
          continue;
        }

        // TODO: Any issues regarding the implementation will be covered in https://bugzil.la/1776611.
        try {
          const formatterHeaderDbgValue = dbgGlobal.makeDebuggeeValue(
            formatter.header
          );
          const debuggeeValue = dbgGlobal.makeDebuggeeValue(rawValue);
          const header = formatterHeaderDbgValue.call(dbgGlobal, debuggeeValue);
          if (header?.return?.class === "Array") {
            let hasBody = false;
            if (typeof formatter?.hasBody === "function") {
              const formatterHasBodyDbgValue = dbgGlobal.makeDebuggeeValue(
                formatter.hasBody
              );
              hasBody = formatterHasBodyDbgValue.call(dbgGlobal, debuggeeValue);
            }

            return {
              useCustomFormatter: true,
              customFormatterIndex: index,
              // As the value represents an array coming from the page,
              // we're cloning it to avoid any interferences with the original
              // variable.
              header: global.structuredClone(header.return.unsafeDereference()),
              hasBody: !!hasBody.return,
            };
          }
        } catch (e) {
          // TODO: For now, just dump the exception. Proper error handling will be done
          // in bug 1764439.
          dump(`💥 ${e}\n`);
        } finally {
          // We need to be absolutely sure that the sideeffect-free debugger's
          // debuggees are removed because otherwise we risk them terminating
          // execution of later code in the case of unexpected exceptions.
          dbg.removeAllDebuggees();
        }
      }
    }

    return null;
  },

  /**
   * Handle a protocol request to get the custom formatter body for an object
   *
   * @param number customFormatterIndex
   *        Index of the custom formatter used for the object
   */
  customFormatterBody(customFormatterIndex) {
    const rawValue = this.rawValue();
    const globalWrapper = Cu.getGlobalForObject(rawValue);
    const global = globalWrapper?.wrappedJSObject;

    // Use makeSideeffectFreeDebugger (from eval-with-debugger.js) and the debugger
    // object for each formatter and use `call` (https://searchfox.org/mozilla-central/rev/5e15e00fa247cba5b765727496619bf9010ed162/js/src/doc/Debugger/Debugger.Object.md#484)
    const dbg = makeSideeffectFreeDebugger();
    try {
      const dbgGlobal = dbg.makeGlobalObjectReference(global);
      const formatter = global.devtoolsFormatters[customFormatterIndex];
      const formatterBodyDbgValue =
        formatter && dbgGlobal.makeDebuggeeValue(formatter.body);
      const body = formatterBodyDbgValue.call(
        dbgGlobal,
        dbgGlobal.makeDebuggeeValue(rawValue)
      );
      if (body?.return?.class === "Array") {
        return {
          // As the value is represents an array coming from the page,
          // we're cloning it to avoid any interferences with the original
          // variable.
          customFormatterBody: global.structuredClone(
            body.return.unsafeDereference()
          ),
        };
      }
    } catch (e) {
      // TODO: For now, just dump the exception. Proper error handling will be done
      // in bug 1764439.
      dump(`💥 ${e}\n`);
    } finally {
      // We need to be absolutely sure that the sideeffect-free debugger's
      // debuggees are removed because otherwise we risk them terminating
      // execution of later code in the case of unexpected exceptions.
      dbg.removeAllDebuggees();
    }

    return {};
  },

  /**
   * Release the actor, when it isn't needed anymore.
   * Protocol.js uses this release method to call the destroy method.
   */
  release() {},
};

exports.ObjectActor = protocol.ActorClassWithSpec(objectSpec, proto);
exports.ObjectActorProto = proto;

function safeGetOwnPropertyDescriptor(obj, name) {
  let desc = null;
  try {
    desc = obj.getOwnPropertyDescriptor(name);
  } catch (ex) {
    // The above can throw if the cache becomes stale.
  }
  return desc;
}

/**
 * Check if the value is rejected promise
 *
 * @param {Object} getterValue
 * @returns {boolean} true if the value is rejected promise, false otherwise.
 */
function isRejectedPromise(getterValue) {
  return (
    getterValue &&
    getterValue.class == "Promise" &&
    getterValue.promiseState == "rejected"
  );
}
