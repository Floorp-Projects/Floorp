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
    const originalOpenTrustedLinkIn = window.openTrustedLinkIn;
    window.openTrustedLinkIn = function (
      url: string | nsIURI,
      where: string,
      options?: {
        userContextId?: number;
        relatedToCurrent?: boolean;
        [key: string]: unknown;
      },
    ) {
      const gWorkspacesServices = Workspaces.getCtx();
      const workspaceUserContextId =
        gWorkspacesServices?.getCurrentWorkspaceUserContextId() ?? 0;

      // Merge options, applying workspace container if:
      // 1. Workspace has a container (userContextId > 0)
      // 2. No userContextId is specified in options (or it's 0/default)
      const baseOptions = options ?? {};
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
        (!originalUserContextId || originalUserContextId === 0) &&
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
    if (window.openUILinkIn) {
      const originalOpenUILinkIn = window.openUILinkIn;
      window.openUILinkIn = function (
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
        // 2. No userContextId is specified in params (or it's 0/default)
        const baseParams = params ?? {};
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
          (!originalUserContextId || originalUserContextId === 0) &&
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

    window.BrowserCommands.openTab = ({
      event,
      url,
    }: {
      event?: MouseEvent;
      url?: string;
    } = {}) => {
      const werePassedURL = !!url;
      url ??= window.BROWSER_NEW_TAB_URL;
      const searchClipboard =
        window.gMiddleClickNewTabUsesPasteboard && event?.button === 1;

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
            where = window.BrowserUtils.whereToOpenLink(event, false, true);

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
              let clipboard = window.readFromClipboard();
              clipboard =
                window.UrlbarUtils.stripUnsafeProtocolOnPaste(clipboard).trim();
              if (clipboard) {
                url = clipboard;
                options.allowThirdPartyFixup = true;
              }
            }
            window.openTrustedLinkIn(url, where, options);
          }),
        },
        "browser-open-newtab-start",
      );
    };
  },
];
