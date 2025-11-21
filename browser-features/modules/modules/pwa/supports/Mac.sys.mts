/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { Manifest } from "../type.ts";

const { ImageTools } = ChromeUtils.importESModule(
  "resource://noraneko/modules/pwa/ImageTools.sys.mjs",
);

type MacPwaService = {
  createOrUpdateApp(
    bundlePath: string,
    appName: string,
    bundleIdentifier: string,
    appId: string,
    startUrl: string,
    floorpExecutablePath: string,
    profileDir: string,
    profileName: string,
    versionString: string,
    iconSourcePath: string,
  ): void;
  removeApp(bundlePath: string): void;
};

export class MacSupport {
  private static macService: MacPwaService | null = null;
  private static cachedBaseDir: string | null = null;

  private static nsIFile = Components.Constructor(
    "@mozilla.org/file/local;1",
    Ci.nsIFile,
    "initWithPath",
  );

  private static get service(): MacPwaService {
    if (!this.macService) {
      const ciWithFloorp = Ci as typeof Ci & {
        nsIFloorpMacPwa: nsIID;
      };
      this.macService = Cc["@noraneko.org/pwa/mac-app-support;1"].getService(
        ciWithFloorp.nsIFloorpMacPwa,
      ) as MacPwaService;
    }
    return this.macService;
  }

  private static getBaseDir(): string {
    if (this.cachedBaseDir) {
      return this.cachedBaseDir;
    }

    const homeDir = (Services.dirsvc.get("Home", Ci.nsIFile) as nsIFile).path;
    const baseDir = PathUtils.join(homeDir, "Applications", "Floorp Apps");
    this.cachedBaseDir = baseDir;
    return baseDir;
  }

  private static normalizeToken(raw: string) {
    return raw
      .toLowerCase()
      .replace(/[^a-z0-9]+/g, "-")
      .replace(/^-+|-+$/g, "")
      .slice(0, 64);
  }

  private static getSlug(ssb: Manifest) {
    const candidates = [ssb.id, ssb.name, ssb.start_url];
    for (const candidate of candidates) {
      if (candidate) {
        const slug = this.normalizeToken(candidate);
        if (slug.length) {
          return slug;
        }
      }
    }
    return Services.uuid
      .generateUUID()
      .toString()
      .replace(/[{}-]/g, "")
      .slice(0, 12);
  }

  private static getBundlePath(slug: string) {
    return PathUtils.join(this.getBaseDir(), `${slug}.app`);
  }

  private static getBundleIdentifier(slug: string) {
    return `one.ablaze.floorp.ssb.${slug}`;
  }

  private static async prepareIcon(ssb: Manifest, slug: string) {
    if (!ssb.icon) {
      return "";
    }

    try {
      const iconDir = PathUtils.join(PathUtils.profileDir, "ssb-icons", slug);
      await IOUtils.makeDirectory(iconDir, {
        createAncestors: true,
        ignoreExisting: true,
      });

      const iconPath = PathUtils.join(iconDir, "icon.png");
      const iconFile = new this.nsIFile(iconPath);
      const iconURI = Services.io.newURI(ssb.icon);
      const savedPath = await ImageTools.saveIconForPlatform(
        iconURI,
        iconFile,
        256,
        256,
      );
      return savedPath ?? "";
    } catch (error) {
      console.error(
        "[MacSupport] Failed to prepare icon for macOS bundle",
        error,
      );
      return "";
    }
  }

  private static async cleanupIcon(slug: string) {
    const iconDir = PathUtils.join(PathUtils.profileDir, "ssb-icons", slug);
    try {
      await IOUtils.remove(iconDir, { recursive: true, ignoreAbsent: true });
    } catch (error) {
      console.warn("[MacSupport] Failed to remove icon directory", error);
    }
  }

  async install(ssb: Manifest) {
    const slug = MacSupport.getSlug(ssb);
    const bundlePath = MacSupport.getBundlePath(slug);
    const bundleDir = PathUtils.parent(bundlePath) ?? MacSupport.getBaseDir();
    await IOUtils.makeDirectory(bundleDir, {
      createAncestors: true,
      ignoreExisting: true,
    });

    const iconPath = await MacSupport.prepareIcon(ssb, slug);
    const floorpExecutable = (
      Services.dirsvc.get("XREExeF", Ci.nsIFile) as nsIFile
    ).path;
    const profileName = (Services.dirsvc.get("ProfD", Ci.nsIFile) as nsIFile)
      .leafName;

    MacSupport.service.createOrUpdateApp(
      bundlePath,
      ssb.name,
      MacSupport.getBundleIdentifier(slug),
      ssb.id,
      ssb.start_url,
      floorpExecutable,
      PathUtils.profileDir,
      profileName,
      Services.appinfo.version,
      iconPath ?? "",
    );
  }

  async uninstall(ssb: Manifest) {
    const slug = MacSupport.getSlug(ssb);
    const bundlePath = MacSupport.getBundlePath(slug);
    try {
      MacSupport.service.removeApp(bundlePath);
    } catch (error) {
      console.error("[MacSupport] Failed to remove macOS bundle", error);
    }
    await MacSupport.cleanupIcon(slug);
  }

  async applyOSIntegration(_ssb: Manifest) {
    // Launching the .app bundle already provides Dock / Cmd+Tab integration.
    // No additional runtime tweaks are required for macOS at this stage.
  }
}
