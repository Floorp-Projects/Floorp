/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { Manifest } from "../type.ts";
import { getSsbDisplayName } from "../containerDisplay.sys.mts";
import { buildSsbKey } from "#libs/pwa/ssbKeyUtils.ts";

type WindowsSsbStore = {
  getCurrentSsbData(): Promise<Record<string, Manifest>>;
  saveSsbData(ssb: Manifest): Promise<void>;
  removeSsbData(key: string): Promise<void>;
};

const { ImageTools } = ChromeUtils.importESModule(
  "resource://noraneko/modules/pwa/ImageTools.sys.mjs",
);
const { ShellService } = ChromeUtils.importESModule(
  "moz-src:///browser/components/shell/ShellService.sys.mjs",
);
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

function defaultStore(): WindowsSsbStore {
  const { DataStoreProvider } = ChromeUtils.importESModule(
    "resource://noraneko/modules/pwa/DataStore.sys.mjs",
  );
  return DataStoreProvider.getDataManager();
}

export class WindowsSupport {
  private static readonly SHORTCUT_FOLDER = "Programs";
  private static mutationQueues = new Map<string, Promise<void>>();
  private static generations = new Map<string, symbol>();
  private static storeQueue: Promise<void> = Promise.resolve();

  private static shellService = Cc[
    "@mozilla.org/browser/shell-service;1"
  ].getService(Ci.nsIWindowsShellService);
  private static uiUtils = Cc["@mozilla.org/windows-ui-utils;1"].getService(
    Ci.nsIWindowsUIUtils,
  );
  private static taskbar = Cc["@mozilla.org/windows-taskbar;1"].getService(
    Ci.nsIWinTaskbar,
  );
  private static nsIFile = Components.Constructor(
    "@mozilla.org/file/local;1",
    Ci.nsIFile,
    "initWithPath",
  );

  // Shared by every support instance. Waiting for one app's icons or pin UI
  // must not block another app, but removal must drain its own pending writes.
  static withMutation<T>(id: string, operation: () => Promise<T>): Promise<T> {
    const result = (this.mutationQueues.get(id) ?? Promise.resolve()).then(
      operation,
    );
    const tail = result.then(() => {}, () => {});
    this.mutationQueues.set(id, tail);
    void tail.then(() => {
      if (this.mutationQueues.get(id) === tail) {
        this.mutationQueues.delete(id);
        this.generations.delete(id);
      }
    });
    return result;
  }

  // ssb.json is shared between apps and between the chrome/module data managers.
  // Serialize only its read/modify/write operations, never icon or UI waits.
  static withStoreMutation<T>(operation: () => Promise<T>): Promise<T> {
    const result = this.storeQueue.then(operation);
    this.storeQueue = result.then(() => {}, () => {});
    return result;
  }

  private static generation(id: string, replace = false): symbol {
    let generation = this.generations.get(id);
    if (replace || !generation) {
      generation = Symbol(id);
      this.generations.set(id, generation);
    }
    return generation;
  }

  private static async installed(id: string, store: WindowsSsbStore) {
    return Object.values(await store.getCurrentSsbData()).find((app) =>
      app.id === id
    );
  }

  private buildGroupId(id: string) {
    return `ablaze.floorp.ssb.${id}`;
  }

  private shortcutPath(ssb: Manifest) {
    return PathUtils.join(
      Services.dirsvc.get("Home", Ci.nsIFile).path,
      "AppData",
      "Roaming",
      "Microsoft",
      "Windows",
      "Start Menu",
      "Programs",
      `${getSsbDisplayName(ssb)}.lnk`,
    );
  }

  private async createShortcutForSsb(
    ssb: Manifest,
    isCurrent: () => boolean,
  ): Promise<string | null> {
    const dir = PathUtils.join(PathUtils.profileDir, "ssb", ssb.id);
    await IOUtils.makeDirectory(dir, { ignoreExisting: true });
    if (!isCurrent()) return null;

    const iconExtension = ShellService.shortcutIconType.extension;
    let iconFile: nsIFile | null = new WindowsSupport.nsIFile(
      PathUtils.join(dir, `icon.${iconExtension}`),
    );
    if (ssb.icon) {
      const { container } = await ImageTools.loadImage(
        Services.io.newURI(ssb.icon),
      );
      if (!isCurrent()) return null;
      const badgedContainer = await ImageTools.applyContainerBadgeToIcon(
        container,
        ssb.userContextId ?? 0,
      );
      if (!isCurrent()) return null;
      await ShellService.writeShortcutIcon(iconFile, badgedContainer);
      if (!isCurrent()) return null;
    } else {
      iconFile = null;
    }

    const displayName = getSsbDisplayName(ssb);
    const shortcutName = `${displayName}.lnk`;
    await WindowsSupport.shellService.createShortcut(
      Services.dirsvc.get("XREExeF", Ci.nsIFile),
      ["-profile", PathUtils.profileDir, "-start-ssb", ssb.id],
      displayName,
      iconFile ?? (null as unknown as nsIFile),
      0,
      this.buildGroupId(ssb.id),
      WindowsSupport.SHORTCUT_FOLDER,
      shortcutName,
    );
    return isCurrent() ? shortcutName : null;
  }

  private async pin(ssb: Manifest, shortcutName: string) {
    try {
      await WindowsSupport.shellService.pinShortcutToTaskbar(
        this.buildGroupId(ssb.id),
        WindowsSupport.SHORTCUT_FOLDER,
        shortcutName,
      );
    } catch (error) {
      console.warn("[WindowsSupport] Failed to pin PWA to taskbar:", error);
    }
  }

  private async removeOSIntegration(ssb: Manifest) {
    try {
      WindowsSupport.shellService.unpinShortcutFromTaskbar(
        WindowsSupport.SHORTCUT_FOLDER,
        `${getSsbDisplayName(ssb)}.lnk`,
      );
    } catch (error) {
      console.warn("[WindowsSupport] Failed to unpin PWA from taskbar:", error);
    }
    await IOUtils.remove(this.shortcutPath(ssb), { ignoreAbsent: true });
    await IOUtils.remove(PathUtils.join(PathUtils.profileDir, "ssb", ssb.id), {
      recursive: true,
      ignoreAbsent: true,
    });
  }

  async install(
    ssb: Manifest,
    store: WindowsSsbStore = defaultStore(),
    integrate = true,
  ): Promise<void> {
    const generation = WindowsSupport.generation(ssb.id, true);
    const isCurrent = () =>
      WindowsSupport.generations.get(ssb.id) === generation;
    return await WindowsSupport.withMutation(ssb.id, async () => {
      try {
        if (integrate && isCurrent()) {
          const shortcutName = await this.createShortcutForSsb(ssb, isCurrent);
          if (shortcutName && isCurrent()) await this.pin(ssb, shortcutName);
        }
        await WindowsSupport.withStoreMutation(async () => {
          if (isCurrent()) await store.saveSsbData(ssb);
        });
      } finally {
        // A cancelled install/rename may have written a new shortcut name that
        // the later uninstall cannot discover from the previously stored name.
        if (integrate && !isCurrent()) await this.removeOSIntegration(ssb);
      }
    });
  }

  async uninstall(
    ssb: Manifest,
    store: WindowsSsbStore = defaultStore(),
    integrate = true,
  ): Promise<void> {
    // Invalidate at request time, before waiting for pending image/native work.
    WindowsSupport.generation(ssb.id, true);
    return await WindowsSupport.withMutation(ssb.id, async () => {
      const current = await WindowsSupport.withStoreMutation(() =>
        WindowsSupport.installed(ssb.id, store)
      );
      if (integrate) await this.removeOSIntegration(current ?? ssb);
      await WindowsSupport.withStoreMutation(async () => {
        const installed = await WindowsSupport.installed(ssb.id, store);
        if (installed) {
          await store.removeSsbData(
            buildSsbKey(installed.start_url, installed.userContextId ?? 0),
          );
        }
      });
    });
  }

  async rename(
    ssb: Manifest,
    newName: string,
    store: WindowsSsbStore = defaultStore(),
    integrate = true,
  ): Promise<boolean> {
    const generation = WindowsSupport.generation(ssb.id, true);
    const isCurrent = () =>
      WindowsSupport.generations.get(ssb.id) === generation;
    return await WindowsSupport.withMutation(ssb.id, async () => {
      const current = await WindowsSupport.withStoreMutation(() =>
        WindowsSupport.installed(ssb.id, store)
      );
      if (!current || !isCurrent()) return false;
      const updated = { ...current, name: newName, short_name: newName };
      try {
        if (integrate) {
          await this.removeOSIntegration(current);
          if (!isCurrent()) return false;
          const shortcutName = await this.createShortcutForSsb(
            updated,
            isCurrent,
          );
          if (shortcutName && isCurrent()) {
            await this.pin(updated, shortcutName);
          }
        }
        return await WindowsSupport.withStoreMutation(async () => {
          if (!isCurrent()) return false;
          await store.saveSsbData(updated);
          return true;
        });
      } finally {
        if (integrate && !isCurrent()) await this.removeOSIntegration(updated);
      }
    });
  }

  async applyOSIntegration(ssb: Manifest, aWindow: Window): Promise<void> {
    const generation = WindowsSupport.generation(ssb.id);
    const isCurrent = () =>
      WindowsSupport.generations.get(ssb.id) === generation;
    return await WindowsSupport.withMutation(ssb.id, async () => {
      const store = defaultStore();
      const current = await WindowsSupport.withStoreMutation(() =>
        WindowsSupport.installed(ssb.id, store)
      );
      if (!current || !isCurrent()) return;
      WindowsSupport.taskbar.setGroupIdForWindow(
        aWindow as unknown as mozIDOMWindowProxy,
        this.buildGroupId(ssb.id),
      );
      const getIcon = async (size: number) => {
        if (!current.icon) return null;
        try {
          const image = await ImageTools.loadImage(
            Services.io.newURI(current.icon),
          );
          if (!isCurrent()) return null;
          return await ImageTools.applyContainerBadgeToIcon(
            image.container,
            current.userContextId ?? 0,
            size,
          );
        } catch (error) {
          console.error("[WindowsSupport] Could not load window icon:", error);
          return null;
        }
      };
      const icons = await Promise.all([
        getIcon(WindowsSupport.uiUtils.systemSmallIconSize),
        getIcon(WindowsSupport.uiUtils.systemLargeIconSize),
      ]);
      if (!isCurrent()) return;
      if (icons[0] || icons[1]) {
        WindowsSupport.uiUtils.setWindowIcon(
          aWindow as unknown as mozIDOMWindowProxy,
          icons[0],
          icons[1],
        );
      }
      await this.showPinPromptIfNeeded(ssb.id, store, isCurrent);
    });
  }

  private async showPinPromptIfNeeded(
    id: string,
    store: WindowsSsbStore,
    isCurrent: () => boolean,
  ): Promise<void> {
    const current = await WindowsSupport.withStoreMutation(async () => {
      if (!isCurrent()) return;
      const installed = await WindowsSupport.installed(id, store);
      if (!installed || installed.pinPromptShown || !isCurrent()) return;
      const updated = { ...installed, pinPromptShown: true };
      await store.saveSsbData(updated);
      return updated;
    });
    if (!current || !isCurrent()) return;
    await new Promise((resolve) => setTimeout(resolve, 2000));
    if (!isCurrent()) return;
    try {
      const exists = await IOUtils.exists(this.shortcutPath(current));
      if (!isCurrent()) return;
      const shortcutName = exists
        ? `${getSsbDisplayName(current)}.lnk`
        : await this.createShortcutForSsb(current, isCurrent);
      if (shortcutName && isCurrent()) await this.pin(current, shortcutName);
    } catch (error) {
      console.warn("[WindowsSupport] Failed to trigger pin dialog:", error);
    }
  }
}
