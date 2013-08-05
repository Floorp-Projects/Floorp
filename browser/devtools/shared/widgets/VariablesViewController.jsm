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

const MAX_LONG_STRING_LENGTH = 200000;

this.EXPORTED_SYMBOLS = ["VariablesViewController"];


/**
 * Controller for a VariablesView that handles interfacing with the debugger
 * protocol. Is able to populate scopes and variables via the protocol as well
 * as manage actor lifespans.
 *
 * @param VariablesView aView
 *        The view to attach to.
 * @param object aOptions
 *        Options for configuring the controller. Supported options:
 *        - getGripClient: callback for creating an object grip client
 *        - getLongStringClient: callback for creating a long string grip client
 *        - releaseActor: callback for releasing an actor when it's no longer needed
 *        - overrideValueEvalMacro: callback for creating an overriding eval macro
 *        - getterOrSetterEvalMacro: callback for creating a getter/setter eval macro
 *        - simpleValueEvalMacro: callback for creating a simple value eval macro
 */
function VariablesViewController(aView, aOptions) {
  this.addExpander = this.addExpander.bind(this);

  this._getGripClient = aOptions.getGripClient;
  this._getLongStringClient = aOptions.getLongStringClient;
  this._releaseActor = aOptions.releaseActor;

  if (aOptions.overrideValueEvalMacro) {
    this._overrideValueEvalMacro = aOptions.overrideValueEvalMacro;
  }
  if (aOptions.getterOrSetterEvalMacro) {
    this._getterOrSetterEvalMacro = aOptions.getterOrSetterEvalMacro;
  }
  if (aOptions.simpleValueEvalMacro) {
    this._simpleValueEvalMacro = aOptions.simpleValueEvalMacro;
  }

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

    this._getGripClient(aGrip).getPrototypeAndProperties(aResponse => {
      let { ownProperties, prototype } = aResponse;
      // safeGetterValues is new and isn't necessary defined on old actors
      let safeGetterValues = aResponse.safeGetterValues || {};
      let sortable = VariablesView.isSortable(aGrip.class);

      // Merge the safe getter values into one object such that we can use it
      // in VariablesView.
      for (let name of Object.keys(safeGetterValues)) {
        if (name in ownProperties) {
          ownProperties[name].getterValue = safeGetterValues[name].getterValue;
          ownProperties[name].getterPrototypeLevel = safeGetterValues[name]
                                                     .getterPrototypeLevel;
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

      // Mark the variable as having retrieved all its properties.
      aTarget._retrieved = true;
      this.view.commitHierarchy();
      deferred.resolve();
    });

    return deferred.promise;
  },

  /**
   * Adds an 'onexpand' callback for a variable, lazily handling
   * the addition of new properties.
   *
   * @param Variable aVar
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

    let deferred = promise.defer();
    aTarget._fetched = deferred.promise;

    if (!aSource) {
      throw new Error("No actor grip was given for the variable.");
    }

    // If the target a Variable or Property then we're fetching properties
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
