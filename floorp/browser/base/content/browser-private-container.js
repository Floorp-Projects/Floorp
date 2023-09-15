/* eslint-disable no-undef */
/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { PrivateContainer } = ChromeUtils.importESModule(
   "resource:///modules/PrivateContainer.sys.mjs"
);

const { setTimeout } = ChromeUtils.importESModule(
    "resource://gre/modules/Timer.sys.mjs"
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
    setTimeout(() => {
        if (!checkPrivateContainerTabExist()) {
            PrivateContainer.Functions.removePrivateContainerData();
        }
    }, 400);
}
