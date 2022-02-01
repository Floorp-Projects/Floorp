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
  BackgroundPageThumbs: "resource://gre/modules/BackgroundPageThumbs.jsm",
  CommonNames: "resource:///modules/CommonNames.jsm",
  Interactions: "resource:///modules/Interactions.jsm",
  PageDataService: "resource:///modules/pagedata/PageDataService.jsm",
  PageThumbs: "resource://gre/modules/PageThumbs.jsm",
  PageThumbsStorage: "resource://gre/modules/PageThumbs.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  PlacesPreviews: "resource://gre/modules/PlacesPreviews.jsm",
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

/**
 * Snapshots are considered overlapping if interactions took place within snapshot_overlap_limit milleseconds of each other.
 * Default to a half-hour on each end of the interactions.
 *
 */
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "snapshot_overlap_limit",
  "browser.places.interactions.snapshotOverlapLimit",
  1800000 // 1000 * 60 * 30
);

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
 * @property {Snapshots.USER_PERSISTED} userPersisted
 *   Whether the user created the snapshot and if they did, through what action.
 * @property {Map<type, data>} pageData
 *   Collection of PageData by type. See PageDataService.jsm
 * @property {Number} overlappingVisitScore
 *   Calculated score based on overlapping visits to the context url. In the range [0.0, 1.0]
 * @property {number} [relevancyScore]
 *   The relevancy score associated with the snapshot.
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
  USER_PERSISTED = {
    // The snapshot was created automatically.
    NO: 0,
    // The user created the snapshot manually, e.g. snapshot keyboard shortcut.
    MANUAL: 1,
    // The user pinned the page which caused the snapshot to be created.
    PINNED: 2,
  };

  constructor() {
    // TODO: we should update the pagedata periodically. We first need a way to
    // track when the last update happened, we may add an updated_at column to
    // snapshots, though that requires some I/O to check it. Thus, we probably
    // want to accumulate changes and update on idle, plus store a cache of the
    // last notified pages to avoid hitting the same page continuously.
    // PageDataService.on("page-data", this.#onPageData);

    if (!PlacesPreviews.enabled) {
      PageThumbs.addExpirationFilter(this);
    }
  }

  /**
   * Only certain urls can be added as Snapshots, either manually or
   * automatically.
   * @returns {Map} A Map keyed by protocol, for each protocol an object may
   *          define stricter requirements, like extension.
   */
  get urlRequirements() {
    return new Map([
      ["http:", {}],
      ["https:", {}],
      ["file:", { extension: "pdf" }],
    ]);
  }

  #notify(topic, urls) {
    Services.obs.notifyObservers(null, topic, JSON.stringify(urls));
  }

  /**
   * This is called by PageThumbs to see what thumbnails should be kept alive.
   * Currently, the last 100 snapshots are kept alive.
   * @param {function} callback
   */
  async filterForThumbnailExpiration(callback) {
    let snapshots = await this.query();
    let urls = [];
    for (let snapshot of snapshots) {
      urls.push(snapshot.url);
    }
    callback(urls);
  }

  /**
   * Fetches page data for the given urls and stores it with snapshots.
   * @param {Array<Objects>} urls Array of {placeId, url} tuples.
   */
  async #addPageData(urls) {
    let pageDataIndex = 0;
    let pageDataValues = [];
    let pageDataBindings = {};

    let placesIndex = 0;
    let placesValues = [];
    let placesBindings = {};

    for (let { placeId, url } of urls) {
      let pageData = PageDataService.getCached(url);
      if (pageData) {
        for (let [type, data] of Object.entries(pageData.data)) {
          pageDataBindings[`id${pageDataIndex}`] = placeId;
          pageDataBindings[`type${pageDataIndex}`] = type;
          pageDataBindings[`data${pageDataIndex}`] = JSON.stringify(data);
          pageDataValues.push(
            `(:id${pageDataIndex}, :type${pageDataIndex}, :data${pageDataIndex})`
          );
          pageDataIndex++;
        }

        let { siteName, description, image: previewImageURL } = pageData;
        let pageInfo = PlacesUtils.validateItemProperties(
          "PageInfo",
          PlacesUtils.PAGEINFO_VALIDATORS,
          { siteName, description, previewImageURL }
        );

        placesBindings[`id${placesIndex}`] = placeId;
        placesBindings[`desc${placesIndex}`] = pageInfo.description ?? null;
        placesBindings[`site${placesIndex}`] = pageInfo.siteName ?? null;
        placesBindings[`image${placesIndex}`] =
          pageInfo.previewImageURL?.toString() ?? null;
        placesValues.push(
          `(:id${placesIndex}, :desc${placesIndex}, :site${placesIndex}, :image${placesIndex})`
        );
        placesIndex++;
      } else {
        // TODO: queuing a fetch will notify page-data once done, if any data
        // was found, but we're not yet handling that, see the constructor.
        PageDataService.queueFetch(url);
      }

      this.#downloadPageImage(url, pageData?.image);
    }

    logConsole.debug(
      `Inserting ${pageDataIndex} page data for: ${urls.map(u => u.url)}.`
    );

    if (pageDataIndex == 0 && placesIndex == 0) {
      return;
    }

    await PlacesUtils.withConnectionWrapper(
      "Snapshots.jsm::addPageData",
      async db => {
        if (placesIndex) {
          await db.execute(
            `
          WITH pd("place_id", "description", "siteName", "image") AS (
            VALUES ${placesValues.join(", ")}
          )
          UPDATE moz_places
            SET
              description = pd.description,
              site_name = pd.siteName,
              preview_image_url = pd.image
            FROM pd
            WHERE moz_places.id = pd.place_id;
          `,
            placesBindings
          );
        }

        if (pageDataIndex) {
          await db.execute(
            `
          INSERT OR REPLACE INTO moz_places_metadata_snapshots_extra
            (place_id, type, data)
            VALUES ${pageDataValues.join(", ")}
          `,
            pageDataBindings
          );
        }
      }
    );
  }

  /**
   * TODO: Store the downloaded and rescaled page image. For now this
   * only kicks off the page thumbnail if there isn't a meta data image.
   *
   * @param {string} url The URL of the page. This is only used if there isn't
   *    a `metaDataImageURL`.
   * @param {string} metaDataImageURL The URL of the meta data page image. If
   *    this exists, it is prioritized over the page thumbnail.
   */
  #downloadPageImage(url, metaDataImageURL = null) {
    if (!metaDataImageURL) {
      // No metadata image was found, start the process to capture a thumbnail
      // so it will be ready when needed. Ignore any errors since we can
      // fallback to a favicon.
      if (PlacesPreviews.enabled) {
        PlacesPreviews.update(url).catch(console.error);
      } else {
        BackgroundPageThumbs.captureIfMissing(url).catch(console.error);
      }
    }
  }

  /**
   * Whether a given URL can be added to snapshots.
   * The rules are defined in this.urlRequirements.
   * @param {string|URL|nsIURI} url The URL to check.
   * @returns {boolean} whether the url can be added to snapshots.
   */
  canSnapshotUrl(url) {
    let protocol, pathname;
    if (typeof url == "string") {
      url = new URL(url);
    }
    if (url instanceof Ci.nsIURI) {
      protocol = url.scheme + ":";
      pathname = url.filePath;
    } else {
      protocol = url.protocol;
      pathname = url.pathname;
    }
    let requirements = this.urlRequirements.get(protocol);
    return (
      requirements &&
      (!requirements.extension || pathname.endsWith(requirements.extension))
    );
  }

  /**
   * Adds a new snapshot.
   *
   * If the snapshot already exists, and this is a user-persisted addition,
   * then the userPersisted value will be updated, and the removed_at flag will be
   * cleared.
   *
   * @param {object} details
   * @param {string} details.url
   *   The url associated with the snapshot.
   * @param {Snapshots.USER_PERSISTED} [details.userPersisted]
   *   Whether the user created the snapshot and if they did, through what
   *   action, defaults to USER_PERSISTED.NO.
   */
  async add({ url, userPersisted = this.USER_PERSISTED.NO }) {
    if (!url) {
      throw new Error("Missing url parameter to Snapshots.add()");
    }
    if (!this.canSnapshotUrl(url)) {
      throw new Error("This url cannot be added to snapshots");
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
            ON CONFLICT DO UPDATE SET user_persisted = :userPersisted, removed_at = NULL WHERE :userPersisted <> 0
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
            rows[0].getResultByName("user_persisted") == this.USER_PERSISTED.NO
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
             user_persisted, description, site_name, preview_image_url,
             group_concat('[' || e.type || ', ' || e.data || ']') AS page_data,
             h.visit_count
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
             user_persisted, description, site_name, preview_image_url,
             group_concat('[' || e.type || ', ' || e.data || ']') AS page_data,
             h.visit_count
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
   * Queries the place id from moz_places for a given url
   *
   * @param {string} url
   *   url to query the place_id of
   * @returns {number}
   *   place_id of the given url or -1 if not found
   */
  async queryPlaceIdFromUrl(url) {
    await this.#ensureVersionUpdates();
    let db = await PlacesUtils.promiseDBConnection();

    let rows = await db.executeCached(
      `SELECT id from moz_places p
      WHERE p.url = :url
      `,
      { url }
    );

    if (!rows.length) {
      return -1;
    }

    return rows[0].getResultByName("id");
  }

  /**
   * Queries snapshots that were browsed within an hour of visiting the given context url
   *
   *   For example, if a user visited Site A two days ago, we would generate a list of snapshots that were visited within an hour of that visit.
   *   Site A may have also been visited four days ago, we would like to see what websites were browsed then.
   *
   * @param {string} context_url
   *   the url that we're collection snapshots whose interactions overlapped
   * @returns {Snapshot[]}
   *   Returns array of overlapping snapshots in order of descending overlappingVisitScore (Calculated as 1.0 to 0.0, as the overlap gap goes to snapshot_overlap_limit)
   */
  async queryOverlapping(context_url) {
    await this.#ensureVersionUpdates();

    let current_id = await this.queryPlaceIdFromUrl(context_url);
    if (current_id == -1) {
      logConsole.debug(`PlaceId not found for url ${context_url}`);
      return [];
    }

    let db = await PlacesUtils.promiseDBConnection();

    let rows = await db.executeCached(
      `SELECT h.url AS url, h.title AS title, o.overlappingVisitScore, created_at, removed_at,
      document_type, first_interaction_at, last_interaction_at,
      user_persisted, description, site_name, preview_image_url, group_concat('[' || e.type || ', ' || e.data || ']') AS page_data,
      h.visit_count
      FROM moz_places_metadata_snapshots s
      JOIN moz_places h ON h.id = s.place_id
      JOIN (
        SELECT place_id,
        MIN(SUM(CASE
          WHEN (page_start > snapshot_end) > 0 THEN MAX(1.0 - (page_start - snapshot_end) / CAST(:snapshot_overlap_limit as float), 0.0)
          WHEN (page_end < snapshot_start) > 0 THEN MAX(1.0 - (snapshot_start - page_end) / CAST(:snapshot_overlap_limit as float), 0.0)
        ELSE 1.0
        END), 1.0) overlappingVisitScore
        FROM
          (SELECT created_at AS page_start, created_at - :snapshot_overlap_limit AS page_start_limit, updated_at AS page_end, updated_at + :snapshot_overlap_limit AS page_end_limit FROM moz_places_metadata WHERE place_id = :current_id) AS current_page
          JOIN
          (SELECT place_id, created_at AS snapshot_start, updated_at AS snapshot_end FROM moz_places_metadata WHERE place_id != :current_id) AS suggestion
        WHERE
          snapshot_start BETWEEN page_start_limit AND page_end_limit
          OR
          snapshot_end BETWEEN page_start_limit AND page_end_limit
          OR
          (snapshot_start < page_start_limit AND snapshot_end > page_end_limit)
        GROUP BY place_id
      ) o ON s.place_id = o.place_id
      LEFT JOIN moz_places_metadata_snapshots_extra e ON e.place_id = s.place_id
      GROUP BY s.place_id
      ORDER BY o.overlappingVisitScore DESC;`,
      { current_id, snapshot_overlap_limit }
    );

    if (!rows.length) {
      logConsole.debug("No overlapping snapshots");
      return [];
    }

    return rows.map(row =>
      this.#translateRow(row, { includeOverlappingVisitScore: true })
    );
  }

  /**
   * Queries snapshots which have interactions sharing a common referrer with the context url
   *
   * @param {string} context_url
   *   the url that we're collecting snapshots for
   * @param {string} referrer_url
   *   the referrer of the url we're collecting snapshots for (may be empty)
   * @returns {Snapshot[]}
   *   Returns array of snapshots with the common referrer
   */
  async queryCommonReferrer(context_url, referrer_url) {
    await this.#ensureVersionUpdates();
    let db = await PlacesUtils.promiseDBConnection();

    let context_place_id = await this.queryPlaceIdFromUrl(context_url);
    if (context_place_id == -1) {
      return [];
    }

    let context_referrer_id = await this.queryPlaceIdFromUrl(referrer_url);
    if (context_referrer_id == -1) {
      return [];
    }

    let rows = await db.executeCached(
      `
      SELECT h.id, h.url AS url, h.title AS title, s.created_at,
        removed_at, s.document_type, first_interaction_at, last_interaction_at, user_persisted,
        description, site_name, preview_image_url, h.visit_count,
        group_concat('[' || e.type || ', ' || e.data || ']') AS page_data
      FROM moz_places_metadata_snapshots s
      JOIN moz_places h
      ON h.id = s.place_id
      LEFT JOIN moz_places_metadata_snapshots_extra e
      ON e.place_id = s.place_id
      WHERE s.place_id != :context_place_id AND s.place_id IN (SELECT DISTINCT place_id FROM moz_places_metadata WHERE referrer_place_id = :context_referrer_id)
      GROUP BY s.place_id
    `,
      { context_referrer_id, context_place_id }
    );

    return rows.map(row => {
      let snapshot = this.#translateRow(row);
      snapshot.commonReferrerScore = 1.0;
      return snapshot;
    });
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
   * @param {object} [options]
   * @param {boolean} [options.includeOverlappingVisitScore]
   *   Whether to retrieve the overlappingVisitScore field
   * @returns {Snapshot}
   */
  #translateRow(row, { includeOverlappingVisitScore = false } = {}) {
    // Maps data type to data.
    let pageData;
    let pageDataStr = row.getResultByName("page_data");
    if (pageDataStr) {
      try {
        let dataArray = JSON.parse(`[${pageDataStr}]`);
        pageData = new Map(dataArray);
      } catch (e) {
        logConsole.error(e);
      }
    }

    let overlappingVisitScore = 0;
    if (includeOverlappingVisitScore) {
      overlappingVisitScore = row.getResultByName("overlappingVisitScore");
    }

    let snapshot = {
      url: row.getResultByName("url"),
      title: row.getResultByName("title"),
      siteName: row.getResultByName("site_name"),
      description: row.getResultByName("description"),
      image: row.getResultByName("preview_image_url"),
      createdAt: this.#toDate(row.getResultByName("created_at")),
      removedAt: this.#toDate(row.getResultByName("removed_at")),
      firstInteractionAt: this.#toDate(
        row.getResultByName("first_interaction_at")
      ),
      lastInteractionAt: this.#toDate(
        row.getResultByName("last_interaction_at")
      ),
      documentType: row.getResultByName("document_type"),
      userPersisted: row.getResultByName("user_persisted"),
      overlappingVisitScore,
      pageData: pageData ?? new Map(),
      visitCount: row.getResultByName("visit_count"),
    };

    snapshot.commonName = CommonNames.getName(snapshot);
    return snapshot;
  }

  /**
   * Get the image that should represent the snapshot. The image URL
   * returned comes from the following priority:
   *   1) meta data page image
   *   2) thumbnail of the page
   * @param {Snapshot} snapshot
   *
   * @returns {string?}
   */
  async getSnapshotImageURL(snapshot) {
    if (snapshot.image) {
      return snapshot.image;
    }
    const url = snapshot.url;
    if (PlacesPreviews.enabled) {
      if (await PlacesPreviews.update(url).catch(console.error)) {
        return PlacesPreviews.getPageThumbURL(url);
      }
    } else {
      await BackgroundPageThumbs.captureIfMissing(url).catch(console.error);
      if (await PageThumbsStorage.fileExistsForURL(url)) {
        return PageThumbs.getThumbnailURL(url);
      }
    }
    return null;
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
        if (urls == undefined) {
          // TODO: checking .pdf file extension in SQL is more complex than we'd
          // like, since the manually adding API is already checking it, this is
          // basically only allowing file:// urls added automatically, and it's
          // likely in the future these picking rules will be replaced by some
          // ML machinery. Thus it seems not worth the added complexity.
          let filters = [];
          for (let protocol of this.urlRequirements.keys()) {
            filters.push(
              `(url_hash BETWEEN hash('${protocol}', 'prefix_lo') AND hash('${protocol}', 'prefix_hi'))`
            );
          }
          urlFilter = " WHERE " + filters.join(" OR ");
        } else {
          let urlMatches = [];
          urls.forEach((url, idx) => {
            if (!this.canSnapshotUrl(url)) {
              logConsole.debug(`Url can't be added to snapshots: ${url}`);
              return;
            }
            bindings[`url${idx}`] = url;
            urlMatches.push(
              `(url_hash = hash(:url${idx}) AND url = :url${idx})`
            );
          });
          if (!urlMatches.length) {
            // All the urls were discarded.
            return [];
          }
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
            bindings[`recency${idx}`] =
              Date.now() - criteria.interactionRecency;
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
              FROM moz_places_metadata
              JOIN moz_places h ON moz_places_metadata.place_id = h.id
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

        logConsole.debug(`Inserted ${results.length} snapshots`);

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
