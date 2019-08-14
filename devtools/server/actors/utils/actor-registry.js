/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { Ci } = require("chrome");
var gRegisteredModules = Object.create(null);

const ActorRegistry = {
  // Map of global actor names to actor constructors.
  globalActorFactories: {},
  // Map of target-scoped actor names to actor constructors.
  targetScopedActorFactories: {},
  init(connections) {
    this._connections = connections;
  },

  /**
   * Register a CommonJS module with the debugger server.
   * @param id string
   *        The ID of a CommonJS module.
   *        The actor is going to be registered immediately, but loaded only
   *        when a client starts sending packets to an actor with the same id.
   *
   * @param options object
   *        An object with 3 mandatory attributes:
   *        - prefix (string):
   *          The prefix of an actor is used to compute:
   *          - the `actorID` of each new actor instance (ex: prefix1).
   *            (See ActorPool.addActor)
   *          - the actor name in the listTabs request. Sending a listTabs
   *            request to the root actor returns actor IDs. IDs are in
   *            dictionaries, with actor names as keys and actor IDs as values.
   *            The actor name is the prefix to which the "Actor" string is
   *            appended. So for an actor with the `console` prefix, the actor
   *            name will be `consoleActor`.
   *        - constructor (string):
   *          the name of the exported symbol to be used as the actor
   *          constructor.
   *        - type (a dictionary of booleans with following attribute names):
   *          - "global"
   *            registers a global actor instance, if true.
   *            A global actor has the root actor as its parent.
   *          - "target"
   *            registers a target-scoped actor instance, if true.
   *            A new actor will be created for each target, such as a tab.
   */
  registerModule(id, options) {
    if (id in gRegisteredModules) {
      return;
    }

    if (!options) {
      throw new Error(
        "ActorRegistry.registerModule requires an options argument"
      );
    }
    const { prefix, constructor, type } = options;
    if (typeof prefix !== "string") {
      throw new Error(
        `Lazy actor definition for '${id}' requires a string ` +
          `'prefix' option.`
      );
    }
    if (typeof constructor !== "string") {
      throw new Error(
        `Lazy actor definition for '${id}' requires a string ` +
          `'constructor' option.`
      );
    }
    if (!("global" in type) && !("target" in type)) {
      throw new Error(
        `Lazy actor definition for '${id}' requires a dictionary ` +
          `'type' option whose attributes can be 'global' or 'target'.`
      );
    }
    const name = prefix + "Actor";
    const mod = {
      id,
      prefix,
      constructorName: constructor,
      type,
      globalActor: type.global,
      targetScopedActor: type.target,
    };
    gRegisteredModules[id] = mod;
    if (mod.targetScopedActor) {
      this.addTargetScopedActor(mod, name);
    }
    if (mod.globalActor) {
      this.addGlobalActor(mod, name);
    }
  },

  /**
   * Unregister a previously-loaded CommonJS module from the debugger server.
   */
  unregisterModule(id) {
    const mod = gRegisteredModules[id];
    if (!mod) {
      throw new Error(
        "Tried to unregister a module that was not previously registered."
      );
    }

    // Lazy actors
    if (mod.targetScopedActor) {
      this.removeTargetScopedActor(mod);
    }
    if (mod.globalActor) {
      this.removeGlobalActor(mod);
    }

    delete gRegisteredModules[id];
  },

  /**
   * Install Firefox-specific actors.
   *
   * /!\ Be careful when adding a new actor, especially global actors.
   * Any new global actor will be exposed and returned by the root actor.
   */
  addBrowserActors() {
    this.registerModule("devtools/server/actors/preference", {
      prefix: "preference",
      constructor: "PreferenceActor",
      type: { global: true },
    });
    this.registerModule("devtools/server/actors/actor-registry", {
      prefix: "actorRegistry",
      constructor: "ActorRegistryActor",
      type: { global: true },
    });
    this.registerModule("devtools/server/actors/addon/addons", {
      prefix: "addons",
      constructor: "AddonsActor",
      type: { global: true },
    });
    this.registerModule("devtools/server/actors/device", {
      prefix: "device",
      constructor: "DeviceActor",
      type: { global: true },
    });
    this.registerModule("devtools/server/actors/heap-snapshot-file", {
      prefix: "heapSnapshotFile",
      constructor: "HeapSnapshotFileActor",
      type: { global: true },
    });
    // Always register this as a global module, even while there is a pref turning
    // on and off the other performance actor. This actor shouldn't conflict with
    // the other one. These are also lazily loaded so there shouldn't be a performance
    // impact.
    this.registerModule("devtools/server/actors/perf", {
      prefix: "perf",
      constructor: "PerfActor",
      type: { global: true },
    });
  },

  /**
   * Install target-scoped actors.
   */
  addTargetScopedActors() {
    this.registerModule("devtools/server/actors/webconsole", {
      prefix: "console",
      constructor: "WebConsoleActor",
      type: { target: true },
    });
    this.registerModule("devtools/server/actors/inspector/inspector", {
      prefix: "inspector",
      constructor: "InspectorActor",
      type: { target: true },
    });
    this.registerModule("devtools/server/actors/stylesheets", {
      prefix: "styleSheets",
      constructor: "StyleSheetsActor",
      type: { target: true },
    });
    this.registerModule("devtools/server/actors/storage", {
      prefix: "storage",
      constructor: "StorageActor",
      type: { target: true },
    });
    this.registerModule("devtools/server/actors/memory", {
      prefix: "memory",
      constructor: "MemoryActor",
      type: { target: true },
    });
    this.registerModule("devtools/server/actors/framerate", {
      prefix: "framerate",
      constructor: "FramerateActor",
      type: { target: true },
    });
    this.registerModule("devtools/server/actors/reflow", {
      prefix: "reflow",
      constructor: "ReflowActor",
      type: { target: true },
    });
    this.registerModule("devtools/server/actors/css-properties", {
      prefix: "cssProperties",
      constructor: "CssPropertiesActor",
      type: { target: true },
    });
    if ("nsIProfiler" in Ci) {
      this.registerModule("devtools/server/actors/performance", {
        prefix: "performance",
        constructor: "PerformanceActor",
        type: { target: true },
      });
    }
    this.registerModule("devtools/server/actors/animation", {
      prefix: "animations",
      constructor: "AnimationsActor",
      type: { target: true },
    });
    this.registerModule("devtools/server/actors/promises", {
      prefix: "promises",
      constructor: "PromisesActor",
      type: { target: true },
    });
    this.registerModule("devtools/server/actors/emulation", {
      prefix: "emulation",
      constructor: "EmulationActor",
      type: { target: true },
    });
    this.registerModule(
      "devtools/server/actors/addon/webextension-inspected-window",
      {
        prefix: "webExtensionInspectedWindow",
        constructor: "WebExtensionInspectedWindowActor",
        type: { target: true },
      }
    );
    this.registerModule("devtools/server/actors/accessibility/accessibility", {
      prefix: "accessibility",
      constructor: "AccessibilityActor",
      type: { target: true },
    });
    this.registerModule("devtools/server/actors/screenshot", {
      prefix: "screenshot",
      constructor: "ScreenshotActor",
      type: { target: true },
    });
    this.registerModule("devtools/server/actors/changes", {
      prefix: "changes",
      constructor: "ChangesActor",
      type: { target: true },
    });
    this.registerModule(
      "devtools/server/actors/network-monitor/websocket-actor",
      {
        prefix: "webSocket",
        constructor: "WebSocketActor",
        type: { target: true },
      }
    );
    this.registerModule("devtools/server/actors/manifest", {
      prefix: "manifest",
      constructor: "ManifestActor",
      type: { target: true },
    });
  },

  /**
   * Registers handlers for new target-scoped request types defined dynamically.
   *
   * Note that the name or actorPrefix of the request type is not allowed to clash with
   * existing protocol packet properties, like 'title', 'url' or 'actor', since that would
   * break the protocol.
   *
   * @param options object
   *        - constructorName: (required)
   *          name of actor constructor, which is also used when removing the actor.
   *        One of the following:
   *          - id:
   *            module ID that contains the actor
   *          - constructorFun:
   *            a function to construct the actor
   * @param name string
   *        The name of the new request type.
   */
  addTargetScopedActor(options, name) {
    if (!name) {
      throw Error("addTargetScopedActor requires the `name` argument");
    }
    if (["title", "url", "actor"].includes(name)) {
      throw Error(name + " is not allowed");
    }
    if (this.targetScopedActorFactories.hasOwnProperty(name)) {
      throw Error(name + " already exists");
    }
    this.targetScopedActorFactories[name] = { options, name };
  },

  /**
   * Unregisters the handler for the specified target-scoped request type.
   *
   * When unregistering an existing target-scoped actor, we remove the actor factory as
   * well as all existing instances of the actor.
   *
   * @param actor object, string
   *        In case of object:
   *          The `actor` object being given to related addTargetScopedActor call.
   *        In case of string:
   *          The `name` string being given to related addTargetScopedActor call.
   */
  removeTargetScopedActor(actorOrName) {
    let name;
    if (typeof actorOrName == "string") {
      name = actorOrName;
    } else {
      const actor = actorOrName;
      for (const factoryName in this.targetScopedActorFactories) {
        const handler = this.targetScopedActorFactories[factoryName];
        if (
          handler.options.constructorName == actor.name ||
          handler.options.id == actor.id
        ) {
          name = factoryName;
          break;
        }
      }
    }
    if (!name) {
      return;
    }
    delete this.targetScopedActorFactories[name];
    for (const connID of Object.getOwnPropertyNames(this._connections)) {
      // DebuggerServerConnection in child process don't have rootActor
      if (this._connections[connID].rootActor) {
        this._connections[connID].rootActor.removeActorByName(name);
      }
    }
  },

  /**
   * Registers handlers for new browser-scoped request types defined dynamically.
   *
   * Note that the name or actorPrefix of the request type is not allowed to clash with
   * existing protocol packet properties, like 'from', 'tabs' or 'selected', since that
   * would break the protocol.
   *
   * @param options object
   *        - constructorName: (required)
   *          name of actor constructor, which is also used when removing the actor.
   *        One of the following:
   *          - id:
   *            module ID that contains the actor
   *          - constructorFun:
   *            a function to construct the actor
   * @param name string
   *        The name of the new request type.
   */
  addGlobalActor(options, name) {
    if (!name) {
      throw Error("addGlobalActor requires the `name` argument");
    }
    if (["from", "tabs", "selected"].includes(name)) {
      throw Error(name + " is not allowed");
    }
    if (this.globalActorFactories.hasOwnProperty(name)) {
      throw Error(name + " already exists");
    }
    this.globalActorFactories[name] = { options, name };
  },

  /**
   * Unregisters the handler for the specified browser-scoped request type.
   *
   * When unregistering an existing global actor, we remove the actor factory as well as
   * all existing instances of the actor.
   *
   * @param actor object, string
   *        In case of object:
   *          The `actor` object being given to related addGlobalActor call.
   *        In case of string:
   *          The `name` string being given to related addGlobalActor call.
   */
  removeGlobalActor(actorOrName) {
    let name;
    if (typeof actorOrName == "string") {
      name = actorOrName;
    } else {
      const actor = actorOrName;
      for (const factoryName in this.globalActorFactories) {
        const handler = this.globalActorFactories[factoryName];
        if (
          handler.options.constructorName == actor.name ||
          handler.options.id == actor.id
        ) {
          name = factoryName;
          break;
        }
      }
    }
    if (!name) {
      return;
    }
    delete this.globalActorFactories[name];
    for (const connID of Object.getOwnPropertyNames(this._connections)) {
      // DebuggerServerConnection in child process don't have rootActor
      if (this._connections[connID].rootActor) {
        this._connections[connID].rootActor.removeActorByName(name);
      }
    }
  },

  destroy() {
    for (const id of Object.getOwnPropertyNames(gRegisteredModules)) {
      this.unregisterModule(id);
    }
    gRegisteredModules = Object.create(null);

    this.globalActorFactories = {};
    this.targetScopedActorFactories = {};
  },
};

exports.ActorRegistry = ActorRegistry;
