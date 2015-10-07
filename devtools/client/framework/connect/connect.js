/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var Cu = Components.utils;
Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
var {gDevTools} = Cu.import("resource:///modules/devtools/client/framework/gDevTools.jsm", {});
var {require} = Cu.import("resource://gre/modules/devtools/shared/Loader.jsm", {});
var {TargetFactory} = require("devtools/client/framework/target");
var {Toolbox} = require("devtools/client/framework/toolbox")
var promise = require("promise");
var {DebuggerClient} = require("devtools/shared/client/main");

var gClient;
var gConnectionTimeout;

XPCOMUtils.defineLazyGetter(window, 'l10n', function () {
  return Services.strings.createBundle('chrome://browser/locale/devtools/connection-screen.properties');
});

/**
 * Once DOM is ready, we prefil the host/port inputs with
 * pref-stored values.
 */
window.addEventListener("DOMContentLoaded", function onDOMReady() {
  window.removeEventListener("DOMContentLoaded", onDOMReady, true);
  let host = Services.prefs.getCharPref("devtools.debugger.remote-host");
  let port = Services.prefs.getIntPref("devtools.debugger.remote-port");

  if (host) {
    document.getElementById("host").value = host;
  }

  if (port) {
    document.getElementById("port").value = port;
  }

  let form = document.querySelector("#connection-form form");
  form.addEventListener("submit", function() {
    window.submit().catch(e => {
      Cu.reportError(e);
      // Bug 921850: catch rare exception from DebuggerClient.socketConnect
      showError("unexpected");
    });
  });
}, true);

/**
 * Called when the "connect" button is clicked.
 */
var submit = Task.async(function*() {
  // Show the "connecting" screen
  document.body.classList.add("connecting");

  let host = document.getElementById("host").value;
  let port = document.getElementById("port").value;

  // Save the host/port values
  try {
    Services.prefs.setCharPref("devtools.debugger.remote-host", host);
    Services.prefs.setIntPref("devtools.debugger.remote-port", port);
  } catch(e) {
    // Fails in e10s mode, but not a critical feature.
  }

  // Initiate the connection
  let transport = yield DebuggerClient.socketConnect({ host, port });
  gClient = new DebuggerClient(transport);
  let delay = Services.prefs.getIntPref("devtools.debugger.remote-timeout");
  gConnectionTimeout = setTimeout(handleConnectionTimeout, delay);
  let response = yield clientConnect();
  yield onConnectionReady(...response);
});

function clientConnect() {
  let deferred = promise.defer();
  gClient.connect((...args) => deferred.resolve(args));
  return deferred.promise;
}

/**
 * Connection is ready. List actors and build buttons.
 */
var onConnectionReady = Task.async(function*(aType, aTraits) {
  clearTimeout(gConnectionTimeout);

  let deferred = promise.defer();
  gClient.listAddons(deferred.resolve);
  let response = yield deferred.promise;

  let parent = document.getElementById("addonActors")
  if (!response.error && response.addons.length > 0) {
    // Add one entry for each add-on.
    for (let addon of response.addons) {
      if (!addon.debuggable) {
        continue;
      }
      buildAddonLink(addon, parent);
    }
  }
  else {
    // Hide the section when there are no add-ons
    parent.previousElementSibling.remove();
    parent.remove();
  }

  deferred = promise.defer();
  gClient.listTabs(deferred.resolve);
  response = yield deferred.promise;

  parent = document.getElementById("tabActors");

  // Add Global Process debugging...
  let globals = Cu.cloneInto(response, {});
  delete globals.tabs;
  delete globals.selected;
  // ...only if there are appropriate actors (a 'from' property will always
  // be there).

  // Add one entry for each open tab.
  for (let i = 0; i < response.tabs.length; i++) {
    buildTabLink(response.tabs[i], parent, i == response.selected);
  }

  let gParent = document.getElementById("globalActors");

  // Build the Remote Process button
  // If Fx<39, tab actors were used to be exposed on RootActor
  // but in Fx>=39, chrome is debuggable via getProcess() and ChromeActor
  if (globals.consoleActor || gClient.mainRoot.traits.allowChromeProcess) {
    let a = document.createElement("a");
    a.onclick = function() {
      if (gClient.mainRoot.traits.allowChromeProcess) {
        gClient.getProcess()
               .then(aResponse => {
                 openToolbox(aResponse.form, true);
               });
      } else if (globals.consoleActor) {
        openToolbox(globals, true, "webconsole", false);
      }
    }
    a.title = a.textContent = window.l10n.GetStringFromName("mainProcess");
    a.className = "remote-process";
    a.href = "#";
    gParent.appendChild(a);
  }
  // Move the selected tab on top
  let selectedLink = parent.querySelector("a.selected");
  if (selectedLink) {
    parent.insertBefore(selectedLink, parent.firstChild);
  }

  document.body.classList.remove("connecting");
  document.body.classList.add("actors-mode");

  // Ensure the first link is focused
  let firstLink = parent.querySelector("a:first-of-type");
  if (firstLink) {
    firstLink.focus();
  }
});

/**
 * Build one button for an add-on actor.
 */
function buildAddonLink(addon, parent) {
  let a = document.createElement("a");
  a.onclick = function() {
    openToolbox(addon, true, "jsdebugger", false);
  }

  a.textContent = addon.name;
  a.title = addon.id;
  a.href = "#";

  parent.appendChild(a);
}

/**
 * Build one button for a tab actor.
 */
function buildTabLink(tab, parent, selected) {
  let a = document.createElement("a");
  a.onclick = function() {
    openToolbox(tab);
  }

  a.textContent = tab.title;
  a.title = tab.url;
  if (!a.textContent) {
    a.textContent = tab.url;
  }
  a.href = "#";

  if (selected) {
    a.classList.add("selected");
  }

  parent.appendChild(a);
}

/**
 * An error occured. Let's show it and return to the first screen.
 */
function showError(type) {
  document.body.className = "error";
  let activeError = document.querySelector(".error-message.active");
  if (activeError) {
    activeError.classList.remove("active");
  }
  activeError = document.querySelector(".error-" + type);
  if (activeError) {
    activeError.classList.add("active");
  }
}

/**
 * Connection timeout.
 */
function handleConnectionTimeout() {
  showError("timeout");
}

/**
 * The user clicked on one of the buttons.
 * Opens the toolbox.
 */
function openToolbox(form, chrome=false, tool="webconsole", isTabActor) {
  let options = {
    form: form,
    client: gClient,
    chrome: chrome,
    isTabActor: isTabActor
  };
  TargetFactory.forRemoteTab(options).then((target) => {
    let hostType = Toolbox.HostType.WINDOW;
    gDevTools.showToolbox(target, tool, hostType).then((toolbox) => {
      toolbox.once("destroyed", function() {
        gClient.close();
      });
    }, console.error.bind(console));
    window.close();
  }, console.error.bind(console));
}
