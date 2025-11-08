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
  color: number | string;
  name: string;
};

/**
 * Get container color from userContextId
 * @param userContextId The user context ID
 * @returns The container color (number or string), or null if not found
 */
function getContainerColor(userContextId: number): number | string | null {
  if (userContextId === 0) {
    return null;
  }

  const container_list = ContextualIdentityService.getPublicIdentities();
  const container = container_list.find(
    (container: Container) => container.userContextId === userContextId,
  );

  if (!container) {
    return null;
  }

  const color = container.color;
  return color ?? null;
}

/**
 * Map container color (number or string) to CSS variable name
 * @param color The container color (number or string)
 * @returns The CSS variable name (e.g., "blue", "green"), or null if not found
 */
function mapColorToCSSVariable(color: number | string | null): string | null {
  if (color === null) {
    return null;
  }

  // If color is already a string (CSS variable name), return it directly
  if (typeof color === "string") {
    const normalizedColor = color.toLowerCase();

    // Map white to toolbar (Firefox uses "toolbar" as the color name for white)
    if (normalizedColor === "white") {
      return "toolbar";
    }

    // Normalize grey/gray to gray
    if (normalizedColor === "grey") {
      return "gray";
    }

    const validColors = [
      "blue",
      "turquoise",
      "green",
      "yellow",
      "orange",
      "red",
      "pink",
      "purple",
      "toolbar",
      "gray",
    ];

    if (validColors.includes(normalizedColor)) {
      return normalizedColor;
    }
    console.warn(`[Workspaces] Unknown container color string: ${color}`);
    return null;
  }

  // Map color number to CSS variable name
  const colorMap: Record<number, string> = {
    0: "blue",
    1: "turquoise",
    2: "green",
    3: "yellow",
    4: "orange",
    5: "red",
    6: "pink",
    7: "purple",
    8: "toolbar", // white is mapped to toolbar
    9: "gray",
  };

  const colorName = colorMap[color];
  if (!colorName) {
    console.warn(`[Workspaces] Unknown container color number: ${color}`);
    return null;
  }

  return colorName;
}

/**
 * Get container color CSS variable name from userContextId
 * @param userContextId The user context ID
 * @returns The CSS variable name (e.g., "blue", "green"), or null if no container
 */
export function getContainerColorName(userContextId: number): string | null {
  const color = getContainerColor(userContextId);
  return mapColorToCSSVariable(color);
}
