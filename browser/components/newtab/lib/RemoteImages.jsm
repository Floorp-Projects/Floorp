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

ChromeUtils.defineModuleGetter(
  lazy,
  "Utils",
  "resource://services-settings/Utils.jsm"
);

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

const PREFETCH_FINISHED_TOPIC = "remote-images:prefetch-finished";

/**
 * Inspectors for FxMS messages.
 *
 * Each member is the name of a FxMS template (spotlight, infobar, etc.) and
 * corresponds to a function that accepts a message and returns all record IDs
 * for remote images.
 */
const MessageInspectors = {
  spotlight(message) {
    if (
      message.content.template === "logo-and-content" &&
      message.content.logo?.imageId
    ) {
      return [message.content.logo.imageId];
    }
    return [];
  },
};

class _RemoteImages {
  #dbPromise;

  #fetching;

  constructor() {
    this.#dbPromise = null;
    this.#fetching = new Map();

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

    // If we are pre-fetching an image, we can piggy-back on that request.
    if (this.#fetching.has(imageId)) {
      const { record, arrayBuffer } = await this.#fetching.get(imageId);
      return new Blob([arrayBuffer], { type: record.data.attachment.mimetype });
    }

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
    // This is OK to run while pre-fetches are ocurring. Pre-fetches don't check
    // if there is a new version available, so there will be no race between
    // syncing an updated image and pre-fetching
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
          .map(entry => this.#download(db, entry.recordId, { fetchOnly: true }))
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
    // This may run while background fetches are happening. However, that
    // doesn't matter because those images will definitely not be expired.
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
   * @param {boolean} options.fetchOnly Whether or not to only fetch the image.
   *
   * @returns If |fetchOnly| is true, a promise that resolves to undefined.
   *          If |fetchOnly| is false, a promise that resolves to a Blob of the
   *          image data.
   */
  async #download(db, recordId, { fetchOnly = false } = {}) {
    // It is safe to call #unsafeDownload here because we hold the db while the
    // entire download runs.
    const { record, arrayBuffer } = await this.#unsafeDownload(recordId);
    const { mimetype, hash } = record.data.attachment;

    if (fetchOnly) {
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

    if (fetchOnly) {
      return undefined;
    }

    return new Blob([arrayBuffer], { type: record.data.attachment.mimetype });
  }

  /**
   * Download an image *without* holding a handle to the database.
   *
   * @param {string} recordId The record ID of the image to download
   *
   * @returns A promise that resolves to the RemoteSettings record and the
   *          downloaded ArrayBuffer.
   */
  async #unsafeDownload(recordId) {
    const client = new lazy.KintoHttpClient(lazy.Utils.SERVER_URL);

    const record = await client
      .bucket(RS_MAIN_BUCKET)
      .collection(RS_COLLECTION)
      .getRecord(recordId);

    const downloader = new lazy.Downloader(RS_MAIN_BUCKET, RS_COLLECTION);
    const arrayBuffer = await downloader.downloadAsBytes(record.data, {
      retries: RS_DOWNLOAD_MAX_RETRIES,
    });

    const path = PathUtils.join(REMOTE_IMAGES_PATH, recordId);

    // Cache to disk.
    //
    // We do not await this promise because any other attempt to interact with
    // the file via IOUtils will have to synchronize via the IOUtils event queue
    // anyway.
    //
    // This is OK to do without holding the db because cleanup will not touch
    // this image.
    IOUtils.write(path, new Uint8Array(arrayBuffer));

    return { record, arrayBuffer };
  }

  /**
   * Prefetch images for the given messages.
   *
   * This will only acquire the db handle when we need to handle internal state
   * so that other consumers can interact with RemoteImages while pre-fetches
   * are happening.
   *
   * NB: This function is not intended to be awaited so that it can run the
   * fetches in the background.
   *
   * @param {object[]} messages The FxMS messages to prefetch images for.
   */
  async prefetchImagesFor(messages) {
    // Collect the list of record IDs from the message, if we have an inspector
    // for it.
    const recordIds = messages
      .filter(
        message =>
          message.template && Object.hasOwn(MessageInspectors, message.template)
      )
      .flatMap(message => MessageInspectors[message.template](message))
      .map(imageId => this.#getRecordId(imageId));

    // If we find some messages, grab the db lock and queue the downloads of
    // each.
    if (recordIds.length) {
      const promises = await this.withDb(
        db =>
          new Map(
            recordIds.reduce((entries, recordId) => {
              const promise = this.#beginPrefetch(db, recordId);

              // If we already have the image, #beginPrefetching will return
              // null instead of a promise.
              if (promise !== null) {
                this.#fetching.set(recordId, promise);
                entries.push([recordId, promise]);
              }

              return entries;
            }, [])
          )
      );

      // We have dropped db lock and the fetches will continue in the background.
      // If we do not drop the lock here, nothing can interact with RemoteImages
      // while we are pre-fetching.
      //
      // As each prefetch request finishes, they will individually grab the db
      // lock (inside #finishPrefetch or #handleFailedPrefetch) to update
      // internal state.
      const prefetchesFinished = Array.from(promises.entries()).map(
        ([recordId, promise]) =>
          promise.then(
            result => this.#finishPrefetch(result),
            () => this.#handleFailedPrefetch(recordId)
          )
      );

      // Wait for all prefetches to finish before we send our notification.
      await Promise.all(prefetchesFinished);

      Services.obs.notifyObservers(null, PREFETCH_FINISHED_TOPIC);
    }
  }

  /**
   * Ensure the image for the given record ID has a database entry.
   * Begin pre-fetching the requested image if we do not already have it locally.
   *
   * @param {JSONFile} db The database.
   * @param {string} recordId The record ID of the image.
   *
   * @returns If the image is already cached locally, null is returned.
   *          Otherwise, a promise that resolves to an object including the
   *          recordId, the Remote Settings record, and the ArrayBuffer of the
   *          downloaded file.
   */
  #beginPrefetch(db, recordId) {
    if (!Object.hasOwn(db.data.images, recordId)) {
      // We kick off the download while we hold the db (so we can record the
      // promise in #fetches), but we do not ensure that the download completes
      // while we hold it.
      //
      // It is safe to call #unsafeDownload here and let the promises resolve
      // outside this function because we record the recordId and promise in
      // #fetching so any concurrent request to load the same image will re-use
      // that promise and not trigger a second download (and therefore IO).
      const promise = this.#unsafeDownload(recordId);
      this.#fetching.set(recordId, promise);

      return promise;
    }

    return null;
  }

  /**
   * Finish prefetching an image.
   *
   * @param {object} options
   * @param {object} options.record The Remote Settings record.
   */
  #finishPrefetch({ record }) {
    return this.withDb(db => {
      const { id: recordId } = record.data;
      const { mimetype, hash } = record.data.attachment;

      this.#fetching.delete(recordId);

      db.data.images[recordId] = {
        recordId,
        mimetype,
        hash,
        lastLoaded: Date.now(),
      };

      db.saveSoon();
    });
  }

  /**
   * Remove the prefetch entry for a fetch that failed.
   */
  #handleFailedPrefetch(recordId) {
    return this.withDb(db => {
      this.#fetching.delete(recordId);
    });
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
