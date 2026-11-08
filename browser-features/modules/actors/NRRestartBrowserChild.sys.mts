import { launchDevPorts } from "../common/dev-port-offset.ts";

/** Launch-local dev port resolver (FDH Gate 2 task 2-B): resolves the
 * table names to the ports this launch actually uses (offset-aware). */
const launchDevPort = launchDevPorts();

export class NRRestartBrowserChild extends JSWindowActorChild {
  actorCreated() {
    console.debug("NRRestartBrowserChild created!");
    const window = this.contentWindow;
    if (
      launchDevPort.isTrustedPort(window?.location.port, ["settings"]) ||
      window?.location.href.startsWith("chrome://") ||
      window?.location.href.startsWith("about:")
    ) {
      console.debug(
        `NRRestartBrowser ${
          launchDevPort.portString("settings")
        } ! or Chrome Page!`,
      );
      Cu.exportFunction(this.NRRestartBrowser.bind(this), window, {
        defineAs: "NRRestartBrowser",
      });
    }
  }

  NRRestartBrowser(callback: (result: boolean) => void = () => {}) {
    const promise = new Promise<boolean>((resolve, _reject) => {
      this.resolveRestartBrowser = resolve;
    });
    this.sendAsyncMessage("RestartBrowser:Restart");
    promise.then((result) => callback(result));
  }

  resolveRestartBrowser: ((result: boolean) => void) | null = null;
  // deno-lint-ignore require-await
  async receiveMessage(message: ReceiveMessageArgument) {
    switch (message.name) {
      case "RestartBrowser:Result": {
        this.resolveRestartBrowser?.(message.data);
        this.resolveRestartBrowser = null;
        break;
      }
    }
  }
  handleEvent(_event: Event): void {
    // No-op
  }
}
