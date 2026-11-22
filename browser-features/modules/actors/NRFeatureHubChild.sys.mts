// SPDX-License-Identifier: MPL-2.0

export class NRFeatureHubChild extends JSWindowActorChild {
  #listenerWindow: Window | null = null;

  actorCreated(): void {
    const contentWindow = this.contentWindow;
    if (!contentWindow) return;
    this.#listenerWindow = contentWindow;
    contentWindow.addEventListener("nora:feature-hub-ack", this, {
      wantUntrusted: true,
    });
    contentWindow.addEventListener("nora:feature-hub-connect", this, {
      wantUntrusted: true,
    });
    contentWindow.addEventListener("nora:feature-hub-disconnect", this, {
      wantUntrusted: true,
    });
    contentWindow.dispatchEvent(
      new contentWindow.Event("nora:feature-hub-ready"),
    );
  }

  receiveMessage(message: ReceiveMessageArgument): void {
    if (
      message.name !== "NRFeatureHub:Snapshot" ||
      typeof message.data !== "string"
    ) return;
    if (message.data.length > 1_048_576) return;
    let snapshot: unknown;
    try {
      snapshot = JSON.parse(message.data);
    } catch {
      return;
    }
    if (!snapshot || typeof snapshot !== "object") return;
    const envelope = snapshot as Record<string, unknown>;
    if (
      Object.getPrototypeOf(envelope) !== Object.prototype ||
      Object.keys(envelope).length !== 4 || envelope.type !== "snapshot" ||
      typeof envelope.connectionNonce !== "string" ||
      envelope.connectionNonce.length < 1 ||
      envelope.connectionNonce.length > 80 ||
      typeof envelope.requestId !== "string" ||
      envelope.requestId.length < 1 || envelope.requestId.length > 80 ||
      !envelope.snapshot || typeof envelope.snapshot !== "object"
    ) return;
    const contentWindow = this.#listenerWindow ?? this.contentWindow;
    contentWindow?.dispatchEvent(
      new contentWindow.CustomEvent(
        "nora:feature-hub-snapshot",
        { detail: message.data },
      ),
    );
  }

  handleEvent(event: Event): void {
    const contentWindow = this.#listenerWindow ?? this.contentWindow;
    if (!contentWindow) return;
    if (event.type === "pageshow") {
      contentWindow.dispatchEvent(
        new contentWindow.Event("nora:feature-hub-ready"),
      );
      return;
    }
    if (event.type === "nora:feature-hub-connect") {
      const detail = (event as CustomEvent<unknown>).detail;
      if (typeof detail === "string" && detail.length <= 4096) {
        this.sendAsyncMessage("NRFeatureHub:Connect", detail);
      }
      return;
    }
    const detail = (event as CustomEvent<unknown>).detail;
    if (event.type === "nora:feature-hub-ack") {
      if (typeof detail === "string" && detail.length <= 4096) {
        this.sendAsyncMessage("NRFeatureHub:Acknowledge", detail);
      }
    } else if (event.type === "nora:feature-hub-disconnect") {
      if (typeof detail === "string" && detail.length <= 4096) {
        this.sendAsyncMessage("NRFeatureHub:Disconnect", detail);
      }
    }
  }

  didDestroy(): void {
    const contentWindow = this.#listenerWindow;
    if (!contentWindow) return;
    contentWindow.removeEventListener("nora:feature-hub-ack", this);
    contentWindow.removeEventListener("nora:feature-hub-connect", this);
    contentWindow.removeEventListener(
      "nora:feature-hub-disconnect",
      this,
    );
    contentWindow.dispatchEvent(
      new contentWindow.Event("nora:feature-hub-disconnected"),
    );
    this.#listenerWindow = null;
  }
}
