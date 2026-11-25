// SPDX-License-Identifier: MPL-2.0

import type { HubDomProbeLink, HubDomProbeRead } from "./HubDomProbeTypes.ts";

const MAX_TEXT_LENGTH = 256;
const MAX_LINKS = 32;
const HUB_ROUTE = /^\/extensions\/[a-z][a-z0-9-]{0,47}\/[a-z][a-z0-9-]{0,47}$/;

function textContent(element: Element | null): string | null {
  const value = element?.textContent?.trim();
  return value ? value.slice(0, MAX_TEXT_LENGTH) : null;
}

function routeFromHref(href: string): string | null {
  const hash = href.indexOf("#");
  const route = hash >= 0 ? href.slice(hash + 1) : href;
  return HUB_ROUTE.test(route) || route === "/overview/home" ? route : null;
}

export class M1HubDomProbeChild extends JSWindowActorChild {
  #protocol: HubDomProbeRead["protocol"] = {
    connectCount: 0,
    blockedConnectCount: 0,
    blockedAckCount: 0,
    disconnectCount: 0,
    connectBlocked: false,
    ackBlocked: false,
    savedAcknowledgementAvailable: false,
    snapshotCount: 0,
    ackCount: 0,
    latest: {
      requestId: null,
      connectionNonce: null,
      epoch: null,
      revision: null,
    },
    lastAcknowledgement: {
      requestId: null,
      connectionNonce: null,
      epoch: null,
      revision: null,
    },
  };
  #pageshow: HubDomProbeRead["pageshow"] = {
    count: 0,
    persistedCount: 0,
    lastPersisted: null,
  };
  #listenerWindow: Window | null = null;
  #savedAcknowledgement: string | null = null;
  #sameOriginFrame: HTMLIFrameElement | null = null;

  actorCreated(): void {
    const contentWindow = this.contentWindow;
    if (!contentWindow) return;
    this.#listenerWindow = contentWindow;
    this.#protocol.connectBlocked = contentWindow.location.search ===
      "?m1-hub-probe=block-connect";
    this.#protocol.ackBlocked = contentWindow.location.search ===
      "?m1-hub-probe=block-ack";
    for (
      const type of [
        "nora:feature-hub-connect",
        "nora:feature-hub-snapshot",
        "nora:feature-hub-ack",
        "nora:feature-hub-disconnected",
      ]
    ) {
      contentWindow.addEventListener(type, this, {
        capture: true,
        wantUntrusted: true,
      });
    }
  }

  receiveMessage(message: ReceiveMessageArgument): string | Promise<string> {
    if (
      message.name === "M1HubDomProbe:FaultInjectTransportDisconnect" &&
      message.data === undefined
    ) {
      return this.sendQuery("M1HubDomProbe:FaultInjectTransportDisconnect")
        .then((response) =>
          typeof response === "string"
            ? response
            : JSON.stringify({ ok: false })
        );
    }
    if (
      message.name === "M1HubDomProbe:UnblockConnect" &&
      message.data === undefined
    ) {
      this.#protocol.connectBlocked = false;
      return JSON.stringify({ ok: true });
    }
    if (
      message.name === "M1HubDomProbe:BlockReactAcknowledgements" &&
      message.data === undefined
    ) {
      this.#protocol.ackBlocked = true;
      return JSON.stringify({ ok: true });
    }
    if (
      message.name === "M1HubDomProbe:UnblockReactAcknowledgements" &&
      message.data === undefined
    ) {
      this.#protocol.ackBlocked = false;
      return JSON.stringify({ ok: true });
    }
    if (
      message.name === "M1HubDomProbe:ReplaySavedReactAcknowledgement" &&
      message.data === undefined
    ) {
      const contentWindow = this.#listenerWindow ?? this.contentWindow;
      const acknowledgement = this.#savedAcknowledgement;
      if (!contentWindow || !acknowledgement) {
        return JSON.stringify({ ok: false });
      }
      this.#protocol.ackBlocked = false;
      this.#savedAcknowledgement = null;
      this.#protocol.savedAcknowledgementAvailable = false;
      contentWindow.dispatchEvent(
        new contentWindow.CustomEvent("nora:feature-hub-ack", {
          detail: acknowledgement,
        }),
      );
      return JSON.stringify({ ok: true });
    }
    if (
      message.name === "M1HubDomProbe:CreateSameOriginFrame" &&
      message.data === undefined
    ) {
      const contentWindow = this.#listenerWindow ?? this.contentWindow;
      if (!contentWindow) return JSON.stringify({ ok: false });
      if (this.#sameOriginFrame?.isConnected) {
        return JSON.stringify({ ok: true });
      }
      const frameURL = new URL(contentWindow.location.href);
      frameURL.searchParams.set("m1-hub-authorization-frame", "1");
      const frame = contentWindow.document.createElement("iframe");
      frame.hidden = true;
      frame.src = frameURL.href;
      contentWindow.document.body.append(frame);
      this.#sameOriginFrame = frame;
      return JSON.stringify({ ok: true });
    }
    if (
      message.name === "M1HubDomProbe:RemoveSameOriginFrame" &&
      message.data === undefined
    ) {
      this.#sameOriginFrame?.remove();
      this.#sameOriginFrame = null;
      return JSON.stringify({ ok: true });
    }
    if (
      message.name === "M1HubDomProbe:Resync" &&
      message.data === undefined
    ) {
      const contentWindow = this.#listenerWindow ?? this.contentWindow;
      if (!contentWindow) return JSON.stringify({ ok: false });
      contentWindow.dispatchEvent(
        new contentWindow.Event("nora:feature-hub-resyncing"),
      );
      return JSON.stringify({ ok: true });
    }
    if (
      message.name === "M1HubDomProbe:EmitDisconnected" &&
      message.data === undefined
    ) {
      const contentWindow = this.#listenerWindow ?? this.contentWindow;
      contentWindow?.dispatchEvent(
        new contentWindow.Event("nora:feature-hub-disconnected"),
      );
      return JSON.stringify({ ok: contentWindow !== null });
    }
    if (message.name === "M1HubDomProbe:Read" && message.data === undefined) {
      return JSON.stringify(this.#read());
    }
    if (
      message.name === "M1HubDomProbe:ClickRoute" &&
      typeof message.data === "string" && message.data.length <= 200
    ) {
      return JSON.stringify(this.#clickRoute(message.data));
    }
    return JSON.stringify({ ok: false });
  }

  handleEvent(event: Event): void {
    const type = event.type;
    if (type === "pageshow") {
      const persisted = (event as PageTransitionEvent).persisted;
      this.#pageshow.count = Math.min(this.#pageshow.count + 1, 65_535);
      if (persisted) {
        this.#pageshow.persistedCount = Math.min(
          this.#pageshow.persistedCount + 1,
          65_535,
        );
      }
      this.#pageshow.lastPersisted = persisted;
      return;
    }
    if (type === "nora:feature-hub-connect") {
      this.#protocol.connectCount = Math.min(
        this.#protocol.connectCount + 1,
        65_535,
      );
      this.#recordMetadata(type, (event as CustomEvent<unknown>).detail);
      if (this.#protocol.connectBlocked) {
        this.#protocol.blockedConnectCount = Math.min(
          this.#protocol.blockedConnectCount + 1,
          65_535,
        );
        event.stopImmediatePropagation();
      }
      return;
    } else if (type === "nora:feature-hub-snapshot") {
      this.#protocol.snapshotCount = Math.min(
        this.#protocol.snapshotCount + 1,
        65_535,
      );
    } else if (type === "nora:feature-hub-ack") {
      this.#protocol.ackCount = Math.min(this.#protocol.ackCount + 1, 65_535);
    } else if (type === "nora:feature-hub-disconnected") {
      this.#protocol.disconnectCount = Math.min(
        this.#protocol.disconnectCount + 1,
        65_535,
      );
    } else {
      return;
    }
    this.#recordMetadata(type, (event as CustomEvent<unknown>).detail);
    if (type === "nora:feature-hub-ack") {
      this.#protocol.lastAcknowledgement = { ...this.#protocol.latest };
    }
    if (type === "nora:feature-hub-ack" && this.#protocol.ackBlocked) {
      this.#protocol.blockedAckCount = Math.min(
        this.#protocol.blockedAckCount + 1,
        65_535,
      );
      const acknowledgement = (event as CustomEvent<unknown>).detail;
      if (
        typeof acknowledgement === "string" && acknowledgement.length <= 4096
      ) {
        this.#savedAcknowledgement = acknowledgement;
        this.#protocol.savedAcknowledgementAvailable = true;
      }
      event.stopImmediatePropagation();
    }
  }

  didDestroy(): void {
    this.#sameOriginFrame?.remove();
    this.#sameOriginFrame = null;
    const contentWindow = this.#listenerWindow;
    if (!contentWindow) return;
    contentWindow.removeEventListener("nora:feature-hub-connect", this, true);
    contentWindow.removeEventListener("nora:feature-hub-snapshot", this, true);
    contentWindow.removeEventListener("nora:feature-hub-ack", this, true);
    contentWindow.removeEventListener(
      "nora:feature-hub-disconnected",
      this,
      true,
    );
    this.#listenerWindow = null;
  }

  #read(): HubDomProbeRead | { error: string } {
    const contentWindow = this.#listenerWindow ?? this.contentWindow;
    if (!contentWindow) return { error: "content window is unavailable" };
    const document = contentWindow.document;
    const dynamicLinks = Array.from(
      document.querySelectorAll<HTMLAnchorElement>("a[href]"),
    ).flatMap((anchor): HubDomProbeLink[] => {
      const route = routeFromHref(anchor.getAttribute("href") ?? "");
      if (!route?.startsWith("/extensions/")) return [];
      const label = textContent(anchor);
      return label ? [{ label, route }] : [];
    }).slice(0, MAX_LINKS);
    const homeAnchor = Array.from(
      document.querySelectorAll<HTMLAnchorElement>("a[href]"),
    ).find((anchor) =>
      routeFromHref(anchor.getAttribute("href") ?? "") === "/overview/home"
    );
    const builtInHomeLink = homeAnchor
      ? {
        label: textContent(homeAnchor) ?? "",
        route: "/overview/home",
      }
      : null;
    const disabledNavigationLabels = Array.from(
      document.querySelectorAll<HTMLElement>('div[aria-disabled="true"]'),
    ).slice(0, MAX_LINKS).flatMap((element) => {
      const label = textContent(element);
      return label ? [label] : [];
    });
    return {
      documentURI: document.documentURI.slice(0, 512),
      hash: contentWindow.location.hash.slice(0, 256),
      settingsRootReady: Boolean(
        document.querySelector(".floorp-settings") &&
          document.querySelector("#settings-content"),
      ),
      messageTitle: textContent(document.querySelector("#hub-message-title")),
      messageBody: textContent(
        document.querySelector(
          'section[aria-labelledby="hub-message-title"] > p',
        ),
      ),
      unavailableTitle: textContent(
        document.querySelector("#hub-unavailable-title"),
      ),
      unavailableBody: textContent(
        document.querySelector(
          'section[aria-labelledby="hub-unavailable-title"] > p',
        ),
      ),
      dynamicLinks,
      builtInHomeLink,
      disabledNavigationLabels,
      pageshow: { ...this.#pageshow },
      protocol: {
        connectCount: this.#protocol.connectCount,
        blockedConnectCount: this.#protocol.blockedConnectCount,
        blockedAckCount: this.#protocol.blockedAckCount,
        disconnectCount: this.#protocol.disconnectCount,
        connectBlocked: this.#protocol.connectBlocked,
        ackBlocked: this.#protocol.ackBlocked,
        savedAcknowledgementAvailable: this.#protocol
          .savedAcknowledgementAvailable,
        snapshotCount: this.#protocol.snapshotCount,
        ackCount: this.#protocol.ackCount,
        latest: { ...this.#protocol.latest },
        lastAcknowledgement: { ...this.#protocol.lastAcknowledgement },
      },
    };
  }

  #clickRoute(route: string): { ok: boolean; route?: string } {
    if (!HUB_ROUTE.test(route) && route !== "/overview/home") {
      return { ok: false };
    }
    const contentWindow = this.#listenerWindow ?? this.contentWindow;
    if (!contentWindow) return { ok: false };
    const anchor = Array.from(
      contentWindow.document.querySelectorAll<HTMLAnchorElement>("a[href]"),
    ).find((candidate) =>
      routeFromHref(candidate.getAttribute("href") ?? "") === route
    );
    if (!anchor) return { ok: false };
    anchor.click();
    return { ok: true, route };
  }

  #recordMetadata(type: string, detail: unknown): void {
    if (type === "nora:feature-hub-connect") {
      if (
        typeof detail === "string" && detail.length > 0 && detail.length <= 80
      ) {
        this.#protocol.latest.requestId = detail;
      }
      return;
    }
    if (typeof detail !== "string" || detail.length > 131_584) return;
    let parsed: unknown;
    try {
      parsed = JSON.parse(detail);
    } catch {
      return;
    }
    if (!parsed || typeof parsed !== "object") return;
    const message = parsed as Record<string, unknown>;
    const snapshot = message.snapshot && typeof message.snapshot === "object"
      ? message.snapshot as Record<string, unknown>
      : null;
    if (
      typeof message.requestId === "string" && message.requestId.length > 0 &&
      message.requestId.length <= 80
    ) this.#protocol.latest.requestId = message.requestId;
    if (
      typeof message.connectionNonce === "string" &&
      message.connectionNonce.length > 0 && message.connectionNonce.length <= 80
    ) this.#protocol.latest.connectionNonce = message.connectionNonce;
    const epoch = message.epoch ?? snapshot?.epoch;
    if (typeof epoch === "string" && epoch.length > 0 && epoch.length <= 128) {
      this.#protocol.latest.epoch = epoch;
    }
    const revision = message.hubRevision ?? snapshot?.hubRevision;
    if (Number.isSafeInteger(revision) && (revision as number) >= 0) {
      this.#protocol.latest.revision = revision as number;
    }
  }
}
