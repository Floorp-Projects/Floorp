// SPDX-License-Identifier: MPL-2.0

export type Cleanup = () => void | Promise<void>;

export interface DisposeReport {
  errors: string[];
  timedOut: boolean;
}

type Resource = {
  id: number;
  cleanup: Cleanup | null;
  owner: ResourceScope | null;
  disposing: boolean;
  disposal: Promise<void> | null;
  dispose: () => Promise<void>;
};

let activeDeadlineTimers = 0;

function startDeadline(callback: () => void, timeoutMs: number): {
  cancel(): void;
} {
  activeDeadlineTimers += 1;
  let active = true;
  const timer = setTimeout(() => {
    if (!active) return;
    active = false;
    activeDeadlineTimers -= 1;
    callback();
  }, timeoutMs);
  return {
    cancel() {
      if (!active) return;
      active = false;
      activeDeadlineTimers -= 1;
      clearTimeout(timer);
    },
  };
}

export function activeM0DeadlineTimersForTests(): number {
  return activeDeadlineTimers;
}

export class ResourceScope {
  readonly #resources = new Map<number, Resource>();
  readonly #children = new Map<string, ResourceScope>();
  readonly #disposals = new Set<Promise<void>>();
  readonly #errors: string[] = [];
  #nextId = 0;
  #revoked = false;
  #disposing: Promise<DisposeReport> | null = null;

  get revoked(): boolean {
    return this.#revoked;
  }

  get resourceCount(): number {
    return this.#resources.size + [...this.#children.values()].reduce(
      (total, child) => total + child.resourceCount,
      0,
    );
  }

  get childCount(): number {
    return this.#children.size + [...this.#children.values()].reduce(
      (total, child) => total + child.childCount,
      0,
    );
  }

  child(windowId: string): ResourceScope {
    if (this.#revoked) throw new Error("resource scope revoked");
    const existing = this.#children.get(windowId);
    if (existing) return existing;
    const child = new ResourceScope();
    this.#children.set(windowId, child);
    return child;
  }

  register(cleanup: Cleanup): { dispose(): Promise<void> } {
    if (this.#revoked) throw new Error("resource scope revoked");
    const id = ++this.#nextId;
    const resource: Resource = {
      id,
      cleanup,
      owner: this,
      disposing: false,
      disposal: null,
      dispose: () => {
        if (resource.disposing || !resource.cleanup) return Promise.resolve();
        resource.disposing = true;
        const cleanupNow = resource.cleanup;
        const owner = resource.owner;
        resource.cleanup = null;
        resource.owner = null;
        const disposal = Promise.resolve().then(cleanupNow).catch((error) => {
          if (owner) owner.#errors.push(String(error));
          throw error;
        }).finally(() => {
          if (owner) {
            owner.#resources.delete(id);
            owner.#disposals.delete(disposal);
          }
          resource.disposal = null;
        });
        resource.disposal = disposal;
        if (owner) owner.#disposals.add(disposal);
        void disposal.catch(() => {});
        return disposal;
      },
    };
    this.#resources.set(id, resource);
    return Object.freeze({ dispose: () => resource.dispose() });
  }

  revoke(): void {
    this.#revoked = true;
    for (const child of this.#children.values()) child.revoke();
  }

  disposeAll(timeoutMs: number): Promise<DisposeReport> {
    if (this.#disposing) return this.#disposing;
    this.revoke();
    this.#disposing = (async () => {
      const children = [...this.#children.values()].map((child) =>
        child.disposeAll(timeoutMs)
      );
      for (const resource of this.#resources.values()) resource.dispose();
      const resources = [...this.#disposals];
      const work = Promise.all([
        Promise.allSettled(children),
        Promise.allSettled(resources),
      ]);
      let timer: { cancel(): void } | undefined;
      const deadline = new Promise<"timeout">((resolve) => {
        timer = startDeadline(() => resolve("timeout"), timeoutMs);
      });
      let timedOut = false;
      const errors: string[] = [];
      try {
        const result = await Promise.race([
          work.then((value) => ({ kind: "complete" as const, value })),
          deadline.then(() => ({ kind: "timeout" as const })),
        ]);
        if (result.kind === "timeout") {
          timedOut = true;
          errors.push("resource cleanup deadline");
          return { errors: [...this.#errors, ...errors], timedOut };
        }
        const [childResults, resourceResults] = result.value;
        for (const result of childResults) {
          if (result.status === "rejected") errors.push(String(result.reason));
          else {
            errors.push(...result.value.errors);
            if (result.value.timedOut) timedOut = true;
          }
        }
        for (const result of resourceResults) {
          if (result.status === "rejected") {
            const error = String(result.reason);
            if (!this.#errors.includes(error)) errors.push(error);
          }
        }
      } finally {
        timer?.cancel();
        this.#resources.clear();
        this.#children.clear();
      }
      return { errors: [...this.#errors, ...errors], timedOut };
    })();
    return this.#disposing;
  }
}

export interface M0Lifecycle {
  activate(
    contextJson: string,
    request: (json: string) => string,
  ): void | Promise<void>;
  attachWindow(
    contextJson: string,
    request: (json: string) => string,
  ): void | Promise<void>;
  health(): string;
  deactivate(reason: string): void | Promise<void>;
}

function isLifecycle(value: unknown): value is M0Lifecycle {
  if (typeof value !== "object" || value === null) return false;
  const candidate = value as Record<string, unknown>;
  return typeof candidate.activate === "function" &&
    typeof candidate.attachWindow === "function" &&
    typeof candidate.health === "function" &&
    typeof candidate.deactivate === "function";
}

function bounded<T>(promise: Promise<T>, timeoutMs: number): Promise<T> {
  let timer: { cancel(): void } | undefined;
  const timeout = new Promise<T>((_, reject) => {
    timer = startDeadline(
      () => reject(new Error("lifecycle deadline")),
      timeoutMs,
    );
  });
  return Promise.race([promise, timeout]).finally(() => {
    timer?.cancel();
  });
}

export class M0ModuleHost {
  readonly scope = new ResourceScope();
  #sandbox: Sandbox | null = null;
  #lifecycle: M0Lifecycle | null = null;
  #stopPromise: Promise<DisposeReport> | null = null;
  #digest: string | null;
  #source: string | null;
  #request:
    | ((
      epoch: string,
      generation: number,
      json: string,
    ) => string)
    | null;

  constructor(
    readonly epoch: string,
    readonly generation: number,
    digest: string,
    source: string,
    request: (
      epoch: string,
      generation: number,
      json: string,
    ) => string,
  ) {
    this.#digest = digest;
    this.#source = source;
    this.#request = request;
  }

  get health(): string {
    if (!this.#lifecycle) throw new Error("generation not loaded");
    return this.#lifecycle.health();
  }

  get lifecycleForTests(): M0Lifecycle | null {
    return this.#lifecycle;
  }

  get scopeDiagnosticsForTests(): { resources: number; childScopes: number } {
    return {
      resources: this.scope.resourceCount,
      childScopes: this.scope.childCount,
    };
  }

  async start(windows: readonly string[]): Promise<void> {
    const digest = this.#digest;
    const source = this.#source;
    const broker = this.#request;
    if (digest === null || source === null || broker === null) {
      throw new Error("module host already stopped");
    }
    const sandbox = Components.utils.Sandbox(
      Services.scriptSecurityManager.getSystemPrincipal(),
      {
        sandboxName: `floorp-hotfix-m0-${digest}`,
        wantComponents: false,
        freshCompartment: true,
      },
    );
    this.#sandbox = sandbox;
    try {
      Components.utils.evalInSandbox(
        source,
        sandbox,
        "latest",
        `floorp-hotfix:${digest}`,
        1,
      );
      const exports =
        (sandbox as unknown as Record<string, unknown>).__floorpM0;
      if (!isLifecycle(exports)) {
        throw new Error("invalid fixture lifecycle ABI");
      }
      this.#lifecycle = exports;
      const request = (json: string): string =>
        broker(this.epoch, this.generation, json);
      await bounded(
        Promise.resolve(exports.activate(
          JSON.stringify({
            epoch: this.epoch,
            generation: this.generation,
          }),
          request,
        )),
        2_000,
      );
      for (const windowId of windows) {
        await bounded(
          Promise.resolve(exports.attachWindow(
            JSON.stringify({
              epoch: this.epoch,
              generation: this.generation,
              windowId,
            }),
            request,
          )),
          2_000,
        );
      }
      if (exports.health().length === 0) {
        throw new Error("fixture health failed");
      }
    } catch (error) {
      const failure = new Error(String(error));
      await this.stop("activation-failed");
      throw failure;
    }
  }

  stop(reason: string): Promise<DisposeReport> {
    if (this.#stopPromise) return this.#stopPromise;
    this.scope.revoke();
    this.#stopPromise = this.#stopOnce(reason);
    return this.#stopPromise;
  }

  async #stopOnce(reason: string): Promise<DisposeReport> {
    const errors: string[] = [];
    let timedOut = false;
    try {
      const lifecycle = this.#lifecycle;
      const hook = lifecycle
        ? bounded(
          Promise.resolve().then(() => lifecycle.deactivate(reason)),
          2_000,
        )
        : Promise.resolve();
      const cleanup = this.scope.disposeAll(2_000);
      const [hookResult, cleanupResult] = await Promise.allSettled([
        hook,
        cleanup,
      ]);
      if (hookResult.status === "rejected") {
        errors.push(String(hookResult.reason));
      }
      if (cleanupResult.status === "rejected") {
        errors.push(String(cleanupResult.reason));
      } else {
        errors.push(...cleanupResult.value.errors);
        timedOut = cleanupResult.value.timedOut;
      }
    } finally {
      const sandbox = this.#sandbox;
      this.#sandbox = null;
      this.#lifecycle = null;
      this.#digest = null;
      this.#source = null;
      this.#request = null;
      if (sandbox) {
        try {
          Components.utils.nukeSandbox(sandbox);
        } catch (error) {
          errors.push(String(error));
        }
      }
    }
    return { errors, timedOut };
  }
}
