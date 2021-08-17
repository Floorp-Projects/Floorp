/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Snapshots"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const VERSION_PREF = "browser.places.snapshots.version";

XPCOMUtils.defineLazyModuleGetters(this, {
  CommonNames: "resource:///modules/CommonNames.jsm",
  Interactions: "resource:///modules/Interactions.jsm",
  PageDataCollector: "resource:///modules/pagedata/PageDataCollector.jsm",
  PageDataService: "resource:///modules/pagedata/PageDataService.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

/**
 * @typedef {object} SnapshotCriteria
 *   A set of tests to check if a set of interactions are a snapshot.
 * @property {string} property
 *   The property to check.
 * @property {string} aggregator
 *   An aggregator to apply ("sum", "avg", "max", "min").
 * @property {number} cutoff
 *   The value to compare against.
 * @property {?number} interactionCount
 *   The maximum number of interactions to consider.
 * @property {?number} interactionRecency
 *   The maximum age of interactions to consider, in milliseconds.
 */

XPCOMUtils.defineLazyGetter(this, "logConsole", function() {
  return console.createInstance({
    prefix: "SnapshotsManager",
    maxLogLevel: Services.prefs.getBoolPref(
      "browser.places.interactions.log",
      false
    )
      ? "Debug"
      : "Warn",
  });
});

const DEFAULT_CRITERIA = [
  {
    property: "total_view_time",
    aggregator: "max",
    cutoff: 30000,
  },
  {
    property: "total_view_time",
    aggregator: "sum",
    cutoff: 120000,
    interactionCount: 5,
  },
  {
    property: "key_presses",
    aggregator: "sum",
    cutoff: 250,
    interactionCount: 10,
  },
];

/**
 * This is an array of criteria that would make a page a snapshot. Each element is a SnapshotCriteria.
 * A page is a snapshot if any of the criteria apply.
 */
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "snapshotCriteria",
  "browser.places.interactions.snapshotCriteria",
  JSON.stringify(DEFAULT_CRITERIA)
);

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
 * @property {Map<type, data>} pageData
 *   Collection of PageData by type. See PageDataService.jsm
 */

/**
 * Handles storing and retrieving of Snapshots in the Places database.
 *
 * Notifications of updates are sent via the observer service:
 * - places-snapshots-added, data: JSON encoded array of urls
 *     Sent when a new snapshot is added
 * - places-snapshots-deleted, data: JSON encoded array of urls
 *     Sent when a snapshot is removed.
 */
const Snapshots = new (class Snapshots {
  constructor() {
    // TODO: we should update the pagedata periodically. We first need a way to
    // track when the last update happened, we may add an updated_at column to
    // snapshots, though that requires some I/O to check it. Thus, we probably
    // want to accumulate changes and update on idle, plus store a cache of the
    // last notified pages to avoid hitting the same page continuously.
    // PageDataService.on("page-data", this.#onPageData);
  }

  #notify(topic, urls) {
    Services.obs.notifyObservers(null, topic, JSON.stringify(urls));
  }

  /**
   * Fetches page data for the given urls and stores it with snapshots.
   * @param {Array<Objects>} urls Array of {placeId, url} tuples.
   */
  async #addPageData(urls) {
    let index = 0;
    let values = [];
    let bindings = {};
    for (let { placeId, url } of urls) {
      let pageData = PageDataService.getCached(url);
      if (pageData?.data.length) {
        for (let data of pageData.data) {
          if (Object.values(PageDataCollector.DATA_TYPE).includes(data.type)) {
            bindings[`id${index}`] = placeId;
            bindings[`type${index}`] = data.type;
            // We store the whole data object that also includes type because
            // it makes easier to query all the data at once and then build a
            // Map from it.
            bindings[`data${index}`] = JSON.stringify(data);
            values.push(`(:id${index}, :type${index}, :data${index})`);
            index++;
          }
        }
      } else {
        // TODO: queuing a fetch will notify page-data once done, if any data
        // was found, but we're not yet handling that, see the constructor.
        PageDataService.queueFetch(url).catch(console.error);
      }
    }

    logConsole.debug(
      `Inserting ${index} page data for: ${urls.map(u => u.url)}.`
    );

    if (index == 0) {
      return;
    }

    await PlacesUtils.withConnectionWrapper(
      "Snapshots.jsm::addPageData",
      async db => {
        await db.execute(
          `
          INSERT OR REPLACE INTO moz_places_metadata_snapshots_extra
            (place_id, type, data)
            VALUES ${values.join(", ")}
          `,
          bindings
        );
      }
    );
  }

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

    let placeId = await PlacesUtils.withConnectionWrapper(
      "Snapshots: add",
      async db => {
        let now = Date.now();
        await this.#maybeInsertPlace(db, new URL(url));
        // When the user asks for a snapshot to be created, we may not yet have
        // a corresponding interaction. We create a snapshot with 0 as the value
        // for first_interaction_at to flag it as missing a corresponding
        // interaction. We have a database trigger that will update this
        // snapshot with real values from the corresponding interaction when the
        // latter is created.
        let rows = await db.executeCached(
          `
            INSERT INTO moz_places_metadata_snapshots
              (place_id, first_interaction_at, last_interaction_at, document_type, created_at, user_persisted)
            SELECT h.id, IFNULL(min(m.created_at), CASE WHEN :userPersisted THEN 0 ELSE NULL END),
                  IFNULL(max(m.created_at), CASE WHEN :userPersisted THEN :createdAt ELSE NULL END),
                  IFNULL(first_value(m.document_type) OVER (PARTITION BY h.id ORDER BY m.created_at DESC), :documentFallback),
                  :createdAt, :userPersisted
            FROM moz_places h
            LEFT JOIN moz_places_metadata m ON m.place_id = h.id
            WHERE h.url_hash = hash(:url) AND h.url = :url
            GROUP BY h.id
            ON CONFLICT DO UPDATE SET user_persisted = :userPersisted, removed_at = NULL WHERE :userPersisted = 1
            RETURNING place_id, created_at, user_persisted
          `,
          {
            createdAt: now,
            url,
            userPersisted,
            documentFallback: Interactions.DOCUMENT_TYPE.GENERIC,
          }
        );

        if (rows.length) {
          // If created_at doesn't match then this url was already a snapshot,
          // and we only overwrite it when the new request is user_persisted.
          if (
            rows[0].getResultByName("created_at") != now &&
            !rows[0].getResultByName("user_persisted")
          ) {
            return null;
          }
          return rows[0].getResultByName("place_id");
        }
        return null;
      }
    );

    if (placeId) {
      await this.#addPageData([{ placeId, url }]);

      this.#notify("places-snapshots-added", [url]);
    }
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
      let placeId = (
        await db.executeCached(
          `UPDATE moz_places_metadata_snapshots
           SET removed_at = :removedAt
           WHERE place_id = (SELECT id FROM moz_places WHERE url_hash = hash(:url) AND url = :url)
           RETURNING place_id`,
          { removedAt: Date.now(), url }
        )
      )[0].getResultByName("place_id");
      // Remove orphan page data.
      await db.executeCached(
        `DELETE FROM moz_places_metadata_snapshots_extra
         WHERE place_id = :placeId`,
        { placeId }
      );
    });

    this.#notify("places-snapshots-deleted", [url]);
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
      SELECT h.url AS url, h.title AS title, created_at, removed_at,
             document_type, first_interaction_at, last_interaction_at,
             user_persisted, group_concat(e.data, ",") AS page_data
             FROM moz_places_metadata_snapshots s
      JOIN moz_places h ON h.id = s.place_id
      LEFT JOIN moz_places_metadata_snapshots_extra e ON e.place_id = s.place_id
      WHERE h.url_hash = hash(:url) AND h.url = :url
       ${extraWhereCondition}
      GROUP BY s.place_id
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
   * @param {number} [options.type]
   *   Restrict the snapshots to those with a particular type of page data available.
   * @returns {Snapshot[]}
   *   Returns snapshots in order of descending last interaction time.
   */
  async query({
    limit = 100,
    includeTombstones = false,
    type = undefined,
  } = {}) {
    await this.#ensureVersionUpdates();
    let db = await PlacesUtils.promiseDBConnection();

    let clauses = [];
    let bindings = { limit };

    if (!includeTombstones) {
      clauses.push("removed_at IS NULL");
    }

    if (type) {
      clauses.push("type = :type");
      bindings.type = type;
    }

    let whereStatement = clauses.length ? `WHERE ${clauses.join(" AND ")}` : "";

    let rows = await db.executeCached(
      `
      SELECT h.url AS url, h.title AS title, created_at, removed_at,
             document_type, first_interaction_at, last_interaction_at,
             user_persisted, group_concat(e.data, ",") AS page_data
             FROM moz_places_metadata_snapshots s
      JOIN moz_places h ON h.id = s.place_id
      LEFT JOIN moz_places_metadata_snapshots_extra e ON e.place_id = s.place_id
      ${whereStatement}
      GROUP BY s.place_id
      ORDER BY last_interaction_at DESC
      LIMIT :limit
    `,
      bindings
    );

    return rows.map(row => this.#translateRow(row));
  }

  /**
   * Ensures that the database is migrated to the latest version. Migrations
   * should be exception-safe: don't throw an uncaught Error, or else we'll skip
   * subsequent migrations.
   */
  async #ensureVersionUpdates() {
    let dbVersion = Services.prefs.getIntPref(VERSION_PREF, 0);
    try {
      if (dbVersion < 1) {
        try {
          // Delete legacy keyframes.sqlite DB.
          let profileDir = await PathUtils.getProfileDir();
          let pathToKeyframes = PathUtils.join(profileDir, "keyframes.sqlite");
          await IOUtils.remove(pathToKeyframes);
        } catch (ex) {
          console.warn(`Failed to delete keyframes.sqlite: ${ex}`);
        }
      }
    } finally {
      Services.prefs.setIntPref(VERSION_PREF, this.currentVersion);
    }
  }

  /**
   * Returns the database's most recent version number.
   * @returns {number}
   */
  get currentVersion() {
    return 1;
  }

  /**
   * Translates a database row to a Snapshot.
   *
   * @param {object} row
   *   The database row to translate.
   * @returns {Snapshot}
   */
  #translateRow(row) {
    // Maps data type to data.
    let pageData = new Map();
    let pageDataStr = row.getResultByName("page_data");
    if (pageDataStr) {
      try {
        let dataArray = JSON.parse(`[${pageDataStr}]`);
        dataArray.forEach(d => pageData.set(d.type, d.data));
      } catch (e) {
        logConsole.error(e);
      }
    }

    let snapshot = {
      url: row.getResultByName("url"),
      title: row.getResultByName("title"),
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
      pageData,
    };

    snapshot.commonName = CommonNames.getName(snapshot);
    return snapshot;
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

  /**
   * Adds snapshots for any pages where the gathered metadata meets a set of
   * criteria.
   *
   * @param {string[]} urls
   *  The list of pages to check, if undefined all pages are checked.
   */
  async updateSnapshots(urls = undefined) {
    if (urls !== undefined && !urls.length) {
      return;
    }

    logConsole.debug(
      `Testing ${urls ? urls.length : "all"} potential snapshots`
    );

    let model;
    try {
      model = JSON.parse(snapshotCriteria);

      if (!model.length) {
        logConsole.debug(
          `No snapshot criteria provided, falling back to default`
        );
        model = DEFAULT_CRITERIA;
      }
    } catch (e) {
      logConsole.error(
        "Invalid snapshot criteria, falling back to default.",
        e
      );
      model = DEFAULT_CRITERIA;
    }

    let insertedUrls = await PlacesUtils.withConnectionWrapper(
      "Snapshots.jsm::updateSnapshots",
      async db => {
        let bindings = {};

        let urlFilter = "";
        if (urls !== undefined) {
          let urlMatches = [];

          urls.forEach((url, idx) => {
            bindings[`url${idx}`] = url;
            urlMatches.push(
              `(url_hash = hash(:url${idx}) AND url = :url${idx})`
            );
          });

          urlFilter = `WHERE ${urlMatches.join(" OR ")}`;
        }

        let modelQueries = [];
        model.forEach((criteria, idx) => {
          let wheres = [];

          if (criteria.interactionCount) {
            wheres.push(`row <= :count${idx}`);
            bindings[`count${idx}`] = criteria.interactionCount;
          }

          if (criteria.interactionRecency) {
            wheres.push(`created_at >= :recency${idx}`);
            bindings[`recency${idx}`] = Date.now() - criteria.interactionCount;
          }

          let where = wheres.length ? `WHERE ${wheres.join(" AND ")}` : "";

          modelQueries.push(
            `
            SELECT
                place_id,
                min(created_at) AS first_interaction_at,
                max(created_at) AS last_interaction_at,
                doc_type,
                :createdAt
              FROM metadata
              ${where}
              GROUP BY place_id
              HAVING ${criteria.aggregator}(${criteria.property}) >= :cutoff${idx}
            `
          );
          bindings[`cutoff${idx}`] = criteria.cutoff;
        });

        let query = `
          WITH metadata AS (
            SELECT
                moz_places_metadata.*,
                row_number() OVER (PARTITION BY place_id ORDER BY created_at DESC) AS row,
                first_value(document_type) OVER (PARTITION BY place_id ORDER BY created_at DESC) AS doc_type
              FROM moz_places_metadata JOIN moz_places ON moz_places_metadata.place_id = moz_places.id
              ${urlFilter}
          )
          INSERT OR IGNORE INTO moz_places_metadata_snapshots
            (place_id, first_interaction_at, last_interaction_at, document_type, created_at)
          ${modelQueries.join(" UNION ")}
          RETURNING place_id, (SELECT url FROM moz_places WHERE id=place_id) AS url, created_at
          `;

        let now = Date.now();

        let results = await db.execute(query, {
          ...bindings,
          createdAt: now,
        });

        let newUrls = [];
        for (let row of results) {
          // If created_at differs from the passed value then this snapshot already existed.
          if (row.getResultByName("created_at") == now) {
            newUrls.push({
              placeId: row.getResultByName("place_id"),
              url: row.getResultByName("url"),
            });
          }
        }

        return newUrls;
      }
    );

    if (insertedUrls.length) {
      logConsole.debug(`${insertedUrls.length} snapshots created`);
      await this.#addPageData(insertedUrls);
      this.#notify(
        "places-snapshots-added",
        insertedUrls.map(result => result.url)
      );
    }
  }

  /**
   * Completely clears the store. This exists for testing purposes.
   */
  async reset() {
    await PlacesUtils.withConnectionWrapper(
      "Snapshots.jsm::reset",
      async db => {
        await db.executeCached(`DELETE FROM moz_places_metadata_snapshots`);
      }
    );
  }

  /**
   * Tries to insert a new place if it doesn't exist yet.
   * @param {mozIStorageAsyncConnection} db
   *   The connection to the Places database.
   * @param {URL} url
   *   A valid URL object.
   */
  async #maybeInsertPlace(db, url) {
    // The IGNORE conflict can trigger on `guid`.
    await db.executeCached(
      `INSERT OR IGNORE INTO moz_places (url, url_hash, rev_host, hidden, frecency, guid)
       VALUES (:url, hash(:url), :rev_host, 1, -1,
               IFNULL((SELECT guid FROM moz_places WHERE url_hash = hash(:url) AND url = :url),
                     GENERATE_GUID()))
      `,
      {
        url: url.href,
        rev_host: PlacesUtils.getReversedHost(url),
      }
    );
    await db.executeCached("DELETE FROM moz_updateoriginsinsert_temp");
  }
})();
