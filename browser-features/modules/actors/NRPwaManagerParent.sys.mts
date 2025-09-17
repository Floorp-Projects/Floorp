// SPDX-License-Identifier: MPL-2.0




export class NRPwaManagerParent extends JSWindowActorParent {
  async receiveMessage(message: ReceiveMessageArgument) {
    switch (message.name) {
      case "PwaManager:GetInstalledApps": {
        this.sendAsyncMessage(
          "PwaManager:GetInstalledApps",
          await this.getInstalledApps(),
        );
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
      }
      case "PwaManager:UninstallSsb": {
        Services.obs.notifyObservers(
          { wrappedJSObject: { id: message.data.id } },
          "nora-ssb-uninstall",
        );
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
