/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { Manifest } from "./type.ts";
import {
  getContainerColorHex,
  mapColorToCSSVariable,
  resolveContainerDisplayColorFromWindow,
} from "#libs/pwa/containerColorMap.ts";

type ContainerIdentity = {
  userContextId: number;
  color: number | string;
};

function getContextualIdentityService() {
  const { ContextualIdentityService } = ChromeUtils.importESModule(
    "resource://gre/modules/ContextualIdentityService.sys.mjs",
  );
  return ContextualIdentityService;
}

/**
 * Returns the CSS color suffix for a container (e.g. "blue", "green").
 */
export function getContainerColorName(userContextId: number): string | null {
  if (userContextId <= 0) {
    return null;
  }

  try {
    const service = getContextualIdentityService();
    const identities = service.getPublicIdentities() as ContainerIdentity[];
    const container = identities.find(
      (identity) => identity.userContextId === userContextId,
    );
    if (!container) {
      return null;
    }
    return mapColorToCSSVariable(container.color);
  } catch {
    return null;
  }
}

/**
 * Returns the localized container label, or null when not in a container.
 */
export function getContainerLabel(userContextId: number): string | null {
  if (userContextId <= 0) {
    return null;
  }

  try {
    const service = getContextualIdentityService();
    const label = service.getUserContextLabel(userContextId);
    return label || null;
  } catch {
    return null;
  }
}

/**
 * Display name for OS shortcuts. Appends container label when assigned.
 */
export function getSsbDisplayName(ssb: Manifest): string {
  const userContextId = ssb.userContextId ?? 0;
  if (userContextId <= 0) {
    return ssb.name;
  }

  const label = getContainerLabel(userContextId);
  return label ? `${ssb.name} (${label})` : ssb.name;
}

function getBrowserChromeWindow(): ColorResolvableWindow | null {
  const enumerator = Services.wm.getEnumerator("navigator:browser");
  while (enumerator.hasMoreElements()) {
    const win = enumerator.getNext() as ColorResolvableWindow | null;
    if (win?.document) {
      return win;
    }
  }
  return null;
}

type ColorResolvableWindow = {
  document: {
    documentElement: {
      appendChild: (node: unknown) => void;
    };
    createElement: (tag: string) => {
      className: string;
      hidden: boolean;
      remove: () => void;
    };
  };
  getComputedStyle: (element: unknown) => {
    getPropertyValue: (property: string) => string;
  };
};

/**
 * Resolves a ContextualIdentity color to a displayable CSS color.
 * Uses Firefox usercontext.css when a browser window is available.
 */
export function resolveContainerColor(
  color: number | string | null | undefined,
): string {
  if (typeof color === "string" && /^#[0-9a-f]{3,8}$/i.test(color)) {
    return color;
  }

  const colorName = mapColorToCSSVariable(color);
  if (!colorName) {
    return typeof color === "string" ? color : getContainerColorHex("blue");
  }

  const win = getBrowserChromeWindow();
  if (win) {
    return resolveContainerDisplayColorFromWindow(colorName, win);
  }

  return getContainerColorHex(colorName);
}
