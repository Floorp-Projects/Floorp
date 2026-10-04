// SPDX-License-Identifier: MPL-2.0

/**
 * True when the page is served by the Vite dev server rather than from
 * `chrome://noraneko-notes/`. In dev the page runs with a content principal and
 * privileged APIs (`Services`, `Cc`, `Ci`, `IOUtils`) are unavailable.
 *
 * This repeats the one-line test used by `rpc/rpc.ts` on purpose: modules that
 * only need to know the environment should not have to import the RPC
 * transport, whose module body opens a birpc channel in dev.
 */
export const isDevMode = import.meta.url?.includes("localhost:5188") ?? false;
