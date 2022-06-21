/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { JSONFile } = ChromeUtils.import("resource://gre/modules/JSONFile.jsm");
const { PromiseUtils } = ChromeUtils.import(
  "resource://gre/modules/PromiseUtils.jsm"
);
const { RemoteSettings } = ChromeUtils.import(
  "resource://services-settings/remote-settings.js"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const lazy = {};

ChromeUtils.defineModuleGetter(
  lazy,
  "Downloader",
  "resource://services-settings/Attachments.jsm"
);

ChromeUtils.defineModuleGetter(
  lazy,
  "KintoHttpClient",
  "resource://services-common/kinto-http-client.js"
);

const RS_SERVER_PREF = "services.settings.server";
const RS_MAIN_BUCKET = "main";
const RS_COLLECTION = "ms-images";
const RS_DOWNLOAD_MAX_RETRIES = 2;

const REMOTE_IMAGES_PATH = PathUtils.join(
  PathUtils.localProfileDir,
  "settings",
  RS_MAIN_BUCKET,
  RS_COLLECTION
);
const REMOTE_IMAGES_DB_PATH = PathUtils.join(REMOTE_IMAGES_PATH, "db.json");

const IMAGE_EXPIRY_DURATION = 30 * 24 * 60 * 60; // 30 days in seconds.

class _RemoteImages {
  #dbPromise;

  constructor() {
    this.#dbPromise = null;

    RemoteSettings(RS_COLLECTION).on("sync", () => this.#onSync());

    // Ensure we migrate all our images to a JSONFile database.
    this.withDb(() => {});
  }

  /**
   * Load the database from disk.
   *
   * If the database does not yet exist, attempt a migration from legacy Remote
   * Images (i.e., image files in |REMOTE_IMAGES_PATH|).
   *
   * @returns {Promise<JSONFile>} A promise that resolves with the database
   *                              instance.
   */
  async #loadDb() {
    let db;

    if (!(await IOUtils.exists(REMOTE_IMAGES_DB_PATH))) {
      db = await this.#migrate();
    } else {
      db = new JSONFile({ path: REMOTE_IMAGES_DB_PATH });
      await db.load();
    }

    return db;
  }

  /**
   * Reset the RemoteImages database
   *
   * NB: This is only meant to be used by unit tests.
   *
   * @returns {Promise<void>} A promise that resolves when the database has been
   *                          reset.
   */
  reset() {
    return this.withDb(async db => {
      // We must reset |#dbPromise| *before* awaiting because if we do not, then
      // another function could call withDb() while we are awaiting and get a
      // promise that will resolve to |db| instead of getting null and forcing a
      // db reload.
      this.#dbPromise = null;
      await db.finalize();
    });
  }

  /*
   * Execute |fn| with the RemoteSettings database.
   *
   * This ensures that only one caller can have a handle to the database at any
   * given time (unless it is leaked through assignment from within |fn|). This
   * prevents re-entrancy issues with multiple calls to cleanup() and calling
   * cleanup while loading images.
   *
   * @param fn The function to call with the database.
   */
  async withDb(fn) {
    const dbPromise = this.#dbPromise ?? this.#loadDb();

    const { resolve, promise } = PromiseUtils.defer();
    // NB: Update |#dbPromise| before awaiting anything so that the next call to
    //     |withDb()| will see the new value of |#dbPromise|.
    this.#dbPromise = promise;

    const db = await dbPromise;

    try {
      return await fn(db);
    } finally {
      resolve(db);
    }
  }

  /**
   * Patch a reference to a remote image in a message with a blob URL.
   *
   * @param message     The remote image reference to be patched.
   * @param replaceWith The property name that will be used to store the blob
   *                    URL on |message|.
   *
   * @return A promise that resolves with an unloading function for the patched
   *         URL, or rejects with an error.
   *
   *         If the message isn't patched (because there isn't a remote image)
   *         then the promise will resolve to null.
   */
  async patchMessage(message, replaceWith = "imageURL") {
    if (!!message && !!message.imageId) {
      const { imageId } = message;
      const urls = await this.load(imageId);

      if (urls.size) {
        const blobURL = urls.get(imageId);

        delete message.imageId;
        message[replaceWith] = blobURL;

        return () => this.unload(urls);
      }
    }
    return null;
  }

  /**
   * Load remote images.
   *
   * If the images have not been previously downloaded, then they will be
   * downloaded from RemoteSettings.
   *
   * @param {...string} imageIds The image IDs to load.
   *
   * @returns {object} An object mapping image Ids to blob: URLs.
   *                   If an image could not be loaded, it will not be present
   *                   in the returned object.
   *
   *                   After the caller is finished with the images, they must call
   *                   |RemoteImages.unload()| on the object.
   */
  load(...imageIds) {
    return this.withDb(async db => {
      // Deduplicate repeated imageIds by using a Map.
      const urls = new Map(imageIds.map(key => [key, undefined]));

      await Promise.all(
        Array.from(urls.keys()).map(async imageId => {
          try {
            urls.set(imageId, await this.#loadImpl(db, imageId));
          } catch (e) {
            Cu.reportError(`Could not load image ID ${imageId}: ${e}`);
            urls.delete(imageId);
          }
        })
      );

      return urls;
    });
  }

  async #loadImpl(db, imageId) {
    const recordId = this.#getRecordId(imageId);

    let blob;
    if (db.data.images[recordId]) {
      // We have previously fetched this image, we can load it from disk.
      try {
        blob = await this.#readFromDisk(db, recordId);
      } catch (e) {
        if (
          !(
            e instanceof Components.Exception &&
            e.name === "NS_ERROR_FILE_NOT_FOUND"
          )
        ) {
          throw e;
        }
      }

      // Fall back to downloading if we cannot read it from disk.
    }

    if (typeof blob === "undefined") {
      blob = await this.#download(db, recordId);
    }

    return URL.createObjectURL(blob);
  }

  /**
   * Unload URLs returned by RemoteImages
   *
   * @param {Map<string, string>} urls The result of calling |RemoteImages.load()|.
   **/
  unload(urls) {
    for (const url of urls.keys()) {
      URL.revokeObjectURL(url);
    }
  }

  #onSync() {
    return this.withDb(async db => {
      await this.#cleanup(db);

      const recordsById = await RemoteSettings(RS_COLLECTION)
        .db.list()
        .then(records =>
          Object.assign({}, ...records.map(record => ({ [record.id]: record })))
        );

      await Promise.all(
        Object.values(db.data.images)
          .filter(
            entry => recordsById[entry.recordId]?.attachment.hash !== entry.hash
          )
          .map(entry => this.#download(db, entry.recordId, { refresh: true }))
      );
    });
  }

  forceCleanup() {
    return this.withDb(db => this.#cleanup(db));
  }

  /**
   * Clean up all files that haven't been touched in 30d.
   *
   * @returns {Promise<undefined>} A promise that resolves once cleanup has
   *                               finished.
   */
  async #cleanup(db) {
    const now = Date.now();
    await Promise.all(
      Object.values(db.data.images)
        .filter(entry => now - entry.lastLoaded >= IMAGE_EXPIRY_DURATION)
        .map(entry => {
          const path = PathUtils.join(REMOTE_IMAGES_PATH, entry.recordId);
          delete db.data.images[entry.recordId];

          return IOUtils.remove(path).catch(e => {
            Cu.reportError(
              `Could not remove remote image ${entry.recordId}: ${e}`
            );
          });
        })
    );

    db.saveSoon();
  }

  /**
   * Return the record ID from an image ID.
   *
   * Prior to Firefox 101, imageIds were of the form ${recordId}.${extension} so
   * that we could infer the mimetype.
   *
   * @returns The RemoteSettings record ID.
   */
  #getRecordId(imageId) {
    const idx = imageId.lastIndexOf(".");
    if (idx === -1) {
      return imageId;
    }
    return imageId.substring(0, idx);
  }

  /**
   * Read the image from disk
   *
   * @param {JSONFile} db The RemoteImages database.
   * @param {string} recordId The record ID of the image.
   *
   * @returns A promise that resolves to a blob, or rejects with an Error.
   */
  async #readFromDisk(db, recordId) {
    const path = PathUtils.join(REMOTE_IMAGES_PATH, recordId);

    try {
      const blob = await File.createFromFileName(path, {
        type: db.data.images[recordId].mimetype,
      });
      db.data.images[recordId].lastLoaded = Date.now();

      return blob;
    } catch (e) {
      // If we cannot read the file from disk, delete the entry.
      delete db.data.images[recordId];

      throw e;
    } finally {
      db.saveSoon();
    }
  }

  /**
   * Download an image from RemoteSettings.
   *
   * @param {JSONFile} db The RemoteImages database.
   * @param {string} recordId The record ID of the image.
   * @param {object} options Options for downloading the image.
   * @param {boolean} options.refresh Whether or not the image is being
   *                                  downloaded because it is out of sync with
   *                                  Remote Settings. If true, the blob will be
   *                                  written to disk and not returned.
   *                                  Additionally, the |lastLoaded| field will
   *                                  not be updated in its db entry.
   *
   * @returns A promise that resolves with a Blob of the image data or rejects
   *          with an Error.
   */
  async #download(db, recordId, { refresh = false } = {}) {
    const client = new lazy.KintoHttpClient(
      Services.prefs.getStringPref(RS_SERVER_PREF)
    );

    const record = await client
      .bucket(RS_MAIN_BUCKET)
      .collection(RS_COLLECTION)
      .getRecord(recordId);

    const downloader = new lazy.Downloader(RS_MAIN_BUCKET, RS_COLLECTION);
    const arrayBuffer = await downloader.downloadAsBytes(record.data, {
      retries: RS_DOWNLOAD_MAX_RETRIES,
    });

    // Cache to disk.
    const path = PathUtils.join(REMOTE_IMAGES_PATH, recordId);

    // We do not await this promise because any other attempt to interact with
    // the file via IOUtils will have to synchronize via the IOUtils event queue
    // anyway.
    IOUtils.write(path, new Uint8Array(arrayBuffer));

    const { mimetype, hash } = record.data.attachment;

    if (refresh) {
      Object.assign(db.data.images[recordId], { mimetype, hash });
    } else {
      db.data.images[recordId] = {
        recordId,
        mimetype,
        hash,
        lastLoaded: Date.now(),
      };
    }

    db.saveSoon();

    if (!refresh) {
      return new Blob([arrayBuffer], { type: record.data.attachment.mimetype });
    }

    return undefined;
  }

  /**
   * Migrate from a file-based store to an index-based store.
   */
  async #migrate() {
    let children;
    try {
      children = await IOUtils.getChildren(REMOTE_IMAGES_PATH);

      // Delete all previously cached entries.
      await Promise.all(
        children.map(async path => {
          try {
            await IOUtils.remove(path);
          } catch (e) {
            Cu.reportError(`RemoteImages could not delete ${path}: ${e}`);
          }
        })
      );
    } catch (e) {
      if (!(DOMException.isInstance(e) && e.name === "NotFoundError")) {
        throw e;
      }
    }

    await IOUtils.makeDirectory(REMOTE_IMAGES_PATH);
    const db = new JSONFile({ path: REMOTE_IMAGES_DB_PATH });
    db.data = {
      version: 1,
      images: {},
    };
    db.saveSoon();
    return db;
  }
}

const RemoteImages = new _RemoteImages();

const EXPORTED_SYMBOLS = [
  "RemoteImages",
  "REMOTE_IMAGES_PATH",
  "REMOTE_IMAGES_DB_PATH",
];
