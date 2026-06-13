/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { resolveContainerColor } = ChromeUtils.importESModule(
  "resource://noraneko/modules/pwa/containerDisplay.sys.mjs",
);
const { DataStoreProvider } = ChromeUtils.importESModule(
  "resource://noraneko/modules/pwa/DataStore.sys.mjs",
);

type WindowGlobalWithDocumentURI = {
  documentURI?: {
    spec?: string;
  };
};

function isAllowedPwaManagerSource(href: string): boolean {
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

export class NRPwaManagerParent extends JSWindowActorParent {
  async receiveMessage(message: ReceiveMessageArgument) {
    if (!this.isAllowedMessageSource()) {
      console.warn(
        `[NRPwaManagerParent] Rejected ${message.name} from unauthorized source`,
      );
      this.replyWithEmptyResult(message.name);
      return;
    }

    switch (message.name) {
      case "PwaManager:GetInstalledApps": {
        this.sendAsyncMessage(
          "PwaManager:GetInstalledApps",
          await this.getInstalledApps(),
        );
        break;
      }
      case "PwaManager:RenameSsb": {
        Services.obs.notifyObservers(
          {
            wrappedJSObject: {
              id: message.data.id,
              key: message.data.key,
              newName: message.data.newName,
            },
          },
          "nora-ssb-rename",
        );
        break;
      }
      case "PwaManager:UninstallSsb": {
        Services.obs.notifyObservers(
          { wrappedJSObject: { id: message.data.id, key: message.data.key } },
          "nora-ssb-uninstall",
        );
        break;
      }
      case "PwaManager:GetContainers": {
        try {
          const { ContextualIdentityService } = ChromeUtils.importESModule(
            "resource://gre/modules/ContextualIdentityService.sys.mjs",
          );
          const identities = ContextualIdentityService.getPublicIdentities();
          const containers = identities.map(
            (
              c: {
                userContextId: number;
                l10nId?: string;
                name: string;
                color: string;
              },
            ) => ({
              userContextId: c.userContextId,
              name: c.l10nId
                ? ContextualIdentityService.getUserContextLabel(c.userContextId)
                : c.name,
              color: resolveContainerColor(c.color),
            }),
          );
          this.sendAsyncMessage(
            "PwaManager:GetContainers",
            JSON.stringify(containers),
          );
        } catch (e) {
          console.error("[NRPwaManagerParent] Error getting containers:", e);
          this.sendAsyncMessage("PwaManager:GetContainers", "[]");
        }
        break;
      }
      case "PwaManager:ResetContainer": {
        Services.obs.notifyObservers(
          { wrappedJSObject: { id: message.data.id, key: message.data.key } },
          "nora-ssb-reset-container",
        );
        break;
      }
    }
  }

  private get sourceSpec(): string {
    const currentWindowGlobal = this.browsingContext
      ?.currentWindowGlobal as WindowGlobalWithDocumentURI | null;
    return currentWindowGlobal?.documentURI?.spec ?? "";
  }

  private isAllowedMessageSource(): boolean {
    return isAllowedPwaManagerSource(this.sourceSpec);
  }

  private replyWithEmptyResult(messageName: string) {
    switch (messageName) {
      case "PwaManager:GetInstalledApps":
        this.sendAsyncMessage("PwaManager:GetInstalledApps", "{}");
        break;
      case "PwaManager:GetContainers":
        this.sendAsyncMessage("PwaManager:GetContainers", "[]");
        break;
    }
  }

  private async getInstalledApps() {
    const installedApps = await DataStoreProvider.getDataManager()
      .getCurrentSsbData();
    return JSON.stringify(installedApps);
  }
}
