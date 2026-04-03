/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import Workspaces from "#features-chrome/common/workspaces";

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
          } catch { return false; }
        });
        if (opentabsTab && opentabsTab !== gBrowser.selectedTab) {
          // Navigate the opentabs tab itself to about:newtab
          console.debug("Workspaces: redirecting about:opentabs pane to about:newtab");
          opentabsTab.linkedBrowser.fixupAndLoadURIString("about:newtab", {
            triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
          });
          return;
        }
        // If opentabs IS the selected tab, just let the redirect happen normally
        // (no infinite loop risk since the page will change away from about:opentabs)
        if (opentabsTab === gBrowser.selectedTab) {
          return originalOpenTrustedLinkIn.call(this, url, where, options);
        }
        // Fallback: block redirect to protect the selected tab
        console.debug("Workspaces: blocked about:newtab redirect in split view context");
        return;
      }

      // Skip workspace container logic for browser-internal about: URLs
      if (urlStr.startsWith("about:")) {
        return originalOpenTrustedLinkIn.call(this, url, where, options);
      }

      const gWorkspacesServices = Workspaces.getCtx();
      const workspaceUserContextId =
        gWorkspacesServices?.getCurrentWorkspaceUserContextId() ?? 0;

      // Merge options, applying workspace container if:
      // 1. Workspace has a container (userContextId > 0)
      // 2. No userContextId is explicitly specified in options
      // Note: If userContextId is explicitly set to 0 ("no container"), respect that choice
      const baseOptions = options ?? {};
      const hasExplicitUserContextId = "userContextId" in baseOptions;
      const originalUserContextId =
        typeof baseOptions.userContextId === "number"
          ? baseOptions.userContextId
          : undefined;

      const potentialTargetBrowser = baseOptions.targetBrowser;
      const targetBrowserUserContextId =
        typeof potentialTargetBrowser === "object" &&
        potentialTargetBrowser !== null &&
        typeof (potentialTargetBrowser as { userContextId?: unknown })
          .userContextId === "number"
          ? (potentialTargetBrowser as { userContextId: number }).userContextId
          : undefined;

      const shouldRespectExistingContext =
        where === "current" || targetBrowserUserContextId !== undefined;

      const shouldApplyWorkspaceContainer =
        workspaceUserContextId > 0 &&
        !hasExplicitUserContextId &&
        !shouldRespectExistingContext;

      const mergedOptions = {
        ...baseOptions,
        userContextId: shouldApplyWorkspaceContainer
          ? workspaceUserContextId
          : (originalUserContextId ?? targetBrowserUserContextId ?? 0),
      };

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
        const workspaceUserContextId =
          gWorkspacesServices?.getCurrentWorkspaceUserContextId() ?? 0;

        // Merge params, applying workspace container if:
        // 1. Workspace has a container (userContextId > 0)
        // 2. No userContextId is explicitly specified in params
        // Note: If userContextId is explicitly set to 0 ("no container"), respect that choice
        const baseParams = params ?? {};
        const hasExplicitUserContextId = "userContextId" in baseParams;
        const originalUserContextId =
          typeof baseParams.userContextId === "number"
            ? baseParams.userContextId
            : undefined;

        const potentialTargetBrowser = baseParams.targetBrowser;
        const targetBrowserUserContextId =
          typeof potentialTargetBrowser === "object" &&
          potentialTargetBrowser !== null &&
          typeof (potentialTargetBrowser as { userContextId?: unknown })
            .userContextId === "number"
            ? (potentialTargetBrowser as { userContextId: number })
                .userContextId
            : undefined;

        const shouldRespectExistingContext =
          where === "current" || targetBrowserUserContextId !== undefined;

        const shouldApplyWorkspaceContainer =
          workspaceUserContextId > 0 &&
          !hasExplicitUserContextId &&
          !shouldRespectExistingContext;

        const mergedParams = {
          ...baseParams,
          userContextId: shouldApplyWorkspaceContainer
            ? workspaceUserContextId
            : (originalUserContextId ?? targetBrowserUserContextId ?? 0),
        };

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

    globalThis.BrowserCommands.openTab = ({
      event,
      url,
    }: {
      event?: MouseEvent;
      url?: string;
    } = {}) => {
      const werePassedURL = !!url;
      url ??= globalThis.BROWSER_NEW_TAB_URL;
      const searchClipboard =
        globalThis.gMiddleClickNewTabUsesPasteboard && event?.button === 1;

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
              userContextId:
                gWorkspacesServices?.getCurrentWorkspaceUserContextId() ?? 0,
              allowThirdPartyFixup: undefined as boolean | undefined,
            } satisfies {
              relatedToCurrent: boolean;
              resolveOnNewTabCreated: (browser: unknown) => void;
              userContextId: number;
              allowThirdPartyFixup?: boolean;
              [key: string]: unknown;
            };
            if (!werePassedURL && searchClipboard) {
              let clipboard = globalThis.readFromClipboard();
              clipboard =
                globalThis.UrlbarUtils.stripUnsafeProtocolOnPaste(clipboard).trim();
              if (clipboard) {
                url = clipboard;
                options.allowThirdPartyFixup = true;
              }
            }
            globalThis.openTrustedLinkIn(url, where, options);
          }),
        },
        "browser-open-newtab-start",
      );
    };
  },
];
