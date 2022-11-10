/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  NetworkHelper:
    "resource://devtools/shared/network-observer/NetworkHelper.sys.mjs",
});

/**
 * Flag for cache session being initialized.
 */
let isCacheSessionInitialized = false;

/**
 * Cache session object.
 */
let cacheSession = null;

/**
 * Initializes our cache session / cache storage session.
 */
function initializeCacheSession(request) {
  try {
    const cacheService = Services.cache2;
    if (cacheService) {
      let loadContext = lazy.NetworkHelper.getRequestLoadContext(request);
      if (!loadContext) {
        // Get default load context if we can't fetch.
        loadContext = Services.loadContextInfo.default;
      }
      cacheSession = cacheService.diskCacheStorage(loadContext);
      isCacheSessionInitialized = true;
    }
  } catch (e) {
    isCacheSessionInitialized = false;
  }
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
      cacheObject.dataSize = cacheEntry.storageDataSize;
    }
  } catch (e) {
    // We just need to handle this in case it's a js file of 0B.
  }
  if (cacheEntry.expirationTime) {
    cacheObject.expires = cacheEntry.expirationTime;
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
    cacheObject.device = cacheEntry.deviceID;
  }
  return cacheObject;
}

/**
 * Does the fetch for the cache entry from the session.
 *
 * @param string request
 *        The request object.
 * @param Function onCacheEntryAvailable
 *        callback function.
 */
export function getCacheEntry(request, onCacheEntryAvailable) {
  if (!isCacheSessionInitialized) {
    initializeCacheSession(request);
  }

  if (cacheSession) {
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
            onCacheEntryAvailable(cacheObject);
          } else {
            onCacheEntryAvailable(null);
          }
        },
      }
    );
  } else {
    onCacheEntryAvailable(null);
  }
}
