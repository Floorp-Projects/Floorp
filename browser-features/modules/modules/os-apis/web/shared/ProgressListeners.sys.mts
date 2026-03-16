/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Global set to prevent garbage collection of active progress listeners.
 * Shared between TabManagerServices and WebScraperServices.
 */
const PROGRESS_LISTENERS = new Set();

export { PROGRESS_LISTENERS };
