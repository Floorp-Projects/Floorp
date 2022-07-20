/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  PlacesPreviews: "resource://gre/modules/PlacesPreviews.sys.mjs",
  PlacesUIUtils: "resource:///modules/PlacesUIUtils.sys.mjs",
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
  SnapshotMonitor: "resource:///modules/SnapshotMonitor.sys.mjs",
  Snapshots: "resource:///modules/Snapshots.sys.mjs",
});

XPCOMUtils.defineLazyModuleGetters(lazy, {
  BackgroundPageThumbs: "resource://gre/modules/BackgroundPageThumbs.jsm",
  PageThumbs: "resource://gre/modules/PageThumbs.jsm",
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "MIN_GROUP_SIZE",
  "browser.places.snapshots.minGroupSize",
  5
);

/**
 * @typedef {object} SnapshotGroup
 *   This object represents a group of snapshots.
 *
 * @property {string} id
 *   The group id. The id property is ignored when adding a group.
 * @property {string} title
 *   The title of the group assigned by the user. This should be used
 *   in preference to the translation supplied title in the builderMetadata.
 * @property {boolean} hidden
 *   Whether the group is hidden or not.
 * @property {string} builder
 *   The builder that was used to create the group (e.g. "domain", "pinned").
 * @property {object} builderMetadata
 *   The metadata from the builder for the SnapshotGroup.
 *   This is mostly for use by the builder only and should otherwise be
 *   considered opaque, the exception to this is for the localisation data.
 * @property {object} builderMetadata.fluentTitle
 *   An object to be passed to fluent for generating the title of the group.
 *   This title should only be used if `title` is not present.
 * @property {string} imageUrl
 *   The image url to use for the group.
 * @property {string} faviconDataUrl
 *   The data url to use for the favicon, null if not available.
 * @property {string} imagePageUrl
 *   The url of the snapshot used to get the image and favicon urls.
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
export const SnapshotGroups = new (class SnapshotGroups {
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
    if (group.title && !group.builderMetadata?.title) {
      if (!group.builderMetadata) {
        group.builderMetadata = {};
      }
      group.builderMetadata.title = group.title;
    }
    await lazy.PlacesUtils.withConnectionWrapper(
      "SnapshotsGroups.jsm:add",
      async db => {
        // Create the new group
        let row = await db.executeCached(
          `
          INSERT INTO moz_places_metadata_snapshots_groups (builder, builder_data)
          VALUES (:builder, :builder_data)
          RETURNING id
          `,
          {
            builder: group.builder,
            builder_data: JSON.stringify(group.builderMetadata),
          }
        );
        id = row[0].getResultByIndex(0);

        await this.#insertUrls(db, id, urls);
      }
    );

    this.#prefetchScreenshotForGroup(id).catch(console.error);
    Services.obs.notifyObservers(null, "places-snapshot-group-added");
    return id;
  }

  /**
   * Modifies the metadata for a snapshot group.
   *
   * @param {SnapshotGroup} group
   *   The partial details of the group to modify. Must include the group's id.
   *   Any other properties update those properties of the group.
   *   If builder, imageUrl, lastAccessed or snapshotCount are specified then
   *   they are ignored.
   *   If a title is specified, it will override the original creation title.
   *   Passing in a null or empty title will restore the original one.
   *   If builderMetadata is passed-in, its properties are merged with the
   *   existing ones: new values for existing properties replace old values,
   *   new properties are added and null properties are removed.
   */
  async updateMetadata(group) {
    let params = { id: group.id };
    let setters = [];
    if (group.builderMetadata) {
      params.builder_data = JSON.stringify(group.builderMetadata);
      setters.push("builder_data = json_patch(builder_data, :builder_data)");
    }
    if ("title" in group) {
      // Store NULL rather than an empty string.
      params.title = group.title || null;
      setters.push("title = :title");
    }
    if ("hidden" in group) {
      params.hidden = group.hidden ? 1 : 0;
      setters.push("hidden = :hidden");
    }
    if (!setters.length) {
      return;
    }

    await lazy.PlacesUtils.withConnectionWrapper(
      "SnapshotsGroups.jsm:updateMetadata",
      async db => {
        await db.executeCached(
          `
            UPDATE moz_places_metadata_snapshots_groups
            SET ${setters.join(", ")}
            WHERE id = :id
          `,
          params
        );
      }
    );

    Services.obs.notifyObservers(null, "places-snapshot-group-updated");
  }

  /**
   * Modifies the urls for a snapshot group.
   * Note: This API does not manage deleting of groups if the number of urls is
   * 0. If there are no urls in the group, consider calling `delete()` instead.
   *
   * @param {number} id
   *   The id of the group to modify.
   * @param {string[]|Set<string>} [urls]
   *   The snapshot urls for the group. If the urls do not have associated
   *   snapshots then they are ignored.
   */
  async updateUrls(id, urls) {
    await lazy.PlacesUtils.withConnectionWrapper(
      "SnapshotsGroups.jsm:updateUrls",
      async db => {
        let params = { id };
        let SQLInFragment = [...urls]
          .map((url, i) => {
            params[`url${i}`] = url;
            return `hash(:url${i})`;
          })
          .join(",");

        await db.executeTransaction(async () => {
          // Note: queries here may need to be kept up to date with the
          // moz_places_metadata_groups_to_snapshots definition in nsPlacesTables.h

          // Create a temporary table to store the data.
          await db.execute(
            `
            CREATE TEMP TABLE __groups_to_snapshots__ AS
            SELECT s.group_id, s.place_id, s.hidden FROM moz_places_metadata_groups_to_snapshots s
            JOIN moz_places h
            ON h.id = s.place_id
            WHERE s.group_id = :id AND h.url_hash IN (${SQLInFragment})
          `,
            params
          );

          // Clear and copy back only what we require.
          await db.executeCached(
            `DELETE FROM moz_places_metadata_groups_to_snapshots WHERE group_id = :id`,
            { id }
          );

          await db.executeCached(
            `
            INSERT INTO moz_places_metadata_groups_to_snapshots(group_id, place_id, hidden)
            SELECT group_id, place_id, hidden FROM __groups_to_snapshots__
            `
          );

          // Finally insert any new urls and clean up.
          await this.#insertUrls(db, id, urls);

          await db.executeCached(`DROP TABLE __groups_to_snapshots__`);
        });
      }
    );

    this.#prefetchScreenshotForGroup(id).catch(console.error);
    Services.obs.notifyObservers(null, "places-snapshot-group-updated");
  }

  /**
   * Hides a url within a group.
   *
   * @param {number} id
   *   The id of the group to modify.
   * @param {string} url
   *   The url to hide.
   * @param {boolean} hidden
   *   If the snapshot should be hidden or not
   */
  async setUrlHidden(id, url, hidden) {
    await lazy.PlacesUtils.withConnectionWrapper(
      "SnapshotsGroups.jsm:hideUrl",
      async db => {
        await db.executeCached(
          `
          UPDATE moz_places_metadata_groups_to_snapshots
          SET hidden = :hidden
          WHERE group_id = :id AND place_id = (
            SELECT id FROM moz_places WHERE url_hash = hash(:url) AND url = :url
          )`,
          { id, url, hidden }
        );
      }
    );

    this.#prefetchScreenshotForGroup(id).catch(console.error);
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
    await lazy.PlacesUtils.withConnectionWrapper(
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
   *   Use -1 to specify no limit.
   * @param {boolean} [options.hidden]
   *   Pass true to also return hidden groups.
   * @param {boolean} [options.countHidden]
   *   Pass true to include hidden snapshots in the count.
   * @param {string} [options.builder]
   *   Limit searching snapshot groups to results from a particular builder.
   * @param {boolean} [options.skipMinimum]
   *   Skips the minimim limit for number of urls in a snapshot group. This is
   *   intended for builders to be able to store and retrieve possible groups
   *   that are below the current limit.
   * @returns {SnapshotGroup[]}
   *   An array of snapshot groups, in descending order of last access time.
   */
  async query({
    limit = 50,
    builder = undefined,
    hidden = false,
    countHidden = false,
    skipMinimum = false,
  } = {}) {
    let db = await lazy.PlacesUtils.promiseDBConnection();

    let params = {};
    let sizeFragment = [];
    let limitFragment = "";
    let joinFragment = "";
    if (!skipMinimum) {
      sizeFragment.push("HAVING snapshot_count >= :minGroupSize");
      params.minGroupSize = lazy.MIN_GROUP_SIZE;

      for (let [
        i,
        name,
      ] of lazy.SnapshotMonitor.skipMinimumSizeBuilders.entries()) {
        params[`name${i}`] = name;
        sizeFragment.push(` OR (builder = :name${i} AND snapshot_count >= 1)`);
      }
    }
    if (limit != -1) {
      params.limit = limit;
      limitFragment = "LIMIT :limit";
    }

    let whereTerms = [];

    if (builder) {
      whereTerms.push("builder = :builder");
      params.builder = builder;
    }

    if (!hidden) {
      whereTerms.push("g.hidden = 0");
    }

    if (!countHidden) {
      joinFragment = "AND s.hidden = 0";
    }

    let where = whereTerms.length ? `WHERE ${whereTerms.join(" AND ")}` : "";

    let rows = await db.executeCached(
      `
      SELECT g.id, IFNULL(g.title, g.builder_data->>'title') AS title, g.hidden, g.builder,
            g.builder_data, COUNT(s.group_id) AS snapshot_count,
            MAX(sn.last_interaction_at) AS last_access,
            (SELECT group_concat(IFNULL(preview_image_url, ''), '|')
                    || '|' ||
                    group_concat(url, '|') FROM (
              SELECT preview_image_url, url
              FROM moz_places_metadata_snapshots sns
              JOIN moz_places_metadata_groups_to_snapshots gs USING(place_id)
              JOIN moz_places h ON h.id = gs.place_id AND gs.hidden = 0
              WHERE gs.group_id = g.id
              ORDER BY sns.last_interaction_at ASC
              LIMIT 2
              )
            ) AS image_urls
      FROM moz_places_metadata_snapshots_groups g
      LEFT JOIN moz_places_metadata_groups_to_snapshots s ON s.group_id = g.id ${joinFragment}
      LEFT JOIN moz_places_metadata_snapshots sn ON sn.place_id = s.place_id
      ${where}
      GROUP BY g.id ${sizeFragment.join(" ")}
      ORDER BY last_access DESC
      ${limitFragment}
        `,
      params
    );

    return Promise.all(rows.map(row => this.#translateSnapshotGroupRow(row)));
  }

  /**
   * Obtains the snapshot urls for a particular group. This is designed for
   * builders to easily grab the list of urls in a group.
   *
   * @param {object} options
   * @param {number} options.id
   *   The id of the snapshot group to get the snapshots for.
   * @param {boolean} [options.hidden]
   *   Pass true to return hidden snapshots
   * @returns {string[]}
   *   An array of urls.
   */
  async getUrls({ id, hidden }) {
    let db = await lazy.PlacesUtils.promiseDBConnection();

    let whereClause = "";
    if (!hidden) {
      whereClause = `AND s.hidden = 0`;
    }

    let urlRows = await db.executeCached(
      `
      SELECT h.url
      FROM moz_places_metadata_groups_to_snapshots s
      JOIN moz_places h ON h.id = s.place_id
      WHERE s.group_id = :group_id
      ${whereClause}
      ORDER BY h.last_visit_date DESC
    `,
      { group_id: id }
    );

    return urlRows.map(row => row.getResultByName("url"));
  }

  /**
   * Obtains the snapshots for a particular group. This is designed
   * for batch use to avoid potentially pulling a large number of
   * snapshots across to the content process at one time.
   *
   * @param {object} options
   * @param {number} options.id
   *   The id of the snapshot group to get the snapshots for.
   * @param {number} [options.startIndex]
   *   The start index of the snapshots to return.
   * @param {number} [options.count]
   *   The number of snapshots to return.
   * @param {boolean} [options.hidden]
   *   Pass true to return hidden snapshots as well.
   * @param {boolean} [options.sortDescending]
   *   Whether or not to sortDescending. Defaults to true.
   * @param {string} [options.sortBy]
   *   A string to choose what to sort the snapshots by, e.g. "last_interaction_at"
   *   By default results are sorted by last_interaction_at.
   * @returns {Snapshots[]}
   *   An array of snapshots, in descending order of last interaction time
   */
  async getSnapshots({
    id,
    startIndex = 0,
    count = 50,
    hidden = false,
    sortDescending = true,
    sortBy = "last_interaction_at",
  } = {}) {
    if (!["last_visit_date", "last_interaction_at"].includes(sortBy)) {
      throw new Error("Unknown sortBy value");
    }
    let start = Math.max(0, startIndex);

    let snapshots = await lazy.Snapshots.query({
      limit: start + count,
      group: id,
      includeHiddenInGroup: hidden,
      sortBy,
      sortDescending,
    });

    let end = Math.min(snapshots.length, count + start);
    snapshots = snapshots.slice(start, end);
    lazy.PlacesUIUtils.insertTitleStartDiffs(snapshots);
    return snapshots;
  }

  /**
   * Inserts a set of urls into the database for a given snapshot group.
   *
   * @param {object} db
   *   The database connection to use.
   * @param {number} id
   *   The id of the group to add the urls to.
   * @param {string[]} urls
   *   An array of urls to insert for the group.
   */
  async #insertUrls(db, id, urls) {
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
      INSERT OR IGNORE INTO moz_places_metadata_groups_to_snapshots (group_id, place_id)
      SELECT :id, s.place_id
      FROM moz_places h
      JOIN moz_places_metadata_snapshots s
      ON h.id = s.place_id
      WHERE h.url_hash IN (${SQLInFragment.join(",")})
    `,
      params
    );
  }

  /**
   * Translates a snapshot group database row to a SnapshotGroup.
   *
   * @param {object} row
   *   The database row to translate.
   * @returns {SnapshotGroup}
   */
  async #translateSnapshotGroupRow(row) {
    // Group image selection should be done in this order:
    //   1. Oldest view in group featured image
    //   2. Second Oldest View in group featured image
    //   3. Oldest View in group screenshot
    // To check for featured image existence we can refer to the
    // moz_places.preview_image_url field, that includes the url of the featured
    // image.
    // TODO (MR2-1610): The features image is not cached yet, it should be
    // cached by PlacesPreview as a replacement for the screenshot, when
    // available.
    // The query returns featured1|featured2|url1|url2
    let imageUrls = row.getResultByName("image_urls")?.split("|");
    let imageUrl, faviconDataUrl, imagePageUrl;
    if (imageUrls) {
      imageUrl = imageUrls[0] || imageUrls[1];
      if (!imageUrl && imageUrls[2]) {
        // We don't have a featured image, thus use a moz-page-thumb screenshot.
        imageUrl = imageUrls[2];
        if (lazy.PlacesPreviews.enabled) {
          imageUrl = lazy.PlacesPreviews.getPageThumbURL(imageUrl);
        } else {
          imageUrl = lazy.PageThumbs.getThumbnailURL(imageUrl);
        }
      }

      // The favicon is for the same page we return a preview image for.
      imagePageUrl =
        imageUrls[2] && !imageUrls[0] && imageUrls[1]
          ? imageUrls[3]
          : imageUrls[2];
      if (imagePageUrl) {
        faviconDataUrl = await new Promise(resolve => {
          lazy.PlacesUtils.favicons.getFaviconDataForPage(
            Services.io.newURI(imagePageUrl),
            (uri, dataLength, data, mimeType) => {
              if (dataLength) {
                // NOTE: honestly this is awkward and inefficient. We build a string
                // with String.fromCharCode and then btoa that. It's a Uint8Array
                // under the hood, and we should probably just expose something in
                // ChromeUtils to Base64 encode a Uint8Array directly, but this is
                // fine for now.
                let b64 = btoa(
                  data.reduce((d, byte) => d + String.fromCharCode(byte), "")
                );
                resolve(`data:${mimeType};base64,${b64}`);
                return;
              }
              resolve(undefined);
            }
          );
        });
      }
    }

    let snapshotGroup = {
      id: row.getResultByName("id"),
      faviconDataUrl,
      imageUrl,
      imagePageUrl,
      title: row.getResultByName("title") || "",
      hidden: row.getResultByName("hidden") == 1,
      builder: row.getResultByName("builder"),
      builderMetadata: JSON.parse(row.getResultByName("builder_data")),
      snapshotCount: row.getResultByName("snapshot_count"),
      lastAccessed: row.getResultByName("last_access"),
    };

    return snapshotGroup;
  }

  /**
   * Prefetch a screenshot for the oldest snapshot in the group.
   * @param {number} id
   *   The id of the group to add the urls to.
   */
  async #prefetchScreenshotForGroup(id) {
    let snapshots = await this.getSnapshots({
      id,
      start: 0,
      count: 1,
      sortBy: "last_interaction_at",
      sortDescending: false,
    });

    if (!snapshots.length) {
      return;
    }

    let url = snapshots[0].url;
    if (lazy.PlacesPreviews.enabled) {
      await lazy.PlacesPreviews.update(url);
    } else {
      await lazy.BackgroundPageThumbs.captureIfMissing(url);
    }
  }
})();
