/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Actor, ActorClassWithSpec } = require("devtools/shared/protocol");
const { gcliSpec } = require("devtools/shared/specs/gcli");
const { createSystem } = require("gcli/system");

/**
 * Manage remote connections that want to talk to GCLI
 */
const GcliActor = ActorClassWithSpec(gcliSpec, {
  initialize: function (conn, tabActor) {
    Actor.prototype.initialize.call(this, conn);

    this._commandsChanged = this._commandsChanged.bind(this);

    this._tabActor = tabActor;
    // see _getRequisition()
    this._requisitionPromise = undefined;
  },

  destroy: function () {
    Actor.prototype.destroy.call(this);

    // If _getRequisition has not been called, just bail quickly
    if (this._requisitionPromise == null) {
      this._commandsChanged = undefined;
      this._tabActor = undefined;
      return Promise.resolve();
    }

    return this._getRequisition().then(requisition => {
      requisition.destroy();

      this._system.commands.onCommandsChange.remove(this._commandsChanged);
      this._system.destroy();
      this._system = undefined;

      this._requisitionPromise = undefined;
      this._tabActor = undefined;

      this._commandsChanged = undefined;
    });
  },

  /**
   * Load a module into the requisition
   */
  _testOnlyAddItemsByModule: function (names) {
    return this._getRequisition().then(requisition => {
      return requisition.system.addItemsByModule(names);
    });
  },

  /**
   * Unload a module from the requisition
   */
  _testOnlyRemoveItemsByModule: function (names) {
    return this._getRequisition().then(requisition => {
      return requisition.system.removeItemsByModule(names);
    });
  },

  /**
   * Retrieve a list of the remotely executable commands
   * @param customProps Array of strings containing additional properties which,
   * if specified in the command spec, will be included in the JSON. Normally we
   * transfer only the properties required for GCLI to function.
   */
  specs: function (customProps) {
    return this._getRequisition().then(requisition => {
      return requisition.system.commands.getCommandSpecs(customProps);
    });
  },

  /**
   * Execute a GCLI command
   * @return a promise of an object with the following properties:
   * - data: The output of the command
   * - type: The type of the data to allow selection of a converter
   * - error: True if the output was considered an error
   */
  execute: function (typed) {
    return this._getRequisition().then(requisition => {
      return requisition.updateExec(typed).then(output => output.toJson());
    });
  },

  /**
   * Get the state of an input string. i.e. requisition.getStateData()
   */
  state: function (typed, start, rank) {
    return this._getRequisition().then(requisition => {
      return requisition.update(typed).then(() => {
        return requisition.getStateData(start, rank);
      });
    });
  },

  /**
   * Call type.parse to check validity. Used by the remote type
   * @return a promise of an object with the following properties:
   * - status: Of of the following strings: VALID|INCOMPLETE|ERROR
   * - message: The message to display to the user
   * - predictions: An array of suggested values for the given parameter
   */
  parseType: function (typed, paramName) {
    return this._getRequisition().then(requisition => {
      return requisition.update(typed).then(() => {
        let assignment = requisition.getAssignment(paramName);
        return Promise.resolve(assignment.predictions).then(predictions => {
          return {
            status: assignment.getStatus().toString(),
            message: assignment.message,
            predictions: predictions
          };
        });
      });
    });
  },

  /**
   * Get the incremented/decremented value of some type
   * @return a promise of a string containing the new argument text
   */
  nudgeType: function (typed, by, paramName) {
    return this.requisition.update(typed).then(() => {
      const assignment = this.requisition.getAssignment(paramName);
      return this.requisition.nudge(assignment, by).then(() => {
        return assignment.arg == null ? undefined : assignment.arg.text;
      });
    });
  },

  /**
   * Perform a lookup on a selection type to get the allowed values
   */
  getSelectionLookup: function (commandName, paramName) {
    return this._getRequisition().then(requisition => {
      const command = requisition.system.commands.get(commandName);
      if (command == null) {
        throw new Error("No command called '" + commandName + "'");
      }

      let type;
      command.params.forEach(param => {
        if (param.name === paramName) {
          type = param.type;
        }
      });

      if (type == null) {
        throw new Error("No parameter called '" + paramName + "' in '" +
                        commandName + "'");
      }

      const reply = type.getLookup(requisition.executionContext);
      return Promise.resolve(reply).then(lookup => {
        // lookup returns an array of objects with name/value properties and
        // the values might not be JSONable, so remove them
        return lookup.map(info => ({ name: info.name }));
      });
    });
  },

  /**
   * Lazy init for a Requisition
   */
  _getRequisition: function () {
    if (this._tabActor == null) {
      throw new Error("GcliActor used post-destroy");
    }

    if (this._requisitionPromise != null) {
      return this._requisitionPromise;
    }

    const Requisition = require("gcli/cli").Requisition;
    const tabActor = this._tabActor;

    this._system = createSystem({ location: "server" });
    this._system.commands.onCommandsChange.add(this._commandsChanged);

    const gcliInit = require("devtools/shared/gcli/commands/index");
    gcliInit.addAllItemsByModule(this._system);

    // this._requisitionPromise should be created synchronously with the call
    // to _getRequisition so that destroy can tell whether there is an async
    // init in progress
    this._requisitionPromise = this._system.load().then(() => {
      const environment = {
        get chromeWindow() {
          throw new Error("environment.chromeWindow is not available in runAt:server" +
            " commands");
        },

        get chromeDocument() {
          throw new Error("environment.chromeDocument is not available in runAt:server" +
            " commands");
        },

        get window() {
          return tabActor.window;
        },

        get document() {
          return tabActor.window && tabActor.window.document;
        }
      };

      return new Requisition(this._system, { environment: environment });
    });

    return this._requisitionPromise;
  },

  /**
   * Pass events from requisition.system.commands.onCommandsChange upwards
   */
  _commandsChanged: function () {
    this.emit("commands-changed");
  },
});

exports.GcliActor = GcliActor;
