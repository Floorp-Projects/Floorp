/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { PwaContainerExperiment } = ChromeUtils.importESModule(
  "resource://noraneko/modules/pwa/PwaContainerExperiment.sys.mjs",
);

/** Firefox container color name → hex mapping (from toolkit/components/usercontext/content/userContext.css) */
const CONTAINER_COLORS: Record<string, string> = {
  blue: "#37adff",
  turquoise: "#00c79a",
  green: "#51cd00",
  yellow: "#ffcb00",
  orange: "#ff9f00",
  red: "#ff613d",
  pink: "#ff4bda",
  purple: "#af51f5",
};

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
        return await this.requestMutation(
          "nora-ssb-rename",
          String(message.data.id),
          String(message.data.newName),
        );
      }
      case "PwaManager:UninstallSsb": {
        return await this.requestMutation(
          "nora-ssb-uninstall",
          String(message.data.id),
        );
      }
      case "PwaManager:GetContainers": {
        if (!PwaContainerExperiment.isEnabled()) {
          this.sendAsyncMessage("PwaManager:GetContainers", "[]");
          break;
        }
        try {
          const { ContextualIdentityService } = ChromeUtils.importESModule(
            "moz-src:///toolkit/components/contextualidentity/ContextualIdentityService.sys.mjs",
          );
          const identities = ContextualIdentityService.getPublicIdentities();
          const containers = identities.map(
            (c: {
              userContextId: number;
              l10nId?: string;
              name: string;
              color: string;
            }) => ({
              userContextId: c.userContextId,
              name: c.l10nId
                ? ContextualIdentityService.getUserContextLabel(c.userContextId)
                : c.name,
              color: CONTAINER_COLORS[c.color] ?? c.color,
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
      case "PwaManager:SetContainer": {
        if (!PwaContainerExperiment.isEnabled()) {
          return "disabled";
        }
        return await this.setContainerForSsb(
          String(message.data.id),
          Number(message.data.userContextId),
        );
      }
    }
  }

  private requestMutation(
    topic: string,
    id: string,
    newName?: string,
  ): Promise<string> {
    return new Promise((resolve) => {
      const request = {
        id,
        newName,
        claimed: false,
        complete: (success: boolean) => resolve(success ? "ok" : "failed"),
      };
      Services.obs.notifyObservers({ wrappedJSObject: request }, topic);
      if (!request.claimed) resolve("unavailable");
    });
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
    return (await IOUtils.readJSON(this.installedAppsStoreFile)) as Record<
      string,
      unknown
    >;
  }

  private async writeSsbData(data: Record<string, unknown>): Promise<void> {
    await IOUtils.writeJSON(this.installedAppsStoreFile, data);
  }

  private async setContainerForSsb(
    id: string,
    userContextId: number,
  ): Promise<string> {
    if (!Number.isSafeInteger(userContextId) || userContextId < 0) {
      return "invalid-container";
    }
    if (Services.appinfo.OS === "Darwin") {
      const { MacOSSupport } = ChromeUtils.importESModule(
        "resource://noraneko/modules/pwa/supports/MacOS.sys.mjs",
      );
      const { NativeAppRuntime } = ChromeUtils.importESModule(
        "resource://noraneko/modules/pwa/NativeAppRuntime.sys.mjs",
      );
      const support = new MacOSSupport();
      if (await support.findNativeAppBundle({ id })) {
        return "native-context-fixed";
      }
      return await NativeAppRuntime.withMutation(
        id,
        () => this.updateContainer(id, userContextId),
      ) ?? "cancelled";
    }
    return await this.updateContainer(id, userContextId);
  }

  private async updateContainer(
    id: string,
    userContextId: number,
  ): Promise<string> {
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
        console.warn(
          "[NRPwaManagerParent] setContainerForSsb: app not found for id:",
          id,
        );
        return "not-found";
      }

      if (Services.appinfo.OS === "Darwin") {
        const { MacOSSupport } = ChromeUtils.importESModule(
          "resource://noraneko/modules/pwa/supports/MacOS.sys.mjs",
        );
        if (await new MacOSSupport().findNativeAppBundle({ id })) {
          return "native-context-fixed";
        }
      }

      const startUrl = foundManifest.start_url as string;
      const previousContext = Number(foundManifest.userContextId ?? 0);
      const newKey = this.buildKey(startUrl, userContextId);
      if (newKey !== foundKey && ssbData[newKey]) return "container-conflict";

      // Remove old key
      delete ssbData[foundKey];

      // Update manifest
      foundManifest.userContextId = userContextId > 0
        ? userContextId
        : undefined;

      // Save with new key
      ssbData[newKey] = foundManifest;

      await this.writeSsbData(ssbData);
      if (Services.appinfo.OS === "Darwin") {
        try {
          const { AppRegistry } = ChromeUtils.importESModule(
            "resource://noraneko/modules/pwa/AppRegistry.sys.mjs",
          );
          await AppRegistry.getForProfile().updateLegacyUserContext(
            id,
            previousContext,
            userContextId,
          );
        } catch (error) {
          console.warn(
            "[NRPwaManagerParent] Could not reconcile optional launcher metadata:",
            error,
          );
        }
      }
      return "ok";
    } catch (e) {
      console.error("[NRPwaManagerParent] setContainerForSsb error:", e);
      return "failed";
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
