/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  CommonNames: "resource:///modules/CommonNames.sys.mjs",
  Interactions: "resource:///modules/Interactions.sys.mjs",
  InteractionsBlocklist: "resource:///modules/InteractionsBlocklist.sys.mjs",
  PageDataService: "resource:///modules/pagedata/PageDataService.sys.mjs",
  PlacesPreviews: "resource://gre/modules/PlacesPreviews.sys.mjs",
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
});

XPCOMUtils.defineLazyModuleGetters(lazy, {
  BackgroundPageThumbs: "resource://gre/modules/BackgroundPageThumbs.jsm",
  PageThumbs: "resource://gre/modules/PageThumbs.jsm",
  PageThumbsStorage: "resource://gre/modules/PageThumbs.jsm",
});

/**
 * @typedef {object} Recommendation
 *   A snapshot recommendation with an associated score.
 * @property {Snapshot} snapshot
 *   The recommended snapshot.
 * @property {number} score
 *   The score for this snapshot.
 * @property {object} [data]
 *   An optional object containing data used to calculate the score, printed
 *   as part of logs for debugging purposes.
 */

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

XPCOMUtils.defineLazyGetter(lazy, "logConsole", function() {
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
 */
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "snapshot_overlap_limit",
  "browser.places.interactions.snapshotOverlapLimit",
  1800000 // 1000 * 60 * 30
);

/**
 * Interval for the timeOfDay heuristic interval. The heuristic looks for
 * interactions within these milliseconds from the current time.
 * This interval is split in half, thus if it's 3pm, with a 1 hour interval, it
 * looks for snapshots between 2:30pm and 3:30 pm.
 */
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "snapshot_timeofday_interval_seconds",
  "browser.places.interactions.snapshotTimeOfDayIntervalSeconds",
  3600
);
/**
 * Maximum number of past days to look for timeOfDay snapshots.
 * Returning very old snapshots from the past may not be particularly useful
 * for the user.
 */
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "snapshot_timeofday_limit_days",
  "browser.places.interactions.snapshotTimeOfDayLimitDays",
  45
);
/**
 * Expected number of interactions with a page during snapshot_timeofday_limit_days
 * to assign maximum score. Less than these will cause a gradual reduced score.
 */
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "snapshot_timeofday_expected_interactions",
  "browser.places.interactions.snapshotTimeOfDayExpectedInteractions",
  10
);

/**
 * We may apply a penalty to timeOfDay scores, based on how much time elapsed
 * from the start of the browsing session.
 * After "start" time passed from the begin of the browsing session, the score
 * will be reduced linearly until 0.1 at the "end" time.
 * Setting the start time to -1 disables the penalty.
 */
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "snapshot_timeofday_sessiondecay_start_s",
  "browser.places.interactions.snapshotTimeOfDaySessionDecayStartSeconds",
  1800 // 30 mins
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "snapshot_timeofday_sessiondecay_end_s",
  "browser.places.interactions.snapshotTimeOfDaySessionDecayEndSeconds",
  28800 // 8 hours
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
  lazy,
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
 * @property {Snapshots.REMOVED_REASON} removedReason
 *   The reason the snapshot was deleted, can be one of REMOVED_REASON.*.
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
export const Snapshots = new (class Snapshots {
  USER_PERSISTED = {
    // The snapshot was created automatically.
    NO: 0,
    // The user created the snapshot manually, e.g. snapshot keyboard shortcut.
    MANUAL: 1,
    // The user pinned the page which caused the snapshot to be created.
    PINNED: 2,
  };

  REMOVED_REASON = {
    DISMISS: 0,
    NOT_RELEVANT: 1,
    PERSONAL: 2,
    EXPIRED: 3,
  };

  constructor() {
    // TODO: we should update the pagedata periodically. We first need a way to
    // track when the last update happened, we may add an updated_at column to
    // snapshots, though that requires some I/O to check it. Thus, we probably
    // want to accumulate changes and update on idle, plus store a cache of the
    // last notified pages to avoid hitting the same page continuously.
    // PageDataService.on("page-data", this.#onPageData);

    if (!lazy.PlacesPreviews.enabled) {
      lazy.PageThumbs.addExpirationFilter(this);
    }

    this.recommendationSources = {
      Overlapping: this.#queryOverlapping.bind(this),
      CommonReferrer: this.#queryCommonReferrer.bind(this),
      TimeOfDay: this.#queryTimeOfDay.bind(this),
    };
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
      let pageData = lazy.PageDataService.getCached(url);
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
        let pageInfo = lazy.PlacesUtils.validateItemProperties(
          "PageInfo",
          lazy.PlacesUtils.PAGEINFO_VALIDATORS,
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
        lazy.PageDataService.queueFetch(url);
      }

      this.#downloadPageImage(url, pageData?.image);
    }

    lazy.logConsole.debug(
      `Inserting ${pageDataIndex} page data for: ${urls.map(u => u.url)}.`
    );

    if (pageDataIndex == 0 && placesIndex == 0) {
      return;
    }

    await lazy.PlacesUtils.withConnectionWrapper(
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
      if (lazy.PlacesPreviews.enabled) {
        lazy.PlacesPreviews.update(url).catch(console.error);
      } else {
        lazy.BackgroundPageThumbs.captureIfMissing(url).catch(console.error);
      }
    }
  }

  /**
   * Returns the url up until, but not including, any hash mark identified fragments
   * For example, given  http://www.example.org/foo.html#bar, this function will return http://www.example.org/foo.html
   * @param {string} url
   *   The url associated with the snapshot.
   * @returns {string}
   *  The url up until, but not including, any fragments
   */
  stripFragments(url) {
    return url?.split("#")[0];
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
   * @param {string} [details.title]
   *   Optional user-provided title for the snapshot.
   * @param {Snapshots.USER_PERSISTED} [details.userPersisted]
   *   Whether the user created the snapshot and if they did, through what
   *   action, defaults to USER_PERSISTED.NO.
   */
  async add({ url, title, userPersisted = this.USER_PERSISTED.NO }) {
    if (!url) {
      throw new Error("Missing url parameter to Snapshots.add()");
    }

    url = this.stripFragments(url);

    if (!lazy.InteractionsBlocklist.canRecordUrl(url)) {
      throw new Error("This url cannot be added to snapshots");
    }

    let placeId = await lazy.PlacesUtils.withConnectionWrapper(
      "Snapshots: add",
      async db => {
        let now = Date.now();
        await lazy.PlacesUtils.maybeInsertPlace(db, new URL(url));

        // Title is updated only if the caller provided it.
        let updateTitleFragment =
          title !== undefined ? ", title = :title " : "";
        // If the user edits the title, update userPersisted accordingly. Note
        // that in case of an update, we'll not override higher USER_PERSISTED
        // values like PINNED with lower ones line MANUAL, to avoid losing
        // precious information.
        if (title !== undefined) {
          userPersisted = this.USER_PERSISTED.MANUAL;
        }

        // When the user asks for a snapshot to be created, we may not yet have
        // a corresponding interaction. We create a snapshot with 0 as the value
        // for first_interaction_at to flag it as missing a corresponding
        // interaction. We have a database trigger that will update this
        // snapshot with real values from the corresponding interaction when the
        // latter is created.
        let rows = await db.executeCached(
          `
            INSERT INTO moz_places_metadata_snapshots
              (place_id, first_interaction_at, last_interaction_at, document_type, created_at, user_persisted, title)
            SELECT h.id, IFNULL(min(m.created_at), CASE WHEN :userPersisted THEN 0 ELSE NULL END),
                  IFNULL(max(m.created_at), CASE WHEN :userPersisted THEN :createdAt ELSE NULL END),
                  IFNULL(first_value(m.document_type) OVER (PARTITION BY h.id ORDER BY m.created_at DESC), :documentFallback),
                  :createdAt, :userPersisted, :title
            FROM moz_places h
            LEFT JOIN moz_places_metadata m ON m.place_id = h.id
            WHERE h.url_hash = hash(:url) AND h.url = :url
            GROUP BY h.id
            ON CONFLICT DO UPDATE SET user_persisted = MAX(:userPersisted, user_persisted),
                                      removed_at = NULL
                                      ${updateTitleFragment}
                                  WHERE :userPersisted <> 0
            RETURNING place_id, created_at, user_persisted
          `,
          {
            createdAt: now,
            url,
            userPersisted,
            documentFallback: lazy.Interactions.DOCUMENT_TYPE.GENERIC,
            title: title || null, // Store null, not an empty string.
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

      this.#notify("places-snapshots-added", [{ url, userPersisted }]);
    }
  }

  /**
   * Deletes one or more snapshots.
   * Unless EXPIRED is passed as reason, this creates a tombstone rather than
   * removing the entry, so that heuristics can take into account user removed
   * snapshots.
   * Note, the caller is expected to take account of the userPersisted value
   * for a Snapshot when appropriate.
   *
   * @param {string|Array<string>} urls
   *   The url of the snapshot to delete, or an Array of urls.
   * @param {integer} reason
   *  One value from REMOVED_REASON.*. This is tracked in the data store.
   */
  async delete(urls, reason = 0) {
    if (
      typeof reason != "number" ||
      !Object.values(this.REMOVED_REASON).includes(reason)
    ) {
      throw new TypeError("Invalid value for 'reason'");
    }
    if (!Array.isArray(urls)) {
      urls = [urls];
    }
    urls = urls.map(this.stripFragments);

    let placeIdsSQLFragment = `
    SELECT id FROM moz_places
    WHERE url_hash IN (${lazy.PlacesUtils.sqlBindPlaceholders(
      urls,
      "hash(",
      ")"
    )}) AND url IN (${lazy.PlacesUtils.sqlBindPlaceholders(urls)})`;
    let queryArgs =
      reason == this.REMOVED_REASON.EXPIRED
        ? [
            `DELETE FROM moz_places_metadata_snapshots
         WHERE place_id IN (${placeIdsSQLFragment})
         RETURNING place_id`,
            [...urls, ...urls],
          ]
        : [
            `UPDATE moz_places_metadata_snapshots
         SET removed_at = ?, removed_reason = ?
         WHERE place_id IN (${placeIdsSQLFragment})
         RETURNING place_id`,
            [Date.now(), reason, ...urls, ...urls],
          ];

    await lazy.PlacesUtils.withConnectionWrapper(
      "Snapshots: delete",
      async db => {
        let placeIds = (await db.executeCached(...queryArgs)).map(r =>
          r.getResultByName("place_id")
        );
        // Remove orphan page data.
        await db.executeCached(
          `DELETE FROM moz_places_metadata_snapshots_extra
         WHERE place_id IN (${lazy.PlacesUtils.sqlBindPlaceholders(placeIds)})`,
          placeIds
        );
      }
    );

    this.#notify("places-snapshots-deleted", urls);
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
    url = this.stripFragments(url);
    let db = await lazy.PlacesUtils.promiseDBConnection();
    let extraWhereCondition = "";

    if (!includeTombstones) {
      extraWhereCondition = " AND removed_at IS NULL";
    }

    let rows = await db.executeCached(
      `
      SELECT h.url AS url, IFNULL(s.title, h.title) AS title, created_at,
             removed_at, removed_reason, document_type,
             first_interaction_at, last_interaction_at,
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
   *   -1 may be used to get all snapshots, e.g. for use by the group builders.
   * @param {boolean} [options.includeTombstones]
   *   Whether to include tombstones in the snapshots to obtain.
   * @param {number} [options.type]
   *   Restrict the snapshots to those with a particular type of page data available.
   * @param {number} [options.group]
   *   Restrict the snapshots to those within a particular group.
   * @param {boolean} [includeSnapshotsInUserManagedGroups]
   *   Whether snapshots that are in a user managed group should be included.
   *   Snapshots that are part of multiple groups are excluded even if only one
   *   of the groups is user managed.
   *   This is ignored if a specific .group id is passed. Defaults to true.
   * @param {boolean} [options.includeHiddenInGroup]
   *   Only applies when querying a particular group. Pass true to include
   *   snapshots that are hidden in the group.
   * @param {boolean} [options.includeUserPersisted]
   *   Whether to include user persisted snapshots.
   * @param {number} [options.lastInteractionBefore]
   *   Restrict to snaphots whose last interaction was before the given time.
   * @param {boolean} [options.sortDescending]
   *   Whether or not to sortDescending. Defaults to true.
   * @param {string} [options.sortBy]
   *   A string to choose what to sort the snapshots by, e.g. "last_interaction_at"
   *   By default results are sorted by last_interaction_at.
   * @returns {Snapshot[]}
   *   Returns snapshots in order of descending last interaction time.
   */
  async query({
    limit = 100,
    includeTombstones = false,
    type = undefined,
    group = undefined,
    includeSnapshotsInUserManagedGroups = true,
    includeHiddenInGroup = false,
    includeUserPersisted = true,
    lastInteractionBefore = undefined,
    sortDescending = true,
    sortBy = "last_interaction_at",
  } = {}) {
    let db = await lazy.PlacesUtils.promiseDBConnection();

    let clauses = [];
    let bindings = {};
    let joins = [];
    let limitStatement = "";

    if (!includeTombstones) {
      clauses.push("removed_at IS NULL");
    }

    if (!includeUserPersisted) {
      clauses.push("user_persisted = :user_persisted");
      bindings.user_persisted = this.USER_PERSISTED.NO;
    }
    if (lastInteractionBefore) {
      clauses.push("last_interaction_at < :last_interaction_before");
      bindings.last_interaction_before = lastInteractionBefore;
    }

    if (type) {
      clauses.push("type = :type");
      bindings.type = type;
    }

    if (group) {
      clauses.push("group_id = :group");
      if (!includeHiddenInGroup) {
        clauses.push("gs.hidden = 0");
      }
      bindings.group = group;
      joins.push(
        "LEFT JOIN moz_places_metadata_groups_to_snapshots gs USING(place_id)"
      );
    } else if (!includeSnapshotsInUserManagedGroups) {
      // TODO: if we change the way to define user managed groups, we should
      // update this condition.
      clauses.push(`NOT EXISTS(
        SELECT 1 FROM moz_places_metadata_snapshots_groups g
        JOIN moz_places_metadata_groups_to_snapshots gs ON g.id = gs.group_id
        WHERE gs.place_id = h.id AND builder == 'user'
      )`);
    }

    if (limit != -1) {
      limitStatement = "LIMIT :limit";
      bindings.limit = limit;
    }

    let whereStatement = clauses.length ? `WHERE ${clauses.join(" AND ")}` : "";

    let rows = await db.executeCached(
      `
      SELECT h.url, IFNULL(s.title, h.title) AS title, created_at,
             removed_at, removed_reason, document_type,
             first_interaction_at, last_interaction_at,
             user_persisted, description, site_name, preview_image_url,
             group_concat('[' || e.type || ', ' || e.data || ']') AS page_data,
             h.visit_count
      FROM moz_places_metadata_snapshots s
      JOIN moz_places h ON h.id = s.place_id
      LEFT JOIN moz_places_metadata_snapshots_extra e USING(place_id)
      ${joins.join(" ")}
      ${whereStatement}
      GROUP BY s.place_id
      ORDER BY ${sortBy} ${sortDescending ? "DESC" : "ASC"}
      ${limitStatement}
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
    let db = await lazy.PlacesUtils.promiseDBConnection();

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
   * Queries snapshots that were browsed within an hour of visiting the given
   * context url
   *
   * For example, if a user visited Site A two days ago, we would generate a
   * list of snapshots that were visited within an hour of that visit.
   * Site A may have also been visited four days ago, we would like to see what
   * websites were browsed then.
   *
   * @param {SelectionContext} selectionContext
   *   the selection context to inform recommendations
   * @returns {Recommendation[]}
   *   Returns array of overlapping snapshots in order of descending overlappingVisitScore (Calculated as 1.0 to 0.0, as the overlap gap goes to snapshot_overlap_limit)
   */
  async #queryOverlapping(selectionContext) {
    let current_id = await this.queryPlaceIdFromUrl(selectionContext.url);
    if (current_id == -1) {
      lazy.logConsole.debug(
        `PlaceId not found for url ${selectionContext.url}`
      );
      return [];
    }

    let db = await lazy.PlacesUtils.promiseDBConnection();

    let rows = await db.executeCached(
      `SELECT h.url AS url, IFNULL(s.title, h.title) AS title,
              o.overlappingVisitScore, created_at, removed_at, removed_reason,
              document_type, first_interaction_at, last_interaction_at,
              user_persisted, description, site_name, preview_image_url,
              group_concat('[' || e.type || ', ' || e.data || ']') AS page_data,
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
      { current_id, snapshot_overlap_limit: lazy.snapshot_overlap_limit }
    );

    if (!rows.length) {
      lazy.logConsole.debug("No overlapping snapshots");
    }

    return rows.map(row => ({
      snapshot: this.#translateRow(row),
      score: row.getResultByName("overlappingVisitScore"),
    }));
  }

  /**
   * Queries snapshots which have interactions sharing a common referrer with
   * the context url's interactions
   *
   * @param {SelectionContext} selectionContext
   *   the selection context to inform recommendations
   * @returns {Recommendation[]}
   *   Returns array of snapshots with the common referrer
   */
  async #queryCommonReferrer(selectionContext) {
    let db = await lazy.PlacesUtils.promiseDBConnection();

    let context_place_id = await this.queryPlaceIdFromUrl(selectionContext.url);
    if (context_place_id == -1) {
      return [];
    }

    let rows = await db.executeCached(
      `
      SELECT h.id, h.url AS url, IFNULL(s.title, h.title) AS title, s.created_at,
             removed_at, removed_reason, s.document_type,
             first_interaction_at, last_interaction_at,
             user_persisted, description, site_name, preview_image_url, h.visit_count,
             group_concat('[' || e.type || ', ' || e.data || ']') AS page_data
      FROM moz_places_metadata_snapshots s
      JOIN moz_places h
      ON h.id = s.place_id
      LEFT JOIN moz_places_metadata_snapshots_extra e
      ON e.place_id = s.place_id
      WHERE s.place_id IN (
        SELECT p1.place_id FROM moz_places_metadata p1 JOIN moz_places_metadata p2 USING (referrer_place_id)
        WHERE p2.place_id = :context_place_id AND p1.place_id <> :context_place_id
      )
      GROUP BY s.place_id
    `,
      { context_place_id }
    );

    if (!rows.length) {
      lazy.logConsole.debug("No common referrer snapshots");
    }

    return rows.map(row => ({
      snapshot: this.#translateRow(row),
      score: 1.0,
    }));
  }

  /**
   * Queries snapshots that were browsed within an hour of the current time of
   * day, but on a previous day.
   *
   * @param {SelectionContext} selectionContext
   *   the selection context to inform recommendations
   * @returns {Recommendation[]}
   *   Returns array of snapshots with the common referrer
   */
  async #queryTimeOfDay(selectionContext) {
    let db = await lazy.PlacesUtils.promiseDBConnection();

    // The query applies the current time to a past date, then calculates the
    // time bracket starting from there. This should be more robust to DST
    // changes than using the number of seconds from start of day.
    let rows = await db.executeCached(
      `
      WITH times AS (
        SELECT time(:context_time_s, 'unixepoch') AS time
      )
      SELECT h.id, h.url AS url, IFNULL(s.title, h.title) AS title, s.created_at,
            removed_at, removed_reason, s.document_type,
            first_interaction_at, last_interaction_at,
            user_persisted, description, site_name, preview_image_url, h.visit_count,
            (SELECT group_concat('[' || e.type || ', ' || e.data || ']')
             FROM moz_places_metadata_snapshots
             LEFT JOIN moz_places_metadata_snapshots_extra e USING(place_id)
             WHERE place_id = h.id) AS page_data,
            count(*) AS interactions
      FROM moz_places_metadata_snapshots s
      JOIN moz_places h ON h.id = s.place_id
      LEFT JOIN times
      JOIN moz_places_metadata i USING(place_id)
      WHERE url_hash <> hash(:context_url)
        AND i.created_at
          BETWEEN unixepoch('now', 'utc', 'start of day', '-' || :days_limit || ' days') * 1000
          AND (:context_time_s - :interval_s / 2) * 1000 /* don't match the current interval */
        AND i.created_at
          BETWEEN (unixepoch(date(i.created_at / 1000, 'unixepoch') || " " || time) - :interval_s / 2) * 1000
              AND (unixepoch(date(i.created_at / 1000, 'unixepoch') || " " || time) + :interval_s / 2) * 1000
      GROUP BY s.place_id
    `,
      {
        context_url: selectionContext.url,
        context_time_s: parseInt(selectionContext.time / 1000),
        interval_s: lazy.snapshot_timeofday_interval_seconds,
        days_limit: lazy.snapshot_timeofday_limit_days,
      }
    );

    if (!rows.length) {
      lazy.logConsole.debug("No timeOfDay snapshots");
    }

    let interactionCounts = { min: 1, max: 1 };
    let entries = rows.map(row => {
      let interactions = row.getResultByName("interactions");
      interactionCounts.max = Math.max(interactionCounts.max, interactions);
      interactionCounts.min = Math.min(interactionCounts.min, interactions);
      return {
        snapshot: this.#translateRow(row),
        data: { interactions },
      };
    });

    // Add a score to each result.
    // For small intervals it doesn't make sense to use a bell curve giving
    // more weight to the exact time, but if we start evaluating much larger
    // intervals in the future, it may be worth investigating.
    // For now instead we assign a score based on the number of interactions
    // with the page during `snapshot_timeofday_limit_days`.
    entries.forEach(e => {
      e.score = this.timeOfDayScore({
        interactions: e.data.interactions,
        context: selectionContext,
        ...interactionCounts,
      });
    });
    return entries;
  }

  /**
   * Calculate score for a timeOfDay entry, based on the number of interactions.
   *
   * @param {number} interactions
   *  The number of interactions with the page.
   * @param {object} context
   *  The selection context.
   * @param {number} min
   *  The minimum number of interactions during snapshot_timeofday_limit_days.
   * @param {number} max
   *  The maximum number of interactions during snapshot_timeofday_limit_days.
   * @returns {float} Calculated score for the page.
   * @note This function is useful for testing scores.
   */
  timeOfDayScore({ interactions, context, min, max }) {
    // Assign score 1.0 to the pages having more than max / 2 interactions,
    // other pages get a decreasing score between 0.5 and 1.0.
    let score = 1.0;
    if (interactions < max / 2) {
      score = 0.5 * (1 + (interactions - min) / (max - min));
    }
    // If the number of interactions is lower than `snapshot_timeofday_expected_interactions`
    // threshold, apply a penalty to the score.
    if (interactions < lazy.snapshot_timeofday_expected_interactions) {
      score *=
        0.5 *
        (1 +
          (interactions - 1) /
            (lazy.snapshot_timeofday_expected_interactions - 1));
    }
    // Finally, if the session decay prefs are set, apply an additional penalty
    // based on how much time passed from the start of the browsing session.
    // The idea behind this is that starting a new session is likely to identify
    // the begin of a workflow, that may be repeated in days.
    // The score is reduced linearly to 0.1 between a "start" and an "end" time.
    if (
      context.sessionStartTime &&
      lazy.snapshot_timeofday_sessiondecay_start_s >= 0
    ) {
      let timeFromSessionStart = context.time - context.sessionStartTime;
      let decay = {
        start: lazy.snapshot_timeofday_sessiondecay_start_s * 1000,
        end: lazy.snapshot_timeofday_sessiondecay_end_s * 1000,
      };
      if (score > 0.1 && timeFromSessionStart > decay.start) {
        if (timeFromSessionStart > decay.end) {
          return 0.1;
        }
        score +=
          ((timeFromSessionStart - decay.start) * (0.1 - score)) / decay.end;
      }
    }
    // Round to 2 decimal positions.
    return Math.round(score * 1e2) / 1e2;
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
    let pageData;
    let pageDataStr = row.getResultByName("page_data");
    if (pageDataStr) {
      try {
        let dataArray = JSON.parse(`[${pageDataStr}]`);
        pageData = new Map(dataArray);
      } catch (e) {
        lazy.logConsole.error(e);
      }
    }

    let snapshot = {
      url: row.getResultByName("url"),
      title: row.getResultByName("title"),
      siteName: row.getResultByName("site_name"),
      description: row.getResultByName("description"),
      image: row.getResultByName("preview_image_url"),
      createdAt: this.#toDate(row.getResultByName("created_at")),
      removedAt: this.#toDate(row.getResultByName("removed_at")),
      removedReason: row.getResultByName("removed_reason"),
      firstInteractionAt: this.#toDate(
        row.getResultByName("first_interaction_at")
      ),
      lastInteractionAt: this.#toDate(
        row.getResultByName("last_interaction_at")
      ),
      documentType: row.getResultByName("document_type"),
      userPersisted: row.getResultByName("user_persisted"),
      pageData: pageData ?? new Map(),
      visitCount: row.getResultByName("visit_count"),
    };

    snapshot.commonName = lazy.CommonNames.getName(snapshot);
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
    if (lazy.PlacesPreviews.enabled) {
      if (await lazy.PlacesPreviews.update(url).catch(console.error)) {
        return lazy.PlacesPreviews.getPageThumbURL(url);
      }
    } else {
      await lazy.BackgroundPageThumbs.captureIfMissing(url).catch(
        console.error
      );
      if (await lazy.PageThumbsStorage.fileExistsForURL(url)) {
        return lazy.PageThumbs.getThumbnailURL(url);
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

    if (urls) {
      lazy.logConsole.debug(
        `Testing ${urls.length} potential snapshots: ${urls}`
      );
    } else {
      lazy.logConsole.debug("Testing for any potential snapshot");
    }

    let model;
    try {
      model = JSON.parse(lazy.snapshotCriteria);

      if (!model.length) {
        lazy.logConsole.debug(
          `No snapshot criteria provided, falling back to default`
        );
        model = DEFAULT_CRITERIA;
      }
    } catch (e) {
      lazy.logConsole.error(
        "Invalid snapshot criteria, falling back to default.",
        e
      );
      model = DEFAULT_CRITERIA;
    }

    let insertedUrls = await lazy.PlacesUtils.withConnectionWrapper(
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
          for (let protocol of lazy.InteractionsBlocklist.urlRequirements.keys()) {
            filters.push(
              `(url_hash BETWEEN hash('${protocol}', 'prefix_lo') AND hash('${protocol}', 'prefix_hi'))`
            );
          }
          urlFilter = " WHERE " + filters.join(" OR ");
        } else {
          let urlMatches = [];
          urls.forEach((url, idx) => {
            if (!lazy.InteractionsBlocklist.canRecordUrl(url)) {
              lazy.logConsole.debug(`Url can't be added to snapshots: ${url}`);
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
      lazy.logConsole.debug(
        `${insertedUrls.length} snapshots created: ${insertedUrls}`
      );
      await this.#addPageData(insertedUrls);
      this.#notify(
        "places-snapshots-added",
        insertedUrls.map(result => {
          return {
            url: result.url,
            userPersisted: this.USER_PERSISTED.NO,
          };
        })
      );
    }
  }

  /**
   * Completely clears the store. This exists for testing purposes.
   */
  async reset() {
    await lazy.PlacesUtils.withConnectionWrapper(
      "Snapshots.jsm::reset",
      async db => {
        await db.executeCached(`DELETE FROM moz_places_metadata_snapshots`);
      }
    );
  }
})();
