/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { Manifest } from "../type.ts";

const { ImageTools } = ChromeUtils.importESModule(
  "resource://noraneko/modules/pwa/ImageTools.sys.mjs",
);

const { ShellService } = ChromeUtils.importESModule(
  "resource:///modules/ShellService.sys.mjs",
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
      const { container } = await ImageTools.loadImage(
        Services.io.newURI(icon),
      );
      await ShellService.writeShortcutIcon(iconFile, container);
    } else {
      iconFile = null;
    }

    const shortcutName = `${ssb.name}.lnk`;
    WindowsSupport.shellService.createShortcut(
      Services.dirsvc.get("XREExeF", Ci.nsIFile),
      ["-profile", PathUtils.profileDir, "-start-ssb", ssb.id],
      ssb.name,
      iconFile ?? (null as unknown as nsIFile),
      0,
      this.buildGroupId(ssb.id),
      WindowsSupport.SHORTCUT_FOLDER,
      shortcutName,
    );

    return shortcutName;
  }

  async install(ssb: Manifest) {
    const shortcutName = await this.createShortcutForSsb(ssb);

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
  }

  async uninstall(ssb: Manifest) {
    const shortcutName = `${ssb.name}.lnk`;

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
      await IOUtils.remove(`${startMenu + ssb.name}.lnk`);
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
    const getIcon = async (_size: number) => {
      const icon = ssb.icon;
      if (!icon) {
        return null;
      }

      try {
        const image = await ImageTools.loadImage(Services.io.newURI(icon));
        return image.container;
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

    const shortcutName = `${ssb.name}.lnk`;

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
