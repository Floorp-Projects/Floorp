/* -*- indent-tabs-mode: nil; js-indent-level: 2; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { createValueGrip } = require("devtools/server/actors/object");

/**
 * Creates an EnvironmentActor. EnvironmentActors are responsible for listing
 * the bindings introduced by a lexical environment and assigning new values to
 * those identifier bindings.
 *
 * @param Debugger.Environment aEnvironment
 *        The lexical environment that will be used to create the actor.
 * @param ThreadActor aThreadActor
 *        The parent thread actor that contains this environment.
 */
function EnvironmentActor(aEnvironment, aThreadActor)
{
  this.obj = aEnvironment;
  this.threadActor = aThreadActor;
}

EnvironmentActor.prototype = {
  actorPrefix: "environment",

  /**
   * Return an environment form for use in a protocol message.
   */
  form: function () {
    let form = { actor: this.actorID };

    // What is this environment's type?
    if (this.obj.type == "declarative") {
      form.type = this.obj.callee ? "function" : "block";
    } else {
      form.type = this.obj.type;
    }

    // Does this environment have a parent?
    if (this.obj.parent) {
      form.parent = (this.threadActor
                     .createEnvironmentActor(this.obj.parent,
                                             this.registeredPool)
                     .form());
    }

    // Does this environment reflect the properties of an object as variables?
    if (this.obj.type == "object" || this.obj.type == "with") {
      form.object = createValueGrip(this.obj.object,
        this.registeredPool, this.threadActor.objectGrip);
    }

    // Is this the environment created for a function call?
    if (this.obj.callee) {
      form.function = createValueGrip(this.obj.callee,
        this.registeredPool, this.threadActor.objectGrip);
    }

    // Shall we list this environment's bindings?
    if (this.obj.type == "declarative") {
      form.bindings = this._bindings();
    }

    return form;
  },

  /**
   * Return the identifier bindings object as required by the remote protocol
   * specification.
   */
  _bindings: function () {
    let bindings = { arguments: [], variables: {} };

    // TODO: this part should be removed in favor of the commented-out part
    // below when getVariableDescriptor lands (bug 725815).
    if (typeof this.obj.getVariable != "function") {
    //if (typeof this.obj.getVariableDescriptor != "function") {
      return bindings;
    }

    let parameterNames;
    if (this.obj.callee) {
      parameterNames = this.obj.callee.parameterNames;
    } else {
      parameterNames = [];
    }
    for (let name of parameterNames) {
      let arg = {};
      let value = this.obj.getVariable(name);

      // TODO: this part should be removed in favor of the commented-out part
      // below when getVariableDescriptor lands (bug 725815).
      let desc = {
        value: value,
        configurable: false,
        writable: !(value && value.optimizedOut),
        enumerable: true
      };

      // let desc = this.obj.getVariableDescriptor(name);
      let descForm = {
        enumerable: true,
        configurable: desc.configurable
      };
      if ("value" in desc) {
        descForm.value = createValueGrip(desc.value,
          this.registeredPool, this.threadActor.objectGrip);
        descForm.writable = desc.writable;
      } else {
        descForm.get = createValueGrip(desc.get, this.registeredPool,
          this.threadActor.objectGrip);
        descForm.set = createValueGrip(desc.set, this.registeredPool,
          this.threadActor.objectGrip);
      }
      arg[name] = descForm;
      bindings.arguments.push(arg);
    }

    for (let name of this.obj.names()) {
      if (bindings.arguments.some(function exists(element) {
                                    return !!element[name];
                                  })) {
        continue;
      }

      let value = this.obj.getVariable(name);

      // TODO: this part should be removed in favor of the commented-out part
      // below when getVariableDescriptor lands.
      let desc = {
        value: value,
        configurable: false,
        writable: !(value &&
                    (value.optimizedOut ||
                     value.uninitialized ||
                     value.missingArguments)),
        enumerable: true
      };

      //let desc = this.obj.getVariableDescriptor(name);
      let descForm = {
        enumerable: true,
        configurable: desc.configurable
      };
      if ("value" in desc) {
        descForm.value = createValueGrip(desc.value,
          this.registeredPool, this.threadActor.objectGrip);
        descForm.writable = desc.writable;
      } else {
        descForm.get = createValueGrip(desc.get || undefined,
          this.registeredPool, this.threadActor.objectGrip);
        descForm.set = createValueGrip(desc.set || undefined,
          this.registeredPool, this.threadActor.objectGrip);
      }
      bindings.variables[name] = descForm;
    }

    return bindings;
  },

  /**
   * Handle a protocol request to change the value of a variable bound in this
   * lexical environment.
   *
   * @param aRequest object
   *        The protocol request object.
   */
  onAssign: function (aRequest) {
    // TODO: enable the commented-out part when getVariableDescriptor lands
    // (bug 725815).
    /*let desc = this.obj.getVariableDescriptor(aRequest.name);

    if (!desc.writable) {
      return { error: "immutableBinding",
               message: "Changing the value of an immutable binding is not " +
                        "allowed" };
    }*/

    try {
      this.obj.setVariable(aRequest.name, aRequest.value);
    } catch (e) {
      if (e instanceof Debugger.DebuggeeWouldRun) {
        return { error: "threadWouldRun",
                 cause: e.cause ? e.cause : "setter",
                 message: "Assigning a value would cause the debuggee to run" };
      } else {
        throw e;
      }
    }
    return { from: this.actorID };
  },

  /**
   * Handle a protocol request to fully enumerate the bindings introduced by the
   * lexical environment.
   *
   * @param aRequest object
   *        The protocol request object.
   */
  onBindings: function (aRequest) {
    return { from: this.actorID,
             bindings: this._bindings() };
  }
};

EnvironmentActor.prototype.requestTypes = {
  "assign": EnvironmentActor.prototype.onAssign,
  "bindings": EnvironmentActor.prototype.onBindings
};

exports.EnvironmentActor = EnvironmentActor;
