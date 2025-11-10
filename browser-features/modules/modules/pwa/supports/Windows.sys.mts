/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { Manifest } from "../type.ts";

const { ImageTools } = ChromeUtils.importESModule(
  "resource://noraneko/modules/pwa/ImageTools.sys.mjs",
);
const { ShellService } = ChromeUtils.importESModule(
  "moz-src:///browser/components/shell/ShellService.sys.mjs",
);

export class WindowsSupport {
  private static shellService = ShellService;

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

  private static readonly MAX_APP_USER_MODEL_ID_LENGTH = 127;
  private buildGroupId(id: string) {
    const safeId = this.sanitizeAppId(id);
    const baseId = `ablaze.floorp.ssb.${safeId}`;
    if (baseId.length <= WindowsSupport.MAX_APP_USER_MODEL_ID_LENGTH) {
      return baseId;
    }
    return baseId.slice(0, WindowsSupport.MAX_APP_USER_MODEL_ID_LENGTH);
  }

  private sanitizeAppId(id: string) {
    const sanitized = id.replace(/[^A-Za-z0-9.]/g, "-");
    if (sanitized.length) {
      return sanitized;
    }
    return Services.uuid.generateUUID().toString().slice(1, -1);
  }

  private getShortcutRelativePath(name: string, id: string) {
    const directory = this.sanitizeFilename("Floorp PWAs", {
      allowDirectoryNames: true,
    });
    const baseName = this.sanitizeFilename(`${name}.lnk`);
    const uniqueSuffix = this.sanitizeFilename(id.replace(/[^A-Za-z0-9]/g, ""));
    if (!uniqueSuffix) {
      return `${directory}\\${baseName}`;
    }
    return `${directory}\\${uniqueSuffix}-${baseName}`;
  }

  private sanitizeFilename(
    wantedName: string,
    { allowDirectoryNames = false }: { allowDirectoryNames?: boolean } = {},
  ) {
    const mimeService = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);
    let flags =
      Ci.nsIMIMEService.VALIDATE_SANITIZE_ONLY |
      Ci.nsIMIMEService.VALIDATE_DONT_COLLAPSE_WHITESPACE |
      Ci.nsIMIMEService.VALIDATE_ALLOW_INVALID_FILENAMES;

    if (allowDirectoryNames) {
      flags |= Ci.nsIMIMEService.VALIDATE_ALLOW_DIRECTORY_NAMES;
    }

    return mimeService.validateFileNameForSaving(wantedName, "", flags);
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
      try {
        await WindowsSupport.shellService.createWindowsIcon(
          iconFile,
          container,
        );
      } catch (e) {
        console.error("Failed to create Windows icon for SSB:", e);
        iconFile = null;
      }
    } else {
      iconFile = null;
    }

    const relativePath = this.getShortcutRelativePath(ssb.name, ssb.id);

    await WindowsSupport.shellService.createShortcut(
      Services.dirsvc.get("XREExeF", Ci.nsIFile),
      ["-profile", PathUtils.profileDir, "-start-ssb", ssb.id],
      ssb.name,
      iconFile,
      0,
      this.buildGroupId(ssb.id),
      "Programs",
      relativePath,
    );
  }

  /**
   * @param {SiteSpecificBrowser} ssb the SSB to uninstall.
   */
  async uninstall(ssb: Manifest) {
    const relativePath = this.getShortcutRelativePath(ssb.name, ssb.id);

    const dir = PathUtils.join(PathUtils.profileDir, "ssb", ssb.id);
    try {
      await WindowsSupport.shellService.deleteShortcut(
        "Programs",
        relativePath,
      );
    } catch (e) {
      console.error("Failed to delete Windows shortcut for SSB:", e);
    }

    try {
      await IOUtils.remove(dir, { recursive: true, ignoreAbsent: true });
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
