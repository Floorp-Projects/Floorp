// SPDX-License-Identifier: MPL-2.0
import type {
  NewTabChangedEvent,
  NewTabMethod,
  NewTabOperations,
  NewTabResponse,
} from "./types.ts";
import type { NewTabBridge, PendingRequest } from "./client-types.ts";

export class NewTabClientError extends Error {
  constructor(public readonly code: string, message: string) {
    super(message);
    this.name = "NewTabClientError";
  }
}

const pending = new Map<string, PendingRequest>();
const listeners = new Set<(event: NewTabChangedEvent) => void>();
let bridgePromise: Promise<Required<NewTabBridge>> | null = null;
let sequence = 0;
function receive(message: string): void {
  try {
    const packet: unknown = JSON.parse(message);
    if (!packet || typeof packet !== "object") return;
    if ("kind" in packet && packet.kind === "changed" && "event" in packet) {
      const event = packet.event as NewTabChangedEvent;
      for (const listener of listeners) {
        try {
          listener(event);
        } catch { /* Isolate subscribers. */ }
      }
    } else if (
      "id" in packet && typeof packet.id === "string" && "response" in packet
    ) {
      const request = pending.get(packet.id);
      if (!request) return;
      pending.delete(packet.id);
      clearTimeout(request.timer);
      const response = packet.response as NewTabResponse;
      if (response.ok) request.resolve(response.value);
      else {request.reject(
          new NewTabClientError(response.error.code, response.error.message),
        );}
    }
  } catch { /* Ignore malformed or late transport messages. */ }
}
function getBridge(): Promise<Required<NewTabBridge>> {
  if (bridgePromise) return bridgePromise;
  bridgePromise = new Promise((resolve, reject) => {
    const started = Date.now();
    const poll = () => {
      const bridge = globalThis as unknown as NewTabBridge;
      if (
        typeof bridge.NRNewTabSend === "function" &&
        typeof bridge.NRNewTabRegisterReceiveCallback === "function"
      ) {
        bridge.NRNewTabRegisterReceiveCallback(receive);
        resolve(bridge as Required<NewTabBridge>);
      } else if (Date.now() - started > 15_000) {
        reject(
          new NewTabClientError(
            "UNAVAILABLE",
            "The browser connection is unavailable. Reload this page.",
          ),
        );
      } else setTimeout(poll, 25);
    };
    poll();
  });
  void bridgePromise.catch(() => {
    bridgePromise = null;
  });
  return bridgePromise;
}
export const newTabClient = {
  async call<K extends NewTabMethod>(
    method: K,
    params: NewTabOperations[K]["input"],
  ): Promise<NewTabOperations[K]["output"]> {
    const bridge = await getBridge();
    bridge.NRNewTabRegisterReceiveCallback(receive);
    const id = `newtab-${Date.now()}-${++sequence}`;
    return new Promise<NewTabOperations[K]["output"]>((resolve, reject) => {
      const timer = setTimeout(() => {
        pending.delete(id);
        reject(
          new NewTabClientError(
            "UNAVAILABLE",
            "The browser request timed out. Reload this page to check whether your change was saved.",
          ),
        );
      }, method === "pickFolder" ? 120_000 : 45_000);
      pending.set(id, {
        timer,
        resolve: (value) => resolve(value as NewTabOperations[K]["output"]),
        reject,
      });
      try {
        bridge.NRNewTabSend(
          JSON.stringify({ id, request: { method, params } }),
        );
      } catch {
        clearTimeout(timer);
        pending.delete(id);
        reject(
          new NewTabClientError(
            "UNAVAILABLE",
            "The browser connection was closed.",
          ),
        );
      }
    });
  },
  subscribe(listener: (event: NewTabChangedEvent) => void): () => void {
    listeners.add(listener);
    void getBridge().catch(() => {});
    return () => {
      listeners.delete(listener);
    };
  },
};
