/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci } = require("chrome");
const {DebuggerClient} = require("devtools/shared/client/debugger-client");

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
   * @param function onResponse
   *        Called with the response packet.
   */
  listTabs: DebuggerClient.requester({ type: "listTabs" }),

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
  getTab: function (filter) {
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
  getWindow: function ({ outerWindowID }) {
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
