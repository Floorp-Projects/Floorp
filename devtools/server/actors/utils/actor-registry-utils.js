/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cu, CC } = require("chrome");

const { DebuggerServer } = require("devtools/server/main");

/**
 * Support for actor registration. Main used by ActorRegistryActor
 * for dynamic registration of new actors.
 *
 * @param sourceText {String} Source of the actor implementation
 * @param fileName {String} URL of the actor module (for proper stack traces)
 * @param options {Object} Configuration object
 */
exports.registerActor = function(sourceText, fileName, options) {
  // Register in the current process
  exports.registerActorInCurrentProcess(sourceText, fileName, options);
  // Register in any child processes
  return DebuggerServer.setupInChild({
    module: "devtools/server/actors/utils/actor-registry-utils",
    setupChild: "registerActorInCurrentProcess",
    args: [sourceText, fileName, options],
    waitForEval: true
  });
};

exports.registerActorInCurrentProcess = function(sourceText, fileName, options) {
  const principal = CC("@mozilla.org/systemprincipal;1", "nsIPrincipal")();
  const sandbox = Cu.Sandbox(principal);
  sandbox.exports = {};
  sandbox.require = require;

  Cu.evalInSandbox(sourceText, sandbox, "1.8", fileName, 1);

  const { prefix, constructor, type } = options;

  if (type.global && !DebuggerServer.globalActorFactories.hasOwnProperty(prefix)) {
    DebuggerServer.addGlobalActor({
      constructorName: constructor,
      constructorFun: sandbox[constructor]
    }, prefix);
  }

  if (type.target && !DebuggerServer.targetScopedActorFactories.hasOwnProperty(prefix)) {
    DebuggerServer.addTargetScopedActor({
      constructorName: constructor,
      constructorFun: sandbox[constructor]
    }, prefix);
  }
};

exports.unregisterActor = function(options) {
  // Unregister in the current process
  exports.unregisterActorInCurrentProcess(options);
  // Unregister in any child processes
  DebuggerServer.setupInChild({
    module: "devtools/server/actors/utils/actor-registry-utils",
    setupChild: "unregisterActorInCurrentProcess",
    args: [options]
  });
};

exports.unregisterActorInCurrentProcess = function(options) {
  if (options.target) {
    DebuggerServer.removeTargetScopedActor(options);
  }

  if (options.global) {
    DebuggerServer.removeGlobalActor(options);
  }
};
