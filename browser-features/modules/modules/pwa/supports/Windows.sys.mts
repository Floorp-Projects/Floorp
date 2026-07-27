/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { Manifest } from "../type.ts";
import { getSsbDisplayName } from "../containerDisplay.sys.mts";

const { ImageTools } = ChromeUtils.importESModule(
  "resource://noraneko/modules/pwa/ImageTools.sys.mjs",
);

const { ShellService } = ChromeUtils.importESModule(
  "moz-src:///browser/components/shell/ShellService.sys.mjs",
);

const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

export class WindowsSupport {
  private static readonly SHORTCUT_FOLDER = "Programs";

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

  private buildGroupId(id: string) {
    return `ablaze.floorp.ssb.${id}`;
  }

  private async createShortcutForSsb(ssb: Manifest) {
    const dir = PathUtils.join(PathUtils.profileDir, "ssb", ssb.id);
    await IOUtils.makeDirectory(dir, { ignoreExisting: true });

    const iconExtension = ShellService.shortcutIconType.extension;
    let iconFile: nsIFile | null = new WindowsSupport.nsIFile(
      PathUtils.join(dir, `icon.${iconExtension}`),
    );

    const icon = ssb.icon;
    if (icon) {
      console.debug("[PWA:install-launch] createShortcutForSsb: loadImage");
      const { container } = await ImageTools.loadImage(
        Services.io.newURI(icon),
      );
      console.debug(
        "[PWA:install-launch] createShortcutForSsb: applyContainerBadge",
      );
      const badgedContainer = await ImageTools.applyContainerBadgeToIcon(
        container,
        ssb.userContextId ?? 0,
      );
      console.debug(
        "[PWA:install-launch] createShortcutForSsb: writeShortcutIcon",
      );
      await ShellService.writeShortcutIcon(iconFile, badgedContainer);
    } else {
      iconFile = null;
    }

    const displayName = getSsbDisplayName(ssb);
    const shortcutName = `${displayName}.lnk`;
    WindowsSupport.shellService.createShortcut(
      Services.dirsvc.get("XREExeF", Ci.nsIFile),
      ["-profile", PathUtils.profileDir, "-start-ssb", ssb.id],
      displayName,
      iconFile ?? (null as unknown as nsIFile),
      0,
      this.buildGroupId(ssb.id),
      WindowsSupport.SHORTCUT_FOLDER,
      shortcutName,
    );

    return shortcutName;
  }

  async install(ssb: Manifest) {
    console.debug("[PWA:install-launch] WindowsSupport.install start");
    const shortcutName = await this.createShortcutForSsb(ssb);
    console.debug(
      "[PWA:install-launch] WindowsSupport.createShortcutForSsb done",
      { shortcutName },
    );

    try {
      await WindowsSupport.shellService.pinShortcutToTaskbar(
        this.buildGroupId(ssb.id),
        WindowsSupport.SHORTCUT_FOLDER,
        shortcutName,
      );
      console.debug(`[WindowsSupport] Pinned PWA to taskbar: ${ssb.name}`);
    } catch (e) {
      console.warn(`[WindowsSupport] Failed to pin PWA to taskbar: ${e}`);
    }
    console.debug("[PWA:install-launch] WindowsSupport.install finished");
  }

  async uninstall(ssb: Manifest) {
    const displayName = getSsbDisplayName(ssb);
    const shortcutName = `${displayName}.lnk`;

    try {
      WindowsSupport.shellService.unpinShortcutFromTaskbar(
        WindowsSupport.SHORTCUT_FOLDER,
        shortcutName,
      );
      console.debug(`[WindowsSupport] Unpinned PWA from taskbar: ${ssb.name}`);
    } catch (e) {
      console.warn(`[WindowsSupport] Failed to unpin PWA from taskbar: ${e}`);
    }

    try {
      const startMenu = `${
        Services.dirsvc.get("Home", Ci.nsIFile).path
      }\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu\\Programs\\`;
      await IOUtils.remove(`${startMenu + displayName}.lnk`);
    } catch (e) {
      console.error(e);
    }

    const dir = PathUtils.join(PathUtils.profileDir, "ssb", ssb.id);
    try {
      await IOUtils.remove(dir, { recursive: true });
    } catch (e) {
      console.error(e);
    }
  }

  async applyOSIntegration(ssb: Manifest, aWindow: Window) {
    WindowsSupport.taskbar.setGroupIdForWindow(
      aWindow as unknown as mozIDOMWindowProxy,
      this.buildGroupId(ssb.id),
    );
    const getIcon = async (size: number) => {
      const icon = ssb.icon;
      if (!icon) {
        return null;
      }

      try {
        const image = await ImageTools.loadImage(Services.io.newURI(icon));
        return await ImageTools.applyContainerBadgeToIcon(
          image.container,
          ssb.userContextId ?? 0,
          size,
        );
      } catch (e) {
        console.error(e);
        return null;
      }
    };

    const icons = await Promise.all([
      getIcon(WindowsSupport.uiUtils.systemSmallIconSize),
      getIcon(WindowsSupport.uiUtils.systemLargeIconSize),
    ]);

    if (icons[0] || icons[1]) {
      WindowsSupport.uiUtils.setWindowIcon(
        aWindow as unknown as mozIDOMWindowProxy,
        icons[0],
        icons[1],
      );
    }

    await this.showPinPromptIfNeeded(ssb, aWindow);
  }

  private async showPinPromptIfNeeded(
    ssb: Manifest,
    _aWindow: Window,
  ): Promise<void> {
    if (ssb.pinPromptShown) return;

    ssb.pinPromptShown = true;
    try {
      const { DataStoreProvider } = ChromeUtils.importESModule(
        "resource://noraneko/modules/pwa/DataStore.sys.mjs",
      );
      await DataStoreProvider.getDataManager().saveSsbData(ssb);
    } catch (e) {
      console.error("[WindowsSupport] Failed to save pinPromptShown:", e);
    }

    await new Promise((resolve) => setTimeout(resolve, 2000));

    const displayName = getSsbDisplayName(ssb);
    const shortcutName = `${displayName}.lnk`;

    try {
      const startMenu = `${
        Services.dirsvc.get("Home", Ci.nsIFile).path
      }\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu\\Programs\\`;
      const shortcutPath = startMenu + shortcutName;

      if (!(await IOUtils.exists(shortcutPath))) {
        await this.createShortcutForSsb(ssb);
      }

      await WindowsSupport.shellService.pinShortcutToTaskbar(
        this.buildGroupId(ssb.id),
        WindowsSupport.SHORTCUT_FOLDER,
        shortcutName,
      );
      console.debug(`[WindowsSupport] Triggered pin dialog for ${ssb.name}`);
    } catch (e) {
      console.warn(`[WindowsSupport] Failed to trigger pin dialog: ${e}`);
    }
  }
}
