/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type {
  ContextMenuConfig,
  ContextMenuItemDescriptor,
  ContextMenuLevelOverride,
  ContextMenuSurfaceConfig,
} from "#features-chrome/common/context-menu/types.ts";
import {
  cloneContextMenuConfig,
  DEFAULT_CONTEXT_MENU_CONFIG,
} from "#features-chrome/common/context-menu/config.ts";

export interface ContextMenuLevelTarget {
  surfaceKey: string;
  profileKey: string;
  containerKey: string;
}

const EMPTY_LEVEL_OVERRIDE: ContextMenuLevelOverride = {};

function cloneLevelOverride(
  override: ContextMenuLevelOverride,
): ContextMenuLevelOverride {
  return {
    ...override,
    ...(override.order ? { order: [...override.order] } : {}),
    ...(override.hidden ? { hidden: [...override.hidden] } : {}),
  };
}

function cloneSurfaceConfig(
  surface: ContextMenuSurfaceConfig | undefined,
): ContextMenuSurfaceConfig {
  return surface
    ? {
      ...surface,
      base: { ...surface.base },
      profiles: { ...surface.profiles },
    }
    : { base: {}, profiles: {} };
}

function uniqueKeys(keys: readonly string[]): string[] {
  const seen = new Set<string>();
  return keys.filter((key) => {
    if (seen.has(key)) return false;
    seen.add(key);
    return true;
  });
}

export function createDefaultContextMenuConfig(): ContextMenuConfig {
  return cloneContextMenuConfig(DEFAULT_CONTEXT_MENU_CONFIG);
}

export function isProfileIndependent(
  config: ContextMenuConfig,
  surfaceKey: string,
  profileKey: string,
): boolean {
  return config.surfaces[surfaceKey]?.profiles[profileKey]?.independent ??
    false;
}

export function hasProfileOverride(
  config: ContextMenuConfig,
  surfaceKey: string,
  profileKey: string,
): boolean {
  return config.surfaces[surfaceKey]?.profiles[profileKey] !== undefined;
}

export function getContextMenuLevelOverride(
  config: ContextMenuConfig,
  target: ContextMenuLevelTarget,
): ContextMenuLevelOverride {
  const surface = config.surfaces[target.surfaceKey];
  const profile = surface?.profiles[target.profileKey];
  if (profile?.independent) {
    return profile.containers[target.containerKey] ?? EMPTY_LEVEL_OVERRIDE;
  }
  return surface?.base[target.containerKey] ?? EMPTY_LEVEL_OVERRIDE;
}

type ContextMenuCapabilityItem = Pick<
  ContextMenuItemDescriptor,
  "customizable" | "movable" | "hideable" | "orderAnchor"
>;

type ContextMenuOrderItem =
  & ContextMenuCapabilityItem
  & Pick<ContextMenuItemDescriptor, "key">;

export function isContextMenuItemMovable(
  item: ContextMenuCapabilityItem,
): boolean {
  return item.movable ?? item.customizable;
}

export function isContextMenuItemHideable(
  item: ContextMenuCapabilityItem,
): boolean {
  return item.hideable ?? item.customizable;
}

export function isContextMenuItemOrderAnchor(
  item: ContextMenuCapabilityItem,
): boolean {
  return item.orderAnchor ?? isContextMenuItemMovable(item);
}

export function getMovableContextMenuItemKeys(
  catalogItems: readonly ContextMenuOrderItem[],
): string[] {
  return uniqueKeys(
    catalogItems.flatMap((item) =>
      isContextMenuItemMovable(item) ? [item.key] : []
    ),
  );
}

export function getContextMenuOrderAnchorKeys(
  catalogItems: readonly ContextMenuOrderItem[],
): string[] {
  return uniqueKeys(
    catalogItems.flatMap((item) =>
      isContextMenuItemOrderAnchor(item) ? [item.key] : []
    ),
  );
}

export type ContextMenuMoveDirection = "up" | "down";

/**
 * Find the next order anchor for a one-step button move. Protected native
 * items are deliberately skipped: the reorder swaps stable anchors around
 * them while native-slot projection keeps each protected item in place.
 */
export function getContextMenuMoveTargetKey(
  orderedItems: readonly ContextMenuOrderItem[],
  activeKey: string,
  direction: ContextMenuMoveDirection,
): string | undefined {
  const activeItem = orderedItems.find((item) => item.key === activeKey);
  if (!activeItem || !isContextMenuItemMovable(activeItem)) return undefined;

  const anchorKeys = getContextMenuOrderAnchorKeys(orderedItems);
  const activeIndex = anchorKeys.indexOf(activeKey);
  if (activeIndex < 0) return undefined;

  return anchorKeys[activeIndex + (direction === "up" ? -1 : 1)];
}

/**
 * Project the configured stable-key order back into Firefox's native slots.
 * Anonymous, protected, or otherwise unstable items keep their catalog
 * positions. Stable separators participate alongside movable commands.
 */
export function projectContextMenuItemKeysIntoNativeSlots(
  catalogItems: readonly ContextMenuOrderItem[],
  override: ContextMenuLevelOverride,
): string[] {
  const anchorKeySet = new Set(getContextMenuOrderAnchorKeys(catalogItems));
  const orderedAnchorKeys = uniqueKeys(override.order ?? []).filter((key) =>
    anchorKeySet.has(key)
  );
  const configuredAnchorKeySet = new Set(orderedAnchorKeys);
  let anchorIndex = 0;

  return catalogItems.map((item) => {
    // An item introduced by a newer Firefox version is not in the saved
    // order. Leave that item in its native slot until the user moves it.
    if (!configuredAnchorKeySet.has(item.key)) return item.key;
    return orderedAnchorKeys[anchorIndex++] ?? item.key;
  });
}

function mergeUnknownKeysIntoAnchorOrder(
  previousOrder: readonly string[],
  catalogKeySet: ReadonlySet<string>,
  anchorKeySet: ReadonlySet<string>,
  anchorOrder: readonly string[],
): string[] {
  const leadingUnknownKeys: string[] = [];
  const unknownKeysAfterAnchor = new Map<string, string[]>();
  let previousAnchor: string | null = null;

  for (const key of uniqueKeys(previousOrder)) {
    if (anchorKeySet.has(key)) {
      previousAnchor = key;
      continue;
    }
    if (catalogKeySet.has(key)) continue;

    if (!previousAnchor) {
      leadingUnknownKeys.push(key);
      continue;
    }
    const trailingKeys = unknownKeysAfterAnchor.get(previousAnchor) ?? [];
    trailingKeys.push(key);
    unknownKeysAfterAnchor.set(previousAnchor, trailingKeys);
  }

  return [
    ...leadingUnknownKeys,
    ...anchorOrder.flatMap((key) => [
      key,
      ...(unknownKeysAfterAnchor.get(key) ?? []),
    ]),
  ];
}

function updateLevelOverride(
  config: ContextMenuConfig,
  target: ContextMenuLevelTarget,
  update: (
    current: ContextMenuLevelOverride,
  ) => ContextMenuLevelOverride,
): ContextMenuConfig {
  const currentSurface = config.surfaces[target.surfaceKey];
  const surface = cloneSurfaceConfig(currentSurface);
  const currentProfile = surface.profiles[target.profileKey];

  if (currentProfile?.independent) {
    surface.profiles[target.profileKey] = {
      ...currentProfile,
      containers: {
        ...currentProfile.containers,
        [target.containerKey]: update(
          currentProfile.containers[target.containerKey] ??
            EMPTY_LEVEL_OVERRIDE,
        ),
      },
    };
  } else {
    surface.base[target.containerKey] = update(
      surface.base[target.containerKey] ?? EMPTY_LEVEL_OVERRIDE,
    );
  }

  return {
    ...config,
    surfaces: {
      ...config.surfaces,
      [target.surfaceKey]: surface,
    },
  };
}

export function setContextMenuItemHidden(
  config: ContextMenuConfig,
  target: ContextMenuLevelTarget,
  itemKey: string,
  hidden: boolean,
): ContextMenuConfig {
  return updateLevelOverride(config, target, (current) => {
    const hiddenKeys = new Set(current.hidden ?? []);
    if (hidden) {
      hiddenKeys.add(itemKey);
    } else {
      hiddenKeys.delete(itemKey);
    }
    return {
      ...cloneLevelOverride(current),
      hidden: [...hiddenKeys],
    };
  });
}

export function reorderContextMenuItemByKey(
  config: ContextMenuConfig,
  target: ContextMenuLevelTarget,
  catalogItems: readonly ContextMenuOrderItem[],
  activeKey: string,
  overKey: string,
): ContextMenuConfig {
  if (activeKey === overKey) return config;

  return updateLevelOverride(config, target, (current) => {
    const catalogKeys = uniqueKeys(catalogItems.map((item) => item.key));
    const catalogKeySet = new Set(catalogKeys);
    const movableKeys = getMovableContextMenuItemKeys(catalogItems);
    const movableKeySet = new Set(movableKeys);
    const anchorKeys = getContextMenuOrderAnchorKeys(catalogItems);
    const anchorKeySet = new Set(anchorKeys);
    if (!movableKeySet.has(activeKey) || !anchorKeySet.has(overKey)) {
      return current;
    }

    const projectedKeys = projectContextMenuItemKeysIntoNativeSlots(
      catalogItems,
      current,
    );
    const projectedAnchorOrder = projectedKeys.filter((key) =>
      anchorKeySet.has(key)
    );
    const oldIndex = projectedAnchorOrder.indexOf(activeKey);
    const newIndex = projectedAnchorOrder.indexOf(overKey);
    if (oldIndex < 0 || newIndex < 0) return current;

    const nextAnchorOrder = [...projectedAnchorOrder];
    const [movedKey] = nextAnchorOrder.splice(oldIndex, 1);
    nextAnchorOrder.splice(newIndex, 0, movedKey);
    // Keep keys that temporarily disappeared from Firefox's catalog. A future
    // browser update can make them available again without losing user intent.
    // Known non-anchors are deliberately removed so protected nodes retain
    // their exact native slots.
    const nextOrder = mergeUnknownKeysIntoAnchorOrder(
      current.order ?? [],
      catalogKeySet,
      anchorKeySet,
      nextAnchorOrder,
    );
    return {
      ...cloneLevelOverride(current),
      order: nextOrder,
    };
  });
}

/**
 * Move an item to an explicit gap in the stable anchor order. `beforeKey`
 * identifies the anchor immediately after that gap; nullish values identify
 * the gap after the final anchor. Protected native items never become anchors,
 * so their native slots remain fixed when the resulting order is projected.
 */
export function moveContextMenuItemBeforeKey(
  config: ContextMenuConfig,
  target: ContextMenuLevelTarget,
  catalogItems: readonly ContextMenuOrderItem[],
  activeKey: string,
  beforeKey?: string | null,
): ContextMenuConfig {
  const catalogKeys = uniqueKeys(catalogItems.map((item) => item.key));
  const catalogKeySet = new Set(catalogKeys);
  const movableKeySet = new Set(getMovableContextMenuItemKeys(catalogItems));
  const anchorKeys = getContextMenuOrderAnchorKeys(catalogItems);
  const anchorKeySet = new Set(anchorKeys);
  if (
    !movableKeySet.has(activeKey) ||
    !anchorKeySet.has(activeKey) ||
    (beforeKey != null && !anchorKeySet.has(beforeKey)) ||
    activeKey === beforeKey
  ) {
    return config;
  }

  const current = getContextMenuLevelOverride(config, target);
  const projectedAnchorOrder = projectContextMenuItemKeysIntoNativeSlots(
    catalogItems,
    current,
  ).filter((key) => anchorKeySet.has(key));
  const oldIndex = projectedAnchorOrder.indexOf(activeKey);
  if (oldIndex < 0) return config;

  const nextAnchorOrder = projectedAnchorOrder.filter((key) =>
    key !== activeKey
  );
  const insertionIndex = beforeKey == null
    ? nextAnchorOrder.length
    : nextAnchorOrder.indexOf(beforeKey);
  if (insertionIndex < 0) return config;
  nextAnchorOrder.splice(insertionIndex, 0, activeKey);

  if (
    nextAnchorOrder.length === projectedAnchorOrder.length &&
    nextAnchorOrder.every((key, index) => key === projectedAnchorOrder[index])
  ) {
    return config;
  }

  return updateLevelOverride(config, target, (level) => ({
    ...cloneLevelOverride(level),
    // Preserve keys that temporarily disappeared from Firefox's catalog,
    // while removing known protected keys from older saved configurations.
    order: mergeUnknownKeysIntoAnchorOrder(
      level.order ?? [],
      catalogKeySet,
      anchorKeySet,
      nextAnchorOrder,
    ),
  }));
}

export function setContextMenuProfileIndependent(
  config: ContextMenuConfig,
  surfaceKey: string,
  profileKey: string,
  independent: boolean,
): ContextMenuConfig {
  const surface = cloneSurfaceConfig(config.surfaces[surfaceKey]);
  const currentProfile = surface.profiles[profileKey];
  const retainedContainers = currentProfile?.containers ?? {};

  surface.profiles[profileKey] = {
    ...currentProfile,
    independent,
    // A newly-independent profile starts with the shared layout. Retain an
    // older independent layout when the switch is turned off and on again.
    containers: independent && Object.keys(retainedContainers).length === 0
      ? Object.fromEntries(
        Object.entries(surface.base).map(([key, value]) => [
          key,
          cloneLevelOverride(value),
        ]),
      )
      : { ...retainedContainers },
  };

  return {
    ...config,
    surfaces: {
      ...config.surfaces,
      [surfaceKey]: surface,
    },
  };
}

export function resetContextMenuProfile(
  config: ContextMenuConfig,
  surfaceKey: string,
  profileKey: string,
): ContextMenuConfig {
  const currentSurface = config.surfaces[surfaceKey];
  if (!currentSurface?.profiles[profileKey]) return config;

  const surface = cloneSurfaceConfig(currentSurface);
  delete surface.profiles[profileKey];
  return {
    ...config,
    surfaces: {
      ...config.surfaces,
      [surfaceKey]: surface,
    },
  };
}
