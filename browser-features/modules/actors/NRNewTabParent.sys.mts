// SPDX-License-Identifier: MPL-2.0
import { createDefaultNewTabSettings } from "#libs/newtab/settings.ts";
import { isNewTabDocument, isRecord } from "#libs/newtab/origin.ts";
import { validateNewTabRequest } from "#libs/newtab/validation.ts";
import type { NewTabResponse, NewTabSnapshot } from "#libs/newtab/types.ts";
import type { NewTabService as ServiceType } from "../modules/newtab/NewTabService.sys.mts";
import type { BrowserWindowContext } from "../modules/newtab/BrowserIntegration.sys.mts";

const { PrivateBrowsingUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
);
export function privateNewTabSnapshot(): NewTabSnapshot {
  const settings = createDefaultNewTabSettings();
  settings.topSites = { pinned: [], blocked: [] };
  settings.background = { ...settings.background, type: "none" };
  settings.home.showHistory = false;
  settings.home.showFeed = false;
  settings.sponsors.enabled = false;
  return {
    settings,
    groups: [],
    workspace: null,
    workspaces: [],
    sponsors: [],
    feeds: [],
    readOnly: true,
    private: true,
  };
}
export class NRNewTabParent extends JSWindowActorParent {
  private stop: (() => void) | null = null;
  private destroyed = false;
  private requests = new Map<string, Promise<NewTabResponse>>();
  // Remember only small operation IDs, never resolved image/settings payloads.
  private completed = new Set<string>();
  private context() {
    const mode = Services.prefs.getStringPref("nora.startup.mode", "");
    const manager = this.manager;
    const browsingContext = this.browsingContext;
    const uri = manager?.documentURI?.spec ?? "";
    if (
      this.destroyed || !manager || !browsingContext ||
      browsingContext.parent || !manager.documentPrincipal ||
      manager.documentPrincipal.isNullPrincipal ||
      !isNewTabDocument(uri, mode === "dev" || mode === "test")
    ) {
      throw Object.assign(
        new Error("This document cannot use new tab browser services."),
        { code: "READ_ONLY" },
      );
    }
    const window = browsingContext.topChromeWindow;
    if (!window || window.closed) {
      throw Object.assign(new Error("The browser window is unavailable."), {
        code: "UNAVAILABLE",
      });
    }
    return {
      window: window as unknown as BrowserWindowContext,
      // CanonicalBrowsingContext implements the native BrowsingContext API.
      browsingContext: browsingContext as unknown as BrowsingContext,
      private: Boolean(PrivateBrowsingUtils.isWindowPrivate(window)),
    };
  }
  receiveMessage(message: ReceiveMessageArgument): Promise<NewTabResponse> {
    if (message.name !== "NRNewTab:Request") {
      return Promise.resolve({
        ok: false,
        error: { code: "VALIDATION", message: "Unknown browser request." },
      });
    }
    const packet: unknown = message.data;
    if (
      !isRecord(packet) || typeof packet.id !== "string" ||
      packet.id.length > 128
    ) {
      return Promise.resolve({
        ok: false,
        error: { code: "VALIDATION", message: "Invalid browser request." },
      });
    }
    // Validate every invocation, including a cached request after navigation.
    try {
      if (this.context().private) return this.execute(packet.request);
    } catch (error) {
      return Promise.resolve(this.error(error));
    }
    if (this.completed.has(packet.id)) {
      return Promise.resolve({
        ok: false,
        error: {
          code: "CONFLICT",
          message:
            "This request already completed. Reload the page to confirm the saved state.",
        },
      });
    }
    const existing = this.requests.get(packet.id);
    if (existing) return existing;
    if (this.requests.size >= 128) {
      return Promise.resolve({
        ok: false,
        error: {
          code: "UNAVAILABLE",
          message: "There are too many pending browser requests.",
        },
      });
    }
    const promise = this.execute(packet.request);
    this.requests.set(packet.id, promise);
    const id = packet.id;
    const method = isRecord(packet.request) ? packet.request.method : "";
    const mutation = [
      "bookmark",
      "patchSettings",
      "subscribe",
      "unsubscribe",
      "articleState",
      "dismissSponsor",
      "restoreSponsors",
      "saveSponsor",
      "repairBookmarks",
      "openHub",
      "pickFolder",
    ].includes(String(method));
    void promise.then((response) => {
      this.requests.delete(id);
      if (mutation && response.ok && !this.destroyed) {
        if (this.completed.size >= 200) {
          this.completed.delete(this.completed.values().next().value!);
        }
        this.completed.add(id);
      }
    });
    return promise;
  }
  protected getService(): typeof ServiceType {
    return (ChromeUtils.importESModule(
      "resource://noraneko/modules/newtab/NewTabService.sys.mjs",
    ) as { NewTabService: typeof ServiceType }).NewTabService;
  }
  private async execute(value: unknown): Promise<NewTabResponse> {
    try {
      const context = this.context();
      const request = validateNewTabRequest(value);
      if (context.private) {
        if (request.method === "snapshot") {
          return { ok: true, value: privateNewTabSnapshot() };
        }
        throw Object.assign(
          new Error(
            "New tab personal data is unavailable in a private window.",
          ),
          { code: "READ_ONLY" },
        );
      }
      const NewTabService = this.getService();
      if (!this.stop && !this.destroyed) {
        this.stop = NewTabService.subscribe((event) => {
          if (this.destroyed) return;
          try {
            if (this.context().private) return;
            this.sendAsyncMessage("NRNewTab:Changed", event);
          } catch {
            /* A navigated document must not receive profile events. */
          }
        });
      }
      // Listen before the first async snapshot so another window's commit is
      // not lost while Places/SQLite is being read.
      const result = await NewTabService.invoke(request, context);
      const current = this.context();
      if (current.private || current.window !== context.window) {
        throw Object.assign(
          new Error("The browser context changed. Reload this page."),
          { code: "READ_ONLY" },
        );
      }
      return { ok: true, value: result };
    } catch (error) {
      return this.error(error);
    }
  }
  private error(error: unknown): NewTabResponse {
    const code = isRecord(error) && typeof error.code === "string"
      ? error.code
      : "UNAVAILABLE";
    return {
      ok: false,
      error: {
        code,
        message: error instanceof Error
          ? error.message
          : "The browser request could not be completed.",
      },
    };
  }
  didDestroy(): void {
    this.destroyed = true;
    this.stop?.();
    this.stop = null;
    this.requests.clear();
    this.completed.clear();
  }
}
