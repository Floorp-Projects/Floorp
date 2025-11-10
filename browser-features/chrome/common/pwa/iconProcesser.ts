/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { Browser, Icon, IconData } from "./type.ts";
import { ImageTools } from "./imageTools.ts";

const { PlacesUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/PlacesUtils.sys.mjs",
);
const { NetUtil } = ChromeUtils.importESModule(
  "resource://gre/modules/NetUtil.sys.mjs",
);

export class IconProcesser {
  private static instance: IconProcesser;
  public static getInstance() {
    if (!IconProcesser.instance) {
      IconProcesser.instance = new IconProcesser();
    }
    return IconProcesser.instance;
  }

  buildIconList(icons: Icon[]) {
    const iconList = [];

    for (const icon of icons) {
      for (const sizeSpec of icon.sizes) {
        const size =
          sizeSpec === "any"
            ? Number.MAX_SAFE_INTEGER
            : Number.parseInt(sizeSpec);

        iconList.push({
          icon,
          size,
        });
      }
    }

    iconList.sort((a, b) => {
      // Given that we're using MAX_SAFE_INTEGER adding a value to that would
      // overflow and give odd behaviour. And we're using numbers supplied by a
      // website so just compare for safety.
      if (a.size < b.size) {
        return -1;
      }

      if (a.size > b.size) {
        return 1;
      }

      return 0;
    });
    return iconList;
  }

  /**
   * Creates an IconResource from the LinkHandler data.
   *
   * @param {IconData} iconData the data from the LinkHandler actor.
   * @returns {Promise<IconResource>} an icon resource.
   */
  async getIconResource(iconData: IconData): Promise<Icon> {
    // This should be a data url so no network traffic.
    const imageData = await ImageTools.loadImage(
      Services.io.newURI(iconData.iconURL),
    );
    if (imageData.container.type === Ci.imgIContainer.TYPE_VECTOR) {
      return {
        src: iconData.iconURL,
        purpose: ["any"],
        type: imageData.type,
        sizes: ["any"],
      };
    }

    return {
      src: iconData.iconURL,
      purpose: ["any"],
      type: imageData.type,
      sizes: [`${imageData.container.width}x${imageData.container.height}`],
    };
  }

  public async getIconForBrowser(browser: Browser): Promise<string> {
    const gFavicons = PlacesUtils.favicons as {
      getFaviconForPage: (uri: nsIURI) => Promise<{
        dataURI: nsIURI;
        mimeType: string;
        rawData?: ArrayBuffer | Uint8Array;
      } | null>;
      defaultFavicon: nsIURI;
    };

    const targetUri = Services.io.newURI(browser.currentURI.spec);
    const container = await this.getFaviconContainer(targetUri, gFavicons);

    if (!container) {
      return "";
    }

    try {
      return await this.containerToDataURL(container);
    } catch (scaleErr) {
      console.warn(
        "[PWA] Icon scaling failed, returning empty icon.",
        scaleErr,
      );
      return "";
    }
  }

  private async containerToDataURL(container: imgIContainer): Promise<string> {
    const blob = (await ImageTools.scaleImage(container, 64, 64)) as Blob;
    return await new Promise<string>((resolve, reject) => {
      const reader = new FileReader();
      reader.onloadend = () => resolve(reader.result as string);
      reader.onerror = () => reject(reader.error);
      reader.readAsDataURL(blob);
    });
  }

  private arrayBufferToDataUri(buffer: ArrayBuffer, mimeType: string) {
    const bytes = new Uint8Array(buffer);
    let binary = "";
    const chunk = 0x8000;
    for (let i = 0; i < bytes.length; i += chunk) {
      const subArray = bytes.subarray(i, i + chunk);
      binary += String.fromCharCode(...subArray);
    }
    return `data:${mimeType};base64,${btoa(binary)}`;
  }

  private async getFaviconContainer(
    uri: nsIURI,
    gFavicons: {
      getFaviconForPage: (target: nsIURI) => Promise<{
        dataURI: nsIURI;
        mimeType: string;
        rawData?: ArrayBuffer | Uint8Array;
      } | null>;
      defaultFavicon: nsIURI;
    },
  ): Promise<imgIContainer | null> {
    try {
      const favicon = await gFavicons.getFaviconForPage(uri);
      if (favicon) {
        if (favicon.mimeType !== "image/svg+xml" && favicon.rawData) {
          const rawArray =
            favicon.rawData instanceof Uint8Array
              ? favicon.rawData
              : new Uint8Array(favicon.rawData);
          const buffer = rawArray.slice(0, rawArray.byteLength).buffer;
          const dataUriSpec = this.arrayBufferToDataUri(
            buffer,
            favicon.mimeType,
          );
          const { container } = await ImageTools.loadImage(
            Services.io.newURI(dataUriSpec),
          );
          return container;
        }
        const { container } = await ImageTools.loadImage(favicon.dataURI);
        return container;
      }
    } catch (e) {
      console.error("[PWA] Failed to retrieve favicon:", e);
    }

    return await this.loadDefaultFavicon(gFavicons.defaultFavicon);
  }

  private async loadDefaultFavicon(uri: nsIURI): Promise<imgIContainer | null> {
    try {
      const { buffer, contentType } = await this.readUriToArrayBuffer(uri);
      const dataUriSpec = this.arrayBufferToDataUri(buffer, contentType);
      const { container } = await ImageTools.loadImage(
        Services.io.newURI(dataUriSpec),
      );
      return container;
    } catch (e) {
      console.error("[PWA] Failed to load default favicon:", e);
      return null;
    }
  }

  private readUriToArrayBuffer(uri: nsIURI) {
    return new Promise<{ buffer: ArrayBuffer; contentType: string }>(
      (resolve, reject) => {
        try {
          const channel = NetUtil.newChannel({
            uri,
            loadUsingSystemPrincipal: true,
            contentPolicyType: Ci.nsIContentPolicy.TYPE_IMAGE,
          });
          NetUtil.asyncFetch(channel, (inputStream, status) => {
            if (!Components.isSuccessCode(status)) {
              reject(
                Components.Exception("Failed to fetch local resource.", status),
              );
              return;
            }

            try {
              const count = inputStream.available();
              const bytes = NetUtil.readInputStreamToArray(inputStream, count);
              inputStream.close();
              const typedArray = new Uint8Array(bytes);
              const buffer = typedArray.buffer.slice(
                typedArray.byteOffset,
                typedArray.byteOffset + typedArray.byteLength,
              );
              resolve({
                buffer,
                contentType: channel.contentType || "image/png",
              });
            } catch (readErr) {
              reject(readErr);
            }
          });
        } catch (err) {
          reject(err);
        }
      },
    );
  }
}
