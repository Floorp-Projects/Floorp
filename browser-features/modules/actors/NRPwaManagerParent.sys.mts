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
        try {
          const { ContextualIdentityService } = ChromeUtils.importESModule(
            "resource://gre/modules/ContextualIdentityService.sys.mjs",
          );
          const identities = ContextualIdentityService.getPublicIdentities();
          const containers = identities.map(
            (c: { userContextId: number; l10nId?: string; name: string; color: string }) => ({
              userContextId: c.userContextId,
              name: c.l10nId
                ? ContextualIdentityService.getUserContextLabel(c.userContextId)
                : c.name,
              color: c.color,
            }),
          );
          this.sendAsyncMessage("PwaManager:GetContainers", JSON.stringify(containers));
        } catch (e) {
          console.error("[NRPwaManagerParent] Error getting containers:", e);
          this.sendAsyncMessage("PwaManager:GetContainers", "[]");
        }
        break;
      }
      case "PwaManager:SetContainer": {
        await this.setContainerForSsb(
          String(message.data.id),
          Number(message.data.userContextId),
        );
        break;
      }
    }
  }

  private get installedAppsStoreFile() {
    return PathUtils.join(PathUtils.profileDir, "ssb", "ssb.json");
  }

  private buildKey(startUrl: string, userContextId: number = 0): string {
    return `${startUrl}:${userContextId}`;
  }

  private async readSsbData(): Promise<Record<string, unknown>> {
    const fileExists = await IOUtils.exists(this.installedAppsStoreFile);
    if (!fileExists) {
      return {};
    }
    return (await IOUtils.readJSON(this.installedAppsStoreFile)) as Record<string, unknown>;
  }

  private async writeSsbData(data: Record<string, unknown>): Promise<void> {
    await IOUtils.writeJSON(this.installedAppsStoreFile, data);
  }

  private async setContainerForSsb(id: string, userContextId: number): Promise<void> {
    try {
      const ssbData = await this.readSsbData();
      // Find the app by id
      let foundKey: string | null = null;
      let foundManifest: Record<string, unknown> | null = null;
      for (const key in ssbData) {
        const entry = ssbData[key] as Record<string, unknown>;
        if (entry.id === id) {
          foundKey = key;
          foundManifest = entry;
          break;
        }
      }

      if (!foundKey || !foundManifest) {
        console.warn("[NRPwaManagerParent] setContainerForSsb: app not found for id:", id);
        return;
      }

      const startUrl = foundManifest.start_url as string;

      // Remove old key
      delete ssbData[foundKey];

      // Update manifest
      foundManifest.userContextId = userContextId > 0 ? userContextId : undefined;

      // Save with new key
      const newKey = this.buildKey(startUrl, userContextId);
      ssbData[newKey] = foundManifest;

      await this.writeSsbData(ssbData);

    } catch (e) {
      console.error("[NRPwaManagerParent] setContainerForSsb error:", e);
    }
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
