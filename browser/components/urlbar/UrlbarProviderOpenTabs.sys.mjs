/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This module exports a provider, returning open tabs matches for the urlbar.
 * It is also used to register and unregister open tabs.
 */

import {
  UrlbarProvider,
  UrlbarUtils,
} from "resource:///modules/UrlbarUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
  UrlbarProvidersManager: "resource:///modules/UrlbarProvidersManager.sys.mjs",
  UrlbarResult: "resource:///modules/UrlbarResult.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "logger", () =>
  UrlbarUtils.getLogger({ prefix: "Provider.OpenTabs" })
);

const PRIVATE_USER_CONTEXT_ID = -1;

/**
 * Class used to create the provider.
 */
export class UrlbarProviderOpenTabs extends UrlbarProvider {
  constructor() {
    super();
  }

  /**
   * Returns the name of this provider.
   *
   * @returns {string} the name of this provider.
   */
  get name() {
    return "OpenTabs";
  }

  /**
   * Returns the type of this provider.
   *
   * @returns {integer} one of the types from UrlbarUtils.PROVIDER_TYPE.*
   */
  get type() {
    return UrlbarUtils.PROVIDER_TYPE.PROFILE;
  }

  /**
   * Whether this provider should be invoked for the given context.
   * If this method returns false, the providers manager won't start a query
   * with this provider, to save on resources.
   *
   * @param {UrlbarQueryContext} queryContext The query context object
   * @returns {boolean} Whether this provider should be invoked for the search.
   */
  isActive(queryContext) {
    // For now we don't actually use this provider to query open tabs, instead
    // we join the temp table in UrlbarProviderPlaces.
    return false;
  }

  /**
   * Tracks whether the memory tables have been initialized yet. Until this
   * happens tabs are only stored in openTabs and later copied over to the
   * memory table.
   */
  static memoryTableInitialized = false;

  /**
   * Maps the open tabs by userContextId.
   */
  static _openTabs = new Map();

  /**
   * Return urls that is opening on given user context id.
   *
   * @param {integer} userContextId Containers user context id
   * @param {boolean} isInPrivateWindow In private browsing window or not
   * @returns {Array} urls
   */
  static getOpenTabs(userContextId, isInPrivateWindow) {
    userContextId = UrlbarProviderOpenTabs.getUserContextIdForOpenPagesTable(
      userContextId,
      isInPrivateWindow
    );
    return UrlbarProviderOpenTabs._openTabs.get(userContextId);
  }

  /**
   * Return userContextId that will be used in moz_openpages_temp table.
   *
   * @param {integer} userContextId Containers user context id
   * @param {boolean} isInPrivateWindow In private browsing window or not
   * @returns {interger} userContextId
   */
  static getUserContextIdForOpenPagesTable(userContextId, isInPrivateWindow) {
    return isInPrivateWindow ? PRIVATE_USER_CONTEXT_ID : userContextId;
  }

  /**
   * Copy over cached open tabs to the memory table once the Urlbar
   * connection has been initialized.
   */
  static promiseDBPopulated =
    lazy.PlacesUtils.largeCacheDBConnDeferred.promise.then(async () => {
      // Must be set before populating.
      UrlbarProviderOpenTabs.memoryTableInitialized = true;
      // Populate the table with the current cached tabs.
      for (let [userContextId, urls] of UrlbarProviderOpenTabs._openTabs) {
        for (let url of urls) {
          await addToMemoryTable(url, userContextId).catch(console.error);
        }
      }
    });

  /**
   * Registers a tab as open.
   *
   * @param {string} url Address of the tab
   * @param {integer} userContextId Containers user context id
   * @param {boolean} isInPrivateWindow In private browsing window or not
   */
  static async registerOpenTab(url, userContextId, isInPrivateWindow) {
    lazy.logger.info("Registering openTab: ", {
      url,
      userContextId,
      isInPrivateWindow,
    });
    userContextId = UrlbarProviderOpenTabs.getUserContextIdForOpenPagesTable(
      userContextId,
      isInPrivateWindow
    );

    if (!UrlbarProviderOpenTabs._openTabs.has(userContextId)) {
      UrlbarProviderOpenTabs._openTabs.set(userContextId, []);
    }
    UrlbarProviderOpenTabs._openTabs.get(userContextId).push(url);
    await addToMemoryTable(url, userContextId).catch(console.error);
  }

  /**
   * Unregisters a previously registered open tab.
   *
   * @param {string} url Address of the tab
   * @param {integer} userContextId Containers user context id
   * @param {boolean} isInPrivateWindow In private browsing window or not
   */
  static async unregisterOpenTab(url, userContextId, isInPrivateWindow) {
    lazy.logger.info("Unregistering openTab: ", {
      url,
      userContextId,
      isInPrivateWindow,
    });
    userContextId = UrlbarProviderOpenTabs.getUserContextIdForOpenPagesTable(
      userContextId,
      isInPrivateWindow
    );

    let openTabs = UrlbarProviderOpenTabs._openTabs.get(userContextId);
    if (openTabs) {
      let index = openTabs.indexOf(url);
      if (index != -1) {
        openTabs.splice(index, 1);
        await removeFromMemoryTable(url, userContextId).catch(console.error);
      }
    }
  }

  /**
   * Starts querying.
   *
   * @param {object} queryContext The query context object
   * @param {Function} addCallback Callback invoked by the provider to add a new
   *        match.
   * @returns {Promise} resolved when the query stops.
   */
  async startQuery(queryContext, addCallback) {
    // Note: this is not actually expected to be used as an internal provider,
    // because normal history search will already coalesce with the open tabs
    // temp table to return proper frecency.
    // TODO:
    //  * properly search and handle tokens, this is just a mock for now.
    let instance = this.queryInstance;
    let conn = await lazy.PlacesUtils.promiseLargeCacheDBConnection();
    await UrlbarProviderOpenTabs.promiseDBPopulated;
    await conn.executeCached(
      `
      SELECT url, userContextId
      FROM moz_openpages_temp
    `,
      {},
      (row, cancel) => {
        if (instance != this.queryInstance) {
          cancel();
          return;
        }
        addCallback(
          this,
          new lazy.UrlbarResult(
            UrlbarUtils.RESULT_TYPE.TAB_SWITCH,
            UrlbarUtils.RESULT_SOURCE.TABS,
            {
              url: row.getResultByName("url"),
              userContextId: row.getResultByName("userContextId"),
            }
          )
        );
      }
    );
  }
}

/**
 * Adds an open page to the memory table.
 *
 * @param {string} url Address of the page
 * @param {number} userContextId Containers user context id
 * @returns {Promise} resolved after the addition.
 */
async function addToMemoryTable(url, userContextId) {
  if (!UrlbarProviderOpenTabs.memoryTableInitialized) {
    return;
  }
  await lazy.UrlbarProvidersManager.runInCriticalSection(async () => {
    let conn = await lazy.PlacesUtils.promiseLargeCacheDBConnection();
    await conn.executeCached(
      `
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
    `,
      { url, userContextId }
    );
  });
}

/**
 * Removes an open page from the memory table.
 *
 * @param {string} url Address of the page
 * @param {number} userContextId Containers user context id
 * @returns {Promise} resolved after the removal.
 */
async function removeFromMemoryTable(url, userContextId) {
  if (!UrlbarProviderOpenTabs.memoryTableInitialized) {
    return;
  }
  await lazy.UrlbarProvidersManager.runInCriticalSection(async () => {
    let conn = await lazy.PlacesUtils.promiseLargeCacheDBConnection();
    await conn.executeCached(
      `
      UPDATE moz_openpages_temp
      SET open_count = open_count - 1
      WHERE url = :url
        AND userContextId = :userContextId
    `,
      { url, userContextId }
    );
  });
}
