/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createBirpc } from "birpc";
import type { NRKeySyncParentFunctions } from "../common/defines.ts";
import type {
  FirefoxKeyEntry,
  KeySyncChangeResult,
} from "../common/NRKeySyncTypes.ts";
import type { XulCombo } from "../common/NRKeySyncCombo.ts";

/**
 * Settings-page bridge for the key sync service. Exposes three functions on
 * the page window (NRKeySyncSend / NRKeySyncRegisterReceiveCallback /
 * NRKeySyncRegisterPushCallback); request/response travels as JSON strings
 * through birpc, and mirror changes arrive through NRKeySync:KeysChanged.
 */
export class NRKeySyncChild extends JSWindowActorChild {
  private static readonly MAX_INSTALL_ATTEMPTS = 200;
  private static readonly INSTALL_RETRY_DELAY_MS = 50;

  private rpc: ReturnType<typeof createBirpc> | null = null;
  /** birpc inbound handler; receives JSON strings sent by the page. */
  private sendToPage: ((data: string) => void) | null = null;
  /** Page-registered receiver for "NRKeySync:KeysChanged" payloads. */
  private pushCallback: ((data: string) => void) | null = null;

  constructor() {
    super();
  }

  private installPageApi(): boolean {
    const document = this.document;
    const window = this.contentWindow;
    if (
      !document ||
      !window ||
      !(
        document.location.port === "5183" ||
        document.location.href.startsWith("chrome://noraneko-settings/") ||
        document.location.href.split(/[?#]/)[0] === "about:hub"
      )
    ) {
      return false;
    }

    const page = window as unknown as Record<string, unknown>;
    if (typeof page.NRKeySyncSend !== "function") {
      Cu.exportFunction(this.NRKeySyncSend.bind(this), window, {
        defineAs: "NRKeySyncSend",
      });
    }
    if (typeof page.NRKeySyncRegisterReceiveCallback !== "function") {
      Cu.exportFunction(
        this.NRKeySyncRegisterReceiveCallback.bind(this),
        window,
        {
          defineAs: "NRKeySyncRegisterReceiveCallback",
        },
      );
    }
    if (typeof page.NRKeySyncRegisterPushCallback !== "function") {
      Cu.exportFunction(
        this.NRKeySyncRegisterPushCallback.bind(this),
        window,
        {
          defineAs: "NRKeySyncRegisterPushCallback",
        },
      );
    }
    return true;
  }

  private retryInstallPageApi(attempt = 0): void {
    if (
      this.installPageApi() ||
      attempt >= NRKeySyncChild.MAX_INSTALL_ATTEMPTS
    ) {
      return;
    }
    this.contentWindow?.setTimeout(
      () => this.retryInstallPageApi(attempt + 1),
      NRKeySyncChild.INSTALL_RETRY_DELAY_MS,
    );
  }

  actorCreated() {
    console.debug("NRKeySyncChild created!");
    this.retryInstallPageApi();
  }

  NRKeySyncSend(data: string) {
    if (this.sendToPage) {
      this.sendToPage(data);
    }
  }

  NRKeySyncRegisterReceiveCallback(callback: (data: string) => void) {
    this.rpc = createBirpc<
      Record<PropertyKey, never>,
      NRKeySyncParentFunctions
    >(
      {
        getFirefoxKeys: (): Promise<FirefoxKeyEntry[]> => {
          return this.sendQuery(
            "getFirefoxKeys",
          ) as Promise<FirefoxKeyEntry[]>;
        },
        isKeySyncWriteEnabled: (): Promise<boolean> => {
          return this.sendQuery("isKeySyncWriteEnabled") as Promise<boolean>;
        },
        changeFirefoxKey: (
          id: string,
          combo: XulCombo,
        ): Promise<KeySyncChangeResult> => {
          return this.sendQuery("changeFirefoxKey", {
            id,
            combo,
          }) as Promise<KeySyncChangeResult>;
        },
        resetFirefoxKey: (id: string): Promise<KeySyncChangeResult> => {
          return this.sendQuery("resetFirefoxKey", {
            id,
          }) as Promise<KeySyncChangeResult>;
        },
        openAboutKeyboard: (): Promise<void> => {
          return this.sendQuery("openAboutKeyboard").then(() => undefined);
        },
      },
      {
        post: (data) => callback(data),
        on: (callback) => {
          this.sendToPage = callback;
        },
        // these are required when using WebSocket
        serialize: (v) => JSON.stringify(v),
        deserialize: (v) => JSON.parse(v),
      },
    );
  }

  NRKeySyncRegisterPushCallback(callback: (data: string) => void) {
    this.pushCallback = callback;
  }

  receiveMessage(message: { name: string; data?: unknown }) {
    switch (message.name) {
      case "NRKeySync:KeysChanged":
        this.pushCallback?.(JSON.stringify(message.data));
        break;
    }
  }

  handleEvent(_event: Event): void {
    // actorCreated can run before the final document URL is available. Retry
    // after the document insertion/DOMContentLoaded events so HTTP-loaded
    // settings pages always receive their RPC bridge.
    this.retryInstallPageApi();
  }
}
