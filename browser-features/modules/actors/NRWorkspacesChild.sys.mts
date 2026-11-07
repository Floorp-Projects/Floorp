import {
  HUB_DEV_PAGES,
  isTrustedDevPort,
} from "#libs/shared/dev-ports/defines.ts";

const WORKSPACES_INIT_TOPIC = "floorp.workspaces.initialize";

type InitializeResult = {
  success: boolean;
  error?: string | null;
};

export class NRWorkspacesChild extends JSWindowActorChild {
  #pendingInitializationPromise: Promise<InitializeResult> | null = null;
  #pendingInitializationResolver: ((result: InitializeResult) => void) | null =
    null;

  actorCreated() {
    const window = this.contentWindow;
    if (
      isTrustedDevPort(window?.location.port, HUB_DEV_PAGES) ||
      window?.location.href.startsWith("chrome://noraneko-settings") ||
      window?.location.href.startsWith("about:")
    ) {
      Cu.exportFunction(this.initializeWorkspaces.bind(this), window, {
        defineAs: "NRInitializeWorkspaces",
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
    }
  }
  handleEvent(_event: Event): void {
    // No-op
  }
}
