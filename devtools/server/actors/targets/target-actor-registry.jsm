/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

var EXPORTED_SYMBOLS = ["TargetActorRegistry"];

// Keep track of all WindowGlobal target actors.
// This is especially used to track the actors using Message manager connector,
// or the ones running in the parent process.
// Top level actors, like tab's top level target or parent process target
// are still using message manager in order to avoid being destroyed on navigation.
// And because of this, these actors aren't using JS Window Actor.
const windowGlobalTargetActors = new Set();
let xpcShellTargetActor = null;

var TargetActorRegistry = {
  registerTargetActor(targetActor) {
    windowGlobalTargetActors.add(targetActor);
  },

  unregisterTargetActor(targetActor) {
    windowGlobalTargetActors.delete(targetActor);
  },

  registerXpcShellTargetActor(targetActor) {
    xpcShellTargetActor = targetActor;
  },

  unregisterXpcShellTargetActor(targetActor) {
    xpcShellTargetActor = null;
  },

  /**
   * Return the first target actor matching the passed watcher's context. Returns null if
   * no matching target actors could be found.
   *
   * @param {Object} context: WatcherActor's context. To only retrieve targets related
   *                          to the scope of this watcher actor.
   *                          See watcher actor constructor for more info.
   * @param {String} connectionPrefix: DevToolsServerConnection's prefix, in order to select only actor
   *                                   related to the same connection. i.e. the same client.
   * @returns {TargetActor|null}
   */
  getTopLevelTargetActorForContext(context, connectionPrefix) {
    if (context.type == "all") {
      if (
        Services.appinfo.processType === Services.appinfo.PROCESS_TYPE_DEFAULT
      ) {
        // The xpcshell target actor also lives in the "parent process", even if it
        // is an instance of ContentProcessTargetActor.
        if (xpcShellTargetActor) {
          return xpcShellTargetActor;
        }

        const actors = this.getTargetActors(context, connectionPrefix);
        // In theory, there should be only one actor here.
        return actors[0];
      }
      return null;
    } else if (
      context.type == "browser-element" ||
      context.type == "webextension"
    ) {
      const actors = this.getTargetActors(context, connectionPrefix);
      return actors.find(actor => {
        return actor.isTopLevelTarget;
      });
    }
    throw new Error("Unsupported context type: " + context.type);
  },

  /**
   * Return the target actors matching the passed browser element id.
   * In some scenarios, the registry can have multiple target actors for a given
   * browserId (e.g. the regular DevTools content toolbox + DevTools WebExtensions targets).
   *
   * @param {Object} context: WatcherActor's context. To only retrieve targets related
   *                          to the scope of this watcher actor.
   *                          See watcher actor constructor for more info.
   * @param {String} connectionPrefix: DevToolsServerConnection's prefix, in order to select only actor
   *                                   related to the same connection. i.e. the same client.
   * @returns {Array<TargetActor>}
   */
  getTargetActors(context, connectionPrefix) {
    const actors = [];
    for (const actor of windowGlobalTargetActors) {
      const isMatchingPrefix = actor.actorID.startsWith(connectionPrefix);
      const isMatchingContext =
        (context.type == "all" && actor.typeName === "parentProcessTarget") ||
        (context.type == "browser-element" &&
          actor.browserId == context.browserId) ||
        (context.type == "webextension" && actor.addonId == context.addonId);
      if (isMatchingPrefix && isMatchingContext) {
        actors.push(actor);
      }
    }
    return actors;
  },

  /**
   * Helper for tests to help track the number of targets created for a given tab.
   * (Used by browser_ext_devtools_inspectedWindow.js)
   *
   * @param {Number} browserId: ID for the tab
   *
   * @returns {Number} Number of targets for this tab.
   */

  getTargetActorsCountForBrowserElement(browserId) {
    let count = 0;
    for (const actor of windowGlobalTargetActors) {
      if (actor.browserId == browserId) {
        count++;
      }
    }
    return count;
  },
};
