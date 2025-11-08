const WORKSPACES_INIT_TOPIC = "floorp.workspaces.initialize";

type InitializeResult = {
  success: boolean;
  error?: string | null;
};

export class NRWorkspacesChild extends JSWindowActorChild {
  #pendingInitialization: ((result: InitializeResult) => void) | null = null;

  actorCreated() {
    const window = this.contentWindow;
    if (
      window?.location.port === "5183" ||
      window?.location.port === "5186" ||
      window?.location.port === "5187" ||
      window?.location.port === "5188" ||
      window?.location.href.startsWith("chrome://noraneko-settings") ||
      window?.location.href.startsWith("about:")
    ) {
      Cu.exportFunction(this.initializeWorkspaces.bind(this), window, {
        defineAs: "NRInitializeWorkspaces",
      });
    }
  }

  initializeWorkspaces(reason?: string | null): Promise<InitializeResult> {
    if (this.#pendingInitialization) {
      console.warn(
        "NRWorkspacesChild: initialization already pending, reusing promise",
      );
    }

    return new Promise<InitializeResult>((resolve) => {
      this.#pendingInitialization = resolve;
      this.sendAsyncMessage("NRWorkspaces:Initialize", {
        topic: WORKSPACES_INIT_TOPIC,
        reason: typeof reason === "string" ? reason : null,
      });
    });
  }

  receiveMessage(message: ReceiveMessageArgument) {
    switch (message.name) {
      case "NRWorkspaces:InitializeResult": {
        this.#pendingInitialization?.(
          message.data && typeof message.data.success === "boolean"
            ? message.data
            : { success: false, error: "unknown" },
        );
        this.#pendingInitialization = null;
        break;
      }
    }
  }
}
