/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci } = require("chrome");
const { arg, DebuggerClient } = require("devtools/shared/client/debugger-client");
loader.lazyRequireGetter(this, "getFront", "devtools/shared/protocol", true);

/**
 * A RootClient object represents a root actor on the server. Each
 * DebuggerClient keeps a RootClient instance representing the root actor
 * for the initial connection; DebuggerClient's 'listTabs' and
 * 'listChildProcesses' methods forward to that root actor.
 *
 * @param client object
 *      The client connection to which this actor belongs.
 * @param greeting string
 *      The greeting packet from the root actor we're to represent.
 *
 * Properties of a RootClient instance:
 *
 * @property actor string
 *      The name of this child's root actor.
 * @property applicationType string
 *      The application type, as given in the root actor's greeting packet.
 * @property traits object
 *      The traits object, as given in the root actor's greeting packet.
 */
function RootClient(client, greeting) {
  this._client = client;
  this.actor = greeting.from;
  this.applicationType = greeting.applicationType;
  this.traits = greeting.traits;

  // Cache root form as this will always be the same value.
  Object.defineProperty(this, "rootForm", {
    get() {
      delete this.rootForm;
      this.rootForm = this._getRoot();
      return this.rootForm;
    },
    configurable: true,
  });

  // Cache of already created global scoped fronts
  // [typeName:string => Front instance]
  this.fronts = new Map();
}
exports.RootClient = RootClient;

RootClient.prototype = {
  constructor: RootClient,

  /**
   * Gets the "root" form, which lists all the global actors that affect the entire
   * browser.  This can replace usages of `listTabs` that only wanted the global actors
   * and didn't actually care about tabs.
   */
  _getRoot: DebuggerClient.requester({ type: "getRoot" }),

   /**
   * List the open tabs.
   *
   * @param object options
   *        Optional flags for listTabs:
   *        - boolean favicons: return favicon data
   * @param function onResponse
   *        Called with the response packet.
   */
  listTabs: DebuggerClient.requester({ type: "listTabs", options: arg(0) }),

  /**
   * List the installed addons.
   *
   * @param function onResponse
   *        Called with the response packet.
   */
  listAddons: DebuggerClient.requester({ type: "listAddons" }),

  /**
   * List the registered workers.
   *
   * @param function onResponse
   *        Called with the response packet.
   */
  listWorkers: DebuggerClient.requester({ type: "listWorkers" }),

  /**
   * List the registered service workers.
   *
   * @param function onResponse
   *        Called with the response packet.
   */
  listServiceWorkerRegistrations: DebuggerClient.requester({
    type: "listServiceWorkerRegistrations",
  }),

  /**
   * List the running processes.
   *
   * @param function onResponse
   *        Called with the response packet.
   */
  listProcesses: DebuggerClient.requester({ type: "listProcesses" }),

  /**
   * Fetch the ParentProcessTargetActor for the main process or ContentProcessTargetActor
   * for a a given child process ID.
   *
   * @param number id
   *        The ID for the process to attach (returned by `listProcesses`).
   *        Connected to the main process if is 0.
   */
  getProcess: DebuggerClient.requester({ type: "getProcess", id: arg(0) }),

  /**
   * Retrieve all service worker registrations as well as workers from the parent and
   * content processes. Listing service workers involves merging information coming from
   * registrations and workers, this method will combine this information to present a
   * unified array of serviceWorkers. If you are only interested in other workers, use
   * listWorkers.
   *
   * @return {Object}
   *         - {Array} service
   *           array of form-like objects for serviceworkers
   *         - {Array} shared
   *           Array of WorkerTargetActor forms, containing shared workers.
   *         - {Array} other
   *           Array of WorkerTargetActor forms, containing other workers.
   */
  listAllWorkers: async function() {
    let registrations = [];
    let workers = [];

    try {
      // List service worker registrations
      ({ registrations } = await this.listServiceWorkerRegistrations());

      // List workers from the Parent process
      ({ workers } = await this.listWorkers());

      // And then from the Child processes
      const { processes } = await this.listProcesses();
      for (const process of processes) {
        // Ignore parent process
        if (process.parent) {
          continue;
        }
        const { form } = await this.getProcess(process.id);
        const processActor = form.actor;
        const response = await this._client.request({
          to: processActor,
          type: "listWorkers",
        });
        workers = workers.concat(response.workers);
      }
    } catch (e) {
      // Something went wrong, maybe our client is disconnected?
    }

    const result = {
      service: [],
      shared: [],
      other: [],
    };

    registrations.forEach(form => {
      result.service.push({
        name: form.url,
        url: form.url,
        scope: form.scope,
        fetch: form.fetch,
        registrationActor: form.actor,
        active: form.active,
        lastUpdateTime: form.lastUpdateTime,
      });
    });

    workers.forEach(form => {
      const worker = {
        name: form.url,
        url: form.url,
        workerTargetActor: form.actor,
      };
      switch (form.type) {
        case Ci.nsIWorkerDebugger.TYPE_SERVICE:
          const registration = result.service.find(r => r.scope === form.scope);
          if (registration) {
            // XXX: Race, sometimes a ServiceWorkerRegistrationInfo doesn't
            // have a scriptSpec, but its associated WorkerDebugger does.
            if (!registration.url) {
              registration.name = registration.url = form.url;
            }
            registration.workerTargetActor = form.actor;
          } else {
            worker.fetch = form.fetch;

            // If a service worker registration could not be found, this means we are in
            // e10s, and registrations are not forwarded to other processes until they
            // reach the activated state. Augment the worker as a registration worker to
            // display it in aboutdebugging.
            worker.scope = form.scope;
            worker.active = false;
            result.service.push(worker);
          }
          break;
        case Ci.nsIWorkerDebugger.TYPE_SHARED:
          result.shared.push(worker);
          break;
        default:
          result.other.push(worker);
      }
    });

    return result;
  },

  /**
   * Fetch the target actor for the currently selected tab, or for a specific
   * tab given as first parameter.
   *
   * @param [optional] object filter
   *        A dictionary object with following optional attributes:
   *         - outerWindowID: used to match tabs in parent process
   *         - tabId: used to match tabs in child processes
   *         - tab: a reference to xul:tab element
   *        If nothing is specified, returns the actor for the currently
   *        selected tab.
   */
  getTab: function(filter) {
    const packet = {
      to: this.actor,
      type: "getTab",
    };

    if (filter) {
      if (typeof (filter.outerWindowID) == "number") {
        packet.outerWindowID = filter.outerWindowID;
      } else if (typeof (filter.tabId) == "number") {
        packet.tabId = filter.tabId;
      } else if ("tab" in filter) {
        const browser = filter.tab.linkedBrowser;
        if (browser.frameLoader.tabParent) {
          // Tabs in child process
          packet.tabId = browser.frameLoader.tabParent.tabId;
        } else if (browser.outerWindowID) {
          // <xul:browser> tabs in parent process
          packet.outerWindowID = browser.outerWindowID;
        } else {
          // <iframe mozbrowser> tabs in parent process
          const windowUtils = browser.contentWindow.windowUtils;
          packet.outerWindowID = windowUtils.outerWindowID;
        }
      } else {
        // Throw if a filter object have been passed but without
        // any clearly idenfified filter.
        throw new Error("Unsupported argument given to getTab request");
      }
    }

    return this.request(packet);
  },

  /**
   * Fetch the ChromeWindowTargetActor for a specific window, like a browser window in
   * Firefox, but it can be used to reach any window in the process.
   *
   * @param number outerWindowID
   *        The outerWindowID of the top level window you are looking for.
   */
  getWindow: function({ outerWindowID }) {
    if (!outerWindowID) {
      throw new Error("Must specify outerWindowID");
    }

    const packet = {
      to: this.actor,
      type: "getWindow",
      outerWindowID,
    };

    return this.request(packet);
  },

  /*
   * This function returns a protocol.js Front for any root actor.
   * i.e. the one directly served from RootActor.listTabs or getRoot.
   *
   * @param String typeName
   *        The type name used in protocol.js's spec for this actor.
   */
  async getFront(typeName) {
    let front = this.fronts.get(typeName);
    if (front) {
      return front;
    }
    const rootForm = await this.rootForm;
    front = getFront(this._client, typeName, rootForm);
    this.fronts.set(typeName, front);
    return front;
  },

  /**
   * Description of protocol's actors and methods.
   *
   * @param function onResponse
   *        Called with the response packet.
   */
  protocolDescription: DebuggerClient.requester({ type: "protocolDescription" }),

  /**
   * Special request, actually supported by all actors to retrieve the list of all
   * the request names supported by this actor.
   */
  requestTypes: DebuggerClient.requester({ type: "requestTypes" }),

  /**
   * Test request that returns the object passed as first argument.
   *
   * `echo` is special as all the property of the given object have to be passed
   * on the packet object. That's not something that can be achieve by requester helper.
   */
  echo(object) {
    const packet = Object.assign(object, {
      to: this.actor,
      type: "echo",
    });
    return this.request(packet);
  },

  /*
   * Methods constructed by DebuggerClient.requester require these forwards
   * on their 'this'.
   */
  get _transport() {
    return this._client._transport;
  },
  get request() {
    return this._client.request;
  },
};

module.exports = RootClient;
