/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { Browser, Icon, Manifest } from "./type.js";
import { IconProcesser } from "./iconProcesser";

const { ManifestObtainer } = ChromeUtils.importESModule(
  "resource://gre/modules/ManifestObtainer.sys.mjs",
);

const { ManifestProcessor } = ChromeUtils.importESModule(
  "resource://gre/modules/ManifestProcessor.sys.mjs",
);

export class ManifestProcesser {
  private iconProcesser: IconProcesser;

  constructor() {
    this.iconProcesser = new IconProcesser();
  }

  private get uuid() {
    return Services.uuid.generateUUID().toString();
  }

  /**
   * Generates an app manifest for Browser. It should be called if Browser has a manifest file.
   * If you want generate SSB Manifest file, You should call generateManifestForBrowser.
   *
   * @param {Browser} browser the browser element the site is loaded in.
   * @returns {Promise<Manifest>} an app manifest.
   */
  public async getManifestFromBrowser(
    browser: Browser,
    useWebManifest: boolean,
  ): Promise<Manifest | null> {
    let manifest = null;
    try {
      if (useWebManifest) {
        manifest = await ManifestObtainer.browserObtainManifest(browser);
      }
    } catch {
      throw new Error("Failed to obtain manifest from browser");
    }

    // YouTube returns monoChrome icon at default. Floorp(Noraneko) will reject monoChrome icon.
    if (manifest?.icons && manifest.icons.length !== 1) {
      manifest.icons = manifest.icons.filter((icon: Icon) => {
        return !icon.purpose?.includes("monochrome");
      });
    }

    // Reject the manifest if its scope doesn't include the current document.
    // This follow to w3c document. We need to generate web manifest from scope.
    if (
      !manifest ||
      !this.scopeIncludes(
        Services.io.newURI(manifest.scope),
        browser.currentURI,
      )
    ) {
      manifest = this.generateManifestForURI(browser.currentURI);
    }

    if (!manifest) {
      return null;
    }

    // Cache all the icons as data URIs since we can need access to them when
    // the website is not loaded.
    manifest.icons = (
      await Promise.all(
        manifest.icons.map(async (icon: Icon) => {
          if (icon.src.startsWith("data:")) {
            return icon;
          }

          const actor = browser.browsingContext.currentWindowGlobal.getActor(
            "NRProgressiveWebApp",
          );
          try {
            icon.src = await actor.sendQuery("LoadIcon", icon.src);
          } catch {
            // If the actor fails, try to fetch the icon directly in the parent process
            // This handles cases where the actor is destroyed (race condition)
            try {
              icon.src = await this.fetchIconAsDataURI(icon.src);
            } catch {
              // Bad icon, drop it from the list.
              return null;
            }
          }

          return icon;
        }),
      )
    ).filter((icon) => icon);

    // If the site provided no icons then try to use the normal page icons.
    if (!manifest.icons.length) {
      const icon = await this.iconProcesser.getIconForBrowser(browser);
      manifest.icons.push({ src: icon });
    }

    if (!manifest.name) {
      manifest.name = browser.contentTitle;
    }

    manifest.id = this.uuid;

    // Find the largest icon
    let bestIcon = manifest.icons[0];
    let bestSize = 0;

    for (const icon of manifest.icons) {
      // Ensure icon.sizes is an array of strings
      let sizes: string[] = [];
      if (Array.isArray(icon.sizes)) {
        sizes = icon.sizes;
      } else if (typeof icon.sizes === "string") {
        // Parse space-separated sizes string (e.g., "72x72 128x128")
        sizes = (icon.sizes as string).trim().split(/\s+/);
      } else {
        // If no sizes provided, consider it as a candidate if we haven't found anything better.
        // Assuming unknown size is small unless we have nothing else.
        if (bestSize === 0 && bestIcon === manifest.icons[0]) {
          // Keep the first icon as fallback, but don't update bestSize/bestIcon explicitly
          // unless we want to prioritize "no size" over "tiny size".
          // For now, just continue.
        }
        continue;
      }

      if (sizes.length > 0) {
        for (const sizeStr of sizes) {
          if (sizeStr === "any") {
            // Prefer "any" size if we haven't found a very large icon yet
            // SVG or high-res fallback often use "any"
            if (bestSize < 1024 * 1024) {
              bestSize = 1024 * 1024; // Treat as very large
              bestIcon = icon;
            }
            continue;
          }
          const [w, h] = sizeStr.split("x").map((s: string) => parseInt(s, 10));
          if (!isNaN(w) && !isNaN(h)) {
            let size = w * h;

            // For desktop environments (Linux/Windows), maskable icons often have excessive padding
            // which makes the icon look small. We apply a penalty to maskable icons to prefer
            // 'any' icons unless the maskable one is significantly larger.
            if (icon.purpose?.includes("maskable")) {
              size = size * 0.1;
            }

            if (size > bestSize) {
              bestSize = size;
              bestIcon = icon;
            }
          }
        }
      }
    }

    manifest.icon = bestIcon.src;
    return manifest;
  }

  private async fetchIconAsDataURI(url: string): Promise<string> {
    const response = await fetch(url, { credentials: "omit" });
    if (!response.ok)
      throw new Error(`Failed to fetch icon: ${response.status}`);
    const blob = await response.blob();
    return new Promise((resolve, reject) => {
      const reader = new FileReader();
      reader.onloadend = () => resolve(reader.result as string);
      reader.onerror = reject;
      reader.readAsDataURL(blob);
    });
  }

  public async checkBrowserCanBeInstallAsPwa(browser: Browser) {
    const manifest = await ManifestObtainer.browserObtainManifest(browser);
    return manifest !== null;
  }

  /**
   * Tests whether an app manifest's scope includes the given URI.
   * @param {nsIURI} scope the manifest's scope.
   * @param {nsIURI} uri the URI to test.
   */
  private scopeIncludes(scope: nsIURI, uri: nsIURI) {
    // https://w3c.github.io/manifest/#dfn-within-scope
    if (scope.prePath !== uri.prePath) {
      return false;
    }
    return uri.filePath.startsWith(scope.filePath);
  }

  /**
   * Generates a basic app manifest for a URI.
   * It should be called if user or Noraneko (Floorp) wants to generate manifest with not depending to Web Manifest.
   *
   * @param {nsIURI} uri the start URI for the site.
   * @returns {Manifest} an app manifest.
   */
  private generateManifestForURI(uri: nsIURI) {
    try {
      const manifestURI = Services.io.newURI("/manifest.json", "", uri);
      return ManifestProcessor.process({
        jsonText: "{}",
        manifestURL: manifestURI.spec,
        docURL: uri.spec,
      });
    } catch {
      // console.error(
      //   `Failed to generate a SSB (PWA) manifest for ${uri.spec}.`,
      //   e,
      // );
      return null;
    }
  }
}
