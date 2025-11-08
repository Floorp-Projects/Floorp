const WORKSPACES_INIT_TOPIC = "floorp.workspaces.initialize";

export class NRWorkspacesParent extends JSWindowActorParent {
  receiveMessage(message: ReceiveMessageArgument) {
    switch (message.name) {
      case "NRWorkspaces:Initialize": {
        const reason =
          message.data && typeof message.data.reason === "string"
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
    }
  }
}
