/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { Manifest } from "./type.ts";
import { mapColorToCSSVariable } from "#libs/pwa/containerColorMap.ts";

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
