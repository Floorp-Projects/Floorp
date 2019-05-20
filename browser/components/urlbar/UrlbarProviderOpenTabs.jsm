/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module exports a provider, returning open tabs matches for the urlbar.
 * It is also used to register and unregister open tabs.
 */

var EXPORTED_SYMBOLS = ["UrlbarProviderOpenTabs"];

const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetters(this, {
  Log: "resource://gre/modules/Log.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  UrlbarProvider: "resource:///modules/UrlbarUtils.jsm",
  UrlbarProvidersManager: "resource:///modules/UrlbarProvidersManager.jsm",
  UrlbarResult: "resource:///modules/UrlbarResult.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

XPCOMUtils.defineLazyGetter(this, "logger",
  () => Log.repository.getLogger("Urlbar.Provider.OpenTabs"));

/**
 * Class used to create the provider.
 */
class ProviderOpenTabs extends UrlbarProvider {
  constructor() {
    super();
    // Maps the open tabs by userContextId.
    this.openTabs = new Map();
    // Maps the running queries by queryContext.
    this.queries = new Map();
  }

  /**
   * Database handle. For performance reasons the temp tables are created and
   * populated only when a query starts, rather than when a tab is added.
   * @returns {object} the Sqlite database handle.
   */
  async promiseDb() {
    if (this._db) {
      return this._db;
    }
    let conn = await PlacesUtils.promiseLargeCacheDBConnection();
    // Create the temp tables to store open pages.
    UrlbarProvidersManager.runInCriticalSection(async () => {
      // These should be kept up-to-date with the definition in nsPlacesTables.h.
      await conn.execute(`
        CREATE TEMP TABLE IF NOT EXISTS moz_openpages_temp (
          url TEXT,
          userContextId INTEGER,
          open_count INTEGER,
          PRIMARY KEY (url, userContextId)
        )
      `);
      await conn.execute(`
        CREATE TEMP TRIGGER IF NOT EXISTS moz_openpages_temp_afterupdate_trigger
        AFTER UPDATE OF open_count ON moz_openpages_temp FOR EACH ROW
        WHEN NEW.open_count = 0
        BEGIN
          DELETE FROM moz_openpages_temp
          WHERE url = NEW.url
            AND userContextId = NEW.userContextId;
        END
      `);
    }).catch(Cu.reportError);

    // Populate the table with the current cache contents...
    for (let [userContextId, urls] of this.openTabs) {
      for (let url of urls) {
        await addToMemoryTable(conn, url, userContextId).catch(Cu.reportError);
      }
    }
    return this._db = conn;
  }

  /**
   * Returns the name of this provider.
   * @returns {string} the name of this provider.
   */
  get name() {
    return "OpenTabs";
  }

  /**
   * Returns the type of this provider.
   * @returns {integer} one of the types from UrlbarUtils.PROVIDER_TYPE.*
   */
  get type() {
    return UrlbarUtils.PROVIDER_TYPE.PROFILE;
  }

  /**
   * Whether this provider should be invoked for the given context.
   * If this method returns false, the providers manager won't start a query
   * with this provider, to save on resources.
   * @param {UrlbarQueryContext} queryContext The query context object
   * @returns {boolean} Whether this provider should be invoked for the search.
   */
  isActive(queryContext) {
    // For now we don't actually use this provider to query open tabs, instead
    // we join the temp table in UnifiedComplete.
    return false;
  }

  /**
   * Whether this provider wants to restrict results to just itself.
   * Other providers won't be invoked, unless this provider doesn't
   * support the current query.
   * @param {UrlbarQueryContext} queryContext The query context object
   * @returns {boolean} Whether this provider wants to restrict results.
   */
  isRestricting(queryContext) {
    return false;
  }

  /**
   * Registers a tab as open.
   * @param {string} url Address of the tab
   * @param {integer} userContextId Containers user context id
   */
  registerOpenTab(url, userContextId = 0) {
    if (!this.openTabs.has(userContextId)) {
      this.openTabs.set(userContextId, []);
    }
    this.openTabs.get(userContextId).push(url);
    if (this._db) {
      addToMemoryTable(this._db, url, userContextId);
    }
  }

  /**
   * Unregisters a previously registered open tab.
   * @param {string} url Address of the tab
   * @param {integer} userContextId Containers user context id
   */
  unregisterOpenTab(url, userContextId = 0) {
    let openTabs = this.openTabs.get(userContextId);
    if (openTabs) {
      let index = openTabs.indexOf(url);
      if (index != -1) {
        openTabs.splice(index, 1);
        if (this._db) {
          removeFromMemoryTable(this._db, url, userContextId);
        }
      }
    }
  }

  /**
   * Starts querying.
   * @param {object} queryContext The query context object
   * @param {function} addCallback Callback invoked by the provider to add a new
   *        match.
   * @returns {Promise} resolved when the query stops.
   */
  async startQuery(queryContext, addCallback) {
    // Note: this is not actually expected to be used as an internal provider,
    // because normal history search will already coalesce with the open tabs
    // temp table to return proper frecency.
    // TODO:
    //  * properly search and handle tokens, this is just a mock for now.
    logger.info(`Starting query for ${queryContext.searchString}`);
    let instance = {};
    this.queries.set(queryContext, instance);
    let conn = await this.promiseDb();
    await conn.executeCached(`
      SELECT url, userContextId
      FROM moz_openpages_temp
    `, {}, (row, cancel) => {
      if (!this.queries.has(queryContext)) {
        cancel();
        return;
      }
      addCallback(this, new UrlbarResult(UrlbarUtils.RESULT_TYPE.TAB_SWITCH,
                                         UrlbarUtils.RESULT_SOURCE.TABS, {
        url: row.getResultByName("url"),
        userContextId: row.getResultByName("userContextId"),
      }));
    });
    // We are done.
    this.queries.delete(queryContext);
  }

  /**
   * Cancels a running query.
   * @param {object} queryContext The query context object
   */
  cancelQuery(queryContext) {
    logger.info(`Canceling query for ${queryContext.searchString}`);
    this.queries.delete(queryContext);
  }
}

var UrlbarProviderOpenTabs = new ProviderOpenTabs();

/**
 * Adds an open page to the memory table.
 * @param {object} conn A Sqlite.jsm database handle
 * @param {string} url Address of the page
 * @param {number} userContextId Containers user context id
 * @returns {Promise} resolved after the addition.
 */
async function addToMemoryTable(conn, url, userContextId) {
  return UrlbarProvidersManager.runInCriticalSection(async () => {
    await conn.executeCached(`
      INSERT OR REPLACE INTO moz_openpages_temp (url, userContextId, open_count)
      VALUES ( :url,
                :userContextId,
                IFNULL( ( SELECT open_count + 1
                          FROM moz_openpages_temp
                          WHERE url = :url
                          AND userContextId = :userContextId ),
                        1
                      )
              )
    `, { url, userContextId });
  });
}

/**
 * Removes an open page from the memory table.
 * @param {object} conn A Sqlite.jsm database handle
 * @param {string} url Address of the page
 * @param {number} userContextId Containers user context id
 * @returns {Promise} resolved after the removal.
 */
async function removeFromMemoryTable(conn, url, userContextId) {
  return UrlbarProvidersManager.runInCriticalSection(async () => {
    await conn.executeCached(`
      UPDATE moz_openpages_temp
      SET open_count = open_count - 1
      WHERE url = :url
        AND userContextId = :userContextId
    `, { url, userContextId });
  });
}
