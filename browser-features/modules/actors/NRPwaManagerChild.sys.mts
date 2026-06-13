/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function isAllowedPwaManagerLocation(href: string): boolean {
  try {
    const url = new URL(href);
    if (url.protocol === "chrome:" && url.hostname === "noraneko-settings") {
      return true;
    }
    if (url.protocol === "about:" && url.pathname === "hub") {
      return true;
    }
    return (
      url.hostname === "localhost" &&
      url.port === "5183" &&
      (url.protocol === "http:" || url.protocol === "https:")
    );
  } catch {
    return false;
  }
}

export class NRPwaManagerChild extends JSWindowActorChild {
  actorCreated() {
    console.debug("NRPwaManagerChild created!");
    const window = this.contentWindow;
    if (window && isAllowedPwaManagerLocation(window.location.href)) {
      console.debug("NRPwaManager allowed page!");
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
      Cu.exportFunction(this.NRResetSsbContainer.bind(this), window, {
        defineAs: "NRResetSsbContainer",
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

  NRRenameSsb(id: string, newName: string, key?: string) {
    this.sendAsyncMessage("PwaManager:RenameSsb", { id, key, newName });
  }

  NRUninstallSsb(id: string, key?: string) {
    this.sendAsyncMessage("PwaManager:UninstallSsb", { id, key });
  }

  NRGetContainers(
    callback: (containersJson: string) => void = () => {},
  ) {
    const promise = new Promise<string>((resolve, _reject) => {
      this.resolveGetContainers = resolve;
    });
    this.sendAsyncMessage("PwaManager:GetContainers");
    promise.then((containersJson) => callback(containersJson));
  }

  NRResetSsbContainer(id: string, key?: string) {
    this.sendAsyncMessage("PwaManager:ResetContainer", { id, key });
  }

  resolveGetInstalledApps:
    // deno-lint-ignore no-explicit-any
    | ((installedApps: Record<string, any>) => void)
    | null = null;
  resolveRenameSsb: ((id: string, newName: string) => void) | null = null;
  resolveUninstallSsb: ((id: string) => void) | null = null;
  resolveGetContainers:
    | ((containersJson: string) => void)
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
