/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");
const Services = require("Services");

class ResourceWatcher {
  /**
   * This class helps retrieving existing and listening to resources.
   * A resource is something that:
   *  - the target you are debugging exposes
   *  - can be created as early as the process/worker/page starts loading
   *  - can already exist, or will be created later on
   *  - doesn't require any user data to be fetched, only a type/category
   *
   * @param {TargetList} targetList
   *        A TargetList instance, which helps communicating to the backend
   *        in order to iterate and listen over the requested resources.
   */

  constructor(targetList) {
    this.targetList = targetList;

    this._onTargetAvailable = this._onTargetAvailable.bind(this);
    this._onTargetDestroyed = this._onTargetDestroyed.bind(this);

    this._onResourceAvailable = this._onResourceAvailable.bind(this);

    this._availableListeners = new EventEmitter();
    this._destroyedListeners = new EventEmitter();

    this._listenerCount = new Map();
  }

  /**
   * Request to start retrieving all already existing instances of given
   * type of resources and also start watching for the one to be created after.
   *
   * @param {Array:string} resources
   *        List of all resources which should be fetched and observed.
   * @param {Function} onAvailable
   *        Function which will be called once per existing resource and
   *        each time a resource is created
   * @param {Function} onDestroyed
   *        Function which will be called each time a resource in the remote
   *        target is destroyed
   */
  async watch(resources, onAvailable, onDestroyed) {
    // First ensuring enabling listening to targets.
    // This will call onTargetAvailable for all already existing targets,
    // as well as for the one created later.
    // Do this *before* calling _startListening in order to register
    // "resource-available" listener before requesting for the resources in _startListening.
    await this._watchAllTargets();

    for (const resource of resources) {
      this._availableListeners.on(resource, onAvailable);
      if (onDestroyed) {
        this._destroyedListeners.on(resource, onDestroyed);
      }
      await this._startListening(resource);
    }
  }

  /**
   * Stop watching for given type of resources.
   * See `watch` for the arguments as both methods receive the same.
   */
  unwatch(resources, onAvailable, onDestroyed) {
    for (const resource of resources) {
      this._availableListeners.off(resource, onAvailable);
      if (onDestroyed) {
        this._destroyedListeners.off(resource, onDestroyed);
      }
      this._stopListening(resource);
    }

    // Stop watching for targets if we removed the last listener.
    let listeners = 0;
    for (const count of this._listenerCount) {
      listeners += count;
    }
    if (listeners <= 0) {
      this._unwatchAllTargets();
    }
  }

  /**
   * Start watching for all already existing and future targets.
   *
   * We are using ALL_TYPES, but this won't force listening to all types.
   * It will only listen for types which are defined by `TargetList.startListening`.
   */
  async _watchAllTargets() {
    if (this._isWatchingTargets) {
      return;
    }
    this._isWatchingTargets = true;
    await this.targetList.watchTargets(
      this.targetList.ALL_TYPES,
      this._onTargetAvailable,
      this._onTargetDestroyed
    );
  }

  async _unwatchAllTargets() {
    if (!this._isWatchingTargets) {
      return;
    }
    this._isWatchingTargets = false;
    await this.targetList.unwatchTargets(
      this.targetList.ALL_TYPES,
      this._onTargetAvailable,
      this._onTargetDestroyed
    );
  }

  /**
   * Method called by the TargetList for each already existing or target which has just been created.
   *
   * @param {string} type
   *        One of the string of TargetList.TYPES to describe which
   *        type of target is available.
   * @param {Front} targetFront
   *        The Front of the target that is available.
   *        This Front inherits from TargetMixin and is typically
   *        composed of a BrowsingContextTargetFront or ContentProcessTargetFront.
   * @param {boolean} isTopLevel
   *        If true, means that this is the top level target.
   *        This typically happens on startup, providing the current
   *        top level target. But also on navigation, when we navigate
   *        to an URL which has to be loaded in a distinct process.
   *        A new top level target is created.
   */
  async _onTargetAvailable({ type, targetFront, isTopLevel }) {
    // For each resource type...
    for (const resourceType of Object.values(ResourceWatcher.TYPES)) {
      // ...which has at least one listener...
      if (!this._listenerCount.get(resourceType)) {
        continue;
      }
      // ...request existing resource and new one to come from this one target
      await this._watchResourcesForTarget(
        type,
        targetFront,
        isTopLevel,
        resourceType
      );
    }
  }

  /**
   * Method called by the TargetList when a target has just been destroyed
   * See _onTargetAvailable for arguments, they are the same.
   */
  _onTargetDestroyed({ type, targetFront }) {
    //TODO: Is there a point in doing anything?
    //
    // We could remove the available/destroyed event, but as the target is destroyed
    // its listeners will be destroyed anyway.
  }

  /**
   * Method called either by:
   * - the backward compatibility code (LegacyListeners)
   * - target actors RDP events
   * whenever an already existing resource is being listed or when a new one
   * has been created.
   *
   * @param {Front} targetFront
   *        The Target Front from which this resource comes from.
   * @param {String} resourceType
   *        One string of ResourceWatcher.TYPES, which designes the types of resources
   *        being reported
   * @param {json/Front} resource
   *        Depending on the resource Type, it can be a JSON object or a Front
   *        which describes the resource.
   */
  _onResourceAvailable(targetFront, resourceType, resource) {
    this._availableListeners.emit(resourceType, {
      resourceType,
      targetFront,
      resource,
    });
  }

  /**
   * Called everytime a resource is destroyed in the remote target.
   * See _onResourceAvailable for the argument description.
   *
   * XXX: No usage of this yet. May be useful for the inspector? sources?
   */
  _onResourceDestroyed(targetFront, resourceType, resource) {
    this._destroyedListeners.emit(resourceType, {
      resourceType,
      targetFront,
      resource,
    });
  }

  /**
   * Start listening for a given type of resource.
   * For backward compatibility code, we register the legacy listeners on
   * each individual target
   *
   * @param {String} resourceType
   *        One string of ResourceWatcher.TYPES, which designates the types of resources
   *        to be listened.
   */
  async _startListening(resourceType) {
    let listeners = this._listenerCount.get(resourceType) || 0;
    listeners++;
    this._listenerCount.set(resourceType, listeners);
    if (listeners > 1) {
      return;
    }
    // If this is the first listener for this type of resource,
    // we should go through all the existing targets as onTargetAvailable
    // has already been called for these existing targets.
    const promises = [];
    for (const targetType of this.targetList.ALL_TYPES) {
      // XXX: May be expose a getReallyAllTarget() on TargetList?
      for (const target of this.targetList.getAllTargets(targetType)) {
        promises.push(
          this._watchResourcesForTarget(
            targetType,
            target,
            target == this.targetList.targetFront,
            resourceType
          )
        );
      }
    }
    await Promise.all(promises);
  }

  /**
   * Call backward compatibility code from `LegacyListeners` in order to listen for a given
   * type of resource from a given target.
   */
  _watchResourcesForTarget(targetType, targetFront, isTopLevel, resourceType) {
    const onAvailable = this._onResourceAvailable.bind(
      this,
      targetFront,
      resourceType
    );
    return LegacyListeners[resourceType]({
      targetList: this.targetList,
      targetType,
      targetFront,
      isTopLevel,
      onAvailable,
    });
  }

  /**
   * Reverse of _startListening. Stop listening for a given type of resource.
   * For backward compatibility, we unregister from each individual target.
   */
  _stopListening(resourceType) {
    let listeners = this._listenerCount.get(resourceType);
    if (!listeners || listeners <= 0) {
      throw new Error(
        `Stopped listening for resource '${resourceType}' that isn't being listened to`
      );
    }
    listeners--;
    this._listenerCount.set(resourceType, listeners);
    if (listeners > 0) {
      return;
    }
    // If this was the last listener, we should stop watching these events from the actors
    // and the actors should stop watching things from the platform
    for (const targetType of this.targetList.ALL_TYPES) {
      // XXX: May be expose a getReallyAllTarget() on TargetList?
      for (const target of this.targetList.getAllTargets(targetType)) {
        this._unwatchResourcesForTarget(targetType, target, resourceType);
      }
    }
  }

  /**
   * Backward compatibility code, reverse of _watchResourcesForTarget.
   */
  _unwatchResourcesForTarget(targetType, targetFront, resourceType) {
    // Is there really a point in:
    // - unregistering `onAvailable` RDP event callbacks from target-scoped actors?
    // - calling `stopListeners()` as we are most likely closing the toolbox and destroying everything?
    //
    // It is important to keep this method synchronous and do as less as possible
    // in the case of toolbox destroy.
    //
    // We are aware of one case where that might be useful.
    // When a panel is disabled via the options panel, after it has been opened.
    // Would that justify doing this? Is there another usecase?
  }
}

ResourceWatcher.TYPES = ResourceWatcher.prototype.TYPES = {
  CONSOLE_MESSAGES: "console-messages",
};
module.exports = { ResourceWatcher };

// Backward compat code for each type of resource.
// Each section added here should eventually be removed once the equivalent server
// code is implement in Firefox, in its release channel.
const LegacyListeners = {
  // Bug 1620243 aims at implementing this from the actor and will eventually replace
  // this client side code.
  async [ResourceWatcher.TYPES.CONSOLE_MESSAGES]({
    targetList,
    targetType,
    targetFront,
    isTopLevel,
    onAvailable,
  }) {
    // Allow the top level target unconditionnally.
    // Also allow frame, but only in content toolbox, when the fission/content toolbox pref is
    // set. i.e. still ignore them in the content of the browser toolbox as we inspect
    // messages via the process targets
    // Also ignore workers as they are not supported yet. (see bug 1592584)
    const isContentToolbox = targetList.targetFront.isLocalTab;
    const listenForFrames =
      isContentToolbox &&
      Services.prefs.getBoolPref("devtools.contenttoolbox.fission");
    const isAllowed =
      isTopLevel ||
      targetType === targetList.TYPES.PROCESS ||
      (targetType === targetList.TYPES.FRAME && listenForFrames);

    if (!isAllowed) {
      return;
    }

    const webConsoleFront = await targetFront.getFront("console");

    // Request notifying about new messages
    await webConsoleFront.startListeners(["ConsoleAPI"]);

    // Fetch already existing messages
    // /!\ The actor implementation requires to call startListeners(ConsoleAPI) first /!\
    const { messages } = await webConsoleFront.getCachedMessages([
      "ConsoleAPI",
    ]);
    // Wrap the message into a `message` attribute, to match `consoleAPICall` behavior
    messages.map(message => ({ message })).forEach(onAvailable);

    // Forward new message events
    webConsoleFront.on("consoleAPICall", onAvailable);
  },
};
