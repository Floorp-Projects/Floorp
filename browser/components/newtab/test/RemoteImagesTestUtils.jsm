/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { BrowserUtils } = ChromeUtils.import(
  "resource://gre/modules/BrowserUtils.jsm"
);
const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");
const { RemoteImages } = ChromeUtils.import(
  "resource://activity-stream/lib/RemoteImages.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const RS_SERVER_PREF = "services.settings.server";

Cu.importGlobalProperties(["fetch"]);

const RemoteImagesTestUtils = {
  /**
   * Serve a mock Remote Settings server with content for Remote Images
   *
   * @params imageInfo An entry describing the image. Should be one of
   *         |RemoteImagesTestUtils.images|.
   *
   * @returns A promise yielding a cleanup function. This function will stop the
   *          internal HTTP server and clean up all Remote Images state.
   */
  async serveRemoteImages(imageInfo) {
    const { imageId, recordId, mimetype, hash, url, size } = imageInfo;

    const server = new HttpServer();
    server.start(-1);

    const baseURL = `http://localhost:${server.identity.primaryPort}/`;

    const arrayBuffer = await fetch(url, { credentials: "omit" }).then(rsp =>
      rsp.arrayBuffer()
    );

    server.registerPathHandler("/v1/", (request, response) => {
      response.write(
        JSON.stringify({
          capabilities: {
            attachments: {
              base_url: `${baseURL}cdn`,
            },
          },
        })
      );

      response.setHeader("Content-Type", "application/json; charset=UTF-8");
      response.setStatusLine(null, 200, "OK");
    });

    server.registerPathHandler(
      `/v1/buckets/main/collections/ms-images/records/${recordId}`,
      (request, response) => {
        response.write(
          JSON.stringify({
            data: {
              attachment: {
                filename: imageId,
                location: `main/ms-images/${imageId}`,
                hash,
                mimetype,
                size,
              },
            },
            id: "image-id",
            last_modified: Date.now(),
          })
        );

        response.setHeader("Content-Type", "application/json; charset=UTF-8");
        response.setStatusLine(null, 200, "OK");
      }
    );

    server.registerPathHandler(
      `/cdn/main/ms-images/${imageId}`,
      async (request, response) => {
        const stream = Cc[
          "@mozilla.org/io/arraybuffer-input-stream;1"
        ].createInstance(Ci.nsIArrayBufferInputStream);
        stream.setData(arrayBuffer, 0, arrayBuffer.byteLength);

        response.bodyOutputStream.writeFrom(stream, size);
        response.setHeader("Content-Type", mimetype);
        response.setStatusLine(null, 200, "OK");
      }
    );

    Services.prefs.setCharPref(RS_SERVER_PREF, `${baseURL}v1`);

    return async () => {
      await new Promise(resolve => server.stop(() => resolve()));
      Services.prefs.clearUserPref(RS_SERVER_PREF);
      await RemoteImagesTestUtils.wipeCache();
    };
  },

  /**
   * Wipe the Remote Images cache.
   */
  async wipeCache() {
    const children = await IOUtils.getChildren(RemoteImages.imagesDir);
    for (const child of children) {
      await IOUtils.remove(child);
    }
  },

  /**
   * Trigger RemoteImages cleanup.
   *
   * Remote Images cleanup is triggered by an observer, but its cleanup function
   * is async, so we must wait for a notification that cleanup is complete.
   */
  async triggerCleanup() {
    Services.obs.notifyObservers(null, "remote-settings:changes-poll-end");
    await BrowserUtils.promiseObserved("remote-images:cleanup-finished");
  },

  /**
   * Remote Image entries.
   */
  images: {
    AboutRobots: {
      imageId: "about-robots.png",
      recordId: "about-robots",
      mimetype: "image/png",
      hash: "29f1fe2cb5181152d2c01c0b2f12e5d9bb3379a61b94fb96de0f734eb360da62",
      url: "chrome://browser/content/aboutRobots-icon.png",
      size: 7599,
    },
  },
};

const EXPORTED_SYMBOLS = ["RemoteImagesTestUtils"];
