/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * TabManager API routes for the OS Server
 *
 * Provides visible tab automation capabilities with visual feedback.
 * Uses shared route handlers for common automation operations.
 */

import type {
  NamespaceBuilder,
  Context as RouterContext,
} from "../router.sys.mts";
import type { ErrorResponse } from "../_os-plugin/api-spec/types.ts";
import type { TabManagerAPI } from "./types.ts";
import { registerCommonAutomationRoutes } from "../shared/routes.sys.mts";

// Lazy import of TabManager module
const TabManagerModule = () =>
  ChromeUtils.importESModule(
    "resource://noraneko/modules/os-apis/web/TabManagerServices.sys.mjs",
  ) as { TabManagerServices: TabManagerAPI };

// -- Route registration -------------------------------------------------------

export function registerTabRoutes(api: NamespaceBuilder): void {
  api.namespace("/tabs", (t: NamespaceBuilder) => {
    // -- Tab-specific routes --

    // Check if instance exists
    t.get("/instances/:id/exists", async (ctx: RouterContext) => {
      const { TabManagerServices } = TabManagerModule();
      const info = await TabManagerServices.getInstanceInfo(ctx.params.id);
      return { status: 200, body: { exists: info != null } };
    });

    // List all tabs
    t.get("/list", async () => {
      const { TabManagerServices } = TabManagerModule();
      const tabs = await TabManagerServices.listTabs();
      return { status: 200, body: tabs };
    });

    // Create a new tab instance (URL required)
    t.post<
      { url: string; inBackground?: boolean },
      { instanceId: string } | ErrorResponse
    >(
      "/instances",
      async (ctx: RouterContext<{ url: string; inBackground?: boolean }>) => {
        const json = ctx.json();
        if (!json?.url) {
          return { status: 400, body: { error: "url required" } };
        }
        const { TabManagerServices } = TabManagerModule();
        const id = await TabManagerServices.createInstance(json.url, {
          inBackground: json.inBackground ?? true,
        });
        return { status: 200, body: { instanceId: id } };
      },
    );

    // Attach to existing tab
    t.post<
      { browserId: string },
      { instanceId: string | null } | ErrorResponse
    >("/attach", async (ctx: RouterContext<{ browserId: string }>) => {
      const json = ctx.json();
      if (!json?.browserId) {
        return { status: 400, body: { error: "browserId required" } };
      }
      const { TabManagerServices } = TabManagerModule();
      const id = await TabManagerServices.attachToTab(json.browserId);
      return { status: 200, body: { instanceId: id } };
    });

    // Get instance info
    t.get("/instances/:id", async (ctx: RouterContext) => {
      const { TabManagerServices } = TabManagerModule();
      const info = await TabManagerServices.getInstanceInfo(ctx.params.id);
      return { status: 200, body: info };
    });

    // Destroy instance (async cleanup, does not close the tab)
    t.delete("/instances/:id", async (ctx: RouterContext) => {
      const { TabManagerServices } = TabManagerModule();
      await TabManagerServices.destroyInstance(ctx.params.id);
      return { status: 200, body: { ok: true } };
    });

    // Close tab (destroys instance AND closes the actual browser tab)
    t.post("/instances/:id/close", async (ctx: RouterContext) => {
      const { TabManagerServices } = TabManagerModule();
      await TabManagerServices.closeTab(ctx.params.id);
      return { status: 200, body: { ok: true } };
    });

    // -- Common automation routes --
    registerCommonAutomationRoutes(
      t,
      () => TabManagerModule().TabManagerServices,
      { includeGetElement: true },
    );
  });
}
