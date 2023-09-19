/* eslint-disable no-undef */
/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { PrivateContainer } = ChromeUtils.importESModule(
  "resource:///modules/PrivateContainer.sys.mjs"
);

if (Services.prefs.getBoolPref("floorp.privateContainer.enabled", false)) {
  // Create a private container.
  PrivateContainer.Functions.StartupCreatePrivateContainer();
  PrivateContainer.Functions.removePrivateContainerData();

  SessionStore.promiseInitialized.then(() => {

    gBrowser.tabContainer.addEventListener(
      "TabClose",
      removeDataIfPrivateContainerTabNotExist
    );
  });
}

function checkPrivateContainerTabExist() {
  let privateContainer = PrivateContainer.Functions.getPrivateContainer();
  if (!privateContainer || !privateContainer.userContextId) {
    return false;
  }

  let tabs = gBrowser.tabs;
  for (let i = 0; i < tabs.length; i++) {
    if (tabs[i].userContextId === privateContainer.userContextId) {
      return true;
    }
  }
  return false;
}

function removeDataIfPrivateContainerTabNotExist() {
  let privateContainerUserContextID =
    PrivateContainer.Functions.getPrivateContainerUserContextId();
  window.setTimeout(() => {
    if (!checkPrivateContainerTabExist()) {
      PrivateContainer.Functions.removePrivateContainerData();
    }

    let tabs = gBrowser.tabs;
    let result = [];
    for (let i = 0; i < tabs.length; i++) {
      if (tabs[i].userContextId === privateContainerUserContextID) {
        result.push(tabs[i]);
      }
    }
    return result;
  }, 400);
}

function checkOpendPrivateContainerTab() {
  let privateContainerUserContextID =
    PrivateContainer.Functions.getPrivateContainerUserContextId();

  let tabs = gBrowser.tabs;
  let result = [];
  for (let i = 0; i < tabs.length; i++) {
    if (tabs[i].userContextId === privateContainerUserContextID) {
      result.push(tabs[i]);
    }
  }
  return result;
}

function tabIsSaveHistory(tab) {
  return tab.getAttribute("historydisabled") === "true";
}

function applyDoNotSaveHistoryToTab(tab) {
  tab.linkedBrowser.setAttribute("disablehistory", "true");
  tab.linkedBrowser.setAttribute("disableglobalhistory", "true");
  tab.setAttribute("floorp-disablehistory", "true");
}

function checkTabIsPrivateContainer(tab) {
  let privateContainerUserContextID =
    PrivateContainer.Functions.getPrivateContainerUserContextId();
  return tab.userContextId === privateContainerUserContextID;
}
