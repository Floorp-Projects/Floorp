/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { Manifest } from "../type.ts";

const { ImageTools } = ChromeUtils.importESModule(
  "resource://noraneko/modules/pwa/ImageTools.sys.mjs",
);

export class MacSupport {
  private static readonly MAC_DOCK_SUPPORT_CONTRACT =
    "@mozilla.org/widget/macdocksupport;1";
  private static readonly MAC_WEB_APP_UTILS_CONTRACT =
    "@mozilla.org/widget/mac-web-app-utils;1";

  private static dockSupport: nsIMacDockSupport | null = (() => {
    try {
      return Cc[MacSupport.MAC_DOCK_SUPPORT_CONTRACT].getService(
        Ci.nsIMacDockSupport,
      );
    } catch (_error) {
      return null;
    }
  })();

  private static macWebAppUtils: nsIMacWebAppUtils | null = (() => {
    try {
      return Cc[MacSupport.MAC_WEB_APP_UTILS_CONTRACT].getService(
        Ci.nsIMacWebAppUtils,
      );
    } catch (_error) {
      return null;
    }
  })();

  private buildBundleId(id: string) {
    return `one.ablaze.floorp.ssb.${id}`;
  }

  private static sanitizeBundleLeaf(ssb: Manifest) {
    const base = `${ssb.name || "FloorpSSB"}-${ssb.id}`;
    const safe = base.replace(/[^A-Za-z0-9._-]+/g, "-");
    return `${safe}.app`;
  }

  private static getInstallRoot(): string {
    const home = Services.dirsvc.get("Home", Ci.nsIFile).path;
    return PathUtils.join(home, "Applications", "Floorp SSBs");
  }

  private getBundleRoot(ssb: Manifest) {
    return PathUtils.join(
      MacSupport.getInstallRoot(),
      MacSupport.sanitizeBundleLeaf(ssb),
    );
  }

  private getContentsDir(bundleRoot: string) {
    return PathUtils.join(bundleRoot, "Contents");
  }

  private getMacOSDir(bundleRoot: string) {
    return PathUtils.join(this.getContentsDir(bundleRoot), "MacOS");
  }

  private getResourcesDir(bundleRoot: string) {
    return PathUtils.join(this.getContentsDir(bundleRoot), "Resources");
  }

  private plistPath(bundleRoot: string) {
    return PathUtils.join(this.getContentsDir(bundleRoot), "Info.plist");
  }

  private executablePath(bundleRoot: string) {
    return PathUtils.join(this.getMacOSDir(bundleRoot), "floorp-ssb");
  }

  private escapePlist(value: string) {
    return value
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;")
      .replace(/"/g, "&quot;")
      .replace(/'/g, "&apos;");
  }

  private async ensureDirectory(path: string) {
    if (!(await IOUtils.exists(path))) {
      await IOUtils.makeDirectory(path, {
        ignoreExisting: true,
      });
    }
  }

  private async writeExecutable(ssb: Manifest, bundleRoot: string) {
    const executable = this.executablePath(bundleRoot);
    const floorpBinary = Services.dirsvc.get("XREExeF", Ci.nsIFile).path;
    const script = `#!/bin/sh
exec "${floorpBinary.replace(/"/g, '\\"')}" -profile "${PathUtils.profileDir}" -start-ssb "${ssb.id}" "$@"
`;
    await IOUtils.writeUTF8(executable, script);
    await IOUtils.setPermissions(executable, 0o755);
  }

  private async writeInfoPlist(
    ssb: Manifest,
    bundleRoot: string,
    iconFileName: string | null,
  ) {
    const infoPlistPath = this.plistPath(bundleRoot);
    const displayName = ssb.name || ssb.id;
    const iconEntry = iconFileName
      ? `<key>CFBundleIconFile</key>
  <string>${this.escapePlist(iconFileName)}</string>
  <key>CFBundleIconName</key>
  <string>${this.escapePlist(iconFileName.replace(/\.[^.]+$/, ""))}</string>
`
      : "";
    const infoPlist = `<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleName</key>
  <string>${this.escapePlist(displayName)}</string>
  <key>CFBundleDisplayName</key>
  <string>${this.escapePlist(displayName)}</string>
  <key>CFBundleIdentifier</key>
  <string>${this.escapePlist(this.buildBundleId(ssb.id))}</string>
  <key>CFBundleExecutable</key>
  <string>floorp-ssb</string>
  <key>CFBundlePackageType</key>
  <string>APPL</string>
  <key>LSBackgroundOnly</key>
  <false/>
  <key>LSMultipleInstancesProhibited</key>
  <true/>
  <key>LSApplicationCategoryType</key>
  <string>public.app-category.productivity</string>
  <key>LSMinimumSystemVersion</key>
  <string>10.15</string>
  ${iconEntry}  <key>CFBundleShortVersionString</key>
  <string>1.0</string>
  <key>CFBundleVersion</key>
  <string>1.0</string>
  <key>NSHighResolutionCapable</key>
  <true/>
</dict>
</plist>
`;
    await IOUtils.writeUTF8(infoPlistPath, infoPlist);
  }

  private async writeIcon(
    ssb: Manifest,
    bundleRoot: string,
  ): Promise<string | null> {
    if (!ssb.icon) {
      return null;
    }

    try {
      const { container } = await ImageTools.loadImage(
        Services.io.newURI(ssb.icon),
      );
      const blob = await ImageTools.scaleImage(container, 512, 512);
      const arrayBuffer = await blob.arrayBuffer();
      const resourcesDir = this.getResourcesDir(bundleRoot);
      await this.ensureDirectory(resourcesDir);
      const iconPath = PathUtils.join(resourcesDir, "icon.png");
      await IOUtils.write(iconPath, new Uint8Array(arrayBuffer));
      return "icon.png";
    } catch (error) {
      console.error("Failed to write macOS icon for SSB", error);
      return null;
    }
  }

  private registerWithLaunchServices(_bundleRoot: string, ssb: Manifest) {
    try {
      MacSupport.macWebAppUtils?.pathForAppWithIdentifier(
        this.buildBundleId(ssb.id),
      );
    } catch (error) {
      console.debug("Failed to update LaunchServices metadata for SSB", error);
    }
  }

  private async unregisterWithLaunchServices(bundleRoot: string) {
    if (!MacSupport.macWebAppUtils) {
      return false;
    }

    return await new Promise<boolean>((resolve) => {
      try {
        MacSupport.macWebAppUtils!.trashApp(bundleRoot, {
          trashAppFinished: (rv: nsresult) => {
            resolve(Components.isSuccessCode(rv));
          },
        });
      } catch (error) {
        console.debug(
          "Failed to move SSB bundle to trash via LaunchServices",
          error,
        );
        resolve(false);
      }
    });
  }

  async install(ssb: Manifest) {
    const installRoot = MacSupport.getInstallRoot();
    await this.ensureDirectory(installRoot);

    const bundleRoot = this.getBundleRoot(ssb);
    await this.ensureDirectory(bundleRoot);
    await this.ensureDirectory(this.getContentsDir(bundleRoot));
    await this.ensureDirectory(this.getMacOSDir(bundleRoot));
    await this.ensureDirectory(this.getResourcesDir(bundleRoot));

    const iconFileName = await this.writeIcon(ssb, bundleRoot);
    await this.writeInfoPlist(ssb, bundleRoot, iconFileName);
    await this.writeExecutable(ssb, bundleRoot);

    this.registerWithLaunchServices(bundleRoot, ssb);
  }

  async uninstall(ssb: Manifest) {
    const bundleRoot = this.getBundleRoot(ssb);
    let removed = false;
    try {
      removed = await this.unregisterWithLaunchServices(bundleRoot);
    } catch (error) {
      console.debug("LaunchServices cleanup threw during SSB uninstall", error);
    }

    if (removed) {
      return;
    }

    try {
      await IOUtils.remove(bundleRoot, { recursive: true });
    } catch (error) {
      console.error("Failed to uninstall macOS SSB bundle", error);
    }
  }

  async applyOSIntegration(ssb: Manifest, _aWindow: Window) {
    if (!ssb.icon || !MacSupport.dockSupport) {
      return;
    }

    try {
      const { container } = await ImageTools.loadImage(
        Services.io.newURI(ssb.icon),
      );
      MacSupport.dockSupport.setBadgeImage(container);
      MacSupport.dockSupport.badgeText = "";
    } catch (error) {
      console.error("Failed to update Dock icon for SSB", error);
    }
  }
}
