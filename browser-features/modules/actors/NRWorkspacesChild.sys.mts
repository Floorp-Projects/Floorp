const { setTimeout, clearTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

import { HUB_DEV_PAGES } from "#libs/shared/dev-ports/defines.ts";
import { launchDevPorts } from "../common/dev-port-offset.ts";

/** Launch-local dev port resolver (FDH Gate 2 task 2-B): resolves the
 * table names to the ports this launch actually uses (offset-aware). */
const launchDevPort = launchDevPorts();

const WORKSPACES_INIT_TOPIC = "floorp.workspaces.initialize";
const REQUEST_TIMEOUT_MS = 30_000;
const RESTORE_TIMEOUT_MS = 10 * 60_000;

type InitializeResult = {
  success: boolean;
  error?: string | null;
};

interface BackupBridgeResult {
  requestId: string;
  success: boolean;
  error?: string;
  data?: unknown;
}

interface PendingBackupRequest {
  resolve: (result: BackupBridgeResult) => void;
  timer: ReturnType<typeof setTimeout>;
}

export class NRWorkspacesChild extends JSWindowActorChild {
  /**
   * The page went away (closed or navigated): settle every pending backup
   * request and drop its timer, so a 10-minute restore timeout does not keep
   * the actor alive.
   */
  didDestroy() {
    for (const [requestId, pending] of this.#pendingBackupRequests) {
      clearTimeout(pending.timer);
      pending.resolve({ requestId, success: false, error: "actor-destroyed" });
    }
    this.#pendingBackupRequests.clear();
    this.#pendingInitializationResolver?.({
      success: false,
      error: "actor-destroyed",
    });
    this.#pendingInitializationResolver = null;
    this.#pendingInitializationPromise = null;
  }

  #pendingInitializationPromise: Promise<InitializeResult> | null = null;
  #pendingInitializationResolver: ((result: InitializeResult) => void) | null =
    null;
  #pendingBackupRequests = new Map<string, PendingBackupRequest>();

  actorCreated() {
    const window = this.contentWindow;
    const href = window?.location.href ?? "";
    const isTrustedDevPort = launchDevPort.isTrustedPort(
      window?.location.port,
      HUB_DEV_PAGES,
    );
    if (
      isTrustedDevPort ||
      href.startsWith("chrome://noraneko-settings") ||
      href.startsWith("about:")
    ) {
      Cu.exportFunction(this.initializeWorkspaces.bind(this), window, {
        defineAs: "NRInitializeWorkspaces",
      });
    }
    // Backup bridge: settings page only (not every about: page).
    if (
      isTrustedDevPort ||
      href.startsWith("chrome://noraneko-settings") ||
      href.startsWith("about:hub")
    ) {
      this.exportBackupBridge(window);
    }
  }

  initializeWorkspaces(reason?: string | null): Promise<InitializeResult> {
    if (this.#pendingInitializationPromise) {
      console.warn(
        "NRWorkspacesChild: initialization already pending, reusing promise",
      );
      return this.#pendingInitializationPromise;
    }

    this.#pendingInitializationPromise = new Promise<InitializeResult>(
      (resolve) => {
        this.#pendingInitializationResolver = resolve;
        this.sendAsyncMessage("NRWorkspaces:Initialize", {
          topic: WORKSPACES_INIT_TOPIC,
          reason: typeof reason === "string" ? reason : null,
        });
      },
    );
    return this.#pendingInitializationPromise;
  }

  receiveMessage(message: ReceiveMessageArgument) {
    switch (message.name) {
      case "NRWorkspaces:InitializeResult": {
        this.#pendingInitializationResolver?.(
          message.data && typeof message.data.success === "boolean"
            ? message.data
            : { success: false, error: "unknown" },
        );
        this.#pendingInitializationPromise = null;
        this.#pendingInitializationResolver = null;
        break;
      }
      case "NRWorkspaces:BackupResult": {
        const data = message.data as BackupBridgeResult | undefined;
        if (!data || typeof data.requestId !== "string") {
          break;
        }
        const pending = this.#pendingBackupRequests.get(data.requestId);
        if (!pending) {
          break;
        }
        this.#pendingBackupRequests.delete(data.requestId);
        clearTimeout(pending.timer);
        pending.resolve(data);
        break;
      }
    }
  }

  backupNow(): Promise<BackupBridgeResult> {
    return this.sendBackupRequest("NRWorkspaces:BackupNow");
  }

  listBackups(): Promise<BackupBridgeResult> {
    return this.sendBackupRequest("NRWorkspaces:ListBackups");
  }

  restoreBackup(fileName: string): Promise<BackupBridgeResult> {
    return this.sendBackupRequest("NRWorkspaces:RestoreBackup", { fileName });
  }

  deleteBackup(fileName: string): Promise<BackupBridgeResult> {
    return this.sendBackupRequest("NRWorkspaces:DeleteBackup", { fileName });
  }

  deleteAllBackups(): Promise<BackupBridgeResult> {
    return this.sendBackupRequest("NRWorkspaces:DeleteAllBackups");
  }

  /** Defines the backup functions on `target` (a content window). */
  exportBackupBridge(target: object): void {
    const exports: Record<string, (...args: string[]) => unknown> = {
      NRWorkspacesBackupNow: () => this.toContent(target, this.backupNow()),
      NRWorkspacesListBackups: () => this.toContent(target, this.listBackups()),
      NRWorkspacesRestoreBackup: (fileName) =>
        this.toContent(target, this.restoreBackup(fileName)),
      NRWorkspacesDeleteBackup: (fileName) =>
        this.toContent(target, this.deleteBackup(fileName)),
      NRWorkspacesDeleteAllBackups: () =>
        this.toContent(target, this.deleteAllBackups()),
    };
    for (const [name, fn] of Object.entries(exports)) {
      Cu.exportFunction(fn, target, { defineAs: name });
    }
  }

  /**
   * Re-creates the result as a Promise of `target` holding a clone in its
   * compartment: a chrome Promise or object is opaque to the page.
   */
  private toContent(
    target: object,
    result: Promise<BackupBridgeResult>,
  ): Promise<unknown> {
    const TargetPromise = (target as { Promise: PromiseConstructor }).Promise;
    return new TargetPromise((resolve: (value: unknown) => void) => {
      result.then(
        (value) => resolve(Cu.cloneInto(value, target)),
        (error: unknown) =>
          resolve(
            Cu.cloneInto(
              { requestId: "", success: false, error: String(error) },
              target,
            ),
          ),
      );
    });
  }

  private sendBackupRequest(
    name: string,
    data: Record<string, unknown> = {},
  ): Promise<BackupBridgeResult> {
    const requestId = `${Date.now()}-${Math.random().toString(36).slice(2)}`;
    return new Promise<BackupBridgeResult>((resolve) => {
      const timer = setTimeout(
        () => {
          this.#pendingBackupRequests.delete(requestId);
          resolve({ requestId, success: false, error: "timeout" });
        },
        name === "NRWorkspaces:RestoreBackup"
          ? RESTORE_TIMEOUT_MS
          : REQUEST_TIMEOUT_MS,
      );
      this.#pendingBackupRequests.set(requestId, { resolve, timer });
      this.sendAsyncMessage(name, { ...data, requestId });
    });
  }

  handleEvent(_event: Event): void {
    // No-op
  }
}
