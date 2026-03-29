/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Shared polling utility for waiting on JSWindowActor readiness
 * with exponential backoff.
 */

interface WaitForActorOptions {
  /** Maximum total wait time in milliseconds (default: 15000) */
  maxMs?: number;
  /** Initial delay between polls in milliseconds (default: 10) */
  initialMs?: number;
  /** Maximum per-poll delay cap in milliseconds (default: 500) */
  capMs?: number;
  /** Multiplicative backoff factor (default: 1.5) */
  factor?: number;
}

/**
 * Polls for a JSWindowActor on a browser element using exponential backoff.
 *
 * The backoff sequence starts at `initialMs` and multiplies by `factor` each
 * iteration, capped at `capMs`: 10, 15, 22, 33, 50, 75, 112, 168, 252, 378, 500, ...
 *
 * This provides fast response when the actor is immediately available (~10ms)
 * while avoiding CPU waste during slow page loads.
 *
 * @param getBrowser Callback returning the current browser element. Called each
 *                   iteration to handle process swaps (remoteness changes).
 *                   Return null/undefined to abort the wait.
 * @param actorName  The actor name (e.g. "NRWebScraper").
 * @param opts       Backoff configuration.
 * @returns The actor instance, or null if the timeout expires or getBrowser
 *          returns null.
 */
export async function waitForActor<T = unknown>(
  getBrowser: () => { browsingContext?: { currentWindowGlobal?: { getActor(name: string): unknown } | null } | null } | null | undefined,
  actorName: string,
  opts?: WaitForActorOptions,
): Promise<T | null> {
  const {
    maxMs = 15000,
    initialMs = 10,
    capMs = 500,
    factor = 1.5,
  } = opts ?? {};

  // First attempt without any delay
  const browser = getBrowser();
  if (browser) {
    const actor = browser.browsingContext?.currentWindowGlobal?.getActor(actorName);
    if (actor) return actor as T;
  }

  let delay = initialMs;
  const deadline = Date.now() + maxMs;

  while (Date.now() < deadline) {
    await new Promise<void>((r) => setTimeout(r, delay));
    delay = Math.min(delay * factor, capMs);

    const b = getBrowser();
    if (!b) return null;

    const actor = b.browsingContext?.currentWindowGlobal?.getActor(actorName);
    if (actor) return actor as T;
  }

  return null;
}
