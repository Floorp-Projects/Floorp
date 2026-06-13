/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { Browser } from "./type.ts";
import i18next from "i18next";

export interface ContainerOption {
  userContextId: number;
  label: string;
}

type PublicIdentity = {
  name: string;
  userContextId: number;
  l10nId?: string;
};

type BrowserTab = XULElement & {
  getAttribute: (name: string) => string | null;
};

type ExtendedGBrowser = typeof globalThis.gBrowser & {
  getTabForBrowser: (browser: Browser) => BrowserTab | null;
};

/**
 * Returns the Firefox Container userContextId for the tab hosting `browser`.
 * Returns 0 when no container is assigned or the tab cannot be resolved.
 */
export function getUserContextIdForBrowser(browser: Browser): number {
  try {
    const gBrowser = globalThis.gBrowser as ExtendedGBrowser;
    const tab = gBrowser.getTabForBrowser?.(browser);
    if (!tab) {
      return 0;
    }
    const raw = tab.getAttribute("usercontextid");
    if (!raw) {
      return 0;
    }
    const parsed = parseInt(raw, 10);
    return Number.isFinite(parsed) && parsed > 0 ? parsed : 0;
  } catch {
    return 0;
  }
}

/**
 * Returns a localized container label for display, or null when not in a container.
 */
export function getContainerLabel(userContextId: number): string | null {
  if (userContextId <= 0) {
    return null;
  }
  try {
    const { ContextualIdentityService } = ChromeUtils.importESModule(
      "resource://gre/modules/ContextualIdentityService.sys.mjs",
    );
    const label = ContextualIdentityService.getUserContextLabel(userContextId);
    return label || null;
  } catch {
    return null;
  }
}

/**
 * Returns container options for install UI, including a "no container" entry.
 */
export function getPublicContainerOptions(): ContainerOption[] {
  const noContainerLabel = i18next.t("ssb.page-action.no-container");
  const options: ContainerOption[] = [
    { userContextId: 0, label: noContainerLabel },
  ];

  try {
    const { ContextualIdentityService } = ChromeUtils.importESModule(
      "resource://gre/modules/ContextualIdentityService.sys.mjs",
    );
    const identities = ContextualIdentityService.getPublicIdentities() as
      PublicIdentity[];

    for (const identity of identities) {
      const label = identity.l10nId
        ? ContextualIdentityService.getUserContextLabel(identity.userContextId)
        : identity.name;
      options.push({
        userContextId: identity.userContextId,
        label,
      });
    }
  } catch (error) {
    console.error("[containerUtils] Failed to load container options:", error);
  }

  return options;
}
