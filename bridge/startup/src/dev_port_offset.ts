// SPDX-License-Identifier: MPL-2.0

import {
  createLaunchDevPorts,
  type LaunchDevPorts,
} from "#libs/shared/dev-ports/launch_offset.ts";

/**
 * Dev/test launch port resolver for bridge/startup (FDH Gate 2 task 2-B).
 *
 * Bridge/startup code runs chrome-privileged inside the launched browser, so
 * the dev/test HTTP loader and preference pages resolve to the ports the
 * launch actually serves (offset-adjusted) instead of the frozen base table.
 */

declare const Services: {
  prefs?: {
    getIntPref(prefName: string, fallback: number): number;
  };
};

/**
 * The launch resolver for bridge/startup. Created on every call; the pref read
 * is one cheap `getIntPref`, so no caching is needed.
 */
export function launchDevPorts(): LaunchDevPorts {
  const prefs = typeof Services === "undefined"
    ? null
    : (Services.prefs as unknown as Parameters<typeof createLaunchDevPorts>[0]);
  return createLaunchDevPorts(prefs);
}
