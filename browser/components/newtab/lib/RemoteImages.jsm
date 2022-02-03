/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "Downloader",
  "resource://services-settings/Attachments.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "KintoHttpClient",
  "resource://services-common/kinto-http-client.js"
);

ChromeUtils.defineModuleGetter(
  this,
  "Services",
  "resource://gre/modules/Services.jsm"
);

const RS_SERVER_PREF = "services.settings.server";
const RS_MAIN_BUCKET = "main";
const RS_COLLECTION = "ms-images";
const RS_DOWNLOAD_MAX_RETRIES = 2;

const CLEANUP_FINISHED_TOPIC = "remote-images:cleanup-finished";

const IMAGE_EXPIRY_DURATION = 30 * 24 * 60 * 60; // 30 days in seconds.

const EXTENSIONS = new Map([
  ["avif", "image/avif"],
  ["png", "image/png"],
  ["svg", "image/svg+xml"],
]);

class _RemoteImages {
  #cleaningUp;

  constructor() {
    this.#cleaningUp = false;

    // Piggy back off of RemoteSettings timer, triggering image cleanup every
    // 24h.
    Services.obs.addObserver(
      () => this.#cleanup(),
      "remote-settings:changes-poll-end"
    );
  }

  /**
   * The directory where images are cached.
   */
  get imagesDir() {
    const value = PathUtils.join(
      PathUtils.localProfileDir,
      "settings",
      RS_MAIN_BUCKET,
      RS_COLLECTION
    );

    Object.defineProperty(this, "imagesDir", { value });
    return value;
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
      const blobURL = await this.load(imageId);

      delete message.imageId;

      message[replaceWith] = blobURL;

      return () => this.unload(blobURL);
    }

    return null;
  }

  /**
   * Load a remote image.
   *
   * If the image has not been previously downloaded then the image will be
   * downloaded from RemoteSettings.
   *
   * @param imageId  The unique image ID.
   *
   * @returns A promise that resolves with a blob URL for the given image, or
   *          rejects with an error.
   *
   *          After the caller is finished with the image, they must call
   *         |RemoteImages.unload()| on the returned URL.
   */
  async load(imageId) {
    let blob;
    try {
      blob = await this.#readFromDisk(imageId);
    } catch (e) {
      if (
        !(
          e instanceof Components.Exception &&
          e.name === "NS_ERROR_FILE_NOT_FOUND"
        )
      ) {
        throw e;
      }

      // If we have not downloaded the file then download the file from
      // RemoteSettings.
      blob = await this.#download(imageId);
    }

    return URL.createObjectURL(blob);
  }

  /**
   * Unload a URL returned by RemoteImages
   *
   * @param url The URL to unload.
   **/
  unload(url) {
    URL.revokeObjectURL(url);
  }

  /**
   * Clean up all files that haven't been touched in 30d.
   */
  async #cleanup() {
    // The observer service doesn't await observers, so guard against re-entry.
    if (this.#cleaningUp) {
      return;
    }
    this.#cleaningUp = true;

    try {
      const now = Date.now();
      const children = await IOUtils.getChildren(this.imagesDir);
      for (const child of children) {
        const stat = await IOUtils.stat(child);

        if (now - stat.lastModified >= IMAGE_EXPIRY_DURATION) {
          await IOUtils.remove(child);
        }
      }
    } finally {
      this.#cleaningUp = false;
      Services.obs.notifyObservers(null, CLEANUP_FINISHED_TOPIC);
    }
  }

  /**
   * Return the record ID and mimetype given an imageId.
   *
   * Remote Settings does not support records with periods in their names, but
   * we use a full filename (e.g., `${recordId}.png`) for image IDs so that we
   * don't also have to carry the mimetype as a separate field and can instead
   * infer it from the imageId.
   *
   * @param imageId The image ID, including its extension.
   *
   * @returns An object containing the |recordId| and the |mimetype|
   */
  #getRecordIdAndMimetype(imageId) {
    const idx = imageId.lastIndexOf(".");
    if (idx === -1) {
      throw new TypeError("imageId must include extension");
    }

    const recordId = imageId.substring(0, idx);
    const ext = imageId.substring(idx + 1);
    const mimetype = EXTENSIONS.get(ext);

    if (!mimetype) {
      throw new TypeError(
        `Unsupported extension '.${ext}' for remote image ${imageId}`
      );
    }

    return { recordId, mimetype };
  }

  /**
   * Read the image from disk
   *
   * @param imageId The image ID.
   *
   * @returns A promise that resolves to a blob, or rejects with an Error.
   */
  async #readFromDisk(imageId) {
    const { mimetype } = this.#getRecordIdAndMimetype(imageId);
    const path = PathUtils.join(this.imagesDir, imageId);
    const blob = await File.createFromFileName(path, { type: mimetype });

    // Update the modification time so that we don't cleanup this image.
    await IOUtils.setModificationTime(path);

    return blob;
  }

  /**
   * Download an image from RemoteSettings.
   *
   * @param imageId  The image ID.
   *
   * @returns A promise that resolves with a Blob of the image data or rejects
   *          with an Error.
   */
  async #download(imageId) {
    const client = new KintoHttpClient(
      Services.prefs.getStringPref(RS_SERVER_PREF)
    );

    const { recordId } = this.#getRecordIdAndMimetype(imageId);
    const record = await client
      .bucket(RS_MAIN_BUCKET)
      .collection(RS_COLLECTION)
      .getRecord(recordId);

    const downloader = new Downloader(RS_MAIN_BUCKET, RS_COLLECTION);

    const arrayBuffer = await downloader.downloadAsBytes(record.data, {
      retries: RS_DOWNLOAD_MAX_RETRIES,
    });

    // Cache to disk.
    const path = PathUtils.join(this.imagesDir, imageId);

    // We do not await this promise because any other attempt to interact with
    // the file via IOUtils will have to synchronize via the IOUtils event queue
    // anyway.
    IOUtils.write(path, new Uint8Array(arrayBuffer));

    return new Blob([arrayBuffer], { type: record.data.attachment.mimetype });
  }
}

const RemoteImages = new _RemoteImages();

const EXPORTED_SYMBOLS = ["RemoteImages"];
