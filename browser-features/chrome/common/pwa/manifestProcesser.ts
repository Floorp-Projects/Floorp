/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type {
  Browser,
  Icon,
  Manifest,
  ProtocolHandler,
  FileHandler,
} from "./type.js";
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
    } catch (e) {
      throw new Error("Failed to obtain manifest from browser");
    }

    // YouTube returns monoChrome icon at default. Floorp(Noraneko) will reject monoChrome icon.
    if (manifest?.icons && manifest.icons.length !== 1) {
      const icons = manifest.icons;
      for (let i = 0; i < icons.length; i++) {
        if (icons[i].purpose.includes("monochrome")) {
          icons.splice(i, 1);
        }
      }
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

    const sanitizedProtocolHandlers = this.sanitizeProtocolHandlers(
      (manifest as Manifest).protocol_handlers,
    );
    if (sanitizedProtocolHandlers) {
      manifest.protocol_handlers = sanitizedProtocolHandlers;
    } else {
      delete (manifest as any).protocol_handlers;
    }
    const sanitizedFileHandlers = this.sanitizeFileHandlers(
      (manifest as Manifest).file_handlers,
    );
    if (sanitizedFileHandlers) {
      manifest.file_handlers = sanitizedFileHandlers;
    } else {
      delete (manifest as any).file_handlers;
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
          } catch (e) {
            // Bad icon, drop it from the list.
            return null;
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
    manifest.icon = manifest.icons[0].src;
    return manifest;
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

  private sanitizeProtocolHandlers(
    handlers: Manifest["protocol_handlers"],
  ): ProtocolHandler[] | undefined {
    if (!Array.isArray(handlers)) {
      return undefined;
    }
    const sanitized: ProtocolHandler[] = [];
    for (const handler of handlers) {
      if (!handler || typeof handler !== "object") {
        continue;
      }
      const protocol =
        typeof handler.protocol === "string" ? handler.protocol.trim() : "";
      const url =
        typeof handler.url === "string" ? handler.url.trim() : "";
      if (!protocol || !url) {
        continue;
      }
      sanitized.push({
        protocol,
        url,
      });
    }
    return sanitized.length ? sanitized : undefined;
  }

  private sanitizeFileHandlers(
    handlers: Manifest["file_handlers"],
  ): FileHandler[] | undefined {
    if (!Array.isArray(handlers)) {
      return undefined;
    }
    const sanitized: FileHandler[] = [];
    for (const handler of handlers) {
      if (!handler || typeof handler !== "object") {
        continue;
      }
      const action =
        typeof handler.action === "string" ? handler.action.trim() : "";
      if (!action) {
        continue;
      }

      const sanitizedHandler: FileHandler = { action };

      if (typeof handler.name === "string" && handler.name.trim()) {
        sanitizedHandler.name = handler.name.trim();
      }

      if (
        typeof handler.launch_type === "string" &&
        handler.launch_type.trim()
      ) {
        sanitizedHandler.launch_type = handler.launch_type.trim();
      }

      if (handler.accept && typeof handler.accept === "object") {
        const accept: Record<string, string[]> = {};
        for (const [mime, values] of Object.entries(handler.accept)) {
          if (!mime || !Array.isArray(values)) {
            continue;
          }
          const sanitizedValues = values
            .filter((value): value is string => typeof value === "string")
            .map((value) => value.trim())
            .filter((value) => value.length > 0);
          if (sanitizedValues.length > 0) {
            accept[mime.trim()] = sanitizedValues;
          }
        }
        if (Object.keys(accept).length > 0) {
          sanitizedHandler.accept = accept;
        }
      }

      sanitized.push(sanitizedHandler);
    }

    return sanitized.length ? sanitized : undefined;
  }
}
