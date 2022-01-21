/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["SnapshotGroups"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
  Snapshots: "resource:///modules/Snapshots.jsm",
});

/**
 * @typedef {object} SnapshotGroup
 *   This object represents a group of snapshots.
 *
 * @property {string} id
 *   The group id. The id property is ignored when adding a group.
 * @property {string} title
 *   The title of the group, this may be automatically generated or
 *   user assigned.
 * @property {string} builder
 *   The builder that was used to create the group (e.g. "domain", "pinned").
 * @property {object} builderMetadata
 *   The metadata from the builder for the SnapshotGroup.
 *   This is for use by the builder only and should otherwise be considered opaque.
 * @property {string} imageUrl
 *   The image url to use for the group.
 * @property {number} lastAccessed
 *   The last access time of the most recently accessed snapshot.
 *   Stored as the number of milliseconds since the epoch.
 * @property {number} snapshotCount
 *   The number of snapshots contained within the group.
 */

/**
 * Handles storing and retrieving of snapshot groups in the Places database.
 *
 * Notifications of updates are sent via the observer service:
 *   places-snapshot-group-added, data: id of the snapshot group.
 *   places-snapshot-group-updated, data: id of the snapshot group.
 *   places-snapshot-group-deleted, data: id of the snapshot group.
 */
const SnapshotGroups = new (class SnapshotGroups {
  constructor() {}

  /**
   * Adds a new snapshot group.
   * Note: Not currently for use from UI code.
   *
   * @param {SnapshotGroup} group
   *   The details of the group to add.
   * @param {string[]} urls
   *   An array of snapshot urls to add to the group. If the urls do not have associated snapshots, then they are ignored.
   * @returns {number} id
   *   The id of the newly added group, or -1 on failure
   */
  async add(group, urls) {
    let id = -1;
    await PlacesUtils.withConnectionWrapper(
      "SnapshotsGroups.jsm:add",
      async db => {
        // Create the new group
        let row = await db.executeCached(
          `
        INSERT INTO moz_places_metadata_snapshots_groups (title, builder, builder_data)
        VALUES (:title, :builder, :builder_data)
        RETURNING id
      `,
          {
            title: group.title,
            builder: group.builder,
            builder_data: JSON.stringify(group.builderMetadata),
          }
        );
        id = row[0].getResultByIndex(0);

        // Construct the sql parameters for the urls
        let params = {};
        let SQLInFragment = [];
        let i = 0;
        for (let url of urls) {
          params[`url${i}`] = url;
          SQLInFragment.push(`hash(:url${i})`);
          i++;
        }
        params.id = id;

        await db.execute(
          `
          INSERT INTO moz_places_metadata_groups_to_snapshots (group_id, place_id)
          SELECT :id, s.place_id 
          FROM moz_places h
          JOIN moz_places_metadata_snapshots s
          ON h.id = s.place_id
          WHERE h.url_hash IN (${SQLInFragment.join(",")})
        `,
          params
        );
      }
    );

    Services.obs.notifyObservers(null, "places-snapshot-group-added");
    return id;
  }

  /**
   * Modifies the metadata for a snapshot group.
   *
   * @param {SnapshotGroup} group
   *   The details of the group to modify. If lastAccessed and SnapshotCount are specified, then they are ignored.
   */
  async updateMetadata(group) {
    await PlacesUtils.withConnectionWrapper(
      "SnapshotsGroups.jsm:updateMetadata",
      db => {
        return db.executeCached(
          `
        UPDATE moz_places_metadata_snapshots_groups
        SET title = :title, builder = :builder, builder_data = :builder_data
        WHERE id = :id
      `,
          {
            id: group.id,
            title: group.title,
            builder: group.builder,
            builder_data: JSON.stringify(group.builderMetadata),
          }
        );
      }
    );

    Services.obs.notifyObservers(null, "places-snapshot-group-updated");
  }

  /**
   * Modifies the urls for a snapshot group.
   *
   * @param {number} id
   *   The id of the group to modify.
   * @param {string[]} [urls]
   *   An array of snapshot urls for the group. If the urls do not have associated snapshots, then they are ignored.
   */
  async updateUrls(id, urls) {
    // TODO
    Services.obs.notifyObservers(null, "places-snapshot-group-updated");
  }

  /**
   * Deletes a snapshot group.
   * Note: Not currently for use from UI code.
   *
   * @param {number} id
   *   The id of the group to delete.
   */
  async delete(id) {
    await PlacesUtils.withConnectionWrapper(
      "SnapshotsGroups.jsm:delete",
      async db => {
        await db.executeCached(
          `
          DELETE FROM moz_places_metadata_snapshots_groups
          WHERE id = :id;
        `,
          { id }
        );
      }
    );
    Services.obs.notifyObservers(null, "places-snapshot-group-deleted");
  }

  /**
   * Queries the list of SnapshotGroups.
   *
   * @param {object} [options]
   * @param {number} [options.limit]
   *   A numerical limit to the number of snapshots to retrieve, defaults to 50.
   * @param {string} [options.builder]
   *   Limit searching snapshot groups to results from a particular builder.
   * @returns {SnapshotGroup[]}
   *   An array of snapshot groups, in descending order of last access time.
   */
  async query({ limit = 50, builder = "" } = {}) {
    let db = await PlacesUtils.promiseDBConnection();

    let rows = await db.executeCached(
      `
      SELECT g.id, g.title, g.builder, g.builder_data, COUNT(h.url) AS snapshot_count, MAX(h.last_visit_date) AS last_access
      FROM moz_places_metadata_snapshots_groups g
      LEFT JOIN moz_places_metadata_groups_to_snapshots s ON s.group_id = g.id
      LEFT JOIN moz_places h ON h.id = s.place_id
      WHERE builder = :builder OR :builder = ""
      GROUP BY g.id
      ORDER BY last_access DESC
      LIMIT :limit
        `,
      { builder, limit }
    );

    return rows.map(row => this.#translateSnapshotGroupRow(row));
  }

  /**
   * Obtains the snapshots for a particular group. This is designed
   * for batch use to avoid potentially pulling a large number of
   * snapshots across to the content process at one time.
   *
   * @param {object} options
   * @param {number} options.id
   *   The id of the snapshot group to get the snapshots for.
   * @param {number} options.startIndex
   *   The start index of the snapshots to return.
   * @param {number} options.count
   *   The number of snapshots to return.
   * @param {boolean} [sortDescending]
   *   Whether or not to sortDescending. Defaults to true.
   * @param {string} [sortBy]
   *   A string to choose what to sort the snapshots by, e.g. "last_interaction_at"
   * @returns {Snapshots[]}
   *   An array of snapshots, in descending order of last interaction time
   */
  async getSnapshots({
    id = "",
    startIndex = 0,
    count = 50,
    sortDescending = true,
    sortBy = "last_interaction_at",
  } = {}) {
    let db = await PlacesUtils.promiseDBConnection();
    let urlRows = await db.executeCached(
      `
      SELECT h.url
      FROM moz_places_metadata_groups_to_snapshots s
      JOIN moz_places h ON h.id = s.place_id
      WHERE s.group_id = :group_id
      ORDER BY h.last_visit_date DESC
    `,
      { group_id: id }
    );

    let snapshots = [];
    let urls = urlRows.map(row => row.getResultByName("url"));

    let start = Math.max(0, startIndex);
    let end = Math.min(urls.length, count + start);
    for (let i = start; i < end; i++) {
      let snapShot = await Snapshots.get(urls[i]);
      snapshots.push(snapShot);
    }

    return snapshots;
  }

  /**
   * Translates a snapshot group database row to a SnapshotGroup.
   *
   * @param {object} row
   *   The database row to translate.
   * @returns {SnapshotGroup}
   */
  #translateSnapshotGroupRow(row) {
    let snapshotGroup = {
      id: row.getResultByName("id"),
      title: row.getResultByName("title"),
      builder: row.getResultByName("builder"),
      builderMetadata: JSON.parse(row.getResultByName("builder_data")),
      snapshotCount: row.getResultByName("snapshot_count"),
    };

    return snapshotGroup;
  }
})();
