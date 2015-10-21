/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://devtools/client/shared/widgets/VariablesView.jsm");
Cu.import("resource://devtools/client/shared/widgets/ViewHelpers.jsm");
var {require} = Cu.import("resource://devtools/shared/Loader.jsm", {});
var promise = require("promise");

Object.defineProperty(this, "WebConsoleUtils", {
  get: function() {
    return require("devtools/shared/webconsole/utils").Utils;
  },
  configurable: true,
  enumerable: true
});

XPCOMUtils.defineLazyGetter(this, "VARIABLES_SORTING_ENABLED", () =>
  Services.prefs.getBoolPref("devtools.debugger.ui.variables-sorting-enabled")
);

XPCOMUtils.defineLazyModuleGetter(this, "console",
  "resource://gre/modules/Console.jsm");

const MAX_LONG_STRING_LENGTH = 200000;
const MAX_PROPERTY_ITEMS = 2000;
const DBG_STRINGS_URI = "chrome://browser/locale/devtools/debugger.properties";

const ELLIPSIS = Services.prefs.getComplexValue("intl.ellipsis", Ci.nsIPrefLocalizedString).data

this.EXPORTED_SYMBOLS = ["VariablesViewController", "StackFrameUtils"];


/**
 * Controller for a VariablesView that handles interfacing with the debugger
 * protocol. Is able to populate scopes and variables via the protocol as well
 * as manage actor lifespans.
 *
 * @param VariablesView aView
 *        The view to attach to.
 * @param object aOptions [optional]
 *        Options for configuring the controller. Supported options:
 *        - getObjectClient: @see this._setClientGetters
 *        - getLongStringClient: @see this._setClientGetters
 *        - getEnvironmentClient: @see this._setClientGetters
 *        - releaseActor: @see this._setClientGetters
 *        - overrideValueEvalMacro: @see _setEvaluationMacros
 *        - getterOrSetterEvalMacro: @see _setEvaluationMacros
 *        - simpleValueEvalMacro: @see _setEvaluationMacros
 */
function VariablesViewController(aView, aOptions = {}) {
  this.addExpander = this.addExpander.bind(this);

  this._setClientGetters(aOptions);
  this._setEvaluationMacros(aOptions);

  this._actors = new Set();
  this.view = aView;
  this.view.controller = this;
}

VariablesViewController.prototype = {
  /**
   * The default getter/setter evaluation macro.
   */
  _getterOrSetterEvalMacro: VariablesView.getterOrSetterEvalMacro,

  /**
   * The default override value evaluation macro.
   */
  _overrideValueEvalMacro: VariablesView.overrideValueEvalMacro,

  /**
   * The default simple value evaluation macro.
   */
  _simpleValueEvalMacro: VariablesView.simpleValueEvalMacro,

  /**
   * Set the functions used to retrieve debugger client grips.
   *
   * @param object aOptions
   *        Options for getting the client grips. Supported options:
   *        - getObjectClient: callback for creating an object grip client
   *        - getLongStringClient: callback for creating a long string grip client
   *        - getEnvironmentClient: callback for creating an environment client
   *        - releaseActor: callback for releasing an actor when it's no longer needed
   */
  _setClientGetters: function(aOptions) {
    if (aOptions.getObjectClient) {
      this._getObjectClient = aOptions.getObjectClient;
    }
    if (aOptions.getLongStringClient) {
      this._getLongStringClient = aOptions.getLongStringClient;
    }
    if (aOptions.getEnvironmentClient) {
      this._getEnvironmentClient = aOptions.getEnvironmentClient;
    }
    if (aOptions.releaseActor) {
      this._releaseActor = aOptions.releaseActor;
    }
  },

  /**
   * Sets the functions used when evaluating strings in the variables view.
   *
   * @param object aOptions
   *        Options for configuring the macros. Supported options:
   *        - overrideValueEvalMacro: callback for creating an overriding eval macro
   *        - getterOrSetterEvalMacro: callback for creating a getter/setter eval macro
   *        - simpleValueEvalMacro: callback for creating a simple value eval macro
   */
  _setEvaluationMacros: function(aOptions) {
    if (aOptions.overrideValueEvalMacro) {
      this._overrideValueEvalMacro = aOptions.overrideValueEvalMacro;
    }
    if (aOptions.getterOrSetterEvalMacro) {
      this._getterOrSetterEvalMacro = aOptions.getterOrSetterEvalMacro;
    }
    if (aOptions.simpleValueEvalMacro) {
      this._simpleValueEvalMacro = aOptions.simpleValueEvalMacro;
    }
  },

  /**
   * Populate a long string into a target using a grip.
   *
   * @param Variable aTarget
   *        The target Variable/Property to put the retrieved string into.
   * @param LongStringActor aGrip
   *        The long string grip that use to retrieve the full string.
   * @return Promise
   *         The promise that will be resolved when the string is retrieved.
   */
  _populateFromLongString: function(aTarget, aGrip){
    let deferred = promise.defer();

    let from = aGrip.initial.length;
    let to = Math.min(aGrip.length, MAX_LONG_STRING_LENGTH);

    this._getLongStringClient(aGrip).substring(from, to, aResponse => {
      // Stop tracking the actor because it's no longer needed.
      this.releaseActor(aGrip);

      // Replace the preview with the full string and make it non-expandable.
      aTarget.onexpand = null;
      aTarget.setGrip(aGrip.initial + aResponse.substring);
      aTarget.hideArrow();

      deferred.resolve();
    });

    return deferred.promise;
  },

  /**
   * Adds pseudo items in case there is too many properties to display.
   * Each item can expand into property slices.
   *
   * @param Scope aTarget
   *        The Scope where the properties will be placed into.
   * @param object aGrip
   *        The property iterator grip.
   * @param object aIterator
   *        The property iterator client.
   */
  _populatePropertySlices: function(aTarget, aGrip, aIterator) {
    if (aGrip.count < MAX_PROPERTY_ITEMS) {
      return this._populateFromPropertyIterator(aTarget, aGrip);
    }

    // Divide the keys into quarters.
    let items = Math.ceil(aGrip.count / 4);

    let promises = [];
    for(let i = 0; i < 4; i++) {
      let start = aGrip.start + i * items;
      let count = i != 3 ? items : aGrip.count - i * items;

      // Create a new kind of grip, with additional fields to define the slice
      let sliceGrip = {
        type: "property-iterator",
        propertyIterator: aIterator,
        start: start,
        count: count
      };

      // Query the name of the first and last items for this slice
      let deferred = promise.defer();
      aIterator.names([start, start + count - 1], ({ names }) => {
        let label = "[" + names[0] + ELLIPSIS + names[1] + "]";
        let item = aTarget.addItem(label);
        item.showArrow();
        this.addExpander(item, sliceGrip);
        deferred.resolve();
      });
      promises.push(deferred.promise);
    }

    return promise.all(promises);
  },

  /**
   * Adds a property slice for a Variable in the view using the already
   * property iterator
   *
   * @param Scope aTarget
   *        The Scope where the properties will be placed into.
   * @param object aGrip
   *        The property iterator grip.
   */
  _populateFromPropertyIterator: function(aTarget, aGrip) {
    if (aGrip.count >= MAX_PROPERTY_ITEMS) {
      // We already started to split, but there is still too many properties, split again.
      return this._populatePropertySlices(aTarget, aGrip, aGrip.propertyIterator);
    }
    // We started slicing properties, and the slice is now small enough to be displayed
    let deferred = promise.defer();
    aGrip.propertyIterator.slice(aGrip.start, aGrip.count,
      ({ ownProperties }) => {
        // Add all the variable properties.
        if (Object.keys(ownProperties).length > 0) {
          aTarget.addItems(ownProperties, {
            sorted: true,
            // Expansion handlers must be set after the properties are added.
            callback: this.addExpander
          });
        }
        deferred.resolve();
      });
    return deferred.promise;
  },

  /**
   * Adds the properties for a Variable in the view using a new feature in FF40+
   * that allows iteration over properties in slices.
   *
   * @param Scope aTarget
   *        The Scope where the properties will be placed into.
   * @param object aGrip
   *        The grip to use to populate the target.
   * @param string aQuery [optional]
   *        The query string used to fetch only a subset of properties
   */
  _populateFromObjectWithIterator: function(aTarget, aGrip, aQuery) {
    // FF40+ starts exposing `ownPropertyLength` on ObjectActor's grip,
    // as well as `enumProperties` request.
    let deferred = promise.defer();
    let objectClient = this._getObjectClient(aGrip);
    let isArray = aGrip.preview && aGrip.preview.kind === "ArrayLike";
    if (isArray) {
      // First enumerate array items, e.g. properties from `0` to `array.length`.
      let options = {
        ignoreNonIndexedProperties: true,
        ignoreSafeGetters: true,
        query: aQuery
      };
      objectClient.enumProperties(options, ({ iterator }) => {
        let sliceGrip = {
          type: "property-iterator",
          propertyIterator: iterator,
          start: 0,
          count: iterator.count
        };
        this._populatePropertySlices(aTarget, sliceGrip, iterator)
            .then(() => {
          // Then enumerate the rest of the properties, like length, buffer, etc.
          let options = {
            ignoreIndexedProperties: true,
            sort: true,
            query: aQuery
          };
          objectClient.enumProperties(options, ({ iterator }) => {
            let sliceGrip = {
              type: "property-iterator",
              propertyIterator: iterator,
              start: 0,
              count: iterator.count
            };
            deferred.resolve(this._populatePropertySlices(aTarget, sliceGrip, iterator));
          });
        });
      });
    } else {
      // For objects, we just enumerate all the properties sorted by name.
      objectClient.enumProperties({ sort: true, query: aQuery }, ({ iterator }) => {
        let sliceGrip = {
          type: "property-iterator",
          propertyIterator: iterator,
          start: 0,
          count: iterator.count
        };
        deferred.resolve(this._populatePropertySlices(aTarget, sliceGrip, iterator));
      });

    }
    return deferred.promise;
  },

  /**
   * Adds the given prototype in the view.
   *
   * @param Scope aTarget
   *        The Scope where the properties will be placed into.
   * @param object aProtype
   *        The prototype grip.
   */
  _populateObjectPrototype: function(aTarget, aPrototype) {
    // Add the variable's __proto__.
    if (aPrototype && aPrototype.type != "null") {
      let proto = aTarget.addItem("__proto__", { value: aPrototype });
      this.addExpander(proto, aPrototype);
    }
  },

  /**
   * Adds properties to a Scope, Variable, or Property in the view. Triggered
   * when a scope is expanded or certain variables are hovered.
   *
   * @param Scope aTarget
   *        The Scope where the properties will be placed into.
   * @param object aGrip
   *        The grip to use to populate the target.
   */
  _populateFromObject: function(aTarget, aGrip) {
    // Fetch properties by slices if there is too many in order to prevent UI freeze.
    if ("ownPropertyLength" in aGrip && aGrip.ownPropertyLength >= MAX_PROPERTY_ITEMS) {
      return this._populateFromObjectWithIterator(aTarget, aGrip)
                 .then(() => {
                   let deferred = promise.defer();
                   let objectClient = this._getObjectClient(aGrip);
                   objectClient.getPrototype(({ prototype }) => {
                     this._populateObjectPrototype(aTarget, prototype);
                     deferred.resolve();
                   });
                   return deferred.promise;
                 });
    }

    if (aGrip.class === "Promise" && aGrip.promiseState) {
      const { state, value, reason } = aGrip.promiseState;
      aTarget.addItem("<state>", { value: state });
      if (state === "fulfilled") {
        this.addExpander(aTarget.addItem("<value>", { value }), value);
      } else if (state === "rejected") {
        this.addExpander(aTarget.addItem("<reason>", { value: reason }), reason);
      }
    }
    return this._populateProperties(aTarget, aGrip);
  },

  _populateProperties: function(aTarget, aGrip, aOptions) {
    let deferred = promise.defer();

    let objectClient = this._getObjectClient(aGrip);
    objectClient.getPrototypeAndProperties(aResponse => {
      let ownProperties = aResponse.ownProperties || {};
      let prototype = aResponse.prototype || null;
      // 'safeGetterValues' is new and isn't necessary defined on old actors.
      let safeGetterValues = aResponse.safeGetterValues || {};
      let sortable = VariablesView.isSortable(aGrip.class);

      // Merge the safe getter values into one object such that we can use it
      // in VariablesView.
      for (let name of Object.keys(safeGetterValues)) {
        if (name in ownProperties) {
          let { getterValue, getterPrototypeLevel } = safeGetterValues[name];
          ownProperties[name].getterValue = getterValue;
          ownProperties[name].getterPrototypeLevel = getterPrototypeLevel;
        } else {
          ownProperties[name] = safeGetterValues[name];
        }
      }

      // Add all the variable properties.
      aTarget.addItems(ownProperties, {
        // Not all variables need to force sorted properties.
        sorted: sortable,
        // Expansion handlers must be set after the properties are added.
        callback: this.addExpander
      });

      // Add the variable's __proto__.
      this._populateObjectPrototype(aTarget, prototype);

      // If the object is a function we need to fetch its scope chain
      // to show them as closures for the respective function.
      if (aGrip.class == "Function") {
        objectClient.getScope(aResponse => {
          if (aResponse.error) {
            // This function is bound to a built-in object or it's not present
            // in the current scope chain. Not necessarily an actual error,
            // it just means that there's no closure for the function.
            console.warn(aResponse.error + ": " + aResponse.message);
            return void deferred.resolve();
          }
          this._populateWithClosure(aTarget, aResponse.scope).then(deferred.resolve);
        });
      } else {
        deferred.resolve();
      }
    });

    return deferred.promise;
  },

  /**
   * Adds the scope chain elements (closures) of a function variable.
   *
   * @param Variable aTarget
   *        The variable where the properties will be placed into.
   * @param Scope aScope
   *        The lexical environment form as specified in the protocol.
   */
  _populateWithClosure: function(aTarget, aScope) {
    let objectScopes = [];
    let environment = aScope;
    let funcScope = aTarget.addItem("<Closure>");
    funcScope.target.setAttribute("scope", "");
    funcScope.showArrow();

    do {
      // Create a scope to contain all the inspected variables.
      let label = StackFrameUtils.getScopeLabel(environment);

      // Block scopes may have the same label, so make addItem allow duplicates.
      let closure = funcScope.addItem(label, undefined, true);
      closure.target.setAttribute("scope", "");
      closure.showArrow();

      // Add nodes for every argument and every other variable in scope.
      if (environment.bindings) {
        this._populateWithEnvironmentBindings(closure, environment.bindings);
      } else {
        let deferred = promise.defer();
        objectScopes.push(deferred.promise);
        this._getEnvironmentClient(environment).getBindings(response => {
          this._populateWithEnvironmentBindings(closure, response.bindings);
          deferred.resolve();
        });
      }
    } while ((environment = environment.parent));

    return promise.all(objectScopes).then(() => {
      // Signal that scopes have been fetched.
      this.view.emit("fetched", "scopes", funcScope);
    });
  },

  /**
   * Adds nodes for every specified binding to the closure node.
   *
   * @param Variable aTarget
   *        The variable where the bindings will be placed into.
   * @param object aBindings
   *        The bindings form as specified in the protocol.
   */
  _populateWithEnvironmentBindings: function(aTarget, aBindings) {
    // Add nodes for every argument in the scope.
    aTarget.addItems(aBindings.arguments.reduce((accumulator, arg) => {
      let name = Object.getOwnPropertyNames(arg)[0];
      let descriptor = arg[name];
      accumulator[name] = descriptor;
      return accumulator;
    }, {}), {
      // Arguments aren't sorted.
      sorted: false,
      // Expansion handlers must be set after the properties are added.
      callback: this.addExpander
    });

    // Add nodes for every other variable in the scope.
    aTarget.addItems(aBindings.variables, {
      // Not all variables need to force sorted properties.
      sorted: VARIABLES_SORTING_ENABLED,
      // Expansion handlers must be set after the properties are added.
      callback: this.addExpander
    });
  },

  /**
   * Adds an 'onexpand' callback for a variable, lazily handling
   * the addition of new properties.
   *
   * @param Variable aTarget
   *        The variable where the properties will be placed into.
   * @param any aSource
   *        The source to use to populate the target.
   */
  addExpander: function(aTarget, aSource) {
    // Attach evaluation macros as necessary.
    if (aTarget.getter || aTarget.setter) {
      aTarget.evaluationMacro = this._overrideValueEvalMacro;
      let getter = aTarget.get("get");
      if (getter) {
        getter.evaluationMacro = this._getterOrSetterEvalMacro;
      }
      let setter = aTarget.get("set");
      if (setter) {
        setter.evaluationMacro = this._getterOrSetterEvalMacro;
      }
    } else {
      aTarget.evaluationMacro = this._simpleValueEvalMacro;
    }

    // If the source is primitive then an expander is not needed.
    if (VariablesView.isPrimitive({ value: aSource })) {
      return;
    }

    // If the source is a long string then show the arrow.
    if (WebConsoleUtils.isActorGrip(aSource) && aSource.type == "longString") {
      aTarget.showArrow();
    }

    // Make sure that properties are always available on expansion.
    aTarget.onexpand = () => this.populate(aTarget, aSource);

    // Some variables are likely to contain a very large number of properties.
    // It's a good idea to be prepared in case of an expansion.
    if (aTarget.shouldPrefetch) {
      aTarget.addEventListener("mouseover", aTarget.onexpand, false);
    }

    // Register all the actors that this controller now depends on.
    for (let grip of [aTarget.value, aTarget.getter, aTarget.setter]) {
      if (WebConsoleUtils.isActorGrip(grip)) {
        this._actors.add(grip.actor);
      }
    }
  },

  /**
   * Adds properties to a Scope, Variable, or Property in the view. Triggered
   * when a scope is expanded or certain variables are hovered.
   *
   * This does not expand the target, it only populates it.
   *
   * @param Scope aTarget
   *        The Scope to be expanded.
   * @param object aSource
   *        The source to use to populate the target.
   * @return Promise
   *         The promise that is resolved once the target has been expanded.
   */
  populate: function(aTarget, aSource) {
    // Fetch the variables only once.
    if (aTarget._fetched) {
      return aTarget._fetched;
    }
    // Make sure the source grip is available.
    if (!aSource) {
      return promise.reject(new Error("No actor grip was given for the variable."));
    }

    let deferred = promise.defer();
    aTarget._fetched = deferred.promise;

    if (aSource.type === "property-iterator") {
      return this._populateFromPropertyIterator(aTarget, aSource);
    }

    // If the target is a Variable or Property then we're fetching properties.
    if (VariablesView.isVariable(aTarget)) {
      this._populateFromObject(aTarget, aSource).then(() => {
        // Signal that properties have been fetched.
        this.view.emit("fetched", "properties", aTarget);
        // Commit the hierarchy because new items were added.
        this.view.commitHierarchy();
        deferred.resolve();
      });
      return deferred.promise;
    }

    switch (aSource.type) {
      case "longString":
        this._populateFromLongString(aTarget, aSource).then(() => {
          // Signal that a long string has been fetched.
          this.view.emit("fetched", "longString", aTarget);
          deferred.resolve();
        });
        break;
      case "with":
      case "object":
        this._populateFromObject(aTarget, aSource.object).then(() => {
          // Signal that variables have been fetched.
          this.view.emit("fetched", "variables", aTarget);
          // Commit the hierarchy because new items were added.
          this.view.commitHierarchy();
          deferred.resolve();
        });
        break;
      case "block":
      case "function":
        this._populateWithEnvironmentBindings(aTarget, aSource.bindings);
        // No need to signal that variables have been fetched, since
        // the scope arguments and variables are already attached to the
        // environment bindings, so pausing the active thread is unnecessary.
        // Commit the hierarchy because new items were added.
        this.view.commitHierarchy();
        deferred.resolve();
        break;
      default:
        let error = "Unknown Debugger.Environment type: " + aSource.type;
        Cu.reportError(error);
        deferred.reject(error);
    }

    return deferred.promise;
  },

  /**
   * Indicates to the view if the targeted actor supports properties search
   *
   * @return boolean True, if the actor supports enumProperty request
   */
  supportsSearch: function () {
    // FF40+ starts exposing ownPropertyLength on object actor's grip
    // as well as enumProperty which allows to query a subset of properties.
    return this.objectActor && ("ownPropertyLength" in this.objectActor);
  },

  /**
   * Try to use the actor to perform an attribute search.
   *
   * @param Scope aScope
   *        The Scope instance to populate with properties
   * @param string aToken
   *        The query string
   */
  performSearch: function(aScope, aToken) {
    this._populateFromObjectWithIterator(aScope, this.objectActor, aToken)
        .then(() => {
          this.view.emit("fetched", "search", aScope);
        });
  },

  /**
   * Release an actor from the controller.
   *
   * @param object aActor
   *        The actor to release.
   */
  releaseActor: function(aActor){
    if (this._releaseActor) {
      this._releaseActor(aActor);
    }
    this._actors.delete(aActor);
  },

  /**
   * Release all the actors referenced by the controller, optionally filtered.
   *
   * @param function aFilter [optional]
   *        Callback to filter which actors are released.
   */
  releaseActors: function(aFilter) {
    for (let actor of this._actors) {
      if (!aFilter || aFilter(actor)) {
        this.releaseActor(actor);
      }
    }
  },

  /**
   * Helper function for setting up a single Scope with a single Variable
   * contained within it.
   *
   * This function will empty the variables view.
   *
   * @param object aOptions
   *        Options for the contents of the view:
   *        - objectActor: the grip of the new ObjectActor to show.
   *        - rawObject: the raw object to show.
   *        - label: the label for the inspected object.
   * @param object aConfiguration
   *        Additional options for the controller:
   *        - overrideValueEvalMacro: @see _setEvaluationMacros
   *        - getterOrSetterEvalMacro: @see _setEvaluationMacros
   *        - simpleValueEvalMacro: @see _setEvaluationMacros
   * @return Object
   *         - variable: the created Variable.
   *         - expanded: the Promise that resolves when the variable expands.
   */
  setSingleVariable: function(aOptions, aConfiguration = {}) {
    this._setEvaluationMacros(aConfiguration);
    this.view.empty();

    let scope = this.view.addScope(aOptions.label);
    scope.expanded = true; // Expand the scope by default.
    scope.locked = true; // Prevent collpasing the scope.

    let variable = scope.addItem("", { enumerable: true });
    let populated;

    if (aOptions.objectActor) {
      // Save objectActor for properties filtering
      this.objectActor = aOptions.objectActor;
      populated = this.populate(variable, aOptions.objectActor);
      variable.expand();
    } else if (aOptions.rawObject) {
      variable.populate(aOptions.rawObject, { expanded: true });
      populated = promise.resolve();
    }

    return { variable: variable, expanded: populated };
  },
};


/**
 * Attaches a VariablesViewController to a VariablesView if it doesn't already
 * have one.
 *
 * @param VariablesView aView
 *        The view to attach to.
 * @param object aOptions
 *        The options to use in creating the controller.
 * @return VariablesViewController
 */
VariablesViewController.attach = function(aView, aOptions) {
  if (aView.controller) {
    return aView.controller;
  }
  return new VariablesViewController(aView, aOptions);
};

/**
 * Utility functions for handling stackframes.
 */
var StackFrameUtils = {
  /**
   * Create a textual representation for the specified stack frame
   * to display in the stackframes container.
   *
   * @param object aFrame
   *        The stack frame to label.
   */
  getFrameTitle: function(aFrame) {
    if (aFrame.type == "call") {
      let c = aFrame.callee;
      return (c.name || c.userDisplayName || c.displayName || "(anonymous)");
    }
    return "(" + aFrame.type + ")";
  },

  /**
   * Constructs a scope label based on its environment.
   *
   * @param object aEnv
   *        The scope's environment.
   * @return string
   *         The scope's label.
   */
  getScopeLabel: function(aEnv) {
    let name = "";

    // Name the outermost scope Global.
    if (!aEnv.parent) {
      name = L10N.getStr("globalScopeLabel");
    }
    // Otherwise construct the scope name.
    else {
      name = aEnv.type.charAt(0).toUpperCase() + aEnv.type.slice(1);
    }

    let label = L10N.getFormatStr("scopeLabel", name);
    switch (aEnv.type) {
      case "with":
      case "object":
        label += " [" + aEnv.object.class + "]";
        break;
      case "function":
        let f = aEnv.function;
        label += " [" +
          (f.name || f.userDisplayName || f.displayName || "(anonymous)") +
        "]";
        break;
    }
    return label;
  }
};

/**
 * Localization convenience methods.
 */
var L10N = new ViewHelpers.L10N(DBG_STRINGS_URI);
