/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  NetworkHelper:
    "resource://devtools/shared/network-observer/NetworkHelper.sys.mjs",
});

/**
 * Global cache session object.
 */
let gCacheSession = null;

/**
 * Get (and create if necessary) a cache session / cache storage session.
 *
 * @param {nsIRequest} request
 */
function getCacheSession(request) {
  if (!gCacheSession) {
    try {
      const cacheService = Services.cache2;
      if (cacheService) {
        let loadContext = lazy.NetworkHelper.getRequestLoadContext(request);
        if (!loadContext) {
          // Get default load context if we can't fetch.
          loadContext = Services.loadContextInfo.default;
        }
        gCacheSession = cacheService.diskCacheStorage(loadContext);
      }
    } catch (e) {
      gCacheSession = null;
    }
  }

  return gCacheSession;
}

/**
 * Parses a cache entry returned from the backend to build a response cache
 * object.
 *
 * @param {nsICacheEntry} cacheEntry
 *     The cache entry from the backend.
 *
 * @returns {Object}
 *     A responseCache object expected by RDP.
 */
function buildResponseCacheObject(cacheEntry) {
  const cacheObject = {};
  try {
    if (cacheEntry.storageDataSize) {
      cacheObject.storageDataSize = cacheEntry.storageDataSize;
    }
  } catch (e) {
    // We just need to handle this in case it's a js file of 0B.
  }
  if (cacheEntry.expirationTime) {
    cacheObject.expirationTime = cacheEntry.expirationTime;
  }
  if (cacheEntry.fetchCount) {
    cacheObject.fetchCount = cacheEntry.fetchCount;
  }
  if (cacheEntry.lastFetched) {
    cacheObject.lastFetched = cacheEntry.lastFetched;
  }
  if (cacheEntry.lastModified) {
    cacheObject.lastModified = cacheEntry.lastModified;
  }
  if (cacheEntry.deviceID) {
    cacheObject.deviceID = cacheEntry.deviceID;
  }
  return cacheObject;
}

/**
 * Does the fetch for the cache entry from the session.
 *
 * @param {nsIRequest} request
 *     The request object.
 *
 * @returns {Promise}
 *     Promise which resolve a response cache object object, or null if none
 *     was available.
 */
export function getResponseCacheObject(request) {
  const cacheSession = getCacheSession(request);
  if (!cacheSession) {
    return null;
  }

  return new Promise(resolve => {
    cacheSession.asyncOpenURI(
      request.URI,
      "",
      Ci.nsICacheStorage.OPEN_SECRETLY,
      {
        onCacheEntryCheck: entry => {
          return Ci.nsICacheEntryOpenCallback.ENTRY_WANTED;
        },
        onCacheEntryAvailable: (cacheEntry, isnew, status) => {
          if (cacheEntry) {
            const cacheObject = buildResponseCacheObject(cacheEntry);
            resolve(cacheObject);
          } else {
            resolve(null);
          }
        },
      }
    );
  });
}
