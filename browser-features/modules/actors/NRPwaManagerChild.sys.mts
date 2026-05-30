
/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class NRPwaManagerChild extends JSWindowActorChild {
  actorCreated() {
    console.debug("NRPwaManagerChild created!");
    const window = this.contentWindow;
    if (
      window?.location.port === "5183" ||
      window?.location.href.startsWith("chrome://") ||
      window?.location.href.startsWith("about:")
    ) {
      console.debug("NRPwaManager 5183 ! or Chrome Page!");
      Cu.exportFunction(this.NRGetInstalledApps.bind(this), window, {
        defineAs: "NRGetInstalledApps",
      });
      Cu.exportFunction(this.NRRenameSsb.bind(this), window, {
        defineAs: "NRRenameSsb",
        asAsync: true,
      });
      Cu.exportFunction(this.NRUninstallSsb.bind(this), window, {
        defineAs: "NRUninstallSsb",
        asAsync: true,
      });
      Cu.exportFunction(this.NRGetContainers.bind(this), window, {
        defineAs: "NRGetContainers",
      });
      Cu.exportFunction(this.NRSetSsbContainer.bind(this), window, {
        defineAs: "NRSetSsbContainer",
        asAsync: true,
      });
    }
  }
  NRGetInstalledApps(
    // deno-lint-ignore no-explicit-any
    callback: (installedApps: Record<string, any>) => void = () => {},
  ) {
    // deno-lint-ignore no-explicit-any
    const promise = new Promise<Record<string, any>>((resolve, _reject) => {
      this.resolveGetInstalledApps = resolve;
    });
    this.sendAsyncMessage("PwaManager:GetInstalledApps");
    promise.then((installedApps) => callback(installedApps));
  }

  NRRenameSsb(id: string, newName: string) {
    this.sendAsyncMessage("PwaManager:RenameSsb", { id, newName });
  }

  NRUninstallSsb(id: string) {
    this.sendAsyncMessage("PwaManager:UninstallSsb", { id });
  }

  NRGetContainers(
    callback: (containers: { userContextId: number; name: string }[]) => void =
      () => {},
  ) {
    if (this.resolveGetContainers) {
      this.resolveGetContainers([]);
    }
    const promise = new Promise<
      { userContextId: number; name: string }[]
    >((resolve, _reject) => {
      this.resolveGetContainers = resolve;
    });
    this.sendAsyncMessage("PwaManager:GetContainers");
    // Parent sends a JSON string to avoid cross-compartment permission errors.
    // Pass the raw string to the callback; the content-side code parses it.
    promise.then((containers) => callback(containers));
  }

  NRSetSsbContainer(id: string, userContextId: number) {
    this.sendAsyncMessage("PwaManager:SetContainer", { id, userContextId });
  }

  resolveGetInstalledApps:
    // deno-lint-ignore no-explicit-any
    | ((installedApps: Record<string, any>) => void)
    | null = null;
  resolveRenameSsb: ((id: string, newName: string) => void) | null = null;
  resolveUninstallSsb: ((id: string) => void) | null = null;
  resolveGetContainers:
    | ((containers: { userContextId: number; name: string }[]) => void)
    | null = null;
  // deno-lint-ignore require-await
  async receiveMessage(message: ReceiveMessageArgument) {
    switch (message.name) {
      case "PwaManager:GetInstalledApps": {
        this.resolveGetInstalledApps?.(message.data);
        this.resolveGetInstalledApps = null;
        break;
      }
      case "PwaManager:RenameSsb": {
        this.resolveRenameSsb?.(
          message.data.wrappedJSObject.id,
          message.data.wrappedJSObject.newName,
        );
        this.resolveRenameSsb = null;
        break;
      }
      case "PwaManager:UninstallSsb": {
        this.resolveUninstallSsb?.(message.data.wrappedJSObject.id);
        this.resolveUninstallSsb = null;
        break;
      }
      case "PwaManager:GetContainers": {
        this.resolveGetContainers?.(message.data);
        this.resolveGetContainers = null;
        break;
      }
    }
  }
  handleEvent(_event: Event): void {
    // No-op
  }
}
