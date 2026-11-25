// SPDX-License-Identifier: MPL-2.0

import { isEligibleFeatureWindow } from "./WindowIdentity.sys.mts";

export const HUB_PANEL_MENU_FEATURE_ID = "hub-panel-menu";
export const HUB_PANEL_MENU_BUTTON_ID = "appMenu-floorp-hub-button";
export const HUB_PANEL_MENU_TRIGGER_ID = "PanelUI-menu-button";
export const HUB_PANEL_MENU_SLOT = "afterSettings";
export const HUB_PANEL_MENU_LOCALIZATION_KEY = "hub.menu.title";
export const HUB_PANEL_MENU_URL = "about:hub";
const DEFAULT_READY_TIMEOUT_MS = 2_000;
const { setTimeout, clearTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

export interface AppMenuOwner {
  readonly featureId: string;
  readonly hostEpoch: string;
  readonly generation: number;
}

export interface AppMenuDescriptor {
  readonly command: "open-hub";
}

export interface AppMenuRenderRequest {
  readonly owner: AppMenuOwner;
  readonly descriptor: AppMenuDescriptor;
  readonly buttonId: typeof HUB_PANEL_MENU_BUTTON_ID;
  readonly slot: typeof HUB_PANEL_MENU_SLOT;
  readonly localizationKey: typeof HUB_PANEL_MENU_LOCALIZATION_KEY;
  readonly url: typeof HUB_PANEL_MENU_URL;
  readonly enabled: boolean;
}

export type AppMenuAcknowledgement = "rendered" | "deferred" | null;
export type AppMenuRemovalReason = "replacement" | "remove";

export interface AppMenuAdapter {
  render(request: AppMenuRenderRequest): AppMenuAcknowledgement;
  setEnabled(owner: AppMenuOwner, enabled: boolean): void;
  remove(request: AppMenuRenderRequest, reason: AppMenuRemovalReason): void;
}

export interface AppMenuGenerationLease {
  readonly owner: AppMenuOwner;
  readonly id: string;
}

export interface AppMenuHostClaim {
  readonly featureId: string;
  readonly hostEpoch: string;
  release(): void;
}

export interface AppMenuAdapterConnection {
  readonly id: string;
  notifyReady(): void;
  invoke(owner: AppMenuOwner): boolean;
  reportFailure(owner: AppMenuOwner, error: Error): boolean;
  dispose(): void;
}

interface AdapterEntry {
  readonly window: Window;
  readonly adapter: AppMenuAdapter;
  readonly connection: AppMenuAdapterConnection;
  disposed: boolean;
  deliveryOwner: AppMenuOwner | null;
  deliveryLease: AppMenuGenerationLease | null;
  deliveredOwner: AppMenuOwner | null;
  deliveredLease: AppMenuGenerationLease | null;
}

interface WindowWaiter {
  readonly lease: AppMenuGenerationLease;
  readonly phase: "staged" | "published";
  readonly promise: Promise<void>;
  readonly resolve: () => void;
  readonly reject: (error: Error) => void;
  readonly onUnload: EventListener;
  readonly onAbort: () => void;
  settled: boolean;
  timedOut: boolean;
}

interface LeaseState {
  readonly owner: AppMenuOwner;
  readonly windows: Set<Window>;
  readonly acknowledgements: Map<Window, AppMenuAdapterConnection>;
  readonly waiters: Map<Window, Set<WindowWaiter>>;
  readonly isCurrent: () => boolean;
  readonly onFailure: (
    owner: AppMenuOwner,
    connectionId: string,
    error: Error,
  ) => void;
  descriptor: AppMenuDescriptor | null;
  staged: boolean;
  published: boolean;
  enabled: boolean;
  revoked: boolean;
  failure: Error | null;
}

interface BrowserWindow extends Window {
  gBrowser: {
    addTab(
      url: typeof HUB_PANEL_MENU_URL,
      options: { relatedToCurrent: true; triggeringPrincipal: nsIPrincipal },
    ): XULElement;
    selectedTab: XULElement;
  };
  PanelUI: { hide(): void };
}

function asError(value: unknown): Error {
  if (value instanceof Error) return value;
  if (typeof value === "object" && value !== null) {
    try {
      const message = Reflect.get(value, "message");
      if (typeof message === "string") return new Error(message);
    } catch {
      return new Error(String(value));
    }
  }
  return new Error(String(value));
}

function sameOwner(left: AppMenuOwner | null, right: AppMenuOwner): boolean {
  return left?.featureId === right.featureId &&
    left.hostEpoch === right.hostEpoch && left.generation === right.generation;
}

export class AppMenuRegistry {
  readonly #leases = new Map<AppMenuGenerationLease, LeaseState>();
  readonly #connections = new WeakMap<Window, AdapterEntry>();
  readonly #entries = new Set<AdapterEntry>();
  #staged: AppMenuGenerationLease | null = null;
  #active: AppMenuGenerationLease | null = null;
  readonly #claims = new Map<string, AppMenuHostClaim>();
  readonly #highestGeneration = new WeakMap<AppMenuHostClaim, number>();

  claimHost(featureId: string, hostEpoch: string): AppMenuHostClaim {
    if (featureId !== HUB_PANEL_MENU_FEATURE_ID || !hostEpoch) {
      throw new Error("invalid app-menu host claim");
    }
    if (this.#claims.has(featureId)) {
      throw new Error("app-menu feature owner already claimed");
    }
    let released = false;
    const claim: AppMenuHostClaim = Object.freeze({
      featureId,
      hostEpoch,
      release: () => {
        if (released) return;
        released = true;
        this.#releaseHost(claim);
      },
    });
    this.#claims.set(featureId, claim);
    return claim;
  }

  beginGeneration(
    owner: AppMenuOwner,
    onFailure: LeaseState["onFailure"],
    isCurrent: () => boolean,
  ): AppMenuGenerationLease {
    if (
      owner.featureId !== HUB_PANEL_MENU_FEATURE_ID ||
      !owner.hostEpoch || !Number.isSafeInteger(owner.generation) ||
      owner.generation < 1
    ) throw new Error("app-menu capability denied");
    const claim = this.#claims.get(owner.featureId);
    if (!claim || claim.hostEpoch !== owner.hostEpoch) {
      throw new Error("app-menu host is not claimed");
    }
    const previous = this.#staged ?? this.#active;
    if (previous && !this.#leases.get(previous)?.revoked) {
      throw new Error("app-menu generation is already staged");
    }
    const highestGeneration = this.#highestGeneration.get(claim) ?? 0;
    if (owner.generation <= highestGeneration) {
      throw new Error("app-menu generation is not monotonic");
    }
    this.#highestGeneration.set(claim, owner.generation);
    const stableOwner = Object.freeze({ ...owner });
    const lease = Object.freeze({
      owner: stableOwner,
      id: Services.uuid.generateUUID().toString(),
    });
    this.#leases.set(lease, {
      owner: stableOwner,
      windows: new Set(),
      acknowledgements: new Map(),
      waiters: new Map(),
      isCurrent,
      onFailure,
      descriptor: null,
      staged: false,
      published: false,
      enabled: false,
      revoked: false,
      failure: null,
    });
    this.#staged = lease;
    return lease;
  }

  stageDescriptor(
    lease: AppMenuGenerationLease,
    value: unknown,
  ): void {
    const state = this.#state(lease);
    if (this.#staged !== lease || state.staged || state.revoked) {
      throw new Error("stale app-menu candidate");
    }
    if (
      typeof value !== "object" || value === null ||
      Array.isArray(value) || Object.keys(value).length !== 1 ||
      (value as Record<string, unknown>).command !== "open-hub"
    ) throw new Error("invalid app-menu descriptor");
    state.descriptor = Object.freeze({ command: "open-hub" });
  }

  async stageAndWait(
    lease: AppMenuGenerationLease,
    windows: readonly Window[],
    signal: AbortSignal,
    timeoutMs = DEFAULT_READY_TIMEOUT_MS,
  ): Promise<void> {
    const state = this.#state(lease);
    if (
      this.#staged !== lease || !state.descriptor || state.revoked ||
      state.published
    ) throw new Error("invalid app-menu staging state");
    state.staged = true;
    if (state.failure) throw state.failure;
    return await this.#prepareWindows(
      lease,
      windows,
      "staged",
      signal,
      timeoutMs,
    );
  }

  async publishAndWait(
    lease: AppMenuGenerationLease,
    windows: readonly Window[],
    signal: AbortSignal,
    timeoutMs = DEFAULT_READY_TIMEOUT_MS,
  ): Promise<void> {
    const state = this.#state(lease);
    if (
      this.#staged !== lease || !state.staged || !state.descriptor ||
      state.revoked
    ) throw new Error("invalid app-menu publication state");
    this.#active = lease;
    state.published = true;
    state.enabled = false;
    if (state.failure) throw state.failure;
    return await this.#prepareWindows(
      lease,
      windows,
      "published",
      signal,
      timeoutMs,
    );
  }

  enable(lease: AppMenuGenerationLease): void {
    const state = this.#state(lease);
    if (
      this.#active !== lease || !state.published || state.revoked ||
      !state.descriptor
    ) throw new Error("app-menu generation is not published");
    if (state.failure) throw state.failure;
    for (const window of state.windows) {
      if (window.closed || !isEligibleFeatureWindow(window)) continue;
      const entry = this.#connections.get(window);
      if (!entry || state.acknowledgements.get(window) !== entry.connection) {
        throw new Error("app-menu adapter readiness was revoked");
      }
    }
    state.enabled = true;
  }

  prepareEnable(lease: AppMenuGenerationLease): void {
    const state = this.#state(lease);
    if (
      this.#active !== lease || !state.published || state.revoked ||
      !state.descriptor
    ) throw new Error("app-menu generation is not published");
    if (state.failure) throw state.failure;
    for (const window of state.windows) {
      if (window.closed || !isEligibleFeatureWindow(window)) continue;
      const entry = this.#connections.get(window);
      if (!entry || state.acknowledgements.get(window) !== entry.connection) {
        throw new Error("app-menu adapter readiness was revoked");
      }
      entry.adapter.setEnabled(state.owner, true);
    }
  }

  revoke(
    lease: AppMenuGenerationLease,
    reason: AppMenuRemovalReason = "remove",
  ): readonly Error[] {
    const state = this.#leases.get(lease);
    if (!state || state.revoked) return [];
    state.enabled = false;
    state.revoked = true;
    if (this.#active === lease) this.#active = null;
    if (this.#staged === lease) this.#staged = null;
    this.#rejectWaiters(state, new Error("app-menu generation revoked"));
    const failures: { entry: AdapterEntry; window: Window; error: Error }[] =
      [];
    for (const window of state.windows) {
      const entry = this.#connections.get(window);
      if (entry) {
        try {
          entry.adapter.remove(this.#request(state, false), reason);
        } catch (error) {
          failures.push({ entry, window, error: asError(error) });
        }
      }
    }
    const errors = failures.map(({ error }) => error);
    if (failures.length && !state.failure) {
      try {
        const failure = failures[0];
        this.#recordFailure(
          state,
          lease,
          failure.window,
          failure.entry.connection.id,
          failure.error,
        );
      } catch (error) {
        errors.push(asError(error));
      }
    }
    state.acknowledgements.clear();
    state.windows.clear();
    this.#leases.delete(lease);
    return errors;
  }

  registerAdapter(
    window: Window,
    adapter: AppMenuAdapter,
  ): AppMenuAdapterConnection {
    if (window.closed || !isEligibleFeatureWindow(window)) {
      throw new Error("ineligible app-menu adapter window");
    }
    const previous = this.#connections.get(window);
    if (previous) previous.connection.dispose();
    const entry = {} as AdapterEntry;
    const connection: AppMenuAdapterConnection = Object.freeze({
      id: Services.uuid.generateUUID().toString(),
      notifyReady: () => this.#notifyReady(entry),
      invoke: (owner: AppMenuOwner) => this.#invoke(entry, owner),
      reportFailure: (owner: AppMenuOwner, error: Error) =>
        this.#reportFailure(entry, owner, error),
      dispose: () => this.#disconnect(entry),
    });
    Object.assign(entry, {
      window,
      adapter,
      connection,
      disposed: false,
      deliveryOwner: null,
      deliveryLease: null,
      deliveredOwner: null,
      deliveredLease: null,
    });
    this.#connections.set(window, entry);
    this.#entries.add(entry);
    const lease = this.#leaseForWindow(window);
    if (lease) {
      const state = this.#leases.get(lease);
      if (state && this.#active === lease && state.published) {
        if (state.windows.has(window)) {
          this.#deliver(entry, lease);
        }
      } else {
        this.#deliver(entry, lease);
      }
    }
    return connection;
  }

  async admitWindow(
    lease: AppMenuGenerationLease,
    window: Window,
    signal: AbortSignal,
    timeoutMs = DEFAULT_READY_TIMEOUT_MS,
  ): Promise<void> {
    const state = this.#state(lease);
    if (state.revoked || window.closed) return;
    if (!isEligibleFeatureWindow(window)) {
      throw new Error("ineligible app-menu admission window");
    }
    state.windows.add(window);
    await this.#waitForWindow(
      lease,
      window,
      state.published ? "published" : "staged",
      signal,
      timeoutMs,
    );
  }

  forgetWindow(lease: AppMenuGenerationLease, window: Window): void {
    const state = this.#leases.get(lease);
    if (!state) return;
    state.windows.delete(window);
    state.acknowledgements.delete(window);
    for (const waiter of [...(state.waiters.get(window) ?? [])]) {
      waiter.resolve();
    }
  }

  closeWindow(lease: AppMenuGenerationLease | null, window: Window): void {
    const entry = this.#connections.get(window);
    const targetLease = lease ?? entry?.deliveredLease ?? null;
    const state = targetLease ? this.#leases.get(targetLease) : null;
    const errors: Error[] = [];
    if (entry) {
      entry.disposed = true;
      this.#connections.delete(window);
      this.#entries.delete(entry);
      if (state?.descriptor) {
        try {
          entry.adapter.remove(this.#request(state, false), "remove");
        } catch (error) {
          errors.push(asError(error));
        }
      }
    }
    if (targetLease) this.forgetWindow(targetLease, window);
    if (state && errors.length && !state.failure) {
      try {
        this.#recordFailure(
          state,
          targetLease!,
          window,
          entry?.connection.id ?? "window-close",
          errors[0],
        );
      } catch (error) {
        errors.push(asError(error));
      }
    }
    if (errors.length) {
      throw new AggregateError(errors, "app-menu window cleanup failed");
    }
  }

  get diagnosticsForTests(): Readonly<{
    connections: number;
    staged: boolean;
    published: boolean;
    enabled: boolean;
    pendingAcknowledgements: number;
  }> {
    const lease = this.#active ?? this.#staged;
    const state = lease ? this.#leases.get(lease) : undefined;
    return Object.freeze({
      connections: this.#entries.size,
      staged: state?.staged ?? false,
      published: state?.published ?? false,
      enabled: state?.enabled ?? false,
      pendingAcknowledgements: state
        ? [...state.waiters.values()].reduce(
          (count, waiters) => count + waiters.size,
          0,
        )
        : 0,
    });
  }

  #state(lease: AppMenuGenerationLease): LeaseState {
    const state = this.#leases.get(lease);
    if (!state) throw new Error("unknown app-menu generation");
    return state;
  }

  #request(state: LeaseState, enabled: boolean): AppMenuRenderRequest {
    if (!state.descriptor) throw new Error("app-menu descriptor missing");
    return Object.freeze({
      owner: state.owner,
      descriptor: state.descriptor,
      buttonId: HUB_PANEL_MENU_BUTTON_ID,
      slot: HUB_PANEL_MENU_SLOT,
      localizationKey: HUB_PANEL_MENU_LOCALIZATION_KEY,
      url: HUB_PANEL_MENU_URL,
      enabled,
    });
  }

  async #prepareWindows(
    lease: AppMenuGenerationLease,
    windows: readonly Window[],
    phase: "staged" | "published",
    signal: AbortSignal,
    timeoutMs: number,
  ): Promise<void> {
    const state = this.#state(lease);
    if (signal.aborted) {
      throw signal.reason ?? new Error("app-menu wait aborted");
    }
    const targets = [...new Set(windows)].filter((window) =>
      !window.closed && isEligibleFeatureWindow(window)
    );
    for (const window of targets) state.windows.add(window);
    try {
      await Promise.all(
        targets.map((window) =>
          this.#waitForWindow(lease, window, phase, signal, timeoutMs)
        ),
      );
    } catch (error) {
      this.#rejectWaiters(
        state,
        error instanceof Error ? error : new Error(String(error)),
      );
      throw error;
    }
  }

  #waitForWindow(
    lease: AppMenuGenerationLease,
    window: Window,
    phase: "staged" | "published",
    signal: AbortSignal,
    timeoutMs: number,
    deliverCurrent = true,
  ): Promise<void> {
    const state = this.#state(lease);
    if (window.closed) return Promise.resolve();
    if (signal.aborted) {
      return Promise.reject(
        signal.reason ?? new Error("app-menu wait aborted"),
      );
    }
    const existing = this.#windowWaiter(state, lease, window, phase);
    if (existing) return existing.promise;
    const entry = this.#connections.get(window);
    if (entry && state.acknowledgements.get(window) === entry.connection) {
      state.acknowledgements.delete(window);
    }
    let resolvePromise!: () => void;
    let rejectPromise!: (error: Error) => void;
    const promise = new Promise<void>((resolve, reject) => {
      resolvePromise = resolve;
      rejectPromise = reject;
    });
    let timer: number | null = null;
    function settle(ok: boolean, reason?: unknown): void {
      if (waiter.settled) return;
      waiter.settled = true;
      if (timer !== null) clearTimeout(timer);
      window.removeEventListener("unload", waiter.onUnload);
      signal.removeEventListener("abort", waiter.onAbort);
      const waiters = state.waiters.get(window);
      waiters?.delete(waiter);
      if (waiters?.size === 0) state.waiters.delete(window);
      if (ok) resolvePromise();
      else {
        if (reason instanceof Error) {
          rejectPromise(reason);
          return;
        }
        let message: unknown;
        if (typeof reason === "object" && reason !== null) {
          try {
            message = Reflect.get(reason, "message");
          } catch {
            message = undefined;
          }
        }
        rejectPromise(
          new Error(
            typeof message === "string" ? message : "app-menu wait aborted",
          ),
        );
      }
    }
    const onUnload: EventListener = () => settle(true);
    const onAbort = () => settle(false, signal.reason);
    const waiter: WindowWaiter = {
      lease,
      phase,
      promise,
      resolve: () => settle(true),
      reject: (error) => settle(false, error),
      onUnload,
      onAbort,
      settled: false,
      timedOut: false,
    };
    const waiters = state.waiters.get(window) ?? new Set<WindowWaiter>();
    waiters.add(waiter);
    state.waiters.set(window, waiters);
    window.addEventListener("unload", onUnload, { once: true });
    signal.addEventListener("abort", onAbort, { once: true });
    timer = setTimeout(() => {
      waiter.timedOut = true;
      settle(false, new Error("app-menu adapter readiness timed out"));
    }, timeoutMs);
    if (deliverCurrent) {
      const current = this.#connections.get(window);
      if (current) this.#deliver(current, lease);
    }
    return promise;
  }

  #deliver(entry: AdapterEntry, lease: AppMenuGenerationLease): void {
    const state = this.#leases.get(lease);
    if (
      !state || state.revoked || entry.disposed ||
      this.#connections.get(entry.window) !== entry || !state.descriptor ||
      !state.windows.has(entry.window) || state.failure || entry.deliveryOwner
    ) return;
    let acknowledgement: AppMenuAcknowledgement = null;
    let renderFailed = false;
    let renderError: unknown;
    entry.deliveryOwner = state.owner;
    entry.deliveryLease = lease;
    entry.deliveredOwner = state.owner;
    entry.deliveredLease = lease;
    try {
      acknowledgement = entry.adapter.render(
        this.#request(state, state.enabled && this.#active === lease),
      );
    } catch (error) {
      renderFailed = true;
      renderError = error;
    } finally {
      entry.deliveryOwner = null;
      entry.deliveryLease = null;
    }
    if (
      this.#leases.get(lease) !== state || state.revoked || state.failure ||
      entry.disposed || this.#connections.get(entry.window) !== entry ||
      !state.windows.has(entry.window) ||
      (this.#active !== lease && this.#staged !== lease)
    ) return;
    if (renderFailed) {
      this.#failDelivery(state, entry, lease, asError(renderError));
      return;
    }
    if (state.failure) return;
    if (acknowledgement === null) {
      this.#markUnavailable(state, entry, lease);
      return;
    }
    const trigger = entry.window.document.getElementById(
      HUB_PANEL_MENU_TRIGGER_ID,
    );
    if (!trigger) {
      this.#markUnavailable(state, entry, lease);
      return;
    }
    const menuOpen = trigger.getAttribute("open") === "true";
    if (
      (menuOpen && acknowledgement !== "rendered") ||
      (!menuOpen && acknowledgement !== "rendered" &&
        acknowledgement !== "deferred")
    ) {
      this.#failDelivery(
        state,
        entry,
        lease,
        new Error("invalid app-menu render acknowledgement"),
      );
      return;
    }
    if (this.#connections.get(entry.window) !== entry || entry.disposed) return;
    state.acknowledgements.set(entry.window, entry.connection);
    this.#settleWaiters(state, entry.window, lease, true);
  }

  #markUnavailable(
    state: LeaseState,
    entry: AdapterEntry,
    lease: AppMenuGenerationLease,
  ): void {
    state.acknowledgements.delete(entry.window);
    if (!state.published) return;
    try {
      entry.adapter.setEnabled(state.owner, false);
    } catch (error) {
      this.#failDelivery(
        state,
        entry,
        lease,
        error instanceof Error ? error : new Error(String(error)),
      );
      return;
    }
    this.#startLateReadinessWait(lease, entry.window, entry.connection.id);
  }

  #startLateReadinessWait(
    lease: AppMenuGenerationLease,
    window: Window,
    connectionId: string,
  ): void {
    const state = this.#leases.get(lease);
    if (
      !state || state.revoked || this.#active !== lease || !state.published ||
      window.closed || this.#windowWaiter(state, lease, window, "published")
    ) return;
    const promise = this.#waitForWindow(
      lease,
      window,
      "published",
      new AbortController().signal,
      DEFAULT_READY_TIMEOUT_MS,
      false,
    );
    const waiter = this.#windowWaiter(state, lease, window, "published");
    if (!waiter) return;
    void promise.catch((error: unknown) => {
      if (
        !waiter.timedOut || state.revoked ||
        this.#leases.get(lease) !== state ||
        this.#active !== lease || window.closed || !state.windows.has(window)
      ) return;
      this.#recordFailure(
        state,
        lease,
        window,
        connectionId,
        error instanceof Error ? error : new Error(String(error)),
      );
    });
  }

  #failDelivery(
    state: LeaseState,
    entry: AdapterEntry,
    lease: AppMenuGenerationLease,
    failure: Error,
  ): void {
    this.#recordFailure(
      state,
      lease,
      entry.window,
      entry.connection.id,
      failure,
    );
  }

  #recordFailure(
    state: LeaseState,
    lease: AppMenuGenerationLease,
    window: Window,
    connectionId: string,
    failure: Error,
  ): void {
    if (state.failure) return;
    state.failure = failure;
    state.enabled = false;
    state.acknowledgements.delete(window);
    this.#settleWaiters(state, window, lease, false, state.failure);
    state.onFailure(state.owner, connectionId, state.failure);
  }

  #windowWaiter(
    state: LeaseState,
    lease: AppMenuGenerationLease,
    window: Window,
    phase: "staged" | "published",
  ): WindowWaiter | undefined {
    return [...(state.waiters.get(window) ?? [])].find((waiter) =>
      waiter.lease === lease && waiter.phase === phase && !waiter.settled
    );
  }

  #settleWaiters(
    state: LeaseState,
    window: Window,
    lease: AppMenuGenerationLease,
    ok: boolean,
    error?: Error,
  ): void {
    for (const waiter of state.waiters.get(window) ?? []) {
      if (waiter.lease !== lease) continue;
      if (ok) waiter.resolve();
      else waiter.reject(error ?? new Error("app-menu render failed"));
    }
  }

  #rejectWaiters(state: LeaseState, error: Error): void {
    for (const [window, waiters] of state.waiters) {
      for (const waiter of waiters) waiter.reject(error);
      state.waiters.delete(window);
    }
  }

  #notifyReady(entry: AdapterEntry): void {
    if (entry.disposed || this.#connections.get(entry.window) !== entry) return;
    const lease = this.#leaseForWindow(entry.window);
    if (lease) this.#deliver(entry, lease);
  }

  #leaseForWindow(window: Window): AppMenuGenerationLease | null {
    const staged = this.#staged;
    if (staged && this.#leases.get(staged)?.windows.has(window)) return staged;
    return this.#active ?? staged;
  }

  #reportFailure(
    entry: AdapterEntry,
    owner: AppMenuOwner,
    error: Error,
  ): boolean {
    if (
      entry.disposed || entry.window.closed ||
      this.#connections.get(entry.window) !== entry
    ) return false;
    const lease = [this.#staged, this.#active].find((candidate) => {
      if (!candidate) return false;
      const state = this.#leases.get(candidate);
      if (
        !state || state.revoked || state.failure ||
        !state.windows.has(entry.window) ||
        owner.featureId !== state.owner.featureId ||
        owner.hostEpoch !== state.owner.hostEpoch ||
        owner.generation !== state.owner.generation
      ) return false;
      return state.acknowledgements.get(entry.window) === entry.connection ||
        (entry.deliveryLease === candidate &&
          sameOwner(entry.deliveryOwner, owner)) ||
        (entry.deliveredLease === candidate &&
          sameOwner(entry.deliveredOwner, owner));
    });
    if (!lease) return false;
    const state = this.#leases.get(lease);
    if (!state) return false;
    this.#failDelivery(state, entry, lease, error);
    return true;
  }

  #invoke(entry: AdapterEntry, owner: AppMenuOwner): boolean {
    const lease = this.#active;
    if (
      !lease || entry.disposed || this.#connections.get(entry.window) !== entry
    ) {
      return false;
    }
    const state = this.#leases.get(lease);
    if (
      !state || state.revoked || !state.published || !state.enabled ||
      !state.isCurrent() ||
      state.acknowledgements.get(entry.window) !== entry.connection ||
      entry.window.closed || !isEligibleFeatureWindow(entry.window) ||
      state.owner.featureId !== HUB_PANEL_MENU_FEATURE_ID ||
      owner.featureId !== state.owner.featureId ||
      owner.hostEpoch !== state.owner.hostEpoch ||
      owner.generation !== state.owner.generation ||
      state.descriptor?.command !== "open-hub"
    ) return false;
    const browserWindow = entry.window as BrowserWindow;
    const tab = browserWindow.gBrowser.addTab(HUB_PANEL_MENU_URL, {
      relatedToCurrent: true,
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
    browserWindow.gBrowser.selectedTab = tab;
    browserWindow.PanelUI.hide();
    return true;
  }

  #disconnect(entry: AdapterEntry): void {
    if (entry.disposed) return;
    entry.disposed = true;
    const current = this.#connections.get(entry.window) === entry;
    const lease = entry.deliveredLease ?? this.#leaseForWindow(entry.window);
    const state = lease ? this.#leases.get(lease) : null;
    const errors: Error[] = [];
    if (current) this.#connections.delete(entry.window);
    this.#entries.delete(entry);
    if (state) {
      state.acknowledgements.delete(entry.window);
      if (state.descriptor) {
        try {
          entry.adapter.remove(this.#request(state, false), "remove");
        } catch (error) {
          errors.push(asError(error));
        }
      }
      if (errors.length && !state.failure) {
        try {
          this.#recordFailure(
            state,
            lease!,
            entry.window,
            entry.connection.id,
            errors[0],
          );
        } catch (error) {
          errors.push(asError(error));
        }
      } else if (
        current && this.#active === lease && state.published &&
        !state.revoked
      ) {
        this.#startLateReadinessWait(lease!, entry.window, entry.connection.id);
      }
    }
    if (errors.length) {
      throw new AggregateError(errors, "app-menu adapter disconnect failed");
    }
  }

  #releaseHost(claim: AppMenuHostClaim): void {
    if (this.#claims.get(claim.featureId) !== claim) return;
    const errors: Error[] = [];
    for (const [lease, state] of this.#leases) {
      if (state.owner.hostEpoch === claim.hostEpoch) {
        errors.push(...this.revoke(lease));
      }
    }
    this.#claims.delete(claim.featureId);
    if (errors.length) {
      throw new AggregateError(errors, "app-menu host release failed");
    }
  }
}

let sharedRegistry: AppMenuRegistry | null = null;

export function getSharedAppMenuRegistry(): AppMenuRegistry {
  if (!sharedRegistry) sharedRegistry = new AppMenuRegistry();
  return sharedRegistry;
}
