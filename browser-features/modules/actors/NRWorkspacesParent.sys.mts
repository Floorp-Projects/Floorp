import { HUB_DEV_PAGES } from "#libs/shared/dev-ports/defines.ts";
import { launchDevPorts } from "../common/dev-port-offset.ts";

const launchDevPort = launchDevPorts();

const WORKSPACES_INIT_TOPIC = "floorp.workspaces.initialize";

/**
 * Module-scope (process-wide, shared by every window's actor) single-flight
 * lock for mutating backup operations. List is never blocked.
 *
 * The lock carries a token and an expiry: if an operation's promise never
 * settles (for example its window closed mid-await), the lock lapses instead
 * of blocking every window until restart. The expiry exceeds the child's
 * 10-minute restore timeout, and a stale holder never releases a newer lock.
 */
const MUTATING_LOCK_TTL_MS = 11 * 60 * 1000;
let mutatingLock: { token: number; expiresAt: number } | null = null;
let nextLockToken = 1;

interface WorkspacesFuncs {
  backupNow(): Promise<unknown>;
  listBackups(): Promise<unknown>;
  restoreBackup(fileName: string): Promise<unknown>;
  deleteBackup(fileName: string): Promise<unknown>;
  deleteAllBackups(): Promise<unknown>;
}

type BrowserWindowWithWorkspaces = Window & {
  /** Backup-only entry point; available even when workspaces are disabled. */
  workspacesBackupFuncs?: WorkspacesFuncs;
  workspacesFuncs?: WorkspacesFuncs;
};

interface PrivateBrowsingUtilsModule {
  PrivateBrowsingUtils: {
    isWindowPrivate(window: Window): boolean;
  };
}

export class NRWorkspacesParent extends JSWindowActorParent {
  receiveMessage(message: ReceiveMessageArgument) {
    switch (message.name) {
      case "NRWorkspaces:Initialize": {
        const reason = message.data && typeof message.data.reason === "string"
          ? message.data.reason
          : null;

        let success = true;
        let errorMessage: string | null = null;

        try {
          Services.obs.notifyObservers(
            null as unknown as nsISupports,
            WORKSPACES_INIT_TOPIC,
            reason,
          );
        } catch (error) {
          success = false;
          errorMessage = error instanceof Error ? error.message : String(error);
          console.error(
            "NRWorkspacesParent: failed to notify workspace initialization",
            error,
          );
        }

        this.sendAsyncMessage("NRWorkspaces:InitializeResult", {
          success,
          error: errorMessage,
        });
        break;
      }
      case "NRWorkspaces:BackupNow":
      case "NRWorkspaces:ListBackups":
      case "NRWorkspaces:RestoreBackup":
      case "NRWorkspaces:DeleteBackup":
      case "NRWorkspaces:DeleteAllBackups": {
        void this.handleBackupRequest(message);
        break;
      }
    }
  }

  /**
   * Resolves the browser window that hosts the requesting document.
   *
   * Using the most-recent window instead would apply a destructive restore to a
   * window the user is not looking at.
   */
  private resolveRequestingWindow(): BrowserWindowWithWorkspaces | null {
    const requesting = this.browsingContext?.topChromeWindow as
      | BrowserWindowWithWorkspaces
      | null;
    if (requesting) {
      return requesting;
    }
    return Services.wm.getMostRecentWindow(
      "navigator:browser",
    ) as BrowserWindowWithWorkspaces | null;
  }

  /** Returns false only when the window is known to be a normal window. */
  private isPrivateWindow(window: Window): boolean | null {
    try {
      const module = ChromeUtils.importESModule(
        "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
      ) as unknown as PrivateBrowsingUtilsModule;
      return module.PrivateBrowsingUtils.isWindowPrivate(window);
    } catch (error) {
      console.error(
        "NRWorkspacesParent: private window detection failed",
        error,
      );
      return null;
    }
  }

  private async handleBackupRequest(
    message: ReceiveMessageArgument,
  ): Promise<void> {
    const requestId = message.data && typeof message.data.requestId === "string"
      ? message.data.requestId
      : null;
    try {
      if (!this.isTrustedBackupDocument()) {
        throw new Error("untrusted-document");
      }
      const window = this.resolveRequestingWindow();
      if (!window) {
        throw new Error("workspaces-unavailable");
      }
      // Restore and backup-now capture/replace the window's tabs; neither may
      // run against a private window (the safety copy excludes them). Listing
      // and deleting backup files is window-independent and stays allowed.
      if (
        (message.name === "NRWorkspaces:RestoreBackup" ||
          message.name === "NRWorkspaces:BackupNow") &&
        this.isPrivateWindow(window) !== false
      ) {
        throw new Error("private-window-unsupported");
      }
      const funcs = window.workspacesBackupFuncs ?? window.workspacesFuncs;
      if (!funcs) {
        throw new Error("workspaces-unavailable");
      }

      const mutating = message.name !== "NRWorkspaces:ListBackups";
      let lockToken: number | null = null;
      if (mutating) {
        // A lock whose expiry lies further ahead than the TTL means the wall
        // clock moved backwards; treat it as lapsed rather than held longer.
        const now = Date.now();
        if (
          mutatingLock && now < mutatingLock.expiresAt &&
          mutatingLock.expiresAt - now <= MUTATING_LOCK_TTL_MS
        ) {
          throw new Error("restore-in-progress");
        }
        lockToken = nextLockToken++;
        mutatingLock = {
          token: lockToken,
          expiresAt: Date.now() + MUTATING_LOCK_TTL_MS,
        };
      }
      let data: unknown;
      try {
        data = await this.runBackupOperation(funcs, message);
      } finally {
        if (lockToken !== null && mutatingLock?.token === lockToken) {
          mutatingLock = null;
        }
      }

      this.sendAsyncMessage("NRWorkspaces:BackupResult", {
        requestId,
        success: true,
        data,
      });
    } catch (error) {
      console.error("NRWorkspacesParent: backup request failed", error);
      this.sendAsyncMessage("NRWorkspaces:BackupResult", {
        requestId,
        success: false,
        error: error instanceof Error ? error.message : String(error),
      });
    }
  }

  private isTrustedBackupDocument(): boolean {
    try {
      const uri = this.manager?.documentURI ??
        this.browsingContext?.currentURI;
      if (!uri) {
        return false;
      }
      // Compare the page itself (no query/fragment) rather than a loose
      // prefix, so e.g. a hypothetical about:hubfoo never matches.
      const page = uri.spec.split(/[?#]/)[0];
      if (
        page.startsWith("chrome://noraneko-settings/") || page === "about:hub"
      ) {
        return true;
      }
      if (uri.scheme === "http" && uri.host === "localhost") {
        return launchDevPort.isTrustedPort(String(uri.port), HUB_DEV_PAGES);
      }
    } catch (error) {
      console.error("NRWorkspacesParent: document check failed", error);
    }
    return false;
  }

  private async runBackupOperation(
    funcs: WorkspacesFuncs,
    message: ReceiveMessageArgument,
  ): Promise<unknown> {
    {
      let data: unknown;
      switch (message.name) {
        case "NRWorkspaces:BackupNow":
          data = await funcs.backupNow();
          break;
        case "NRWorkspaces:ListBackups":
          data = await funcs.listBackups();
          break;
        case "NRWorkspaces:RestoreBackup":
        case "NRWorkspaces:DeleteBackup": {
          const fileName = message.data &&
              typeof message.data.fileName === "string"
            ? message.data.fileName
            : null;
          if (!fileName) {
            throw new Error("invalid-file-name");
          }
          data = message.name === "NRWorkspaces:RestoreBackup"
            ? await funcs.restoreBackup(fileName)
            : await funcs.deleteBackup(fileName);
          break;
        }
        case "NRWorkspaces:DeleteAllBackups":
          data = await funcs.deleteAllBackups();
          break;
      }
      return data;
    }
  }
}
