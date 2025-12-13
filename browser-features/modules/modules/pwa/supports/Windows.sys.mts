/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { Manifest } from "../type.ts";

const { ImageTools } = ChromeUtils.importESModule(
  "resource://noraneko/modules/pwa/ImageTools.sys.mjs",
);

export class WindowsSupport {
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

  async install(ssb: Manifest) {
    const dir = PathUtils.join(PathUtils.profileDir, "ssb", ssb.id);
    await IOUtils.makeDirectory(dir, {
      ignoreExisting: true,
    });

    let iconFile: nsIFile | null = new WindowsSupport.nsIFile(
      PathUtils.join(dir, "icon.ico"),
    );

    // We should be embedding multiple icon sizes, but the current icon encoder
    // does not support this. For now just embed a sensible size.
    const icon = ssb.icon;
    if (icon) {
      const { container } = await ImageTools.loadImage(
        Services.io.newURI(icon),
      );
      ImageTools.saveIcon(container, 128, 128, iconFile);
    } else {
      iconFile = null;
    }

    const shortcutName = `${ssb.name}.lnk`;

    // Create shortcut in Start Menu
    if (iconFile) {
      WindowsSupport.shellService.createShortcut(
        Services.dirsvc.get("XREExeF", Ci.nsIFile),
        ["-profile", PathUtils.profileDir, "-start-ssb", ssb.id],
        ssb.name,
        iconFile,
        0,
        this.buildGroupId(ssb.id),
        "Programs",
        shortcutName,
      );
    } else {
      // Create shortcut without icon
      WindowsSupport.shellService.createShortcut(
        Services.dirsvc.get("XREExeF", Ci.nsIFile),
        ["-profile", PathUtils.profileDir, "-start-ssb", ssb.id],
        ssb.name,
        null as unknown as nsIFile,
        0,
        this.buildGroupId(ssb.id),
        "Programs",
        shortcutName,
      );
    }

    // Pin the shortcut to the taskbar (like TaskBarTabs)
    try {
      // Second argument is the shortcut path relative to Programs folder
      await WindowsSupport.shellService.pinShortcutToTaskbar(
        this.buildGroupId(ssb.id),
        shortcutName,
      );
      console.debug(`[WindowsSupport] Pinned PWA to taskbar: ${ssb.name}`);
    } catch (e) {
      // Pinning may fail if the shortcut doesn't exist yet or user denies
      console.warn(`[WindowsSupport] Failed to pin PWA to taskbar: ${e}`);
    }
  }

  /**
   * @param {SiteSpecificBrowser} ssb the SSB to uninstall.
   */
  async uninstall(ssb: Manifest) {
    const shortcutName = `${ssb.name}.lnk`;

    // Unpin from taskbar first (like TaskBarTabs)
    try {
      WindowsSupport.shellService.unpinShortcutFromTaskbar(shortcutName);
      console.debug(`[WindowsSupport] Unpinned PWA from taskbar: ${ssb.name}`);
    } catch (e) {
      // Unpinning may fail if not pinned
      console.warn(`[WindowsSupport] Failed to unpin PWA from taskbar: ${e}`);
    }

    // Remove shortcut from Start Menu
    try {
      const startMenu = `${
        Services.dirsvc.get("Home", Ci.nsIFile).path
      }\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu\\Programs\\`;
      await IOUtils.remove(`${startMenu + ssb.name}.lnk`);
    } catch (e) {
      console.error(e);
    }

    // Remove icon and data directory
    const dir = PathUtils.join(PathUtils.profileDir, "ssb", ssb.id);
    try {
      await IOUtils.remove(dir, { recursive: true });
    } catch (e) {
      console.error(e);
    }
  }

  /**
   * Applies the necessary OS integration to an open SSB.
   *
   * Sets the window icon based on the available icons.
   *
   * @param {SiteSpecificBrowser} ssb the SSB.
   * @param {DOMWindow} aWindow the window showing the SSB.
   */
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
  }
}
