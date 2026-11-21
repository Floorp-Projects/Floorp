// SPDX-License-Identifier: MPL-2.0

const { setTimeout, clearTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

export type Cleanup = () => void | Promise<void>;

export interface DisposeReport {
  errors: string[];
  timedOut: boolean;
  taskCount?: number;
  pendingTasks?: number;
  observedTaskRejections?: number;
  taskFailures?: number;
  pendingResources?: number;
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

class CleanupDeadline {
  readonly promise: Promise<void>;
  #timer!: { cancel(): void };
  #expired = false;

  constructor(timeoutMs: number) {
    this.promise = new Promise((resolve) => {
      this.#timer = startDeadline(() => {
        this.#expired = true;
        resolve();
      }, timeoutMs);
    });
  }

  get expired(): boolean {
    return this.#expired;
  }

  cancel(): void {
    this.#timer.cancel();
  }
}

function boundedByDeadline<T>(
  promise: Promise<T>,
  deadline: CleanupDeadline,
  message: string,
): Promise<T> {
  return Promise.race([
    promise,
    deadline.promise.then(() => Promise.reject(new Error(message))),
  ]);
}

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
  readonly #tasks = new Set<Promise<void>>();
  readonly #childDisposals = new Map<Promise<DisposeReport>, ResourceScope>();
  readonly #errors: string[] = [];
  readonly #controller = new AbortController();
  readonly #abortReason = new Error("resource scope revoked");
  readonly #parent: ResourceScope | null;
  #nextId = 0;
  #revoked = false;
  #disposing: Promise<DisposeReport> | null = null;
  #timedOut = false;
  #taskCount = 0;
  #pendingTaskCount = 0;
  #observedTaskRejections = 0;
  #taskFailures = 0;

  constructor(parent: ResourceScope | null = null) {
    this.#parent = parent;
  }

  get revoked(): boolean {
    return this.#revoked;
  }

  get signal(): AbortSignal {
    return this.#controller.signal;
  }

  get taskCount(): number {
    return this.#taskCount;
  }

  get pendingTaskCount(): number {
    return this.#pendingTaskCount;
  }

  get observedTaskRejections(): number {
    return this.#observedTaskRejections;
  }

  get taskFailures(): number {
    return this.#taskFailures;
  }

  get resourceCount(): number {
    const tracked = new Set(
      [...this.#resources.values()].flatMap((resource) =>
        resource.disposal ? [resource.disposal] : []
      ),
    );
    return this.#resources.size +
      [...this.#disposals].filter((disposal) => !tracked.has(disposal)).length +
      [...this.#children.values()].reduce(
        (total, child) => total + child.resourceCount,
        0,
      ) +
      [...this.#childDisposals.values()].reduce(
        (total, child) => total + child.resourceCount,
        0,
      );
  }

  get childCount(): number {
    return this.#children.size +
      [...this.#children.values()].reduce(
        (total, child) => total + child.childCount,
        0,
      ) +
      this.#childDisposals.size +
      [...this.#childDisposals.values()].reduce(
        (total, child) => total + child.childCount,
        0,
      );
  }

  child(windowId: string): ResourceScope {
    if (this.#revoked) throw new Error("resource scope revoked");
    const existing = this.#children.get(windowId);
    if (existing) return existing;
    const child = new ResourceScope(this);
    this.#children.set(windowId, child);
    return child;
  }

  disposeChild(windowId: string, timeoutMs = 2_000): Promise<DisposeReport> {
    const child = this.#children.get(windowId);
    if (!child) return Promise.resolve({ errors: [], timedOut: false });
    child.revoke();
    this.#children.delete(windowId);
    const disposal = child.disposeAll(timeoutMs);
    return this.#trackChildDisposal(child, disposal);
  }

  #recordError(error: unknown): void {
    const message = String(error).slice(0, 256);
    if (!this.#errors.includes(message) && this.#errors.length < 32) {
      this.#errors.push(message);
    }
  }

  #taskStarted(): void {
    this.#taskCount += 1;
    this.#pendingTaskCount += 1;
    if (this.#parent) this.#parent.#descendantTaskStarted();
  }

  #descendantTaskStarted(): void {
    this.#taskCount += 1;
    this.#pendingTaskCount += 1;
    if (this.#parent) this.#parent.#descendantTaskStarted();
  }

  #taskSettled(): void {
    this.#pendingTaskCount -= 1;
    if (this.#parent) this.#parent.#descendantTaskSettled();
  }

  #descendantTaskSettled(): void {
    this.#pendingTaskCount -= 1;
    if (this.#parent) this.#parent.#descendantTaskSettled();
  }

  #recordTaskRejection(error: unknown): void {
    this.#observedTaskRejections += 1;
    const expected = this.#revoked && error === this.signal.reason;
    if (this.#parent) {
      this.#parent.#recordDescendantTaskRejection(error, expected);
    }
    if (expected) return;
    this.#taskFailures += 1;
    this.#recordError(`managed task rejected: ${String(error)}`);
  }

  #recordDescendantTaskRejection(error: unknown, expected: boolean): void {
    this.#observedTaskRejections += 1;
    if (this.#parent) {
      this.#parent.#recordDescendantTaskRejection(error, expected);
    }
    if (!expected) {
      this.#recordError(`managed task rejected: ${String(error)}`);
      this.#taskFailures += 1;
    }
  }

  runTask<T>(factory: (signal: AbortSignal) => T | PromiseLike<T>): Promise<T> {
    if (this.#revoked) throw new Error("resource scope revoked");
    this.#taskStarted();
    let settle!: () => void;
    const settled = new Promise<void>((resolve) => settle = resolve);
    this.#tasks.add(settled);
    const finish = (): void => {
      if (!this.#tasks.delete(settled)) return;
      this.#taskSettled();
      settle();
    };
    let work: T | PromiseLike<T>;
    try {
      work = factory(this.signal);
    } catch (error) {
      work = Promise.reject(error);
    }
    const tracked = Promise.resolve(work).then(
      (value) => {
        finish();
        return value;
      },
      (error) => {
        this.#recordTaskRejection(error);
        finish();
        throw error;
      },
    );
    void tracked.catch(() => {});
    return tracked;
  }

  #recordReport(report: DisposeReport): void {
    for (const error of report.errors) this.#recordError(error);
    if (report.timedOut) this.#timedOut = true;
  }

  #recordChildEvidence(child: ResourceScope): void {
    for (const error of child.#errors) this.#recordError(error);
    if (child.#timedOut) this.#timedOut = true;
  }

  #pendingWorkCount(): number {
    return this.#disposals.size + this.#tasks.size + this.#childDisposals.size +
      [...this.#children.values()].reduce(
        (total, child) => total + child.#pendingWorkCount(),
        0,
      ) +
      [...this.#childDisposals.values()].reduce(
        (total, child) => total + child.#pendingWorkCount(),
        0,
      );
  }

  #pendingWorkSettled(): Promise<void> {
    const pending: Promise<unknown>[] = [
      ...[...this.#disposals].map((disposal) => disposal.catch(() => {})),
      ...this.#tasks,
    ];
    for (const [disposal, child] of this.#childDisposals) {
      pending.push(disposal.catch(() => {}), child.#pendingWorkSettled());
    }
    for (const child of this.#children.values()) {
      pending.push(child.#pendingWorkSettled());
    }
    return Promise.all(pending).then(() => {});
  }

  #trackChildDisposal(
    child: ResourceScope,
    disposal: Promise<DisposeReport>,
  ): Promise<DisposeReport> {
    this.#childDisposals.set(disposal, child);
    const finish = (): void => {
      this.#recordChildEvidence(child);
      if (child.#pendingWorkCount() === 0) {
        if (this.#childDisposals.get(disposal) === child) {
          this.#childDisposals.delete(disposal);
        }
        return;
      }
      void child.#pendingWorkSettled().then(() => {
        this.#recordChildEvidence(child);
        if (this.#childDisposals.get(disposal) === child) {
          this.#childDisposals.delete(disposal);
        }
      });
    };
    void disposal.then(
      (report) => {
        this.#recordReport(report);
        finish();
      },
      (error) => {
        this.#recordError(error);
        finish();
      },
    );
    return disposal;
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
          if (owner) owner.#recordError(error);
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
    if (this.#revoked) return;
    const tree: Array<{
      scope: ResourceScope;
      controller: AbortController;
      reason: Error;
    }> = [{
      scope: this,
      controller: this.#controller,
      reason: this.#abortReason,
    }];
    for (let index = 0; index < tree.length; index += 1) {
      const entry = tree[index];
      if (entry) {
        for (const child of entry.scope.#children.values()) {
          tree.push({
            scope: child,
            controller: child.#controller,
            reason: child.#abortReason,
          });
        }
      }
    }
    for (const entry of tree) entry.scope.#revoked = true;
    for (const entry of tree) entry.controller.abort(entry.reason);
  }

  disposeAll(
    timeoutMs: number,
    sharedDeadline?: CleanupDeadline,
  ): Promise<DisposeReport> {
    if (this.#disposing) return this.#disposing;
    const deadline = sharedDeadline ?? new CleanupDeadline(timeoutMs);
    const ownsDeadline = !sharedDeadline;
    let resolve!: (report: DisposeReport) => void;
    this.#disposing = new Promise((done) => resolve = done);
    if (ownsDeadline) {
      void this.#disposing.then(() => deadline.cancel());
    }
    try {
      this.revoke();
    } catch (error) {
      this.#recordError(error);
    }
    void this.#drain(deadline).then(resolve, (error) => {
      this.#recordError(error);
      resolve(this.#report(deadline.expired, this.pendingTaskCount));
    });
    return this.#disposing;
  }

  #report(timedOut: boolean, pendingTasks: number): DisposeReport {
    return {
      errors: [...this.#errors],
      timedOut: timedOut || this.#timedOut,
      taskCount: this.#taskCount,
      pendingTasks,
      observedTaskRejections: this.#observedTaskRejections,
      taskFailures: this.#taskFailures,
      pendingResources: this.resourceCount,
    };
  }

  async #drain(deadline: CleanupDeadline): Promise<DisposeReport> {
    const pendingChildren = [...this.#childDisposals.keys()];
    const pendingChildScopes = [...this.#childDisposals.values()];
    const childScopes = [...this.#children.values()];
    this.#children.clear();
    const children = childScopes.map((child) =>
      this.#trackChildDisposal(child, child.disposeAll(2_000, deadline))
    );
    children.push(...pendingChildren);
    for (const resource of this.#resources.values()) resource.dispose();
    const resources = [...this.#disposals];
    const work = Promise.all([
      Promise.allSettled(children),
      Promise.allSettled(resources),
      Promise.allSettled([...this.#tasks]),
      Promise.all(
        pendingChildScopes.map((child) => child.#pendingWorkSettled()),
      ),
      Promise.all(childScopes.map((child) => child.#pendingWorkSettled())),
    ]);
    const result = await Promise.race([
      work.then((value) => ({ kind: "complete" as const, value })),
      deadline.promise.then(() => ({ kind: "timeout" as const })),
    ]);
    if (result.kind === "timeout") {
      this.#timedOut = true;
      const pendingTasks = this.pendingTaskCount;
      this.#recordError(
        `resource cleanup deadline (pendingTasks=${pendingTasks}, pendingResources=${this.resourceCount})`,
      );
      return this.#report(true, pendingTasks);
    }
    const [childResults, resourceResults] = result.value;
    for (const childResult of childResults) {
      if (childResult.status === "rejected") {
        this.#recordError(childResult.reason);
      } else this.#recordReport(childResult.value);
    }
    for (const resourceResult of resourceResults) {
      if (resourceResult.status === "rejected") {
        this.#recordError(resourceResult.reason);
      }
    }
    this.#resources.clear();
    this.#children.clear();
    return this.#report(false, this.pendingTaskCount);
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

  get scopeDiagnosticsForTests(): {
    resources: number;
    childScopes: number;
    tasks: number;
    pendingTasks: number;
    observedTaskRejections: number;
    taskFailures: number;
  } {
    return {
      resources: this.scope.resourceCount,
      childScopes: this.scope.childCount,
      tasks: this.scope.taskCount,
      pendingTasks: this.scope.pendingTaskCount,
      observedTaskRejections: this.scope.observedTaskRejections,
      taskFailures: this.scope.taskFailures,
    };
  }

  async start(windows: readonly string[] = []): Promise<void> {
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
      const request = (json: string): string => {
        if (this.scope.revoked || this.#stopPromise) {
          throw new Error("resource scope revoked");
        }
        return broker(this.epoch, this.generation, json);
      };
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
      if (exports.health().length === 0) {
        throw new Error("fixture health failed");
      }
      for (const windowId of windows) await this.attachWindow(windowId);
    } catch (error) {
      const failure = new Error(String(error));
      await this.stop("activation-failed");
      throw failure;
    }
  }

  async attachWindow(windowId: string): Promise<boolean> {
    const lifecycle = this.#lifecycle;
    const broker = this.#request;
    if (!lifecycle || !broker || this.#stopPromise || this.scope.revoked) {
      throw new Error("generation is not active");
    }
    const child = this.scope.child(windowId);
    try {
      await bounded(
        Promise.resolve(lifecycle.attachWindow(
          JSON.stringify({
            epoch: this.epoch,
            generation: this.generation,
            windowId,
          }),
          (json) => {
            if (this.scope.revoked || this.#stopPromise) {
              throw new Error("resource scope revoked");
            }
            return broker(this.epoch, this.generation, json);
          },
        )),
        2_000,
      );
      return !child.revoked && !this.scope.revoked && !this.#stopPromise;
    } catch (error) {
      if (child.revoked || this.scope.revoked || this.#stopPromise) {
        return false;
      }
      throw error;
    }
  }

  detachWindow(windowId: string): Promise<DisposeReport> {
    return this.scope.disposeChild(windowId);
  }

  stop(reason: string): Promise<DisposeReport> {
    if (this.#stopPromise) return this.#stopPromise;
    let resolve!: (report: DisposeReport) => void;
    const shared = new Promise<DisposeReport>((done) => resolve = done);
    this.#stopPromise = shared;
    const deadline = new CleanupDeadline(2_000);
    const errors: string[] = [];
    try {
      this.scope.revoke();
    } catch (error) {
      errors.push(String(error));
    }
    void this.#stopOnce(reason, deadline, errors).then(
      (report) => {
        deadline.cancel();
        resolve(report);
      },
      (error) => {
        deadline.cancel();
        resolve({
          errors: [...errors, String(error)],
          timedOut: deadline.expired,
          taskCount: this.scope.taskCount,
          pendingTasks: this.scope.pendingTaskCount,
          observedTaskRejections: this.scope.observedTaskRejections,
          taskFailures: this.scope.taskFailures,
          pendingResources: this.scope.resourceCount,
        });
      },
    );
    return shared;
  }

  async #stopOnce(
    reason: string,
    deadline: CleanupDeadline,
    initialErrors: string[],
  ): Promise<DisposeReport> {
    const errors = [...initialErrors];
    let cleanupReport: DisposeReport | null = null;
    try {
      const lifecycle = this.#lifecycle;
      const hook = lifecycle
        ? boundedByDeadline(
          Promise.resolve().then(() => lifecycle.deactivate(reason)),
          deadline,
          "lifecycle deadline",
        )
        : Promise.resolve();
      const cleanup = this.scope.disposeAll(2_000, deadline);
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
        cleanupReport = cleanupResult.value;
        errors.push(...cleanupResult.value.errors);
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
    return {
      errors,
      timedOut: deadline.expired || cleanupReport?.timedOut === true,
      taskCount: this.scope.taskCount,
      pendingTasks: cleanupReport?.pendingTasks ?? this.scope.pendingTaskCount,
      observedTaskRejections: this.scope.observedTaskRejections,
      taskFailures: this.scope.taskFailures,
      pendingResources: cleanupReport?.pendingResources ??
        this.scope.resourceCount,
    };
  }
}
