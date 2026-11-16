import { HUB_DEV_PAGES } from "#libs/shared/dev-ports/defines.ts";
import { launchDevPorts } from "../common/dev-port-offset.ts";

/** Launch-local dev port resolver (FDH Gate 2 task 2-B): resolves the
 * table names to the ports this launch actually uses (offset-aware). */
const launchDevPort = launchDevPorts();

const WORKSPACES_INIT_TOPIC = "floorp.workspaces.initialize";

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
  timer: number;
}

export class NRWorkspacesChild extends JSWindowActorChild {
  #pendingInitializationPromise: Promise<InitializeResult> | null = null;
  #pendingInitializationResolver: ((result: InitializeResult) => void) | null =
    null;
  #pendingBackupRequests = new Map<string, PendingBackupRequest>();

  actorCreated() {
    const window = this.contentWindow;
    if (
      launchDevPort.isTrustedPort(window?.location.port, HUB_DEV_PAGES) ||
      window?.location.href.startsWith("chrome://noraneko-settings") ||
      window?.location.href.startsWith("about:")
    ) {
      Cu.exportFunction(this.initializeWorkspaces.bind(this), window, {
        defineAs: "NRInitializeWorkspaces",
      });
      Cu.exportFunction(this.backupNow.bind(this), window, {
        defineAs: "NRWorkspacesBackupNow",
      });
      Cu.exportFunction(this.listBackups.bind(this), window, {
        defineAs: "NRWorkspacesListBackups",
      });
      Cu.exportFunction(this.restoreBackup.bind(this), window, {
        defineAs: "NRWorkspacesRestoreBackup",
      });
      Cu.exportFunction(this.deleteBackup.bind(this), window, {
        defineAs: "NRWorkspacesDeleteBackup",
      });
      Cu.exportFunction(this.deleteAllBackups.bind(this), window, {
        defineAs: "NRWorkspacesDeleteAllBackups",
      });
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
        globalThis.clearTimeout(pending.timer);
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

  private sendBackupRequest(
    name: string,
    data: Record<string, unknown> = {},
  ): Promise<BackupBridgeResult> {
    const requestId = `${Date.now()}-${Math.random().toString(36).slice(2)}`;
    return new Promise<BackupBridgeResult>((resolve) => {
      const timer = globalThis.setTimeout(() => {
        this.#pendingBackupRequests.delete(requestId);
        resolve({ requestId, success: false, error: "timeout" });
      }, 30_000);
      this.#pendingBackupRequests.set(requestId, { resolve, timer });
      this.sendAsyncMessage(name, { ...data, requestId });
    });
  }

  handleEvent(_event: Event): void {
    // No-op
  }
}
