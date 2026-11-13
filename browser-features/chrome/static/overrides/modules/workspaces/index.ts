/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { readNewTabClipboard } from "./clipboard.ts";
import Workspaces from "#features-chrome/common/workspaces";
import { chooseNewTabContainer } from "#features-chrome/common/workspaces/workspace-container-ui.ts";
import { WORKSPACE_ENABLED_PREF_NAME } from "#features-chrome/common/workspaces/utils/workspaces-static-names.ts";
import {
  resolveWorkspaceOpenLinkUserContext,
  shouldBlockWorkspaceOpenLink,
  workspaceDefaultIsPending,
} from "./open-link-user-context.ts";

const { PrivateBrowsingUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
);

function defaultContainerPending(canOpen: boolean | undefined): boolean {
  return workspaceDefaultIsPending(
    Services.prefs.getBoolPref(WORKSPACE_ENABLED_PREF_NAME, true),
    PrivateBrowsingUtils.isWindowPrivate(window),
    canOpen,
    Services.prefs.getBoolPref("privacy.userContext.enabled", false),
  );
}

console.log("Workspaces override loaded");

export const overrides = [
  () => {
    // Override openTrustedLinkIn to apply workspace container
    // This handles tabs opened from bookmarks, external links, etc.
    const originalOpenTrustedLinkIn = globalThis.openTrustedLinkIn;
    globalThis.openTrustedLinkIn = function (
      url: string | nsIURI,
      where: string,
      options?: {
        userContextId?: number;
        relatedToCurrent?: boolean;
        [key: string]: unknown;
      },
    ) {
      const urlStr = typeof url === "string" ? url : url.spec;

      // Handle about:opentabs → about:newtab redirect in split view.
      // When about:opentabs has no unsplit tabs to display, its render()
      // calls openTrustedLinkIn("about:newtab", "current"), which would
      // navigate the SELECTED tab (user's page) to about:newtab.
      // Instead, find the about:opentabs tab and navigate IT to about:newtab,
      // so the user's page is preserved and the empty pane shows a new tab.
      if (
        urlStr === "about:newtab" &&
        where === "current" &&
        // deno-lint-ignore no-explicit-any
        (window as any).gBrowser?.selectedTab?.splitview
      ) {
        // deno-lint-ignore no-explicit-any
        const gBrowser = (window as any).gBrowser;
        // Find the about:opentabs tab in the split view
        // deno-lint-ignore no-explicit-any
        const opentabsTab = gBrowser.tabs.find((t: any) => {
          try {
            return t.linkedBrowser?.currentURI?.spec === "about:opentabs";
          } catch {
            return false;
          }
        });
        if (opentabsTab && opentabsTab !== gBrowser.selectedTab) {
          // Navigate the opentabs tab itself to about:newtab
          console.debug(
            "Workspaces: redirecting about:opentabs pane to about:newtab",
          );
          opentabsTab.linkedBrowser.fixupAndLoadURIString("about:newtab", {
            triggeringPrincipal: Services.scriptSecurityManager
              .getSystemPrincipal(),
          });
          return;
        }
        // If opentabs IS the selected tab, just let the redirect happen normally
        // (no infinite loop risk since the page will change away from about:opentabs)
        if (opentabsTab === gBrowser.selectedTab) {
          return originalOpenTrustedLinkIn.call(this, url, where, options);
        }
        // Fallback: block redirect to protect the selected tab
        console.debug(
          "Workspaces: blocked about:newtab redirect in split view context",
        );
        return;
      }

      // Skip workspace container logic for browser-internal about: URLs
      if (urlStr.startsWith("about:")) {
        return originalOpenTrustedLinkIn.call(this, url, where, options);
      }

      const gWorkspacesServices = Workspaces.getCtx();
      if (
        shouldBlockWorkspaceOpenLink(
          options,
          where,
          defaultContainerPending(gWorkspacesServices?.canOpenWorkspaceTab()),
        )
      ) {
        console.error(
          "[Workspaces] Trusted link blocked: default container is pending",
        );
        return;
      }
      const workspaceUserContextId =
        Services.prefs.getBoolPref("privacy.userContext.enabled", false)
          ? gWorkspacesServices?.getCurrentWorkspaceUserContextId() ?? 0
          : 0;

      const {
        options: mergedOptions,
        originalUserContextId,
        shouldApplyWorkspaceContainer,
      } = resolveWorkspaceOpenLinkUserContext(
        options,
        where,
        workspaceUserContextId,
      );

      console.debug("Workspaces: openTrustedLinkIn override", {
        url: typeof url === "string" ? url : url.spec,
        where,
        originalUserContextId,
        shouldApplyWorkspaceContainer,
        appliedUserContextId: mergedOptions.userContextId,
      });

      return originalOpenTrustedLinkIn.call(this, url, where, mergedOptions);
    };

    // Override openUILinkIn to apply workspace container
    // This is used by some bookmark and external link handlers
    if (globalThis.openUILinkIn) {
      const originalOpenUILinkIn = globalThis.openUILinkIn;
      globalThis.openUILinkIn = function (
        url: string | nsIURI,
        where: string,
        params?: {
          userContextId?: number;
          [key: string]: unknown;
        },
      ) {
        const gWorkspacesServices = Workspaces.getCtx();
        if (
          shouldBlockWorkspaceOpenLink(
            params,
            where,
            defaultContainerPending(gWorkspacesServices?.canOpenWorkspaceTab()),
          )
        ) {
          console.error(
            "[Workspaces] UI link blocked: default container is pending",
          );
          return;
        }
        const workspaceUserContextId =
          Services.prefs.getBoolPref("privacy.userContext.enabled", false)
            ? gWorkspacesServices?.getCurrentWorkspaceUserContextId() ?? 0
            : 0;

        const {
          options: mergedParams,
          originalUserContextId,
          shouldApplyWorkspaceContainer,
        } = resolveWorkspaceOpenLinkUserContext(
          params,
          where,
          workspaceUserContextId,
        );

        console.debug("Workspaces: openUILinkIn override", {
          url: typeof url === "string" ? url : url.spec,
          where,
          originalUserContextId,
          shouldApplyWorkspaceContainer,
          appliedUserContextId: mergedParams.userContextId,
        });

        return originalOpenUILinkIn.call(this, url, where, mergedParams);
      };
    }

    let choosingNewTabContainer = false;
    globalThis.BrowserCommands.openTab = ({
      event,
      url,
    }: {
      event?: MouseEvent;
      url?: string;
    } = {}) => {
      if (choosingNewTabContainer) {
        console.error("[Workspaces] New tab blocked: container chooser is open");
        return;
      }
      const werePassedURL = !!url;
      url ??= globalThis.BROWSER_NEW_TAB_URL;
      const searchClipboard = globalThis.gMiddleClickNewTabUsesPasteboard &&
        event?.button === 1;

      let relatedToCurrent = false;
      let where = "tab";
      const OPEN_NEW_TAB_POSITION_PREF = Services.prefs.getIntPref(
        "floorp.browser.tabs.openNewTabPosition",
      );

      switch (OPEN_NEW_TAB_POSITION_PREF) {
        case 0:
          relatedToCurrent = false;
          break;
        case 1:
          relatedToCurrent = true;
          break;
        default:
          if (event) {
            where = globalThis.BrowserUtils.whereToOpenLink(event, false, true);

            switch (where) {
              case "tab":
              case "tabshifted":
                // When accel-click or middle-click are used, open the new tab as
                // related to the current tab.
                relatedToCurrent = true;
                break;
              case "current":
                where = "tab";
                break;
            }
          }
      }

      // A notification intended to be useful for modular peformance tracking
      // starting as close as is reasonably possible to the time when the user
      // expressed the intent to open a new tab.  Since there are a lot of
      // entry points, this won't catch every single tab created, but most
      // initiated by the user should go through here.
      //
      // Note 1: This notification gets notified with a promise that resolves
      //         with the linked browser when the tab gets created
      // Note 2: This is also used to notify a user that an extension has changed
      //         the New Tab page.

      const gWorkspacesServices = Workspaces.getCtx();
      let userContextId: number;
      if (defaultContainerPending(gWorkspacesServices?.canOpenWorkspaceTab())) {
        if (
          werePassedURL || searchClipboard ||
          !window.windowUtils.isHandlingUserInput
        ) {
          console.error(
            "[Workspaces] New tab blocked: default container is pending",
          );
          return;
        }
        const workspaceId = gWorkspacesServices?.getSelectedWorkspaceID();
        choosingNewTabContainer = true;
        let selectedContainer: number | null;
        try {
          selectedContainer = chooseNewTabContainer();
        } finally {
          choosingNewTabContainer = false;
        }
        if (
          selectedContainer === null || window.closed ||
          workspaceId !== Workspaces.getCtx()?.getSelectedWorkspaceID()
        ) return;
        userContextId = selectedContainer;
      } else {
        userContextId =
          Services.prefs.getBoolPref("privacy.userContext.enabled", false)
            ? gWorkspacesServices?.getCurrentWorkspaceUserContextId() ?? 0
            : 0;
      }

      console.log(
        "gWorkspacesServices?.getCurrentWorkspaceUserContextId()",
        url,
      );

      Services.obs.notifyObservers(
        {
          wrappedJSObject: new Promise((resolve) => {
            const options = {
              relatedToCurrent,
              resolveOnNewTabCreated: resolve,
              userContextId,
              allowThirdPartyFixup: undefined as boolean | undefined,
            } satisfies {
              relatedToCurrent: boolean;
              resolveOnNewTabCreated: (browser: unknown) => void;
              userContextId: number;
              allowThirdPartyFixup?: boolean;
              [key: string]: unknown;
            };
            if (!werePassedURL && searchClipboard) {
              const clipboard = readNewTabClipboard(
                () => globalThis.readFromClipboard?.() || "",
                () => {
                  const trans = Cc[
                    "@mozilla.org/widget/transferable;1"
                  ].createInstance(Ci.nsITransferable);
                  const context = window.docShell?.QueryInterface?.(
                    Ci.nsILoadContext,
                  );
                  if (!context) return "";
                  trans.init(context);
                  trans.addDataFlavor("text/plain");
                  Services.clipboard.getData(
                    trans,
                    Ci.nsIClipboard.kGlobalClipboard,
                  );
                  const data: { value?: nsISupports } = {};
                  trans.getTransferData("text/plain", data);
                  return data.value?.QueryInterface?.(Ci.nsISupportsString)
                    .data || "";
                },
                globalThis.UrlbarShared?.stripUnsafeProtocolOnPaste ??
                  globalThis.UrlbarUtils?.stripUnsafeProtocolOnPaste,
              );

              if (clipboard) {
                url = clipboard;
                options.allowThirdPartyFixup = true;
              }
            }
            globalThis.openTrustedLinkIn(url!, where, options);
          }),
        },
        "browser-open-newtab-start",
      );
    };
  },
];
