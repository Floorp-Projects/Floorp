/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var {require} = ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});
var Services = require("Services");
var {gDevTools} = require("devtools/client/framework/devtools");
var {TargetFactory} = require("devtools/client/framework/target");
var {Toolbox} = require("devtools/client/framework/toolbox");
var {DebuggerClient} = require("devtools/shared/client/debugger-client");
var {LocalizationHelper} = require("devtools/shared/l10n");
var L10N = new LocalizationHelper("devtools/client/locales/connection-screen.properties");

var gClient;
var gConnectionTimeout;

/**
 * Once DOM is ready, we prefil the host/port inputs with
 * pref-stored values.
 */
window.addEventListener("DOMContentLoaded", function() {
  const host = Services.prefs.getCharPref("devtools.debugger.remote-host");
  const port = Services.prefs.getIntPref("devtools.debugger.remote-port");

  if (host) {
    document.getElementById("host").value = host;
  }

  if (port) {
    document.getElementById("port").value = port;
  }

  const form = document.querySelector("#connection-form form");
  form.addEventListener("submit", function() {
    window.submit().catch(e => {
      console.error(e);
      // Bug 921850: catch rare exception from DebuggerClient.socketConnect
      showError("unexpected");
    });
  });
}, {capture: true, once: true});

/**
 * Called when the "connect" button is clicked.
 */
/* exported submit */
var submit = async function() {
  // Show the "connecting" screen
  document.body.classList.add("connecting");

  const host = document.getElementById("host").value;
  const port = document.getElementById("port").value;

  // Save the host/port values
  try {
    Services.prefs.setCharPref("devtools.debugger.remote-host", host);
    Services.prefs.setIntPref("devtools.debugger.remote-port", port);
  } catch (e) {
    // Fails in e10s mode, but not a critical feature.
  }

  // Initiate the connection
  const transport = await DebuggerClient.socketConnect({ host, port });
  gClient = new DebuggerClient(transport);
  const delay = Services.prefs.getIntPref("devtools.debugger.remote-timeout");
  gConnectionTimeout = setTimeout(handleConnectionTimeout, delay);
  const response = await gClient.connect();
  await onConnectionReady(...response);
};

/**
 * Connection is ready. List actors and build buttons.
 */
var onConnectionReady = async function([aType, aTraits]) {
  clearTimeout(gConnectionTimeout);

  let addons = [];
  try {
    const response = await gClient.listAddons();
    if (!response.error && response.addons.length > 0) {
      addons = response.addons;
    }
  } catch (e) {
    // listAddons throws if the runtime doesn't support addons
  }

  let parent = document.getElementById("addonTargetActors");
  if (addons.length > 0) {
    // Add one entry for each add-on.
    for (const addon of addons) {
      if (!addon.debuggable) {
        continue;
      }
      buildAddonLink(addon, parent);
    }
  } else {
    // Hide the section when there are no add-ons
    parent.previousElementSibling.remove();
    parent.remove();
  }

  const response = await gClient.listTabs();

  parent = document.getElementById("tabTargetActors");

  // Add Global Process debugging...
  const globals = Cu.cloneInto(response, {});
  delete globals.tabs;
  delete globals.selected;
  // ...only if there are appropriate actors (a 'from' property will always
  // be there).

  // Add one entry for each open tab.
  for (let i = 0; i < response.tabs.length; i++) {
    buildTabLink(response.tabs[i], parent, i == response.selected);
  }

  const gParent = document.getElementById("globalActors");

  // Build the Remote Process button
  // If Fx<39, chrome target actors were used to be exposed on RootActor
  // but in Fx>=39, chrome is debuggable via getProcess() and ParentProcessTargetActor
  if (globals.consoleActor || gClient.mainRoot.traits.allowChromeProcess) {
    const a = document.createElement("a");
    a.onclick = function() {
      if (gClient.mainRoot.traits.allowChromeProcess) {
        gClient.getProcess()
               .then(aResponse => {
                 openToolbox(aResponse.form, true);
               });
      } else if (globals.consoleActor) {
        openToolbox(globals, true, "webconsole", false);
      }
    };
    a.title = a.textContent = L10N.getStr("mainProcess");
    a.className = "remote-process";
    a.href = "#";
    gParent.appendChild(a);
  }
  // Move the selected tab on top
  const selectedLink = parent.querySelector("a.selected");
  if (selectedLink) {
    parent.insertBefore(selectedLink, parent.firstChild);
  }

  document.body.classList.remove("connecting");
  document.body.classList.add("actors-mode");

  // Ensure the first link is focused
  const firstLink = parent.querySelector("a:first-of-type");
  if (firstLink) {
    firstLink.focus();
  }
};

/**
 * Build one button for an add-on.
 */
function buildAddonLink(addon, parent) {
  const a = document.createElement("a");
  a.onclick = async function() {
    const isBrowsingContext = addon.isWebExtension;
    openToolbox(addon, true, "webconsole", isBrowsingContext);
  };

  a.textContent = addon.name;
  a.title = addon.id;
  a.href = "#";

  parent.appendChild(a);
}

/**
 * Build one button for a tab.
 */
function buildTabLink(tab, parent, selected) {
  const a = document.createElement("a");
  a.onclick = function() {
    openToolbox(tab);
  };

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
function openToolbox(form, chrome = false, tool = "webconsole", isBrowsingContext) {
  const options = {
    form: form,
    client: gClient,
    chrome: chrome,
    isBrowsingContext: isBrowsingContext
  };
  TargetFactory.forRemoteTab(options).then((target) => {
    const hostType = Toolbox.HostType.WINDOW;
    gDevTools.showToolbox(target, tool, hostType).then((toolbox) => {
      toolbox.once("destroyed", function() {
        gClient.close();
      });
    }, console.error);
    window.close();
  }, console.error);
}
