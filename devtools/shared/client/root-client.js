/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci } = require("chrome");
const { arg, DebuggerClient } = require("devtools/shared/client/debugger-client");

const TYPE_SERVICE = Ci.nsIWorkerDebugger.TYPE_SERVICE;

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
}
exports.RootClient = RootClient;

RootClient.prototype = {
  constructor: RootClient,

  /**
   * Gets the "root" form, which lists all the global actors that affect the entire
   * browser.  This can replace usages of `listTabs` that only wanted the global actors
   * and didn't actually care about tabs.
   */
  getRoot: DebuggerClient.requester({ type: "getRoot" }),

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
    type: "listServiceWorkerRegistrations"
  }),

  /**
   * List the running processes.
   *
   * @param function onResponse
   *        Called with the response packet.
   */
  listProcesses: DebuggerClient.requester({ type: "listProcesses" }),

  /**
   * Fetch the TabActor for the currently selected tab, or for a specific
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
    let packet = {
      to: this.actor,
      type: "getTab"
    };

    if (filter) {
      if (typeof (filter.outerWindowID) == "number") {
        packet.outerWindowID = filter.outerWindowID;
      } else if (typeof (filter.tabId) == "number") {
        packet.tabId = filter.tabId;
      } else if ("tab" in filter) {
        let browser = filter.tab.linkedBrowser;
        if (browser.frameLoader.tabParent) {
          // Tabs in child process
          packet.tabId = browser.frameLoader.tabParent.tabId;
        } else if (browser.outerWindowID) {
          // <xul:browser> tabs in parent process
          packet.outerWindowID = browser.outerWindowID;
        } else {
          // <iframe mozbrowser> tabs in parent process
          let windowUtils = browser.contentWindow
                                   .QueryInterface(Ci.nsIInterfaceRequestor)
                                   .getInterface(Ci.nsIDOMWindowUtils);
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
   * Fetch the WindowActor for a specific window, like a browser window in
   * Firefox, but it can be used to reach any window in the process.
   *
   * @param number outerWindowID
   *        The outerWindowID of the top level window you are looking for.
   */
  getWindow: function({ outerWindowID }) {
    if (!outerWindowID) {
      throw new Error("Must specify outerWindowID");
    }

    let packet = {
      to: this.actor,
      type: "getWindow",
      outerWindowID,
    };

    return this.request(packet);
  },

  /**
   * Retrieve the service worker registrations currently available for the this client.
   *
   * @param  {Form} forms
   *         All available worker forms
   * @return {Array} array of form-like objects dedicated to service workers:
   *         - {String} url: url of the worker
   *         - {String} scope: scope controlled by this worker
   *         - {String} fetch: fetch url if available
   *         - {Boolean} active: is the service worker active
   *         - {String} registrationActor: id of the registration actor, needed to start,
   *           unregister and get push information for the registation. Available on
   *           active service workers only in e10s.
   *         - {String} workerActor: id of the worker actor, needed to debug and send push
   *           notifications.
   */
  _mergeServiceWorkerForms: async function(workers) {
    // List service worker registrations
    let { registrations } = await this.listServiceWorkerRegistrations();

    // registrations contain only service worker registrations, add each registration
    // to the workers array.
    let fromRegistrations = registrations.map(form => {
      return {
        url: form.url,
        scope: form.scope,
        fetch: form.fetch,
        active: form.active,
        registrationActor: form.actor,
      };
    });

    // Loop on workers to retrieve the worker actor corresponding to the current
    // instance of the service worker if available.
    let fromWorkers = [];
    workers
      .filter(form => form.type === TYPE_SERVICE)
      .forEach(form => {
        let registration = fromRegistrations.find(w => w.scope === form.scope);
        if (registration) {
          if (!registration.url) {
            // XXX: Race, sometimes a ServiceWorkerRegistrationInfo doesn't
            // have a scriptSpec, but its associated WorkerDebugger does.
            registration.url = form.url;
          }
          // Add the worker actor as "workerActor" on the registration.
          registration.workerActor = form.actor;
        } else {
          // If a service worker registration could not be found, this means we are in
          // e10s, and registrations are not forwarded to other processes until they
          // reach the activated state.
          fromWorkers.push({
            url: form.url,
            scope: form.scope,
            fetch: form.fetch,
            // Infer active=false since the service worker is not supposed to be
            // registered yet in this edge case.
            active: false,
            workerActor: form.actor,
          });
        }
      });

    // Concatenate service workers found in forms.registrations and forms.workers.
    let combined = [...fromRegistrations, ...fromWorkers];

    // Filter out service workers missing a url.
    return combined.filter(form => !!form.url);
  },

  /**
   * Retrieve all service worker registrations as well as workers from the parent
   * and child processes. Listing service workers involves merging information coming from
   * registrations and workers, this method will combine this information to present a
   * unified array of serviceWorkers. If you are only interested in other workers, use
   * listWorkers.
   *
   * @return {Object}
   *         - {Array} serviceWorkers
   *           array of form-like objects for serviceworkers (cf _mergeServiceWorkerForms)
   *         - {Array} workers
   *           Array of WorkerActor forms, containing all possible workers.
   */
  listAllWorkers: async function() {
    let workers = [], serviceWorkers = [];

    try {
      // List workers from the Parent process
      ({ workers } = await this.listWorkers());

      // And then from the Child processes
      let { processes } = await this.listProcesses();
      for (let process of processes) {
        // Ignore parent process
        if (process.parent) {
          continue;
        }
        let { form } = await this._client.getProcess(process.id);
        let processActor = form.actor;
        let response = await this._client.request({
          to: processActor,
          type: "listWorkers"
        });
        workers = workers.concat(response.workers);
      }

      // Combine information from registrations and workers to create a usable list of
      // workers for consumer code.
      serviceWorkers = await this._mergeServiceWorkerForms(workers);
    } catch (e) {
      console.warn("Error while listing all workers, is the client disconnected?", e);
    }


    return {
      serviceWorkers,
      workers
    };
  },

  /**
   * Description of protocol's actors and methods.
   *
   * @param function onResponse
   *        Called with the response packet.
   */
  protocolDescription: DebuggerClient.requester({ type: "protocolDescription" }),

  /*
   * Methods constructed by DebuggerClient.requester require these forwards
   * on their 'this'.
   */
  get _transport() {
    return this._client._transport;
  },
  get request() {
    return this._client.request;
  }
};

module.exports = RootClient;
