/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Snapshots"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

/**
 * @typedef {object} Snapshot
 *   A snapshot summarises a collection of interactions.
 * @property {string} url
 *   The associated URL.
 * @property {Date} createdAt
 *   The date/time the snapshot was created.
 * @property {Date} removedAt
 *   The date/time the snapshot was deleted.
 * @property {Date} firstInteractionAt
 *   The date/time of the first interaction with the snapshot.
 * @property {Date} lastInteractionAt
 *   The date/time of the last interaction with the snapshot.
 * @property {Interactions.DOCUMENT_TYPE} documentType
 *   The document type of the snapshot.
 * @property {boolean} userPersisted
 *   True if the user created or persisted the snapshot in some way.
 */

/**
 * Handles storing and retrieving of Snapshots in the Places database.
 *
 * Notifications of updates are sent via the observer service:
 * - places-snapshot-added, data: url
 *     Sent when a new snapshot is added
 * - places-snapshot-deleted, data: url
 *     Sent when a snapshot is removed.
 */
const Snapshots = new (class Snapshots {
  #snapshots = new Map();

  /**
   * Adds a new snapshot.
   *
   * @param {object} details
   * @param {string} details.url
   *   The url associated with the snapshot.
   * @param {boolean} [details.userPersisted]
   *   True if the user created or persisted the snapshot in some way, defaults to
   *   false.
   */
  async add({ url, userPersisted = false }) {
    let now = new Date();

    let db = await PlacesUtils.promiseDBConnection();
    let rows = await db.executeCached(
      `
      WITH places(place_id) AS (SELECT id FROM moz_places WHERE url_hash = hash(:url) AND url = :url),
        inserts(created_at, updated_at, document_type) AS (
          VALUES(
            (SELECT min(created_at) FROM moz_places_metadata WHERE place_id in places),
            (SELECT max(updated_at) FROM moz_places_metadata WHERE place_id in places),
            (SELECT document_type FROM moz_places_metadata WHERE place_id in places ORDER BY updated_at DESC LIMIT 1)
          )
        )
      SELECT * from inserts WHERE created_at is not null
    `,
      { url }
    );
    if (!rows.length) {
      throw new Error("Could not find existing interactions");
    }

    this.#snapshots.set(url, {
      url,
      userPersisted,
      createdAt: now,
      removedAt: null,
      documentType: rows[0].getResultByName("document_type"),
      firstInteractionAt: new Date(rows[0].getResultByName("created_at")),
      lastInteractionAt: new Date(rows[0].getResultByName("updated_at")),
    });

    Services.obs.notifyObservers(null, "places-snapshot-added", url);
  }

  /**
   * Deletes a snapshot, creating a tombstone.
   *
   * @param {string} url
   *   The url of the snapshot to delete.
   */
  async delete(url) {
    let snapshot = this.#snapshots.get(url);

    if (snapshot) {
      snapshot.removedAt = new Date();
      Services.obs.notifyObservers(null, "places-snapshot-deleted", url);
    }
  }

  /**
   * Gets the details for a particular snapshot based on the url.
   *
   * @param {string} url
   *   The url of the snapshot to obtain.
   * @param {boolean} [includeTombstones]
   *   Whether to include tombstones in the snapshots to obtain.
   * @returns {?Snapshot}
   */
  async get(url, includeTombstones = false) {
    let snapshot = this.#snapshots.get(url);
    if (!snapshot || (snapshot.removedAt && !includeTombstones)) {
      return null;
    }

    return snapshot;
  }

  /**
   * Queries the current snapshots in the database.
   *
   * @param {object} [options]
   * @param {number} [options.limit]
   *   A numerical limit to the number of snapshots to retrieve, defaults to 100.
   * @param {boolean} [options.includeTombstones]
   *   Whether to include tombstones in the snapshots to obtain.
   * @returns {Snapshot[]}
   *   Returns snapshots in order of descending last interaction time.
   */
  async query({ limit = 100, includeTombstones = false } = {}) {
    let snapshots = Array.from(this.#snapshots.values());

    if (!includeTombstones) {
      return snapshots.filter(s => !s.removedAt).slice(0, limit);
    }
    return snapshots.slice(0, limit);
  }
})();
