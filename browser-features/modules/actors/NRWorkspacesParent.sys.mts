const WORKSPACES_INIT_TOPIC = "floorp.workspaces.initialize";

interface WorkspacesFuncs {
  backupNow(): Promise<unknown>;
  listBackups(): Promise<unknown>;
  restoreBackup(fileName: string): Promise<unknown>;
  deleteBackup(fileName: string): Promise<unknown>;
  deleteAllBackups(): Promise<unknown>;
}

type BrowserWindowWithWorkspaces = Window & {
  workspacesFuncs?: WorkspacesFuncs;
};

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
          Services.obs.notifyObservers(null, WORKSPACES_INIT_TOPIC, reason);
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

  private async handleBackupRequest(
    message: ReceiveMessageArgument,
  ): Promise<void> {
    const requestId = message.data && typeof message.data.requestId === "string"
      ? message.data.requestId
      : null;
    try {
      const window = Services.wm.getMostRecentWindow(
        "navigator:browser",
      ) as BrowserWindowWithWorkspaces | null;
      const funcs = window?.workspacesFuncs;
      if (!funcs) {
        throw new Error("workspaces-unavailable");
      }

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
}
