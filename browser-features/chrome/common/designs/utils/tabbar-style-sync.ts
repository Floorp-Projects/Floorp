/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { TFloorpDesignConfigs } from "../type.ts";

/**
 * Firefox's own switch for the native vertical tab bar.
 *
 * Setting it to `true` also makes Firefox set `sidebar.revamp` to `true`
 * itself, so Floorp must never write `sidebar.revamp` on its own: doing so
 * would break Firefox's invariant that vertical tabs imply the revamped
 * sidebar.
 */
export const VERTICAL_TABS_PREF = "sidebar.verticalTabs";

export type TabbarStyle = TFloorpDesignConfigs["tabbar"]["tabbarStyle"];

/** Validate a value read from JSON or another untyped source. */
export function normalizeTabbarStyle(value: unknown): TabbarStyle {
  return value === "horizontal" || value === "vertical" || value === "multirow"
    ? value
    : "horizontal";
}

/**
 * Derive the Floorp tab bar style from the native vertical tabs pref.
 *
 * Turning native vertical tabs on always means "vertical". Turning them off
 * only demotes an existing vertical style back to "horizontal"; horizontal and
 * multirow styles are left untouched, since the native pref says nothing about
 * them.
 *
 * This module must stay free of `Services` access so the settings page, which
 * does not load the Gecko typings, can import it too.
 */
export function resolveTabbarStyleFromNative(
  isVertical: boolean,
  current: TabbarStyle,
): TabbarStyle {
  if (isVertical) return "vertical";
  return current === "vertical" ? "horizontal" : current;
}
