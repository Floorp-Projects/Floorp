/* -*- Mode: Javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cu = Components.utils;
Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/devtools/dbg-client.jsm");
let {gDevTools} = Cu.import("resource:///modules/devtools/gDevTools.jsm", {});
let {devtools} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});

let gClient;
let gConnectionTimeout;

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

}, true);

/**
 * Called when the "connect" button is clicked.
 */
function submit() {
  // Show the "connecting" screen
  document.body.classList.add("connecting");

  // Save the host/port values
  let host = document.getElementById("host").value;
  Services.prefs.setCharPref("devtools.debugger.remote-host", host);

  let port = document.getElementById("port").value;
  Services.prefs.setIntPref("devtools.debugger.remote-port", port);

  // Initiate the connection
  let transport;
  try {
    transport = debuggerSocketConnect(host, port);
  } catch(e) {
    // Bug 921850: catch rare exception from debuggerSocketConnect
    showError("unexpected");
    return;
  }
  gClient = new DebuggerClient(transport);
  let delay = Services.prefs.getIntPref("devtools.debugger.remote-timeout");
  gConnectionTimeout = setTimeout(handleConnectionTimeout, delay);
  gClient.connect(onConnectionReady);
}

/**
 * Connection is ready. List actors and build buttons.
 */
function onConnectionReady(aType, aTraits) {
  clearTimeout(gConnectionTimeout);
  gClient.listTabs(function(aResponse) {
    document.body.classList.remove("connecting");
    document.body.classList.add("actors-mode");

    let parent = document.getElementById("tabActors");

    // Add Global Process debugging...
    let globals = JSON.parse(JSON.stringify(aResponse));
    delete globals.tabs;
    delete globals.selected;
    // ...only if there are appropriate actors (a 'from' property will always
    // be there).

    // Add one entry for each open tab.
    for (let i = 0; i < aResponse.tabs.length; i++) {
      buildLink(aResponse.tabs[i], parent, i == aResponse.selected);
    }

    let gParent = document.getElementById("globalActors");

    // Build the Remote Process button
    if (Object.keys(globals).length > 1) {
      let a = document.createElement("a");
      a.onclick = function() {
        openToolbox(globals, true);

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

    // Ensure the first link is focused
    let firstLink = parent.querySelector("a:first-of-type");
    if (firstLink) {
      firstLink.focus();
    }

  });
}

/**
 * Build one button for an actor.
 */
function buildLink(tab, parent, selected) {
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
function openToolbox(form, chrome=false) {
  let options = {
    form: form,
    client: gClient,
    chrome: chrome
  };
  devtools.TargetFactory.forRemoteTab(options).then((target) => {
    let hostType = devtools.Toolbox.HostType.WINDOW;
    gDevTools.showToolbox(target, "webconsole", hostType).then((toolbox) => {
      toolbox.once("destroyed", function() {
        gClient.close();
      });
    });
    window.close();
  });
}
