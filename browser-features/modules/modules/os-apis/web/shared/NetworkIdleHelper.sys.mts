/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Shared network idle detection utility for browser automation services.
 * Uses GlobalHTTPTracker polling (100ms interval) for reliable idle detection
 * without observer registration or leak risks.
 */

import { GlobalHTTPTracker } from "./GlobalHTTPTracker.sys.mts";
import { AutomationConstants } from "../../../os-server/shared/http-tracker.sys.mts";

const { setTimeout, clearTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

interface NetworkIdleOptions {
  timeout?: number;
  maxInflight?: number;
  idleDuration?: number;
  ignorePatterns?: string[];
}

export class NetworkIdleHelper {
  /**
   * Waits for the network to become idle for a given browsing context.
   *
   * Polls GlobalHTTPTracker every 100ms. Resolves `true` when the network
   * has been idle (activeCount <= maxInflight) for `idleDuration` ms,
   * or `false` if `timeout` ms elapse first.
   */
  static waitForIdle(
    bcid: number,
    options?: number | NetworkIdleOptions,
  ): Promise<boolean> {
    // Backward compatibility: bare number → { timeout }
    const opts: NetworkIdleOptions =
      typeof options === "number" ? { timeout: options } : options ?? {};
    const {
      timeout = AutomationConstants.NETWORK_IDLE_TIMEOUT_MS,
      maxInflight = 0,
      idleDuration = AutomationConstants.NETWORK_IDLE_THRESHOLD_MS,
      ignorePatterns = [],
    } = opts;

    // Pre-compile ignore patterns
    const ignoreRegexps = ignorePatterns.map((p) => new RegExp(p));

    return new Promise((resolve) => {
      const state = {
        pollTimer: null as ReturnType<typeof setTimeout> | null,
        resolved: false,
        startTime: Date.now(),
        idleSince: 0,
      };

      const cleanup = () => {
        if (state.pollTimer) {
          clearTimeout(state.pollTimer);
          state.pollTimer = null;
        }
      };

      const poll = () => {
        if (state.resolved) return;

        let activeCount = GlobalHTTPTracker.getActiveCount(bcid);

        // Subtract requests matching ignore patterns
        if (ignoreRegexps.length > 0 && activeCount > 0) {
          const activeUrls = GlobalHTTPTracker.getActiveURLs?.(bcid) ?? [];
          const ignored = activeUrls.filter((url: string) =>
            ignoreRegexps.some((re) => re.test(url)),
          );
          activeCount = Math.max(0, activeCount - ignored.length);
        }

        const elapsed = Date.now() - state.startTime;

        if (activeCount <= maxInflight) {
          if (state.idleSince === 0) {
            state.idleSince = Date.now();
          }
          if (Date.now() - state.idleSince >= idleDuration) {
            console.log(
              `[NetworkIdleHelper] Network idle for BCID ${bcid} after ${elapsed}ms`,
            );
            state.resolved = true;
            cleanup();
            resolve(true);
            return;
          }
        } else {
          state.idleSince = 0;
        }

        if (elapsed >= timeout) {
          console.log(
            `[NetworkIdleHelper] Timed out after ${timeout}ms. Active: ${activeCount}`,
          );
          state.resolved = true;
          cleanup();
          resolve(false);
        } else {
          state.pollTimer = setTimeout(poll, 100);
        }
      };

      poll();
    });
  }
}
