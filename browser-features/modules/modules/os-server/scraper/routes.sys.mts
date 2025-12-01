/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * WebScraper API routes for the OS Server
 *
 * Provides headless browsing and web scraping capabilities.
 * Uses shared route handlers for common automation operations.
 */

import type {
  NamespaceBuilder,
  Context as RouterContext,
} from "../router.sys.mts";
import type { WebScraperAPI } from "./types.ts";
import { registerCommonAutomationRoutes } from "../shared/routes.sys.mts";

// Lazy import of WebScraper module
const WebScraperModule = () =>
  ChromeUtils.importESModule(
    "resource://noraneko/modules/os-apis/web/WebScraperServices.sys.mjs",
  ) as { WebScraper: WebScraperAPI };

// -- Route registration -------------------------------------------------------

export function registerScraperRoutes(api: NamespaceBuilder): void {
  api.namespace("/scraper", (s: NamespaceBuilder) => {
    // -- Scraper-specific routes --

    // Check if instance exists
    s.get("/instances/:id/exists", async (ctx: RouterContext) => {
      const { WebScraper } = WebScraperModule();
      try {
        // Any call that touches the instance and throws when missing
        await WebScraper.getURI(ctx.params.id);
        return { status: 200, body: { exists: true } };
      } catch (e) {
        const msg = String(e ?? "");
        if (/instance\s+not\s+found/i.test(msg)) {
          return { status: 200, body: { exists: false } };
        }
        throw e;
      }
    });

    // Create a new scraper instance (no URL required - headless)
    s.post<undefined, { instanceId: string }>("/instances", async () => {
      const { WebScraper } = WebScraperModule();
      const id = await WebScraper.createInstance();
      return { status: 200, body: { instanceId: id } };
    });

    // Destroy a scraper instance (synchronous cleanup)
    s.delete("/instances/:id", (ctx: RouterContext) => {
      const id = ctx.params.id;
      const { WebScraper } = WebScraperModule();
      WebScraper.destroyInstance(id);
      return { status: 200, body: { ok: true } };
    });

    // -- Common automation routes --
    registerCommonAutomationRoutes(
      s,
      () => WebScraperModule().WebScraper,
      { includeGetElement: false },
    );
  });
}
