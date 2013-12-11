/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
let promise = Cu.import("resource://gre/modules/commonjs/sdk/core/promise.js").Promise;
Cu.import("resource:///modules/devtools/VariablesView.jsm");
Cu.import("resource:///modules/devtools/ViewHelpers.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "devtools",
  "resource://gre/modules/devtools/Loader.jsm");

Object.defineProperty(this, "WebConsoleUtils", {
  get: function() {
    return devtools.require("devtools/toolkit/webconsole/utils").Utils;
  },
  configurable: true,
  enumerable: true
});

XPCOMUtils.defineLazyGetter(this, "VARIABLES_SORTING_ENABLED", () =>
  Services.prefs.getBoolPref("devtools.debugger.ui.variables-sorting-enabled")
);

XPCOMUtils.defineLazyModuleGetter(this, "console",
  "resource://gre/modules/devtools/Console.jsm");

const MAX_LONG_STRING_LENGTH = 200000;
const DBG_STRINGS_URI = "chrome://browser/locale/devtools/debugger.properties";

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

      // Mark the string as having retrieved.
      aTarget._retrieved = true;
      deferred.resolve();
    });

    return deferred.promise;
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
    let deferred = promise.defer();

    // Mark the specified variable as having retrieved all its properties.
    let finish = variable => {
      variable._retrieved = true;
      this.view.commitHierarchy();
      deferred.resolve();
    };

    let objectClient = this._getObjectClient(aGrip);
    objectClient.getPrototypeAndProperties(aResponse => {
      let { ownProperties, prototype } = aResponse;
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
      if (ownProperties) {
        aTarget.addItems(ownProperties, {
          // Not all variables need to force sorted properties.
          sorted: sortable,
          // Expansion handlers must be set after the properties are added.
          callback: this.addExpander
        });
      }

      // Add the variable's __proto__.
      if (prototype && prototype.type != "null") {
        let proto = aTarget.addItem("__proto__", { value: prototype });
        // Expansion handlers must be set after the properties are added.
        this.addExpander(proto, prototype);
      }

      // If the object is a function we need to fetch its scope chain
      // to show them as closures for the respective function.
      if (aGrip.class == "Function") {
        objectClient.getScope(aResponse => {
          if (aResponse.error) {
            // This function is bound to a built-in object or it's not present
            // in the current scope chain. Not necessarily an actual error,
            // it just means that there's no closure for the function.
            console.warn(aResponse.error + ": " + aResponse.message);
            return void finish(aTarget);
          }
          this._addVarScope(aTarget, aResponse.scope).then(() => finish(aTarget));
        });
      } else {
        finish(aTarget);
      }
    });

    return deferred.promise;
  },

  /**
   * Adds the scope chain elements (closures) of a function variable to the
   * view.
   *
   * @param Variable aTarget
   *        The variable where the properties will be placed into.
   * @param Scope aScope
   *        The lexical environment form as specified in the protocol.
   */
  _addVarScope: function(aTarget, aScope) {
    let objectScopes = [];
    let environment = aScope;
    let funcScope = aTarget.addItem("<Closure>");
    funcScope._target.setAttribute("scope", "");
    funcScope._fetched = true;
    funcScope.showArrow();
    do {
      // Create a scope to contain all the inspected variables.
      let label = StackFrameUtils.getScopeLabel(environment);
      // Block scopes have the same label, so make addItem allow duplicates.
      let closure = funcScope.addItem(label, undefined, true);
      closure._target.setAttribute("scope", "");
      closure._fetched = environment.class == "Function";
      closure.showArrow();
      // Add nodes for every argument and every other variable in scope.
      if (environment.bindings) {
        this._addBindings(closure, environment.bindings);
        funcScope._retrieved = true;
        closure._retrieved = true;
      } else {
        let deferred = Promise.defer();
        objectScopes.push(deferred.promise);
        this._getEnvironmentClient(environment).getBindings(response => {
          this._addBindings(closure, response.bindings);
          funcScope._retrieved = true;
          closure._retrieved = true;
          deferred.resolve();
        });
      }
    } while ((environment = environment.parent));
    aTarget.expand();

    return Promise.all(objectScopes).then(() => {
      // Signal that scopes have been fetched.
      this.view.emit("fetched", "scopes", funcScope);
    });
  },

  /**
   * Adds nodes for every specified binding to the closure node.
   *
   * @param Variable closure
   *        The node where the bindings will be placed into.
   * @param object bindings
   *        The bindings form as specified in the protocol.
   */
  _addBindings: function(closure, bindings) {
    for (let argument of bindings.arguments) {
      let name = Object.getOwnPropertyNames(argument)[0];
      let argRef = closure.addItem(name, argument[name]);
      let argVal = argument[name].value;
      this.addExpander(argRef, argVal);
    }

    let aVariables = bindings.variables;
    let variableNames = Object.keys(aVariables);

    // Sort all of the variables before adding them, if preferred.
    if (VARIABLES_SORTING_ENABLED) {
      variableNames.sort();
    }
    // Add the variables to the specified scope.
    for (let name of variableNames) {
      let varRef = closure.addItem(name, aVariables[name]);
      let varVal = aVariables[name].value;
      this.addExpander(varRef, varVal);
    }
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
    aTarget.onexpand = () => this.expand(aTarget, aSource);

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
   * @param Scope aTarget
   *        The Scope to be expanded.
   * @param object aSource
   *        The source to use to populate the target.
   * @return Promise
   *         The promise that is resolved once the target has been expanded.
   */
  expand: function(aTarget, aSource) {
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

    // If the target is a Variable or Property then we're fetching properties.
    if (VariablesView.isVariable(aTarget)) {
      this._populateFromObject(aTarget, aSource).then(() => {
        deferred.resolve();
        // Signal that properties have been fetched.
        this.view.emit("fetched", "properties", aTarget);
      });
      return deferred.promise;
    }

    switch (aSource.type) {
      case "longString":
        this._populateFromLongString(aTarget, aSource).then(() => {
          deferred.resolve();
          // Signal that a long string has been fetched.
          this.view.emit("fetched", "longString", aTarget);
        });
        break;
      case "with":
      case "object":
        this._populateFromObject(aTarget, aSource.object).then(() => {
          deferred.resolve();
          // Signal that variables have been fetched.
          this.view.emit("fetched", "variables", aTarget);
        });
        break;
      case "block":
      case "function":
        // Add nodes for every argument and every other variable in scope.
        let args = aSource.bindings.arguments;
        if (args) {
          for (let arg of args) {
            let name = Object.getOwnPropertyNames(arg)[0];
            let ref = aTarget.addItem(name, arg[name]);
            let val = arg[name].value;
            this.addExpander(ref, val);
          }
        }

        aTarget.addItems(aSource.bindings.variables, {
          // Not all variables need to force sorted properties.
          sorted: VARIABLES_SORTING_ENABLED,
          // Expansion handlers must be set after the properties are added.
          callback: this.addExpander
        });

        // No need to signal that variables have been fetched, since
        // the scope arguments and variables are already attached to the
        // environment bindings, so pausing the active thread is unnecessary.

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
    let expanded;

    if (aOptions.objectActor) {
      expanded = this.expand(variable, aOptions.objectActor);
    } else if (aOptions.rawObject) {
      variable.populate(aOptions.rawObject, { expanded: true });
      expanded = promise.resolve();
    }

    return { variable: variable, expanded: expanded };
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
let StackFrameUtils = {
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
let L10N = new ViewHelpers.L10N(DBG_STRINGS_URI);
