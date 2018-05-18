/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const promise = require("promise");

loader.lazyRequireGetter(this, "gcliInit", "devtools/shared/gcli/commands/index");

/**
 * A collection of utilities to help working with commands
 */
var CommandUtils = {
  /**
   * Caches requisitions created when calling executeOnTarget:
   * Target => Requisition Promise
   */
  _requisitions: new WeakMap(),

  /**
   * Utility to execute a command string on a given target
   */
  async executeOnTarget(target, command) {
    let requisitionPromise = this._requisitions.get(target);
    if (!requisitionPromise) {
      requisitionPromise = this.createRequisition(target, {
        environment: CommandUtils.createEnvironment({ target }, "target")
      });
      // Store the promise to avoid races by storing the promise immediately
      this._requisitions.set(target, requisitionPromise);
    }
    let requisition = await requisitionPromise;
    requisition.updateExec(command);
  },

  /**
   * Utility to ensure that things are loaded in the correct order
   */
  createRequisition: function(target, options) {
    if (!gcliInit) {
      return promise.reject("Unable to load gcli");
    }
    return gcliInit.getSystem(target).then(system => {
      let Requisition = require("gcli/cli").Requisition;
      return new Requisition(system, options);
    });
  },

  /**
   * Destroy the remote side of the requisition as well as the local side
   */
  destroyRequisition: function(requisition, target) {
    requisition.destroy();
    gcliInit.releaseSystem(target);
  },

  /**
   * A helper function to create the environment object that is passed to
   * GCLI commands.
   * @param targetContainer An object containing a 'target' property which
   * reflects the current debug target
   */
  createEnvironment: function(container, targetProperty = "target") {
    if (!container[targetProperty].toString ||
        !/TabTarget/.test(container[targetProperty].toString())) {
      throw new Error("Missing target");
    }

    return {
      get target() {
        if (!container[targetProperty].toString ||
            !/TabTarget/.test(container[targetProperty].toString())) {
          throw new Error("Removed target");
        }

        return container[targetProperty];
      },

      get chromeWindow() {
        return this.target.tab.ownerDocument.defaultView;
      },

      get chromeDocument() {
        return this.target.tab.ownerDocument.defaultView.document;
      },

      get window() {
        // throw new
        //    Error("environment.window is not available in runAt:client commands");
        return this.chromeWindow.gBrowser.contentWindowAsCPOW;
      },

      get document() {
        // throw new
        //    Error("environment.document is not available in runAt:client commands");
        return this.chromeWindow.gBrowser.contentDocumentAsCPOW;
      }
    };
  },
};

exports.CommandUtils = CommandUtils;
