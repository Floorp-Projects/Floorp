/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");
const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");
const { RemoteImages, REMOTE_IMAGES_PATH } = ChromeUtils.import(
  "resource://activity-stream/lib/RemoteImages.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const RS_SERVER_PREF = "services.settings.server";

class RemoteSettingsRecord {
  constructor(id) {
    this.data = {
      id,
      last_modified: Date.now(),
    };
  }

  set(attrs) {
    const { id } = this.data;
    Object.assign(this.data, attrs, {
      id,
      last_modified: Date.now(),
    });
  }

  toJSON() {
    return { data: this.data };
  }
}

class RemoteSettingsCollection {
  get ChildType() {
    return RemoteSettingsRecord;
  }

  constructor(id) {
    this.data = {
      id,
      last_modified: Date.now(),
    };

    this.children = new Map();
  }

  get(id) {
    return this.children.get(id);
  }

  getOrCreate(id, { update = false } = {}) {
    return (update ? this.update(id) : this.get(id)) ?? this.create(id);
  }

  create(id) {
    if (this.get(id)) {
      throw new Error(`already have child ${id}`);
    }

    let child = new this.ChildType(id);
    this.children.set(id, child);
    this.data.last_modified = Date.now();
    return child;
  }

  update(id) {
    let child = this.get(id);
    if (child) {
      this.data.last_modified = Date.now();
    }
    return child;
  }

  toJSON() {
    return {
      data: Array.from(this.children.values()).map(child => ({
        id: child.data.id,
        last_modified: child.data.last_modified,
      })),
    };
  }
}

class RemoteSettingsBucket extends RemoteSettingsCollection {
  get ChildType() {
    return RemoteSettingsCollection;
  }
}

class RemoteSettingsRoot extends RemoteSettingsCollection {
  get ChildType() {
    return RemoteSettingsBucket;
  }
  constructor() {
    super("root");
  }
}

class RemoteSettingsAttachment {
  constructor(attrs) {
    Object.assign(this, attrs);
  }

  writeTo(response) {
    const stream = NetUtil.newChannel({
      uri: NetUtil.newURI(this.url),
      loadUsingSystemPrincipal: true,
    }).open();

    try {
      response.setHeader("Content-Type", this.mimetype);
      response.bodyOutputStream.writeFrom(stream, this.size);
      response.setStatusLine(null, 200, "OK");
    } finally {
      stream.close();
    }
  }
}

class RemoteSettingsServer {
  constructor() {
    this.server = new HttpServer();
    this.buckets = new RemoteSettingsRoot();
    this.attachments = new Map();

    this.server.registerPathHandler("/v1/", (request, response) => {
      response.setHeader("Content-Type", "application/json; charset=UTF-8");
      response.setStatusLine(null, 200, "OK");
      response.write(
        JSON.stringify({
          capabilities: {
            attachments: {
              base_url: `${this.baseURL}attachments`,
            },
          },
        })
      );
    });

    const recordRegex = new RegExp(
      "/v1/buckets/(?<bucketId>[^/]+)/collections/(?<collectionId>[^/]+)/records/(?<recordId>[^/]+)"
    );
    this.server.registerPrefixHandler("/v1/buckets/", (request, response) => {
      const match = recordRegex.exec(request.path);
      if (!match) {
        response.setStatusLine(null, 404, "Not Found");
        response.write("404");
        return;
      }

      const record = this.buckets
        .get(match.groups.bucketId)
        ?.get(match.groups.collectionId)
        ?.get(match.groups.recordId);
      if (!record) {
        response.setStatusLine(null, 404, "Not Found");
        response.write("404");
        return;
      }

      response.setHeader("Content-Type", "application/json; charset=UTF-8");
      response.setStatusLine(null, 200, "OK");
      response.write(JSON.stringify(record));
    });

    const ATTACHMENTS_PREFIX = "/attachments/";
    this.server.registerPrefixHandler(
      ATTACHMENTS_PREFIX,
      (request, response) => {
        const attachmentId = request.path.substring(ATTACHMENTS_PREFIX.length);
        const attachment = this.attachments.get(attachmentId);

        if (!attachment) {
          response.setStatusLine(null, 400, "Not Found");
          response.write("404");
          return;
        }

        attachment.writeTo(response);
      }
    );
  }

  start() {
    this.server.start(-1);
    Services.prefs.setCharPref(RS_SERVER_PREF, `${this.baseURL}v1`);
  }

  async stop() {
    await new Promise(resolve => this.server.stop(resolve));
    Services.prefs.clearUserPref(RS_SERVER_PREF);
  }

  get baseURL() {
    return `http://localhost:${this.server.identity.primaryPort}/`;
  }

  addRemoteImage(imageInfo) {
    const { filename, recordId, mimetype, hash, url, size } = imageInfo;

    const location = `main/ms-images/${recordId}`;

    this.buckets
      .getOrCreate("main", { update: true })
      .getOrCreate("ms-images", { update: true })
      .create(recordId)
      .set({
        attachment: {
          filename,
          location,
          hash,
          mimetype,
          size,
        },
      });

    this.attachments.set(
      location,
      new RemoteSettingsAttachment({
        mimetype,
        size,
        url,
      })
    );
  }
}

const RemoteImagesTestUtils = {
  /**
   * Serve a mock Remote Settings server with content for Remote Images
   *
   * @param imageInfo An entry describing the image. Should be one of
   *        |RemoteImagesTestUtils.images|.
   *
   * @returns A promise yielding a cleanup function. This function will stop the
   *          internal HTTP server and clean up all Remote Images state.
   */
  serveRemoteImages(...imageInfos) {
    const server = new RemoteSettingsServer();

    for (const imageInfo of imageInfos) {
      server.addRemoteImage(imageInfo);
    }

    server.start();

    return async () => {
      await server.stop();
      await RemoteImagesTestUtils.wipeCache();
    };
  },

  /**
   * Wipe the Remote Images cache.
   */
  async wipeCache() {
    await RemoteImages.reset();

    const children = await IOUtils.getChildren(REMOTE_IMAGES_PATH);
    for (const child of children) {
      await IOUtils.remove(child);
    }
  },

  /**
   * Trigger RemoteImages cleanup.
   */
  triggerCleanup() {
    return RemoteImages.forceCleanup();
  },

  /**
   * Write an image into the remote images directory.
   *
   * @param imageInfo An entry describing the image. Should be one of
   *                 |RemoteImagesTestUtils.images|.
   *
   * @param filename An optional filename to save as inside the directory. If
   *                 not provided, |imageInfo.recordId| will be used.
   */
  async writeImage(imageInfo, filename = undefined) {
    const data = new Uint8Array(
      await fetch(imageInfo.url, { credentials: "omit" }).then(rsp =>
        rsp.arrayBuffer()
      )
    );

    await IOUtils.write(
      PathUtils.join(REMOTE_IMAGES_PATH, filename ?? imageInfo.recordId),
      data
    );
  },

  /**
   * Return a RemoteImages database entry for the given image info.
   *
   * @param imageInfo An entry describing the image. Should be one of
   *                 |RemoteImagesTestUtils.images|.
   *
   * @param lastLoaded The timestamp to use for when the image was last loaded
   *                   (in UTC). If not provided, the current time is used.
   */
  dbEntryFor(imageInfo, lastLoaded = undefined) {
    return {
      [imageInfo.recordId]: {
        recordId: imageInfo.recordId,
        hash: imageInfo.hash,
        mimetype: imageInfo.mimetype,
        lastLoaded: lastLoaded ?? Date.UTC(),
      },
    };
  },

  /**
   * Remote Image entries.
   */
  images: {
    AboutRobots: {
      filename: "about-robots.png",
      recordId: "about-robots",
      mimetype: "image/png",
      hash: "29f1fe2cb5181152d2c01c0b2f12e5d9bb3379a61b94fb96de0f734eb360da62",
      url: "chrome://browser/content/aboutRobots-icon.png",
      size: 7599,
    },

    Mountain: {
      filename: "mountain.svg",
      recordId: "mountain",
      mimetype: "image/svg+xml",
      hash: "96902f3d784e1b5e49547c543a5c121442c64b180deb2c38246fada1d14597ac",
      url:
        "chrome://activity-stream/content/data/content/assets/remote/mountain.svg",
      size: 1650,
    },
  },
};

const EXPORTED_SYMBOLS = ["RemoteImagesTestUtils", "RemoteSettingsServer"];
