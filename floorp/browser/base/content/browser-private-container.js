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

    gBrowser.tabContainer.addEventListener(
      "TabOpen",
      handleTabModifications
    );
    
    // Add a tab context menu to reopen in private container.
    let beforeElem = document.getElementById("context_selectAllTabs");
    let menuitemElem = window.MozXULElement.parseXULToFragment(`
      <menuitem id="context_toggleToPrivateContainer" data-l10n-id="floorp-toggle-private-container" accesskey="D" oncommand="reopenInPrivateContainer(event);"/>
    `);
    beforeElem.before(menuitemElem);

    // add URL link a context menu to open in private container.
    addContextBox(
      "open_in_private_container",
      "open-in_private-container",
      "context-openlink",
      "openWithPrivateContainer(gContextMenu.linkURL);",
      "context-openlink",
      function () {
        document.getElementById("bsb-context-link-add").hidden =
          document.getElementById("context-openlink").hidden;
      }
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


function handleTabModifications() {
  let tabs = gBrowser.tabs;
  for (let i = 0; i < tabs.length; i++) {
    if (checkTabIsPrivateContainer(tabs[i]) && !tabIsSaveHistory(tabs[i])) {
      applyDoNotSaveHistoryToTab(tabs[i]);
    }
  }
}

function openWithPrivateContainer(url) {
  let relatedToCurrent = false; //"relatedToCurrent" decide where to open the new tab. Default work as last tab (right side). Floorp use this.
  let _OPEN_NEW_TAB_POSITION_PREF = Services.prefs.getIntPref(
    "floorp.browser.tabs.openNewTabPosition"
  );
  switch (_OPEN_NEW_TAB_POSITION_PREF) {
    case 0:
      // Open the new tab as unrelated to the current tab.
      relatedToCurrent = false;
      break;
    case 1:
      // Open the new tab as related to the current tab.
      relatedToCurrent = true;
      break;
  }
  let privateContainerUserContextID = PrivateContainer.Functions.getPrivateContainerUserContextId();
    Services.obs.notifyObservers(
      {
        wrappedJSObject: new Promise(resolve => {
          openTrustedLinkIn(url, "tab", {
            relatedToCurrent,
            resolveOnNewTabCreated: resolve,
            userContextId: privateContainerUserContextID
          });
        }),
      },
      "browser-open-newtab-start"
    );
}

function reopenInPrivateContainer() {
  let userContextId = PrivateContainer.Functions.getPrivateContainerUserContextId();
  let reopenedTabs = TabContextMenu.contextTab.multiselected
    ? gBrowser.selectedTabs
    : [TabContextMenu.contextTab];

  for (let tab of reopenedTabs) {
    /* Create a triggering principal that is able to load the new tab
       For content principals that are about: chrome: or resource: we need system to load them.
       Anything other than system principal needs to have the new userContextId.
    */
    let triggeringPrincipal;

    if (tab.linkedPanel) {
      triggeringPrincipal = tab.linkedBrowser.contentPrincipal;
    } else {
      // For lazy tab browsers, get the original principal
      // from SessionStore
      let tabState = JSON.parse(SessionStore.getTabState(tab));
      try {
        triggeringPrincipal = E10SUtils.deserializePrincipal(
          tabState.triggeringPrincipal_base64
        );
      } catch (ex) {
        continue;
      }
    }

    if (!triggeringPrincipal || triggeringPrincipal.isNullPrincipal) {
      // Ensure that we have a null principal if we couldn't
      // deserialize it (for lazy tab browsers) ...
      // This won't always work however is safe to use.
      triggeringPrincipal =
        Services.scriptSecurityManager.createNullPrincipal({ userContextId });
    } else if (triggeringPrincipal.isContentPrincipal) {
      triggeringPrincipal = Services.scriptSecurityManager.principalWithOA(
        triggeringPrincipal,
        {
          userContextId,
        }
      );
    }
    
    let currentTabUserContextId = tab.getAttribute("usercontextid");
    if (currentTabUserContextId == userContextId) {
      userContextId = 0;
    }

    let newTab = gBrowser.addTab(tab.linkedBrowser.currentURI.spec, {
      userContextId,
      pinned: tab.pinned,
      index: tab._tPos + 1,
      triggeringPrincipal,
    });

    if (gBrowser.selectedTab == tab) {
      gBrowser.selectedTab = newTab;
    }
    if (tab.muted && !newTab.muted) {
      newTab.toggleMuteAudio(tab.muteReason);
    }

    gBrowser.removeTab(tab);
  }
}
