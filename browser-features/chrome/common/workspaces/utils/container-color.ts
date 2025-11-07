/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { ContextualIdentityService } = ChromeUtils.importESModule(
  "resource://gre/modules/ContextualIdentityService.sys.mjs",
);

type Container = {
  userContextId: number;
  public: boolean;
  icon: string;
  color: number;
  name: string;
};

/**
 * Get container color number from userContextId
 * @param userContextId The user context ID
 * @returns The container color number, or null if not found
 */
function getContainerColorNumber(userContextId: number): number | null {
  if (userContextId === 0) {
    return null;
  }

  const container_list = ContextualIdentityService.getPublicIdentities();
  return (
    container_list.find(
      (container: Container) => container.userContextId === userContextId,
    )?.color ?? null
  );
}

/**
 * Map container color number to CSS variable name
 * @param colorNumber The container color number
 * @returns The CSS variable name (e.g., "blue", "green"), or null if not found
 */
function mapColorNumberToCSSVariable(
  colorNumber: number | null,
): string | null {
  if (colorNumber === null) {
    return null;
  }

  const colorMap: Record<number, string> = {
    0: "blue",
    1: "turquoise",
    2: "green",
    3: "yellow",
    4: "orange",
    5: "red",
    6: "pink",
    7: "purple",
  };

  return colorMap[colorNumber] ?? null;
}

/**
 * Get container color CSS variable name from userContextId
 * @param userContextId The user context ID
 * @returns The CSS variable name (e.g., "blue", "green"), or null if no container
 */
export function getContainerColorName(userContextId: number): string | null {
  const colorNumber = getContainerColorNumber(userContextId);
  return mapColorNumberToCSSVariable(colorNumber);
}
