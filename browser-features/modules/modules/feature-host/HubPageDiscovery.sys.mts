// SPDX-License-Identifier: MPL-2.0

import { launchDevPorts } from "../../common/dev-port-offset.ts";
import {
  configureSharedHubClientDiscovery,
  getSharedHubRegistry,
  type HubRegistry,
} from "./HubRegistry.sys.mts";
import {
  getFeatureWindowIdentity,
  isEligibleFeatureWindow,
} from "./WindowIdentity.sys.mts";
import type {
  HubClientConnection,
  HubClientDiscoveryAdapter,
  HubLiveDocument,
  HubSnapshot,
} from "./HubRegistryTypes.ts";
import type { HubPageSnapshotEnvelope } from "./HubPageProtocolTypes.ts";

type HubActor = JSWindowActorParent & {
  sendAsyncMessage(name: string, data?: unknown): void;
};
interface HubWindowGlobal {
  readonly isClosed: boolean;
  readonly documentURI: nsIURI | null;
  getActor(name: string): HubActor;
  readonly browsingContext: {
    readonly parent: unknown;
    readonly currentWindowGlobal: HubWindowGlobal | null;
    readonly top: { readonly embedderElement: XULElement | null };
  };
}
interface HubBrowser extends XULElement {
  readonly browsingContext: {
    readonly currentWindowGlobal: HubWindowGlobal | null;
  } | null;
  readonly ownerDocument: Document & { defaultView: HubChromeWindow | null };
}
interface HubChromeWindow extends Window {
  gBrowser?: {
    readonly browsers: readonly HubBrowser[];
    readonly tabContainer?: EventTarget;
    addTabsProgressListener(listener: {
      onLocationChange(browser: HubBrowser): void;
    }): void;
    removeTabsProgressListener(listener: {
      onLocationChange(browser: HubBrowser): void;
    }): void;
  };
}

const STARTUP_MODE = Services.prefs.getStringPref("nora.startup.mode", "");
const DEVELOPMENT_ENABLED = STARTUP_MODE === "dev" || STARTUP_MODE === "test";
const ALLOW_HTTP = DEVELOPMENT_ENABLED &&
  Services.prefs.getBoolPref("nora.dev.allow_http_loader", false);
const launchPorts = launchDevPorts();

function isAuthorizedHubURI(uri: nsIURI | null): uri is nsIURI {
  if (!DEVELOPMENT_ENABLED || !uri) return false;
  if (uri.spec.split(/[?#]/)[0] === "about:hub") return true;
  if (uri.schemeIs("chrome") && uri.host === "noraneko-settings") return true;
  return ALLOW_HTTP && uri.schemeIs("http") &&
    (uri.host === "localhost" || uri.host === "127.0.0.1") &&
    launchPorts.isTrustedPort(uri.port, ["settings"]);
}

function isCurrentTopLevel(manager: HubWindowGlobal): boolean {
  return !manager.isClosed && manager.browsingContext.parent === null &&
    manager.browsingContext.currentWindowGlobal === manager;
}

function isDocumentForManager(
  document: HubLiveDocument,
  manager: HubWindowGlobal,
): document is HubPageDocument {
  const candidate = document as HubPageDocument;
  return candidate.manager === manager &&
    typeof candidate.setConnection === "function";
}

function runTeardownAction(action: () => void): void {
  try {
    action();
  } catch (error) {
    console.error("[HubPageDiscovery]", error);
  }
}

class HubPageConnection implements HubClientConnection {
  readonly clientId: string;
  readonly connectionId: string;
  #closed = false;
  #deliveredEpoch: string | null = null;
  #deliveredRevision = -1;
  readonly #listeners = new Set<() => void>();

  constructor(
    readonly actor: HubActor,
    readonly manager: HubWindowGlobal,
    readonly document: HubPageDocument,
    readonly connectionNonce: string,
    readonly requestId: string | null,
    readonly windowId: string,
  ) {
    this.clientId = `hub-page:${windowId}:${connectionNonce}`;
    this.connectionId = connectionNonce;
  }

  isClosed(): boolean {
    return this.#closed || !isCurrentTopLevel(this.manager) ||
      this.document.isClosed;
  }

  sendSnapshot(json: string): void {
    if (
      this.isClosed() || !this.requestId || json.length > 1_048_576
    ) return;
    let message: unknown;
    try {
      message = JSON.parse(json);
    } catch {
      return;
    }
    if (!message || typeof message !== "object") return;
    const snapshot = (message as { snapshot?: unknown }).snapshot;
    if (!snapshot || typeof snapshot !== "object") return;
    const revision = (snapshot as { hubRevision?: unknown }).hubRevision;
    const epoch = (snapshot as { epoch?: unknown }).epoch;
    if (
      typeof epoch !== "string" || !Number.isSafeInteger(revision) ||
      (revision as number) < 0
    ) return;
    this.#deliveredEpoch = epoch;
    this.#deliveredRevision = revision as number;
    const snapshotJson = message as { snapshot: unknown };
    const envelope: HubPageSnapshotEnvelope = {
      type: "snapshot",
      connectionNonce: this.connectionNonce,
      requestId: this.requestId,
      snapshot: snapshotJson.snapshot as HubSnapshot,
    };
    this.actor.sendAsyncMessage(
      "NRFeatureHub:Snapshot",
      JSON.stringify(envelope),
    );
  }

  matchesDelivery(epoch: string, revision: number): boolean {
    return this.#deliveredEpoch === epoch &&
      this.#deliveredRevision === revision;
  }

  onClosed(callback: () => void): () => void {
    if (this.isClosed()) {
      runTeardownAction(callback);
      return () => {};
    }
    this.#listeners.add(callback);
    return () => this.#listeners.delete(callback);
  }

  close(): void {
    if (this.#closed) return;
    this.#closed = true;
    for (const callback of [...this.#listeners]) {
      runTeardownAction(callback);
    }
    this.#listeners.clear();
  }
}

class HubPageDocument implements HubLiveDocument {
  readonly documentId = Services.uuid.generateUUID().toString();
  readonly windowId: string;
  readonly #available = new Set<() => void>();
  readonly #closedListeners = new Set<() => void>();
  #closed = false;
  #connection: HubPageConnection | null = null;
  readonly #tabCloseListener: EventListener;
  readonly #windowUnloadListener: EventListener;
  readonly #progressListener: { onLocationChange(browser: HubBrowser): void };

  constructor(
    readonly browser: HubBrowser,
    readonly manager: HubWindowGlobal,
    readonly chromeWindow: HubChromeWindow,
  ) {
    this.windowId = getFeatureWindowIdentity(chromeWindow);
    this.#progressListener = {
      onLocationChange: (browser) => {
        if (
          browser === this.browser &&
          browser.browsingContext?.currentWindowGlobal !== this.manager
        ) this.close();
      },
    };
    this.#tabCloseListener = (event) => {
      const tab = event.target as
        | (EventTarget & { linkedBrowser?: unknown })
        | null;
      if (tab?.linkedBrowser === this.browser) this.close();
    };
    this.#windowUnloadListener = () => this.close();
    chromeWindow.gBrowser?.addTabsProgressListener(this.#progressListener);
    chromeWindow.gBrowser?.tabContainer?.addEventListener(
      "TabClose",
      this.#tabCloseListener,
    );
    chromeWindow.addEventListener("unload", this.#windowUnloadListener, {
      once: true,
    });
  }

  get isClosed(): boolean {
    return this.#closed;
  }

  isLive(): boolean {
    return !this.#closed && !this.browser.ownerDocument.defaultView?.closed &&
      isEligibleFeatureWindow(this.chromeWindow) &&
      isCurrentTopLevel(this.manager);
  }

  ensureParentClient(): HubClientConnection | null {
    if (!this.isLive() || !isAuthorizedHubURI(this.manager.documentURI)) {
      return null;
    }
    let actor: HubActor;
    try {
      actor = this.manager.getActor("NRFeatureHub");
    } catch {
      return null;
    }
    if (!actor || hubPageDiscovery.isRetiredActor(actor)) return null;
    if (!this.#connection || this.#connection.isClosed()) {
      this.#connection = hubPageDiscovery.connectActor(
        actor,
        this.manager,
        this,
        null,
      );
    }
    return this.#connection;
  }

  onParentClientAvailable(callback: () => void): () => void {
    this.#available.add(callback);
    return () => this.#available.delete(callback);
  }

  onClosed(callback: () => void): () => void {
    if (!this.isLive()) {
      runTeardownAction(callback);
      return () => {};
    }
    this.#closedListeners.add(callback);
    return () => this.#closedListeners.delete(callback);
  }

  notifyAvailable(): void {
    for (const callback of [...this.#available]) {
      runTeardownAction(callback);
    }
  }

  invalidateConnection(
    connection: HubPageConnection,
    notify = true,
  ): void {
    if (this.#connection !== connection) return;
    this.#connection = null;
    if (notify) this.notifyAvailable();
  }

  setConnection(connection: HubPageConnection): void {
    this.#connection = connection;
  }

  clearConnection(connection: HubPageConnection): void {
    if (this.#connection === connection) this.#connection = null;
  }

  close(): void {
    if (this.#closed) return;
    this.#closed = true;
    runTeardownAction(() =>
      this.chromeWindow.gBrowser?.removeTabsProgressListener(
        this.#progressListener,
      )
    );
    runTeardownAction(() =>
      this.chromeWindow.gBrowser?.tabContainer?.removeEventListener(
        "TabClose",
        this.#tabCloseListener,
      )
    );
    runTeardownAction(() =>
      this.chromeWindow.removeEventListener(
        "unload",
        this.#windowUnloadListener,
      )
    );
    runTeardownAction(() => hubPageDiscovery.closeDocument(this));
    this.#connection = null;
    for (const callback of [...this.#closedListeners]) {
      runTeardownAction(callback);
    }
    this.#closedListeners.clear();
    this.#available.clear();
  }
}

class HubPageDiscovery implements HubClientDiscoveryAdapter {
  readonly #documents = new WeakMap<HubWindowGlobal, HubPageDocument>();
  readonly #connections = new Map<HubActor, HubPageConnection>();
  readonly #retiredActors = new WeakSet<HubActor>();

  isRetiredActor(actor: HubActor): boolean {
    return this.#retiredActors.has(actor);
  }

  discoverLiveHubDocuments(
    windowIds: readonly string[],
  ): readonly HubLiveDocument[] {
    if (!DEVELOPMENT_ENABLED || windowIds.length === 0) return [];
    const wanted = new Set(windowIds);
    const documents: HubPageDocument[] = [];
    const windows = Services.wm.getEnumerator("navigator:browser");
    while (windows.hasMoreElements()) {
      const chromeWindow = windows.getNext() as HubChromeWindow;
      if (!isEligibleFeatureWindow(chromeWindow)) continue;
      if (!wanted.has(getFeatureWindowIdentity(chromeWindow))) continue;
      for (const browser of chromeWindow.gBrowser?.browsers ?? []) {
        const manager = browser.browsingContext?.currentWindowGlobal;
        if (
          !manager || !isCurrentTopLevel(manager) ||
          !isAuthorizedHubURI(manager.documentURI)
        ) continue;
        let document = this.#documents.get(manager);
        if (!document || document.isClosed) {
          document = new HubPageDocument(browser, manager, chromeWindow);
          this.#documents.set(manager, document);
        }
        documents.push(document);
      }
    }
    return documents;
  }

  connectActor(
    actor: HubActor,
    manager: HubWindowGlobal,
    document: HubPageDocument,
    requestId: string | null = null,
  ): HubPageConnection {
    const current = this.#connections.get(actor);
    if (current && !current.isClosed() && current.requestId === requestId) {
      return current;
    }
    const nonce = Services.uuid.generateUUID().toString();
    const connection = new HubPageConnection(
      actor,
      manager,
      document,
      nonce,
      requestId,
      document.windowId,
    );
    document.setConnection(connection);
    this.#connections.set(actor, connection);
    if (current) {
      current.document.invalidateConnection(current, false);
      current.close();
      getSharedHubRegistry().removeClient(current);
      if (current.document !== document) current.document.notifyAvailable();
    }
    document.notifyAvailable();
    return connection;
  }

  disconnectActor(actor: HubActor, notifyAvailable = true): void {
    const connection = this.#connections.get(actor);
    if (!connection) return;
    this.#connections.delete(actor);
    connection.document.invalidateConnection(connection, false);
    connection.close();
    getSharedHubRegistry().removeClient(connection);
    if (notifyAvailable) connection.document.notifyAvailable();
  }

  closeDocument(document: HubPageDocument): void {
    this.#documents.delete(document.manager);
    for (const [actor, connection] of this.#connections) {
      if (connection.document !== document) continue;
      this.#connections.delete(actor);
      document.clearConnection(connection);
      runTeardownAction(() => connection.close());
      runTeardownAction(() => getSharedHubRegistry().removeClient(connection));
    }
  }

  closeActor(actor: HubActor): void {
    this.#retiredActors.add(actor);
    const connection = this.#connections.get(actor);
    if (!connection) return;
    let documentIsLive: boolean;
    try {
      documentIsLive = connection.document.isLive();
    } catch {
      this.disconnectActor(actor, false);
      return;
    }
    if (!documentIsLive) {
      connection.document.close();
      return;
    }
    this.disconnectActor(actor, false);
  }

  currentConnection(actor: HubActor): HubPageConnection | null {
    return this.#connections.get(actor) ?? null;
  }

  handleAcknowledgement(actor: HubActor, json: unknown): boolean {
    if (typeof json !== "string" || json.length > 4096) return false;
    const connection = this.#connections.get(actor);
    if (!connection || connection.isClosed()) return false;
    let message: unknown;
    try {
      message = JSON.parse(json);
    } catch {
      return false;
    }
    if (
      !message || typeof message !== "object" ||
      Object.getPrototypeOf(message) !== Object.prototype
    ) return false;
    const ack = message as Record<string, unknown>;
    if (
      Object.keys(ack).length !== 5 || ack.type !== "snapshot-ack" ||
      ack.connectionNonce !== connection.connectionId ||
      ack.requestId !== connection.requestId ||
      typeof ack.requestId !== "string" || ack.requestId.length < 1 ||
      ack.requestId.length > 80 ||
      typeof ack.epoch !== "string" ||
      !Number.isSafeInteger(ack.hubRevision) ||
      !connection.matchesDelivery(ack.epoch, ack.hubRevision as number)
    ) return false;
    const registryAck = JSON.stringify({
      type: "snapshot-ack",
      epoch: ack.epoch,
      hubRevision: ack.hubRevision,
    });
    return getSharedHubRegistry().acknowledge(connection, registryAck);
  }
}

const discoveryKey = Symbol.for("floorp.featureHub.discoveryService");
const sharedServices = globalThis as unknown as Record<symbol, unknown>;
const hubPageDiscovery =
  (sharedServices[discoveryKey] ??= new HubPageDiscovery()) as HubPageDiscovery;
const BUILTIN_SETTINGS_ROUTES = [
  "/",
  "/search",
  "/overview/home",
  "/features/design",
  "/features/design/chrome-extras",
  "/features/design/lepton",
  "/features/sidebar",
  "/features/workspaces",
  "/features/webapps",
  "/features/command-palette",
  "/features/floorp-os",
  "/features/accounts",
  "/features/gesture",
  "/features/shortcuts",
  "/features/context-menu",
  "/features/performance",
  "/about/browser",
  "/about/updates",
];

export function initializeHubPageDiscovery(): typeof HubPageDiscoveryService {
  configureSharedHubClientDiscovery(hubPageDiscovery, BUILTIN_SETTINGS_ROUTES);
  return HubPageDiscoveryService;
}

export const HubPageDiscoveryService = {
  discoveryIdentityForTests(): object {
    return hubPageDiscovery;
  },
  sharedRegistryForTests(): HubRegistry {
    return getSharedHubRegistry();
  },
  usesConfiguredDiscoveryForTests(): boolean {
    return getSharedHubRegistry().isUsingDiscoveryAdapterForTests(
      hubPageDiscovery,
    );
  },
  connectActor(
    actor: HubActor,
    manager: HubWindowGlobal,
    requestId: unknown,
  ): HubPageConnection | null {
    if (
      !DEVELOPMENT_ENABLED || !isCurrentTopLevel(manager) ||
      !isAuthorizedHubURI(manager.documentURI) ||
      hubPageDiscovery.isRetiredActor(actor)
    ) return null;
    try {
      if (manager.getActor("NRFeatureHub") !== actor) return null;
    } catch {
      return null;
    }
    if (
      typeof requestId !== "string" || requestId.length < 1 ||
      requestId.length > 80
    ) return null;
    const browser = manager.browsingContext.top.embedderElement as
      | HubBrowser
      | null;
    const chromeWindow = browser?.ownerDocument.defaultView;
    if (!browser || !chromeWindow || !isEligibleFeatureWindow(chromeWindow)) {
      return null;
    }
    const document = hubPageDiscovery.discoverLiveHubDocuments([
      getFeatureWindowIdentity(chromeWindow),
    ])
      .find((item) => isDocumentForManager(item, manager));
    if (!document) return null;
    const connection = hubPageDiscovery.connectActor(
      actor,
      manager,
      document,
      requestId,
    );
    getSharedHubRegistry().registerClient(connection);
    return connection;
  },
  disconnectActor(actor: HubActor): void {
    hubPageDiscovery.disconnectActor(actor);
  },
  disconnectRequest(actor: HubActor, data: unknown): void {
    if (typeof data !== "string" || data.length > 4096) return;
    let value: unknown;
    try {
      value = JSON.parse(data);
    } catch {
      return;
    }
    if (
      !value || typeof value !== "object" ||
      Object.getPrototypeOf(value) !== Object.prototype
    ) return;
    const request = value as Record<string, unknown>;
    const connection = hubPageDiscovery.currentConnection(actor);
    if (
      Object.keys(request).length !== 2 ||
      typeof request.requestId !== "string" || request.requestId.length < 1 ||
      request.requestId.length > 80 ||
      (request.connectionNonce !== null &&
        (typeof request.connectionNonce !== "string" ||
          request.connectionNonce.length > 80)) ||
      !connection || request.requestId !== connection.requestId ||
      (request.connectionNonce !== null &&
        request.connectionNonce !== connection.connectionNonce)
    ) return;
    hubPageDiscovery.disconnectActor(actor);
  },
  closeActor(actor: HubActor): void {
    hubPageDiscovery.closeActor(actor);
  },
  handleAcknowledgement(actor: HubActor, json: unknown): boolean {
    return hubPageDiscovery.handleAcknowledgement(actor, json);
  },
};
