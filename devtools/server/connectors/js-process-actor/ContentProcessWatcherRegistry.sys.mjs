/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};
ChromeUtils.defineESModuleGetters(
  lazy,
  {
    releaseDistinctSystemPrincipalLoader:
      "resource://devtools/shared/loader/DistinctSystemPrincipalLoader.sys.mjs",
    useDistinctSystemPrincipalLoader:
      "resource://devtools/shared/loader/DistinctSystemPrincipalLoader.sys.mjs",
    loader: "resource://devtools/shared/loader/Loader.sys.mjs",
  },
  { global: "contextual" }
);

// Name of the attribute into which we save data in `sharedData` object.
const SHARED_DATA_KEY_NAME = "DevTools:watchedPerWatcher";

// Map(String => Object)
// Map storing the data objects for all currently active watcher actors.
// The data objects are defined by `createWatcherDataObject()`.
// The main attribute of interest is the `sessionData` one which is set alongside
// various other attributes necessary to maintain state per watcher in the content process.
//
// The Session Data object is maintained by ParentProcessWatcherRegistry, in the parent process
// and is fetched from the content process via `sharedData` API.
// It is then manually maintained via DevToolsProcess JS Actor queries.
let gAllWatcherData = null;

export const ContentProcessWatcherRegistry = {
  _getAllWatchersDataMap() {
    if (gAllWatcherData) {
      return gAllWatcherData;
    }
    const { sharedData } = Services.cpmm;
    const sessionDataByWatcherActorID = sharedData.get(SHARED_DATA_KEY_NAME);
    if (!sessionDataByWatcherActorID) {
      throw new Error("Missing session data in `sharedData`");
    }

    // Initialize a distinct Map to replicate the one read from `sharedData`.
    // This distinct Map will be updated via DevToolsProcess JS Actor queries.
    // This helps better control the execution flow.
    gAllWatcherData = new Map();

    // The Browser Toolbox will load its server modules in a distinct global/compartment whose name is "DevTools global".
    // (See https://searchfox.org/mozilla-central/rev/0e9ea50a999420d93df0e4e27094952af48dd3b8/js/xpconnect/loader/mozJSModuleLoader.cpp#699)
    // It means that this class will be instantiated twice, one in each global (the shared one and the browser toolbox one).
    // We then have to distinguish the two subset of watcher actors accordingly within `sharedMap`,
    // as `sharedMap` will be shared between the two module instances.
    // Session type "all" relates to the Browser Toolbox.
    const isInBrowserToolboxLoader =
      // eslint-disable-next-line mozilla/reject-globalThis-modification
      Cu.getRealmLocation(globalThis) == "DevTools global";

    for (const [watcherActorID, sessionData] of sessionDataByWatcherActorID) {
      // Filter in/out the watchers based on the current module loader and the watcher session type.
      const isBrowserToolboxWatcher = sessionData.sessionContext.type == "all";
      if (
        (isInBrowserToolboxLoader && !isBrowserToolboxWatcher) ||
        (!isInBrowserToolboxLoader && isBrowserToolboxWatcher)
      ) {
        continue;
      }

      gAllWatcherData.set(
        watcherActorID,
        createWatcherDataObject(watcherActorID, sessionData)
      );
    }

    return gAllWatcherData;
  },

  /**
   * Get all data objects for all currently active watcher actors.
   * If a specific target type is passed, this will only return objects of watcher actively watching for a given target type.
   *
   * @param {String} targetType
   *        Optional target type to filter only a subset of watchers.
   * @return {Array|Iterator}
   *         List of data objects. (see createWatcherDataObject)
   */
  getAllWatchersDataObjects(targetType) {
    if (targetType) {
      const list = [];
      for (const watcherDataObject of this._getAllWatchersDataMap().values()) {
        if (watcherDataObject.sessionData.targets?.includes(targetType)) {
          list.push(watcherDataObject);
        }
      }
      return list;
    }
    return this._getAllWatchersDataMap().values();
  },

  /**
   * Get the watcher data object for a given watcher actor.
   *
   * @param {String} watcherActorID
   * @param {Boolean} onlyFromCache
   *        If set explicitly to true, will avoid falling back to shared data.
   *        This is typically useful on destructor/removing/cleanup to avoid creating unexpected data.
   *        It is also used to avoid the exception thrown when sharedData is cleared on toolbox destruction.
   */
  getWatcherDataObject(watcherActorID, onlyFromCache = false) {
    let data =
      ContentProcessWatcherRegistry._getAllWatchersDataMap().get(
        watcherActorID
      );
    if (!data && !onlyFromCache) {
      // When there is more than one DevTools opened, the DevToolsProcess JS Actor spawned by the first DevTools
      // created a cached Map in `_getAllWatchersDataMap`.
      // When opening a second DevTools, this cached Map may miss some new SessionData related to this new DevTools instance,
      // and new Watcher Actor.
      // When such scenario happens, fallback to `sharedData` which should hopefully be containing the latest DevTools instance SessionData.
      //
      // May be the watcher should trigger a very first JS Actor query before any others in order to transfer the base Session Data object?
      const { sharedData } = Services.cpmm;
      const sessionDataByWatcherActorID = sharedData.get(SHARED_DATA_KEY_NAME);
      const sessionData = sessionDataByWatcherActorID.get(watcherActorID);
      if (!sessionData) {
        throw new Error("Unable to find data for watcher " + watcherActorID);
      }
      data = createWatcherDataObject(watcherActorID, sessionData);
      gAllWatcherData.set(watcherActorID, data);
    }
    return data;
  },

  /**
   * Instantiate a DevToolsServerConnection for a given Watcher.
   *
   * This function will be the one forcing to load the first DevTools CommonJS modules
   * and spawning the DevTools Loader as well as the DevToolsServer. So better call it
   * only once when it is strictly necessary.
   *
   * This connection will be the communication channel for RDP between this content process
   * and the parent process, which will route RDP packets from/to the client by using
   * a unique "forwarding prefix".
   *
   * @param {String} watcherActorID
   * @param {Boolean} useDistinctLoader
   *        To be set to true when debugging a privileged context running the shared system principal global.
   *        This is a requirement for spidermonkey Debugger API used by the thread actor.
   * @return {Object}
   *         Object with connection (DevToolsServerConnection) and loader (DevToolsLoader) attributes.
   */
  getOrCreateConnectionForWatcher(watcherActorID, useDistinctLoader) {
    const watcherDataObject =
      ContentProcessWatcherRegistry.getWatcherDataObject(watcherActorID);
    let { connection, loader } = watcherDataObject;

    if (connection) {
      return { connection, loader };
    }

    const { sessionContext, forwardingPrefix } = watcherDataObject;
    // For the browser toolbox, we need to use a distinct loader in order to debug privileged JS.
    // The thread actor ultimately need to be in a distinct compartments from its debuggees.
    loader =
      useDistinctLoader || sessionContext.type == "all"
        ? lazy.useDistinctSystemPrincipalLoader(watcherDataObject)
        : lazy.loader;
    watcherDataObject.loader = loader;

    // Note that this a key step in loading DevTools backend / modules.
    const { DevToolsServer } = loader.require(
      "resource://devtools/server/devtools-server.js"
    );

    DevToolsServer.init();

    // Within the content process, we only need the target scoped actors.
    // (inspector, console, storage,...)
    DevToolsServer.registerActors({ target: true });

    // Instantiate a DevToolsServerConnection which will pipe all its outgoing RDP packets
    // up to the parent process manager via DevToolsProcess JS Actor messages.
    connection = DevToolsServer.connectToParentWindowActor(
      watcherDataObject.jsProcessActor,
      forwardingPrefix,
      "DevToolsProcessChild:packet"
    );
    watcherDataObject.connection = connection;

    return { connection, loader };
  },

  /**
   * Method to be called each time a new target actor is instantiated.
   *
   * @param {Object} watcherDataObject
   * @param {Actor} targetActor
   * @param {Boolean} isDocumentCreation
   */
  onNewTargetActor(watcherDataObject, targetActor, isDocumentCreation = false) {
    // There is no root actor in content processes and so
    // the target actor can't be managed by it, but we do have to manage
    // the actor to have it working and be registered in the DevToolsServerConnection.
    // We make it manage itself and become a top level actor.
    targetActor.manage(targetActor);

    const { watcherActorID } = watcherDataObject;
    targetActor.once("destroyed", options => {
      // Maintain the registry and notify the parent process
      ContentProcessWatcherRegistry.destroyTargetActor(
        watcherDataObject,
        targetActor,
        options
      );
    });

    watcherDataObject.actors.push(targetActor);

    // Immediately queue a message for the parent process,
    // in order to ensure that the JSWindowActorTransport is instantiated
    // before any packet is sent from the content process.
    // As messages are guaranteed to be delivered in the order they
    // were queued, we don't have to wait for anything around this sendAsyncMessage call.
    // In theory, the Target Actor may emit events in its constructor.
    // If it does, such RDP packets may be lost. But in practice, no events
    // are emitted during its construction. Instead the frontend will start
    // the communication first.
    const { forwardingPrefix } = watcherDataObject;
    watcherDataObject.jsProcessActor.sendAsyncMessage(
      "DevToolsProcessChild:targetAvailable",
      {
        watcherActorID,
        forwardingPrefix,
        targetActorForm: targetActor.form(),
      }
    );

    // Pass initialization data to the target actor
    const { sessionData } = watcherDataObject;
    for (const type in sessionData) {
      // `sessionData` will also contain `browserId` as well as entries with empty arrays,
      // which shouldn't be processed.
      const entries = sessionData[type];
      if (!Array.isArray(entries) || !entries.length) {
        continue;
      }
      targetActor.addOrSetSessionDataEntry(
        type,
        sessionData[type],
        isDocumentCreation,
        "set"
      );
    }
  },

  /**
   * Method to be called each time a target actor is meant to be destroyed.
   *
   * @param {Object} watcherDataObject
   * @param {Actor} targetActor
   * @param {object} options
   * @param {boolean} options.isModeSwitching
   *        true when this is called as the result of a change to the devtools.browsertoolbox.scope pref
   */
  destroyTargetActor(watcherDataObject, targetActor, options) {
    const idx = watcherDataObject.actors.indexOf(targetActor);
    if (idx != -1) {
      watcherDataObject.actors.splice(idx, 1);
    }
    const form = targetActor.form();
    targetActor.destroy(options);

    // And this will destroy the parent process one
    try {
      watcherDataObject.jsProcessActor.sendAsyncMessage(
        "DevToolsProcessChild:targetDestroyed",
        {
          actors: [
            {
              watcherActorID: watcherDataObject.watcherActorID,
              targetActorForm: form,
            },
          ],
          options,
        }
      );
    } catch (e) {
      // Ignore exception when the JSProcessActorChild has already been destroyed.
      // We often try to emit this message while the process is being destroyed,
      // but sendAsyncMessage doesn't have time to complete and throws.
      if (
        !e.message.includes("JSProcessActorChild cannot send at the moment")
      ) {
        throw e;
      }
    }
  },

  /**
   * Method to know if a given Watcher Actor is still registered.
   *
   * @param {String} watcherActorID
   * @return {Boolean}
   */
  has(watcherActorID) {
    return gAllWatcherData.has(watcherActorID);
  },

  /**
   * Method to unregister a given Watcher Actor.
   *
   * @param {Object} watcherDataObject
   */
  remove(watcherDataObject) {
    // We do not need to destroy each actor individually as they
    // are all registered in this DevToolsServerConnection, which will
    // destroy all the registered actors.
    if (watcherDataObject.connection) {
      watcherDataObject.connection.close();
    }
    // If we were using a distinct and dedicated loader,
    // we have to manually release it.
    if (watcherDataObject.loader && watcherDataObject.loader !== lazy.loader) {
      lazy.releaseDistinctSystemPrincipalLoader(watcherDataObject);
    }

    gAllWatcherData.delete(watcherDataObject.watcherActorID);
    if (gAllWatcherData.size == 0) {
      gAllWatcherData = null;
    }
  },

  /**
   * Method to know if there is no more Watcher registered.
   *
   * @return {Boolean}
   */
  isEmpty() {
    return !gAllWatcherData || gAllWatcherData.size == 0;
  },

  /**
   * Method to unregister all the Watcher Actors
   */
  clear() {
    if (!gAllWatcherData) {
      return;
    }
    // Query gAllWatcherData internal map directly as we don't want to re-create the map from sharedData
    for (const watcherDataObject of gAllWatcherData.values()) {
      ContentProcessWatcherRegistry.remove(watcherDataObject);
    }
    gAllWatcherData = null;
  },
};

function createWatcherDataObject(watcherActorID, sessionData) {
  // The prefix of the DevToolsServerConnection of the Watcher Actor in the parent process.
  // This is used to compute a unique ID for this process.
  const parentConnectionPrefix = sessionData.connectionPrefix;

  // Compute a unique prefix, just for this DOM Process.
  // (nsIDOMProcessChild's childID should be unique across processes)
  //
  // This prefix will be used to create a JSWindowActorTransport pair between content and parent processes.
  // This is slightly hacky as we typically compute Prefix and Actor ID via `DevToolsServerConnection.allocID()`,
  // but here, we can't have access to any DevTools connection as we could run really early in the content process startup.
  //
  // Ensure appending a final slash, otherwise the prefix may be the same between childID 1 and 10...
  const forwardingPrefix =
    parentConnectionPrefix +
    "process" +
    ChromeUtils.domProcessChild.childID +
    "/";

  // The browser toolbox uses a distinct JS Actor, loaded in the "devtools" ESM loader.
  const jsActorName =
    sessionData.sessionContext.type == "all"
      ? "BrowserToolboxDevToolsProcess"
      : "DevToolsProcess";
  const jsProcessActor = ChromeUtils.domProcessChild.getActor(jsActorName);

  return {
    // {String}
    // Actor ID for this watcher
    watcherActorID,

    // {Array<String>}
    // List of currently watched target types for this watcher
    watchingTargetTypes: [],

    // {DevtoolsServerConnection}
    // Connection bridge made from this content process to the parent process.
    connection: null,

    // {JSActor}
    // Reference to the related DevToolsProcessChild instance.
    jsProcessActor,

    // {Object}
    // Watcher's sessionContext object, which help identify the browser toolbox usecase.
    sessionContext: sessionData.sessionContext,

    // {Object}
    // Watcher's sessionData object, which is initiated with `sharedData` version,
    // but is later updated on each Session Data update (addOrSetSessionDataEntry/removeSessionDataEntry).
    // `sharedData` isn't timely updated and can be out of date.
    sessionData,

    // {String}
    // Prefix used against all RDP packets to route them correctly from/to this content process
    forwardingPrefix,

    // {Array<Object>}
    // List of active WindowGlobal and ContentProcess target actor instances.
    actors: [],

    // {Array<Object>}
    // We store workers independently as we don't have access to the TargetActor instance (it is in the worker thread)
    // and we need to keep reference to some other specifics
    // - {WorkerDebugger} dbg
    workers: [],

    // {Set<Array<Object>>}
    // A Set of arrays which will be populated with concurrent Session Data updates
    // being done while a worker target is being instantiated.
    // Each pending worker being initialized register a new dedicated array which will be removed
    // from the Set once its initialization is over.
    pendingWorkers: new Set(),
  };
}
