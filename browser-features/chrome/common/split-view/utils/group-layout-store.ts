/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type {
  SplitViewLayout,
  SplitViewPaneSizes,
} from "../data/types.js";

export type GroupLayoutEntry = {
  groupId: string;
  layout: SplitViewLayout;
  paneSizes?: SplitViewPaneSizes;
};

export type GroupLayoutStore = {
  groups: GroupLayoutEntry[];
};

const VALID_LAYOUTS = new Set<SplitViewLayout>([
  "horizontal",
  "vertical",
  "grid-2x2",
  "grid-3pane-left-main",
  "grid-3pane-right-main",
  "grid-3pane-top-main",
  "grid-3pane-bottom-main",
]);

function isValidPaneSizes(
  value: unknown,
): value is SplitViewPaneSizes {
  if (typeof value !== "object" || value === null) {
    return false;
  }
  const candidate = value as {
    flexRatios?: unknown;
    gridColRatio?: unknown;
    gridRowRatio?: unknown;
  };
  return (
    Array.isArray(candidate.flexRatios) &&
    candidate.flexRatios.every((entry) => typeof entry === "number") &&
    typeof candidate.gridColRatio === "number" &&
    Number.isFinite(candidate.gridColRatio) &&
    typeof candidate.gridRowRatio === "number" &&
    Number.isFinite(candidate.gridRowRatio)
  );
}

export function parseGroupLayoutStore(raw: string): GroupLayoutStore {
  const empty: GroupLayoutStore = { groups: [] };
  try {
    const parsed = JSON.parse(raw) as unknown;
    if (typeof parsed !== "object" || parsed === null) {
      return empty;
    }
    const groups = Array.isArray(
      (parsed as { groups?: unknown }).groups,
    )
      ? (parsed as { groups: unknown[] }).groups
      : [];
    return {
      groups: groups.flatMap((entry) => {
        if (typeof entry !== "object" || entry === null) {
          return [];
        }
        const candidate = entry as {
          groupId?: unknown;
          layout?: unknown;
        };
        if (
          typeof candidate.groupId !== "string" ||
          !VALID_LAYOUTS.has(candidate.layout as SplitViewLayout)
        ) {
          return [];
        }
        return [{
          groupId: candidate.groupId,
          layout: candidate.layout as SplitViewLayout,
          paneSizes: isValidPaneSizes(
            (candidate as { paneSizes?: unknown }).paneSizes,
          )
            ? (candidate as { paneSizes: SplitViewPaneSizes }).paneSizes
            : undefined,
        }];
      }),
    };
  } catch {
    return empty;
  }
}

export function getGroupLayoutFromStore(
  store: GroupLayoutStore,
  groupId: string,
): SplitViewLayout | null {
  return store.groups.find((entry) => entry.groupId === groupId)?.layout ?? null;
}

export function getGroupPaneSizesFromStore(
  store: GroupLayoutStore,
  groupId: string,
): SplitViewPaneSizes | null {
  return store.groups.find((entry) => entry.groupId === groupId)?.paneSizes ?? null;
}

export function upsertGroupLayoutInStore(
  store: GroupLayoutStore,
  groupId: string,
  layout: SplitViewLayout,
): GroupLayoutStore {
  const nextGroups = [...store.groups];
  const idx = nextGroups.findIndex((entry) => entry.groupId === groupId);
  if (idx >= 0) {
    nextGroups[idx] = { groupId, layout };
  } else {
    nextGroups.push({ groupId, layout });
  }
  return { groups: nextGroups };
}

export function upsertGroupPaneSizesInStore(
  store: GroupLayoutStore,
  groupId: string,
  paneSizes: SplitViewPaneSizes,
): GroupLayoutStore {
  const nextGroups = [...store.groups];
  const idx = nextGroups.findIndex((entry) => entry.groupId === groupId);
  if (idx >= 0) {
    nextGroups[idx] = { ...nextGroups[idx]!, paneSizes };
  } else {
    nextGroups.push({
      groupId,
      layout: "horizontal",
      paneSizes,
    });
  }
  return { groups: nextGroups };
}
