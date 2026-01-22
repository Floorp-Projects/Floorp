/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { ContextMenuUtils } from "#features-chrome/utils/context-menu.tsx";
import { PrivateContainer } from "./PrivateContainer.ts";
import { createRootHMR } from "@nora/solid-xul";

export class FloorpPrivateContainer {
  private get privateContainerMenuItem() {
    return document?.querySelector("#open_in_private_container") as XULElement;
  }
  private get openLinkMenuItem() {
    return document?.querySelector("#context-openlink") as XULElement;
  }

  constructor() {
    // Create a private container.
    PrivateContainer.StartupCreatePrivateContainer();
    PrivateContainer.removePrivateContainerData();

    const tabContainer = window.gBrowser?.tabContainer;
    if (!tabContainer) {
      console.error(
        "[FloorpPrivateContainer] Tab container is unavailable; skip listener registration.",
      );
      return;
    }

    tabContainer.addEventListener(
      "TabClose",
      FloorpPrivateContainer.removeDataIfPrivateContainerTabNotExist,
    );

    tabContainer.addEventListener(
      "TabOpen",
      FloorpPrivateContainer.handleTabModifications,
    );

    FloorpPrivateContainer.handleTabModifications();

    const sessionInitialized = window.SessionStore?.promiseInitialized;
    sessionInitialized?.catch((error: Error) => {
      console.error(
        "[FloorpPrivateContainer] SessionStore initialization failure detected",
        error,
      );
    });

    createRootHMR(() => {
      // add URL link a context menu to open in private container.
      ContextMenuUtils.addContextBox(
        "open_in_private_container",
        "privateContainer.openInPrivateContainer",
        "context-openlink",
        () =>
          FloorpPrivateContainer.openWithPrivateContainer(
            window.gContextMenu.linkURL,
          ),
        "context-openlink",
        () => {
          this.privateContainerMenuItem.hidden = this.openLinkMenuItem.hidden;
        },
      );
    }, import.meta.hot);
  }

  public static checkPrivateContainerTabExist() {
    const privateContainer = PrivateContainer.getPrivateContainer();
    if (!privateContainer || !privateContainer.userContextId) {
      return false;
    }
    return window.gBrowser.tabs.some(
      (tab: { userContextId: number }) =>
        tab.userContextId === privateContainer.userContextId,
    );
  }

  private static removeDataIfPrivateContainerTabNotExist() {
    const privateContainerUserContextID = PrivateContainer
      .getPrivateContainerUserContextId();
    setTimeout(() => {
      if (!FloorpPrivateContainer.checkPrivateContainerTabExist()) {
        PrivateContainer.removePrivateContainerData();
      }

      return window.gBrowser.tabs.filter(
        (tab: { userContextId: number }) =>
          tab.userContextId === privateContainerUserContextID,
      );
    }, 400);
  }

  private static tabIsSaveHistory(tab: {
    getAttribute: (arg0: string) => string;
  }) {
    return tab.getAttribute("historydisabled") === "true";
  }

  private static applyDoNotSaveHistoryToTab(tab: {
    linkedBrowser: { setAttribute: (arg0: string, arg1: string) => void };
    setAttribute: (arg0: string, arg1: string) => void;
  }) {
    tab.linkedBrowser.setAttribute("disablehistory", "true");
    tab.linkedBrowser.setAttribute("disableglobalhistory", "true");
    tab.setAttribute("historydisabled", "true");
    tab.setAttribute("floorp-disablehistory", "true");

    const linkedBrowser = tab.linkedBrowser as unknown as {
      disableGlobalHistory?: () => void;
      docShell?: { useGlobalHistory: boolean };
      browsingContext?: { useGlobalHistory: boolean };
    };

    linkedBrowser.disableGlobalHistory?.();
    if (linkedBrowser.docShell) {
      linkedBrowser.docShell.useGlobalHistory = false;
    }
    if (linkedBrowser.browsingContext) {
      linkedBrowser.browsingContext.useGlobalHistory = false;
    }
  }

  private static checkTabIsPrivateContainer(tab: { userContextId: number }) {
    const privateContainerUserContextID = PrivateContainer
      .getPrivateContainerUserContextId();
    return tab.userContextId === privateContainerUserContextID;
  }

  private static handleTabModifications() {
    const tabs = window.gBrowser.tabs;
    for (const tab of tabs) {
      if (
        FloorpPrivateContainer.checkTabIsPrivateContainer(tab) &&
        !FloorpPrivateContainer.tabIsSaveHistory(tab)
      ) {
        FloorpPrivateContainer.applyDoNotSaveHistoryToTab(tab);
      }
    }
  }

  public static openWithPrivateContainer(url: URI) {
    let relatedToCurrent = false; //"relatedToCurrent" decide where to open the new tab. Default work as last tab (right side). Floorp use this.
    const _OPEN_NEW_TAB_POSITION_PREF = Services.prefs.getIntPref(
      "floorp.browser.tabs.openNewTabPosition",
      -1,
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
    const privateContainerUserContextID = PrivateContainer
      .getPrivateContainerUserContextId();
    if (!privateContainerUserContextID) {
      console.error(
        "[FloorpPrivateContainer] Failed to resolve private container userContextId",
      );
      return;
    }

    console.info(
      "[FloorpPrivateContainer] Opening URL in private container tab",
      {
        url: String(url),
      },
    );

    const triggeringPrincipal = Services.scriptSecurityManager
      .createNullPrincipal({
        userContextId: privateContainerUserContextID,
      });

    const newTab = window.gBrowser.addTrustedTab("about:blank", {
      relatedToCurrent,
      userContextId: privateContainerUserContextID,
      triggeringPrincipal,
    });

    if (!newTab?.linkedBrowser) {
      console.error(
        "[FloorpPrivateContainer] Failed to create browser element for private container tab",
      );
      return;
    }

    const uri = typeof url === "string" ? Services.io.newURI(url) : url;

    try {
      FloorpPrivateContainer.applyDoNotSaveHistoryToTab(newTab);

      Services.obs.notifyObservers(
        {
          wrappedJSObject: Promise.resolve(newTab.linkedBrowser),
        } as nsISupports,
        "browser-open-newtab-start",
      );

      newTab.linkedBrowser.loadURI(uri, {
        triggeringPrincipal,
        loadFlags: Ci.nsIWebNavigation.LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP,
      });
    } catch (error) {
      console.error(
        "[FloorpPrivateContainer] Failed to load URL in private container tab",
        error,
      );
    }
  }

  public static reopenInPrivateContainer() {
    let userContextId = PrivateContainer.getPrivateContainerUserContextId();
    const reopenedTabs = window.TabContextMenu.contextTab.multiselected
      ? window.gBrowser.selectedTabs
      : [window.TabContextMenu.contextTab];

    for (const tab of reopenedTabs) {
      /* Create a triggering principal that is able to load the new tab
           For content principals that are about: chrome: or resource: we need system to load them.
           Anything other than system principal needs to have the new userContextId.
        */
      let triggeringPrincipal: nsIPrincipal;

      if (tab.linkedPanel) {
        triggeringPrincipal = tab.linkedBrowser.contentPrincipal;
      } else {
        // For lazy tab browsers, get the original principal
        // from SessionStore
        const tabState = JSON.parse(window.SessionStore.getTabState(tab));
        try {
          triggeringPrincipal = window.E10SUtils.deserializePrincipal(
            tabState.triggeringPrincipal_base64,
          );
        } catch (error) {
          console.error(
            "[FloorpPrivateContainer] Failed to deserialize tab principal",
            error,
          );
          continue;
        }
      }

      if (!triggeringPrincipal || triggeringPrincipal.isNullPrincipal) {
        // Ensure that we have a null principal if we couldn't
        // deserialize it (for lazy tab browsers) ...
        // This won't always work however is safe to use.
        triggeringPrincipal = Services.scriptSecurityManager
          .createNullPrincipal({
            userContextId,
          });
      } else if (triggeringPrincipal.isContentPrincipal) {
        triggeringPrincipal = Services.scriptSecurityManager.principalWithOA(
          triggeringPrincipal,
          {
            userContextId,
          },
        );
      }

      const currentTabUserContextId = Number.parseInt(
        tab.getAttribute("usercontextid"),
      );

      if (currentTabUserContextId === userContextId) {
        userContextId = 0;
      }

      const newTab = window.gBrowser.addTab(tab.linkedBrowser.currentURI.spec, {
        userContextId,
        pinned: tab.pinned,
        index: tab._tPos + 1,
        triggeringPrincipal,
      });

      if (window.gBrowser.selectedTab === tab) {
        window.gBrowser.selectedTab = newTab;
      }
      if (tab.muted && !newTab.muted) {
        newTab.toggleMuteAudio(tab.muteReason);
      }

      window.gBrowser.removeTab(tab);
    }
  }
}
