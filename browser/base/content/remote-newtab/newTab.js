/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
/*globals XPCOMUtils, Components, sendAsyncMessage, addMessageListener, removeMessageListener,
          Services, PrivateBrowsingUtils*/
"use strict";

const {utils: Cu, interfaces: Ci} = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");

(function() {
  let remoteNewTabLocation;
  let remoteIFrame;

  /**
   * Attempts to handle commands sent from the remote IFrame within this content frame.
   * Expected commands below, with data types explained.
   *
   * @returns {Boolean} whether or not the command was handled
   * @param {String} command
   *        The command passed from the remote IFrame
   * @param {Object} data
   *        Parameters to the command
   */
  function handleCommand(command, data) {
    let commandHandled = true;
    switch (command) {
    case "NewTab:UpdateTelemetryProbe":
      /**
       * Update a given Telemetry histogram
       *
       * @param {String} data.probe
       *        Probe name to update
       * @param {Number} data.value
       *        Value to update histogram by
       */
      Services.telemetry.getHistogramById(data.probe).add(data.value);
      break;
    case "NewTab:Register":
      registerEvent(data.type);
      break;
    case "NewTab:GetInitialState":
      getInitialState();
      break;
    default:
      commandHandled = false;
    }
    return commandHandled;
  }

  function initRemotePage(initData) {
    // Messages that the iframe sends the browser will be passed onto
    // the privileged parent process
    remoteNewTabLocation = initData;
    remoteIFrame = document.querySelector("#remotedoc");

    let loadHandler = () => {
      if (remoteIFrame.src !== remoteNewTabLocation.href) {
        return;
      }

      remoteIFrame.removeEventListener("load", loadHandler);

      remoteIFrame.contentDocument.addEventListener("NewTabCommand", (e) => {
        // If the commands are not handled within this content frame, the command will be
        // passed on to main process, in RemoteAboutNewTab.jsm
        let handled = handleCommand(e.detail.command, e.detail.data);
        if (!handled) {
          sendAsyncMessage(e.detail.command, e.detail.data);
        }
      });
      registerEvent("NewTab:Observe");
      let ev = new CustomEvent("NewTabCommandReady");
      remoteIFrame.contentDocument.dispatchEvent(ev);
    };

    remoteIFrame.src = remoteNewTabLocation.href;
    remoteIFrame.addEventListener("load", loadHandler);
  }

  /**
   * Allows the content IFrame to register a listener to an event sent by
   * the privileged parent process, in RemoteAboutNewTab.jsm
   *
   * @param {String} eventName
   *        Event name to listen to
   */
  function registerEvent(eventName) {
    addMessageListener(eventName, (message) => {
      remoteIFrame.contentWindow.postMessage(message, remoteNewTabLocation.origin);
    });
  }

  /**
   * Sends the initial data payload to a content IFrame so it can bootstrap
   */
  function getInitialState() {
    let prefs = Services.prefs;
    let isPrivate = PrivateBrowsingUtils.isContentWindowPrivate(window);
    let state = {
      enabled: prefs.getBoolPref("browser.newtabpage.enabled"),
      enhanced: prefs.getBoolPref("browser.newtabpage.enhanced"),
      rows: prefs.getIntPref("browser.newtabpage.rows"),
      columns: prefs.getIntPref("browser.newtabpage.columns"),
      introShown: prefs.getBoolPref("browser.newtabpage.introShown"),
      windowID: window.QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIDOMWindowUtils).outerWindowID,
      privateBrowsingMode: isPrivate
    };
    remoteIFrame.contentWindow.postMessage({
      name: "NewTab:State",
      data: state
    }, remoteNewTabLocation.origin);
  }

  addMessageListener("NewTabFrame:Init", function loadHandler(message) {
    // Everything is loaded. Initialize the New Tab Page.
    removeMessageListener("NewTabFrame:Init", loadHandler);
    initRemotePage(message.data);
  });
  sendAsyncMessage("NewTabFrame:GetInit");
}());
