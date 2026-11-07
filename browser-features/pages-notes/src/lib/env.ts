// SPDX-License-Identifier: MPL-2.0

/**
 * True when the page is served by a localhost http Vite dev server rather
 * than from `chrome://noraneko-notes/`. In dev the page runs with a content
 * principal and privileged APIs (`Services`, `Cc`, `Ci`, `IOUtils`) are
 * unavailable.
 *
 * The check is offset-agnostic (FDH Gate 2 task 2-B): the dev server serves
 * whatever port the launch bound, and in production module URLs are never
 * localhost http documents.
 *
 * This repeats the one-line test used by `rpc/rpc.ts` on purpose: modules that
 * only need to know the environment should not have to import the RPC
 * transport, whose module body opens a birpc channel in dev.
 */
export const isDevMode = moduleUrlServedByDevHttpServer(
  import.meta.url,
) ?? false;

import { moduleUrlServedByDevHttpServer } from "../../../../libs/shared/dev-ports/defines.ts";
