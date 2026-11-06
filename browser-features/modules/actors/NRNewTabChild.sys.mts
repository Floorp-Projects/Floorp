// SPDX-License-Identifier: MPL-2.0
import { isNewTabDocument, isRecord } from "#libs/newtab/origin.ts";
import type { NewTabResponse } from "#libs/newtab/types.ts";

export class NRNewTabChild extends JSWindowActorChild {
  private listener: ((message: string) => void) | null = null;
  private destroyed = false;
  private installed = false;
  actorCreated(): void {
    this.installBridge();
  }
  private installBridge(): void {
    const window = this.contentWindow;
    const context = this.browsingContext;
    // Loopback matching is enabled by BrowserGlue only in dev/test mode; the
    // parent rechecks the startup mode for every call. Child prefs can lag.
    const uri = this.document?.documentURI ?? window?.location.href ?? "";
    if (
      this.installed || !window || !context || context.parent ||
      !isNewTabDocument(uri, true)
    ) return;
    Cu.exportFunction(this.send.bind(this), window, {
      defineAs: "NRNewTabSend",
    });
    Cu.exportFunction(
      (listener: (message: string) => void) => {
        if (typeof listener === "function") this.listener = listener;
      },
      window,
      { defineAs: "NRNewTabRegisterReceiveCallback" },
    );
    this.installed = true;
  }
  private send(raw: string): void {
    if (
      this.destroyed || typeof raw !== "string" || raw.length > 8 * 1024 * 1024
    ) return;
    let packet: unknown;
    try {
      packet = JSON.parse(raw);
    } catch {
      return;
    }
    if (
      !isRecord(packet) || typeof packet.id !== "string" ||
      packet.id.length > 128
    ) return;
    const id = packet.id;
    void this.sendQuery("NRNewTab:Request", packet).then(
      (response: NewTabResponse) => {
        if (!this.destroyed) {
          this.listener?.(JSON.stringify({ kind: "response", id, response }));
        }
      },
      () => {
        if (!this.destroyed) {
          this.listener?.(
            JSON.stringify({
              kind: "response",
              id,
              response: {
                ok: false,
                error: {
                  code: "UNAVAILABLE",
                  message: "The browser connection was closed.",
                },
              },
            }),
          );
        }
      },
    );
  }
  receiveMessage(message: ReceiveMessageArgument): void {
    if (message.name === "NRNewTab:Changed" && !this.destroyed) {
      this.listener?.(JSON.stringify({ kind: "changed", event: message.data }));
    }
  }
  didDestroy(): void {
    this.destroyed = true;
    this.listener = null;
  }
  handleEvent(): void {
    this.installBridge();
  }
}
