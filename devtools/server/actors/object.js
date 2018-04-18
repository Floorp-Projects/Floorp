/* -*- indent-tabs-mode: nil; js-indent-level: 2; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cu } = require("chrome");
const { GeneratedLocation } = require("devtools/server/actors/common");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const { assert } = DevToolsUtils;

loader.lazyRequireGetter(this, "PropertyIteratorActor", "devtools/server/actors/object/property-iterator", true);
loader.lazyRequireGetter(this, "SymbolIteratorActor", "devtools/server/actors/object/symbol-iterator", true);
loader.lazyRequireGetter(this, "previewers", "devtools/server/actors/object/previewers");
loader.lazyRequireGetter(this, "stringify", "devtools/server/actors/object/stringifiers");

const {
  getArrayLength,
  getPromiseState,
  isArray,
  isTypedArray,
} = require("devtools/server/actors/object/utils");
/**
 * Creates an actor for the specified object.
 *
 * @param obj Debugger.Object
 *        The debuggee object.
 * @param hooks Object
 *        A collection of abstract methods that are implemented by the caller.
 *        ObjectActor requires the following functions to be implemented by
 *        the caller:
 *          - createValueGrip
 *              Creates a value grip for the given object
 *          - sources
 *              TabSources getter that manages the sources of a thread
 *          - createEnvironmentActor
 *              Creates and return an environment actor
 *          - getGripDepth
 *              An actor's grip depth getter
 *          - incrementGripDepth
 *              Increment the actor's grip depth
 *          - decrementGripDepth
 *              Decrement the actor's grip depth
 *          - globalDebugObject
 *              The Debuggee Global Object as given by the ThreadActor
 */
function ObjectActor(obj, {
  createValueGrip: createValueGripHook,
  sources,
  createEnvironmentActor,
  getGripDepth,
  incrementGripDepth,
  decrementGripDepth,
  getGlobalDebugObject
}) {
  assert(!obj.optimizedOut,
         "Should not create object actors for optimized out values!");
  this.obj = obj;
  this.hooks = {
    createValueGrip: createValueGripHook,
    sources,
    createEnvironmentActor,
    getGripDepth,
    incrementGripDepth,
    decrementGripDepth,
    getGlobalDebugObject
  };
  this.iterators = new Set();
}

ObjectActor.prototype = {
  actorPrefix: "obj",

  rawValue: function() {
    return this.obj.unsafeDereference();
  },

  /**
   * Returns a grip for this actor for returning in a protocol message.
   */
  grip: function() {
    let g = {
      "type": "object",
      "actor": this.actorID,
      "class": this.obj.class,
    };

    let unwrapped = DevToolsUtils.unwrap(this.obj);

    // Unsafe objects must be treated carefully.
    if (!DevToolsUtils.isSafeDebuggerObject(this.obj)) {
      if (DevToolsUtils.isCPOW(this.obj)) {
        // Cross-process object wrappers can't be accessed.
        g.class = "CPOW: " + g.class;
      } else if (unwrapped === undefined) {
        // Objects belonging to an invisible-to-debugger compartment might be proxies,
        // so just in case they shouldn't be accessed.
        g.class = "InvisibleToDebugger: " + g.class;
      } else if (unwrapped.isProxy) {
        // Proxy objects can run traps when accessed, so just create a preview with
        // the target and the handler.
        g.class = "Proxy";
        this.hooks.incrementGripDepth();
        previewers.Proxy[0](this, g, null);
        this.hooks.decrementGripDepth();
      }
      return g;
    }

    // If the debuggee does not subsume the object's compartment, most properties won't
    // be accessible. Cross-orgin Window and Location objects might expose some, though.
    // Change the displayed class, but when creating the preview use the original one.
    if (unwrapped === null) {
      g.class = "Restricted";
    }

    this.hooks.incrementGripDepth();

    g.extensible = this.obj.isExtensible();
    g.frozen = this.obj.isFrozen();
    g.sealed = this.obj.isSealed();

    if (g.class == "Promise") {
      g.promiseState = this._createPromiseState();
    }

    // FF40+: Allow to know how many properties an object has to lazily display them
    // when there is a bunch.
    if (isTypedArray(g)) {
      // Bug 1348761: getOwnPropertyNames is unnecessary slow on TypedArrays
      g.ownPropertyLength = getArrayLength(this.obj);
    } else {
      try {
        g.ownPropertyLength = this.obj.getOwnPropertyNames().length;
      } catch (err) {
        // The above can throw when the debuggee does not subsume the object's
        // compartment, or for some WrappedNatives like Cu.Sandbox.
      }
    }

    let raw = this.obj.unsafeDereference();

    // If Cu is not defined, we are running on a worker thread, where xrays
    // don't exist.
    if (Cu) {
      raw = Cu.unwaiveXrays(raw);
    }

    if (!DevToolsUtils.isSafeJSObject(raw)) {
      raw = null;
    }

    for (let fn of previewers[this.obj.class] || previewers.Object) {
      try {
        if (fn(this, g, raw)) {
          break;
        }
      } catch (e) {
        let msg = "ObjectActor.prototype.grip previewer function";
        DevToolsUtils.reportException(msg, e);
      }
    }

    this.hooks.decrementGripDepth();
    return g;
  },

  /**
   * Returns an object exposing the internal Promise state.
   */
  _createPromiseState: function() {
    const { state, value, reason } = getPromiseState(this.obj);
    let promiseState = { state };

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

    return promiseState;
  },

  /**
   * Releases this actor from the pool.
   */
  release: function() {
    if (this.registeredPool.objectActors) {
      this.registeredPool.objectActors.delete(this.obj);
    }
    this.iterators.forEach(actor => this.registeredPool.removeActor(actor));
    this.iterators.clear();
    this.registeredPool.removeActor(this);
  },

  /**
   * Handle a protocol request to provide the definition site of this function
   * object.
   */
  onDefinitionSite: function() {
    if (this.obj.class != "Function") {
      return {
        from: this.actorID,
        error: "objectNotFunction",
        message: this.actorID + " is not a function."
      };
    }

    if (!this.obj.script) {
      return {
        from: this.actorID,
        error: "noScript",
        message: this.actorID + " has no Debugger.Script"
      };
    }

    return this.hooks.sources().getOriginalLocation(new GeneratedLocation(
      this.hooks.sources().createNonSourceMappedActor(this.obj.script.source),
      this.obj.script.startLine,
      0 // TODO bug 901138: use Debugger.Script.prototype.startColumn
    )).then((originalLocation) => {
      return {
        source: originalLocation.originalSourceActor.form(),
        line: originalLocation.originalLine,
        column: originalLocation.originalColumn
      };
    });
  },

  /**
   * Handle a protocol request to provide the names of the properties defined on
   * the object and not its prototype.
   */
  onOwnPropertyNames: function() {
    let props = [];
    if (DevToolsUtils.isSafeDebuggerObject(this.obj)) {
      try {
        props = this.obj.getOwnPropertyNames();
      } catch (err) {
        // The above can throw when the debuggee does not subsume the object's
        // compartment, or for some WrappedNatives like Cu.Sandbox.
      }
    }
    return { from: this.actorID, ownPropertyNames: props };
  },

  /**
   * Creates an actor to iterate over an object property names and values.
   * See PropertyIteratorActor constructor for more info about options param.
   *
   * @param request object
   *        The protocol request object.
   */
  onEnumProperties: function(request) {
    let actor = new PropertyIteratorActor(this, request.options);
    this.registeredPool.addActor(actor);
    this.iterators.add(actor);
    return { iterator: actor.form() };
  },

  /**
   * Creates an actor to iterate over entries of a Map/Set-like object.
   */
  onEnumEntries: function() {
    let actor = new PropertyIteratorActor(this, { enumEntries: true });
    this.registeredPool.addActor(actor);
    this.iterators.add(actor);
    return { iterator: actor.form() };
  },

  /**
   * Creates an actor to iterate over an object symbols properties.
   */
  onEnumSymbols: function() {
    let actor = new SymbolIteratorActor(this);
    this.registeredPool.addActor(actor);
    this.iterators.add(actor);
    return { iterator: actor.form() };
  },

  /**
   * Handle a protocol request to provide the prototype and own properties of
   * the object.
   *
   * @returns {Object} An object containing the data of this.obj, of the following form:
   *          - {string} from: this.obj's actorID.
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
  onPrototypeAndProperties: function() {
    let proto = null;
    let names = [];
    let symbols = [];
    if (DevToolsUtils.isSafeDebuggerObject(this.obj)) {
      try {
        proto = this.obj.proto;
        names = this.obj.getOwnPropertyNames();
        symbols = this.obj.getOwnPropertySymbols();
      } catch (err) {
        // The above can throw when the debuggee does not subsume the object's
        // compartment, or for some WrappedNatives like Cu.Sandbox.
      }
    }

    let ownProperties = Object.create(null);
    let ownSymbols = [];

    for (let name of names) {
      ownProperties[name] = this._propertyDescriptor(name);
    }

    for (let sym of symbols) {
      ownSymbols.push({
        name: sym.toString(),
        descriptor: this._propertyDescriptor(sym)
      });
    }

    return { from: this.actorID,
             prototype: this.hooks.createValueGrip(proto),
             ownProperties,
             ownSymbols,
             safeGetterValues: this._findSafeGetterValues(names) };
  },

  /**
   * Find the safe getter values for the current Debugger.Object, |this.obj|.
   *
   * @private
   * @param array ownProperties
   *        The array that holds the list of known ownProperties names for
   *        |this.obj|.
   * @param number [limit=0]
   *        Optional limit of getter values to find.
   * @return object
   *         An object that maps property names to safe getter descriptors as
   *         defined by the remote debugging protocol.
   */
  _findSafeGetterValues: function(ownProperties, limit = 0) {
    let safeGetterValues = Object.create(null);
    let obj = this.obj;
    let level = 0, i = 0;

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
      let getters = this._findSafeGetters(obj);
      for (let name of getters) {
        // Avoid overwriting properties from prototypes closer to this.obj. Also
        // avoid providing safeGetterValues from prototypes if property |name|
        // is already defined as an own property.
        if (name in safeGetterValues ||
            (obj != this.obj && ownProperties.includes(name))) {
          continue;
        }

        // Ignore __proto__ on Object.prototye.
        if (!obj.proto && name == "__proto__") {
          continue;
        }

        let desc = null, getter = null;
        try {
          desc = obj.getOwnPropertyDescriptor(name);
          getter = desc.get;
        } catch (ex) {
          // The above can throw if the cache becomes stale.
        }
        if (!getter) {
          obj._safeGetters = null;
          continue;
        }

        let result = getter.call(this.obj);
        if (result && !("throw" in result)) {
          let getterValue = undefined;
          if ("return" in result) {
            getterValue = result.return;
          } else if ("yield" in result) {
            getterValue = result.yield;
          }
          // WebIDL attributes specified with the LenientThis extended attribute
          // return undefined and should be ignored.
          if (getterValue !== undefined) {
            safeGetterValues[name] = {
              getterValue: this.hooks.createValueGrip(getterValue),
              getterPrototypeLevel: level,
              enumerable: desc.enumerable,
              writable: level == 0 ? desc.writable : true,
            };
            if (limit && ++i == limit) {
              break;
            }
          }
        }
      }
      if (limit && i == limit) {
        break;
      }

      obj = obj.proto;
      level++;
    }

    return safeGetterValues;
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
  _findSafeGetters: function(object) {
    if (object._safeGetters) {
      return object._safeGetters;
    }

    let getters = new Set();

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

    for (let name of names) {
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
  onPrototype: function() {
    let proto = null;
    if (DevToolsUtils.isSafeDebuggerObject(this.obj)) {
      proto = this.obj.proto;
    }
    return { from: this.actorID,
             prototype: this.hooks.createValueGrip(proto) };
  },

  /**
   * Handle a protocol request to provide the property descriptor of the
   * object's specified property.
   *
   * @param request object
   *        The protocol request object.
   */
  onProperty: function(request) {
    if (!request.name) {
      return { error: "missingParameter",
               message: "no property name was specified" };
    }

    return { from: this.actorID,
             descriptor: this._propertyDescriptor(request.name) };
  },

  /**
   * Handle a protocol request to provide the display string for the object.
   */
  onDisplayString: function() {
    const string = stringify(this.obj);
    return { from: this.actorID,
             displayString: this.hooks.createValueGrip(string) };
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
  _propertyDescriptor: function(name, onlyEnumerable) {
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
        value: e.name
      };
    }

    if (!desc || onlyEnumerable && !desc.enumerable) {
      return undefined;
    }

    let retval = {
      configurable: desc.configurable,
      enumerable: desc.enumerable
    };

    if ("value" in desc) {
      retval.writable = desc.writable;
      retval.value = this.hooks.createValueGrip(desc.value);
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
   * Handle a protocol request to provide the source code of a function.
   *
   * @param request object
   *        The protocol request object.
   */
  onDecompile: function(request) {
    if (this.obj.class !== "Function") {
      return { error: "objectNotFunction",
               message: "decompile request is only valid for object grips " +
                        "with a 'Function' class." };
    }

    return { from: this.actorID,
             decompiledCode: this.obj.decompile(!!request.pretty) };
  },

  /**
   * Handle a protocol request to provide the parameters of a function.
   */
  onParameterNames: function() {
    if (this.obj.class !== "Function") {
      return { error: "objectNotFunction",
               message: "'parameterNames' request is only valid for object " +
                        "grips with a 'Function' class." };
    }

    return { parameterNames: this.obj.parameterNames };
  },

  /**
   * Handle a protocol request to release a thread-lifetime grip.
   */
  onRelease: function() {
    this.release();
    return {};
  },

  /**
   * Handle a protocol request to provide the lexical scope of a function.
   */
  onScope: function() {
    if (this.obj.class !== "Function") {
      return { error: "objectNotFunction",
               message: "scope request is only valid for object grips with a" +
                        " 'Function' class." };
    }

    let envActor = this.hooks.createEnvironmentActor(this.obj.environment,
                                                     this.registeredPool);
    if (!envActor) {
      return { error: "notDebuggee",
               message: "cannot access the environment of this function." };
    }

    return { from: this.actorID, scope: envActor.form() };
  },

  /**
   * Handle a protocol request to get the list of dependent promises of a
   * promise.
   *
   * @return object
   *         Returns an object containing an array of object grips of the
   *         dependent promises
   */
  onDependentPromises: function() {
    if (this.obj.class != "Promise") {
      return { error: "objectNotPromise",
               message: "'dependentPromises' request is only valid for " +
                        "object grips with a 'Promise' class." };
    }

    let promises = this.obj.promiseDependentPromises
                           .map(p => this.hooks.createValueGrip(p));

    return { promises };
  },

  /**
   * Handle a protocol request to get the allocation stack of a promise.
   */
  onAllocationStack: function() {
    if (this.obj.class != "Promise") {
      return { error: "objectNotPromise",
               message: "'allocationStack' request is only valid for " +
                        "object grips with a 'Promise' class." };
    }

    let stack = this.obj.promiseAllocationSite;
    let allocationStacks = [];

    while (stack) {
      if (stack.source) {
        let source = this._getSourceOriginalLocation(stack);

        if (source) {
          allocationStacks.push(source);
        }
      }
      stack = stack.parent;
    }

    return Promise.all(allocationStacks).then(stacks => {
      return { allocationStack: stacks };
    });
  },

  /**
   * Handle a protocol request to get the fulfillment stack of a promise.
   */
  onFulfillmentStack: function() {
    if (this.obj.class != "Promise") {
      return { error: "objectNotPromise",
               message: "'fulfillmentStack' request is only valid for " +
                        "object grips with a 'Promise' class." };
    }

    let stack = this.obj.promiseResolutionSite;
    let fulfillmentStacks = [];

    while (stack) {
      if (stack.source) {
        let source = this._getSourceOriginalLocation(stack);

        if (source) {
          fulfillmentStacks.push(source);
        }
      }
      stack = stack.parent;
    }

    return Promise.all(fulfillmentStacks).then(stacks => {
      return { fulfillmentStack: stacks };
    });
  },

  /**
   * Handle a protocol request to get the rejection stack of a promise.
   */
  onRejectionStack: function() {
    if (this.obj.class != "Promise") {
      return { error: "objectNotPromise",
               message: "'rejectionStack' request is only valid for " +
                        "object grips with a 'Promise' class." };
    }

    let stack = this.obj.promiseResolutionSite;
    let rejectionStacks = [];

    while (stack) {
      if (stack.source) {
        let source = this._getSourceOriginalLocation(stack);

        if (source) {
          rejectionStacks.push(source);
        }
      }
      stack = stack.parent;
    }

    return Promise.all(rejectionStacks).then(stacks => {
      return { rejectionStack: stacks };
    });
  },

  /**
   * Helper function for fetching the source location of a SavedFrame stack.
   *
   * @param SavedFrame stack
   *        The promise allocation stack frame
   * @return object
   *         Returns an object containing the source location of the SavedFrame
   *         stack.
   */
  _getSourceOriginalLocation: function(stack) {
    let source;

    // Catch any errors if the source actor cannot be found
    try {
      source = this.hooks.sources().getSourceActorByURL(stack.source);
    } catch (e) {
      // ignored
    }

    if (!source) {
      return null;
    }

    return this.hooks.sources().getOriginalLocation(new GeneratedLocation(
      source,
      stack.line,
      stack.column
    )).then((originalLocation) => {
      return {
        source: originalLocation.originalSourceActor.form(),
        line: originalLocation.originalLine,
        column: originalLocation.originalColumn,
        functionDisplayName: stack.functionDisplayName
      };
    });
  }
};

ObjectActor.prototype.requestTypes = {
  "definitionSite": ObjectActor.prototype.onDefinitionSite,
  "parameterNames": ObjectActor.prototype.onParameterNames,
  "prototypeAndProperties": ObjectActor.prototype.onPrototypeAndProperties,
  "enumProperties": ObjectActor.prototype.onEnumProperties,
  "prototype": ObjectActor.prototype.onPrototype,
  "property": ObjectActor.prototype.onProperty,
  "displayString": ObjectActor.prototype.onDisplayString,
  "ownPropertyNames": ObjectActor.prototype.onOwnPropertyNames,
  "decompile": ObjectActor.prototype.onDecompile,
  "release": ObjectActor.prototype.onRelease,
  "scope": ObjectActor.prototype.onScope,
  "dependentPromises": ObjectActor.prototype.onDependentPromises,
  "allocationStack": ObjectActor.prototype.onAllocationStack,
  "fulfillmentStack": ObjectActor.prototype.onFulfillmentStack,
  "rejectionStack": ObjectActor.prototype.onRejectionStack,
  "enumEntries": ObjectActor.prototype.onEnumEntries,
  "enumSymbols": ObjectActor.prototype.onEnumSymbols,
};

exports.ObjectActor = ObjectActor;
