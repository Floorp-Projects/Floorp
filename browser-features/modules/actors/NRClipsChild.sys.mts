/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createBirpc } from "birpc";
import type { ActiveTabInfo, NRClipsParentFunctions } from "../common/defines.ts";

/**
 * The bridge the Clips page talks to while it is served from the Vite dev
 * server. In production the page is a chrome:// page and reaches these things
 * through `Services` itself, so this actor is only doing work in dev.
 */
export class NRClipsChild extends JSWindowActorChild {
  rpc: ReturnType<typeof createBirpc> | null = null;

  actorCreated() {
    const window = this.contentWindow;
    if (
      window?.location.port === "5189" ||
      window?.location.href.startsWith("chrome://noraneko-clips/")
    ) {
      Cu.exportFunction(this.NRClipsSend.bind(this), window, {
        defineAs: "NRClipsSend",
      });
      Cu.exportFunction(
        this.NRClipsRegisterReceiveCallback.bind(this),
        window,
        { defineAs: "NRClipsRegisterReceiveCallback" },
      );
    }
  }

  sendToPage: ((data: string) => void) | null = null;

  NRClipsSend(data: string) {
    this.sendToPage?.(data);
  }

  NRClipsRegisterReceiveCallback(callback: (data: string) => void) {
    this.rpc = createBirpc<Record<PropertyKey, never>, NRClipsParentFunctions>(
      {
        getActiveTabInfo: () =>
          this.ask("getActiveTabInfo") as Promise<ActiveTabInfo | null>,
        openLinkInTab: (url: string) =>
          this.ask("openLinkInTab", { url }) as Promise<void>,
        readClipboardText: () =>
          this.ask("readClipboardText") as Promise<string | null>,
        fileExists: (path: string) =>
          this.ask("fileExists", { path }) as Promise<boolean>,
        revealFile: (path: string) =>
          this.ask("revealFile", { path }) as Promise<boolean>,
        launchFile: (path: string) =>
          this.ask("launchFile", { path }) as Promise<boolean>,
        getSessionStartTime: () =>
          this.ask("getSessionStartTime") as Promise<number>,
      },
      {
        post: (data) => callback(data),
        on: (cb) => {
          this.sendToPage = cb;
        },
        serialize: (v) => JSON.stringify(v),
        deserialize: (v) => JSON.parse(v),
      },
    );
  }

  private async ask(name: string, data?: unknown): Promise<unknown> {
    try {
      return await this.sendQuery(name, data);
    } catch (e) {
      console.error(`[NRClips] ${name} failed:`, e);
      return null;
    }
  }

  handleEvent(_event: Event): void {
    // No-op
  }
}
