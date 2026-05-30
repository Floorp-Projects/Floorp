/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class NRPwaManagerParent extends JSWindowActorParent {
  async receiveMessage(message: ReceiveMessageArgument) {
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
              newName: message.data.newName,
            },
          },
          "nora-ssb-rename",
        );
        break;
      }
      case "PwaManager:UninstallSsb": {
        Services.obs.notifyObservers(
          { wrappedJSObject: { id: message.data.id } },
          "nora-ssb-uninstall",
        );
        break;
      }
      case "PwaManager:GetContainers": {
        const { ContextualIdentityService } = ChromeUtils.importESModule(
          "resource://gre/modules/ContextualIdentityService.sys.mjs",
        );
        const containers = ContextualIdentityService.getPublicIdentities().map(
          (c: { userContextId: number; l10nId?: string; name: string }) => ({
            userContextId: c.userContextId,
            name: c.l10nId
              ? ContextualIdentityService.getUserContextLabel(c.userContextId)
              : c.name,
          }),
        );
        // Serialize to string to avoid cross-compartment permission errors
        // when the data crosses into content scope via Cu.exportFunction callback.
        this.sendAsyncMessage(
          "PwaManager:GetContainers",
          JSON.stringify(containers),
        );
        break;
      }
      case "PwaManager:SetContainer": {
        Services.obs.notifyObservers(
          {
            wrappedJSObject: {
              id: message.data.id,
              userContextId: message.data.userContextId,
            },
          },
          "nora-ssb-set-container",
        );
        break;
      }
    }
  }

  private get installedAppsStoreFile() {
    return PathUtils.join(PathUtils.profileDir, "ssb", "ssb.json");
  }

  private async getInstalledApps() {
    const fileExists = await IOUtils.exists(this.installedAppsStoreFile);
    if (!fileExists) {
      IOUtils.writeJSON(this.installedAppsStoreFile, {});
      return {};
    }
    const installedApps = await IOUtils.readJSON(this.installedAppsStoreFile);
    return JSON.stringify(installedApps);
  }
}
