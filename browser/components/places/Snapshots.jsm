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
   * If the snapshot already exists, and this is a user-persisted addition,
   * then the userPersisted flag will be set, and the removed_at flag will be
   * cleared.
   *
   * @param {object} details
   * @param {string} details.url
   *   The url associated with the snapshot.
   * @param {boolean} [details.userPersisted]
   *   True if the user created or persisted the snapshot in some way, defaults to
   *   false.
   */
  async add({ url, userPersisted = false }) {
    if (!url) {
      throw new Error("Missing url parameter to Snapshots.add()");
    }
    await PlacesUtils.withConnectionWrapper("Snapshots: add", async db => {
      await db.executeCached(
        `
        INSERT INTO moz_places_metadata_snapshots
          (place_id, first_interaction_at, last_interaction_at, document_type, created_at, user_persisted)
        SELECT place_id, min(created_at), max(created_at),
               first_value(document_type) OVER (PARTITION BY place_id ORDER BY created_at DESC),
               :createdAt, :userPersisted
        FROM moz_places_metadata
        WHERE place_id = (SELECT id FROM moz_places WHERE url_hash = hash(:url) AND url = :url)
        ON CONFLICT DO UPDATE SET user_persisted = :userPersisted, removed_at = NULL WHERE :userPersisted = 1
      `,
        { createdAt: Date.now(), url, userPersisted }
      );
    });

    Services.obs.notifyObservers(null, "places-snapshot-added", url);
  }

  /**
   * Deletes a snapshot, creating a tombstone. Note, the caller is expected
   * to take account of the userPersisted value for a Snapshot when appropriate.
   *
   * @param {string} url
   *   The url of the snapshot to delete.
   */
  async delete(url) {
    await PlacesUtils.withConnectionWrapper("Snapshots: delete", async db => {
      await db.executeCached(
        `
        UPDATE moz_places_metadata_snapshots
          SET removed_at = :removedAt
        WHERE place_id = (SELECT id FROM moz_places WHERE url_hash = hash(:url) AND url = :url)
      `,
        { removedAt: Date.now(), url }
      );
    });

    Services.obs.notifyObservers(null, "places-snapshot-deleted", url);
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
    let db = await PlacesUtils.promiseDBConnection();
    let extraWhereCondition = "";

    if (!includeTombstones) {
      extraWhereCondition = " AND removed_at IS NULL";
    }

    let rows = await db.executeCached(
      `
      SELECT h.url AS url, created_at, removed_at, document_type,
             first_interaction_at, last_interaction_at,
             user_persisted FROM moz_places_metadata_snapshots s
      JOIN moz_places h ON h.id = s.place_id
      WHERE h.url_hash = hash(:url) AND h.url = :url
       ${extraWhereCondition}
    `,
      { url }
    );

    if (!rows.length) {
      return null;
    }

    return this.#translateRow(rows[0]);
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
    let db = await PlacesUtils.promiseDBConnection();

    let whereStatement = "";

    if (!includeTombstones) {
      whereStatement = " WHERE removed_at IS NULL";
    }

    let rows = await db.executeCached(
      `
      SELECT h.url AS url, created_at, removed_at, document_type,
             first_interaction_at, last_interaction_at,
             user_persisted FROM moz_places_metadata_snapshots s
      JOIN moz_places h ON h.id = s.place_id
      ${whereStatement}
      ORDER BY last_interaction_at DESC
      LIMIT :limit
    `,
      { limit }
    );

    return rows.map(row => this.#translateRow(row));
  }

  /**
   * Translates a database row to a Snapshot.
   *
   * @param {object} row
   *   The database row to translate.
   * @returns {Snapshot}
   */
  #translateRow(row) {
    return {
      url: row.getResultByName("url"),
      createdAt: this.#toDate(row.getResultByName("created_at")),
      removedAt: this.#toDate(row.getResultByName("removed_at")),
      firstInteractionAt: this.#toDate(
        row.getResultByName("first_interaction_at")
      ),
      lastInteractionAt: this.#toDate(
        row.getResultByName("last_interaction_at")
      ),
      documentType: row.getResultByName("document_type"),
      userPersisted: !!row.getResultByName("user_persisted"),
    };
  }

  /**
   * Translates a date value from the database.
   *
   * @param {number} value
   *   The date in milliseconds from the epoch.
   * @returns {Date?}
   */
  #toDate(value) {
    if (value) {
      return new Date(value);
    }
    return null;
  }
})();
