/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { BaseFeature } from "resource:///modules/urlbar/private/BaseFeature.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  TaskQueue: "resource:///modules/UrlbarUtils.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
});

/**
 * A set of blocked suggestions for quick suggest.
 */
export class BlockedSuggestions extends BaseFeature {
  constructor() {
    super();
    this.#taskQueue = new lazy.TaskQueue();
    lazy.UrlbarPrefs.addObserver(this);
  }

  get shouldEnable() {
    // Return true so that we'll always load blocked digests when quick suggest
    // is enabled, even if blocking new suggestions is currently disabled.
    // Blocking may have been enabled previously, and blocked suggestions should
    // remain blocked as long as quick suggest as a whole remains enabled.
    return true;
  }

  enable(enabled) {
    if (enabled) {
      this.#loadDigests();
    }
  }

  /**
   * Blocks a suggestion.
   *
   * @param {string} originalUrl
   *   The suggestion's original URL with its unreplaced timestamp template.
   */
  async add(originalUrl) {
    this.logger.debug(`Queueing add: ${originalUrl}`);
    await this.#taskQueue.queue(async () => {
      this.logger.info(`Blocking suggestion: ${originalUrl}`);
      let digest = await this.#getDigest(originalUrl);
      this.logger.debug(`Got digest for '${originalUrl}': ${digest}`);
      this.#digests.add(digest);
      let json = JSON.stringify([...this.#digests]);
      this.#updatingDigests = true;
      try {
        lazy.UrlbarPrefs.set("quicksuggest.blockedDigests", json);
      } finally {
        this.#updatingDigests = false;
      }
      this.logger.debug(`All blocked suggestions: ${json}`);
    });
  }

  /**
   * Gets whether a suggestion is blocked.
   *
   * @param {string} originalUrl
   *   The suggestion's original URL with its unreplaced timestamp template.
   * @returns {boolean}
   *   Whether the suggestion is blocked.
   */
  async has(originalUrl) {
    this.logger.debug(`Queueing has: ${originalUrl}`);
    return this.#taskQueue.queue(async () => {
      this.logger.info(`Getting blocked status: ${originalUrl}`);
      let digest = await this.#getDigest(originalUrl);
      this.logger.debug(`Got digest for '${originalUrl}': ${digest}`);
      let isBlocked = this.#digests.has(digest);
      this.logger.info(`Blocked status for '${originalUrl}': ${isBlocked}`);
      return isBlocked;
    });
  }

  /**
   * Unblocks all suggestions.
   */
  async clear() {
    this.logger.debug(`Queueing clearBlockedSuggestions`);
    await this.#taskQueue.queue(() => {
      this.logger.info(`Clearing all blocked suggestions`);
      this.#digests.clear();
      lazy.UrlbarPrefs.clear("quicksuggest.blockedDigests");
    });
  }

  /**
   * Called when a urlbar pref changes.
   *
   * @param {string} pref
   *   The name of the pref relative to `browser.urlbar`.
   */
  onPrefChanged(pref) {
    switch (pref) {
      case "quicksuggest.blockedDigests":
        if (!this.#updatingDigests) {
          this.logger.info(
            "browser.urlbar.quicksuggest.blockedDigests changed"
          );
          this.#loadDigests();
        }
        break;
    }
  }

  /**
   * Loads blocked suggestion digests from the pref into `#digests`.
   */
  async #loadDigests() {
    this.logger.debug(`Queueing #loadDigests`);
    await this.#taskQueue.queue(() => {
      this.logger.info(`Loading blocked suggestion digests`);
      let json = lazy.UrlbarPrefs.get("quicksuggest.blockedDigests");
      this.logger.debug(
        `browser.urlbar.quicksuggest.blockedDigests value: ${json}`
      );
      if (!json) {
        this.logger.info(`There are no blocked suggestion digests`);
        this.#digests.clear();
      } else {
        try {
          this.#digests = new Set(JSON.parse(json));
          this.logger.info(`Successfully loaded blocked suggestion digests`);
        } catch (error) {
          this.logger.error(
            `Error loading blocked suggestion digests: ${error}`
          );
        }
      }
    });
  }

  /**
   * Returns the SHA-1 digest of a string as a 40-character hex-encoded string.
   *
   * @param {string} string
   *   The string to convert to SHA-1
   * @returns {string}
   *   The hex-encoded digest of the given string.
   */
  async #getDigest(string) {
    let stringArray = new TextEncoder().encode(string);
    let hashBuffer = await crypto.subtle.digest("SHA-1", stringArray);
    let hashArray = new Uint8Array(hashBuffer);
    return Array.from(hashArray, b => b.toString(16).padStart(2, "0")).join("");
  }

  get _test_readyPromise() {
    return this.#taskQueue.emptyPromise;
  }

  get _test_digests() {
    return this.#digests;
  }

  _test_getDigest(string) {
    return this.#getDigest(string);
  }

  // Set of digests of the original URLs of blocked suggestions. A suggestion's
  // "original URL" is its URL straight from the source with an unreplaced
  // timestamp template. For details on the digests, see `#getDigest()`.
  //
  // The only reason we use URL digests is that suggestions currently do not
  // have persistent IDs. We could use the URLs themselves but SHA-1 digests are
  // only 40 chars long, so they save a little space. This is also consistent
  // with how blocked tiles on the newtab page are stored, but they use MD5. We
  // do *not* store digests for any security or obfuscation reason.
  //
  // This value is serialized as a JSON'ed array to the
  // `browser.urlbar.quicksuggest.blockedDigests` pref.
  #digests = new Set();

  // Used to serialize access to blocked suggestions. This is only necessary
  // because getting a suggestion's URL digest is async.
  #taskQueue = null;

  // Whether blocked digests are currently being updated.
  #updatingDigests = false;
}
