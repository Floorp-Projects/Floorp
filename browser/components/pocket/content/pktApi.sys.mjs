/*
 * LICENSE
 *
 * POCKET MARKS
 *
 * Notwithstanding the permitted uses of the Software (as defined below) pursuant to the license set forth below, "Pocket," "Read It Later" and the Pocket icon and logos (collectively, the “Pocket Marks”) are registered and common law trademarks of Read It Later, Inc. This means that, while you have considerable freedom to redistribute and modify the Software, there are tight restrictions on your ability to use the Pocket Marks. This license does not grant you any rights to use the Pocket Marks except as they are embodied in the Software.
 *
 * ---
 *
 * SOFTWARE
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
 * Pocket API module
 *
 * Public API Documentation: http://getpocket.com/developer/
 *
 *
 * Definition of keys stored in preferences to preserve user state:
 *      premium_status:   Current premium status for logged in user if available
 *                        Can be 0 for no premium and 1 for premium
 *      latestSince:      Last timestamp a save happened
 *      tags:             All tags for logged in user
 *      usedTags:         All used tags from within the extension sorted by recency
 */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  IndexedDB: "resource://gre/modules/IndexedDB.sys.mjs",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "gCookieFirstPartyIsolate",
  "privacy.firstparty.isolate",
  false
);

const DB_NAME = "SaveToPocket";
const STORE_NAME = "pktAPI";
const DB_VERSION = 1;
const RECENT_SAVES_UPDATE_TIME = 5 * 60 * 1000; // 30 minutes

/**
 * Create a new connection to the database.
 */
function openDatabase() {
  return lazy.IndexedDB.open(DB_NAME, DB_VERSION, db => {
    db.createObjectStore(STORE_NAME);
  });
}

/**
 * Cache the database connection so that it is shared among multiple operations.
 */
let databasePromise;
function getDatabase() {
  if (!databasePromise) {
    databasePromise = openDatabase();
  }
  return databasePromise;
}

export var pktApi = (function () {
  /**
   * Configuration
   */

  // Base url for all api calls
  var pocketSiteHost = Services.prefs.getCharPref("extensions.pocket.site"); // getpocket.com

  /**
   *
   */
  var prefBranch = Services.prefs.getBranch("extensions.pocket.settings.");

  /**
   * Helper
   */

  var extend = function (out) {
    out = out || {};

    for (var i = 1; i < arguments.length; i++) {
      if (!arguments[i]) {
        continue;
      }

      for (var key in arguments[i]) {
        if (arguments[i].hasOwnProperty(key)) {
          out[key] = arguments[i][key];
        }
      }
    }
    return out;
  };

  var parseJSON = function (jsonString) {
    try {
      var o = JSON.parse(jsonString);

      // Handle non-exception-throwing cases:
      // Neither JSON.parse(false) or JSON.parse(1234) throw errors, hence the type-checking,
      // but... JSON.parse(null) returns 'null', and typeof null === "object",
      // so we must check for that, too.
      if (o && typeof o === "object" && o !== null) {
        return o;
      }
    } catch (e) {}

    return undefined;
  };

  /**
   * Settings
   */

  /**
   * Wrapper for different plattforms to get settings for a given key
   * @param  {string} key A string containing the name of the key you want to
   *                  retrieve the value of
   * @return {string} String containing the value of the key. If the key
   *                  does not exist, null is returned
   */
  function getSetting(key) {
    // TODO : Move this to sqlite or a local file so it's not editable (and is safer)
    // https://developer.mozilla.org/en-US/Add-ons/Overlay_Extensions/XUL_School/Local_Storage

    if (!prefBranch.prefHasUserValue(key)) {
      return undefined;
    }

    return prefBranch.getStringPref(key);
  }

  /**
   * Wrapper for different plattforms to set a value for a given key in settings
   * @param {string} key     A string containing the name of the key you want
   *                         to create/update.
   * @param {string} value   String containing the value you want to give
   *                         the key you are creating/updating.
   */
  function setSetting(key, value) {
    // TODO : Move this to sqlite or a local file so it's not editable (and is safer)
    // https://developer.mozilla.org/en-US/Add-ons/Overlay_Extensions/XUL_School/Local_Storage

    if (!value) {
      prefBranch.clearUserPref(key);
    } else {
      // We use complexValue as tags can have utf-8 characters in them
      prefBranch.setStringPref(key, value);
    }
  }

  /**
   * Auth
   */

  /*
   *  All cookies from the Pocket domain
   *  The return format: { cookieName:cookieValue, cookieName:cookieValue, ... }
   */
  function getCookiesFromPocket() {
    var cookies = {};
    let oa = {};
    if (lazy.gCookieFirstPartyIsolate) {
      oa.firstPartyDomain = pocketSiteHost;
    }
    oa.privateBrowsingId = lazy.PrivateBrowsingUtils.permanentPrivateBrowsing
      ? 1
      : 0;
    for (let cookie of Services.cookies.getCookiesFromHost(
      pocketSiteHost,
      oa
    )) {
      if (cookie.host === pocketSiteHost) {
        cookies[cookie.name] = cookie.value;
      }
    }
    return cookies;
  }

  /**
   * Returns access token or undefined if no logged in user was found
   * @return {string | undefined} Access token for logged in user user
   */
  function getAccessToken() {
    var pocketCookies = getCookiesFromPocket();

    // If no cookie was found just return undefined
    if (typeof pocketCookies.ftv1 === "undefined") {
      return undefined;
    }

    // Check if a new user logged in in the meantime and clearUserData if so
    var sessionId = pocketCookies.fsv1;
    var lastSessionId = getSetting("fsv1");
    if (sessionId !== lastSessionId) {
      clearUserData();
      setSetting("fsv1", sessionId);
    }

    // Return access token
    return pocketCookies.ftv1;
  }

  /**
   * Get the current premium status of the user
   * @return {number | undefined} Premium status of user
   */
  function getPremiumStatus() {
    var premiumStatus = getSetting("premium_status");
    if (typeof premiumStatus === "undefined") {
      // Premium status is not in settings try get it from cookie
      var pocketCookies = getCookiesFromPocket();
      premiumStatus = pocketCookies.ps;
    }
    return premiumStatus;
  }

  /**
   * Helper method to check if a user is premium or not
   * @return {Boolean} Boolean if user is premium or not
   */
  function isPremiumUser() {
    return getPremiumStatus() == 1;
  }

  /**
   * Returns users logged in status
   * @return {Boolean} Users logged in status
   */
  function isUserLoggedIn() {
    return typeof getAccessToken() !== "undefined";
  }

  /**
   * API
   */

  /**
   * Helper function for executing api requests. It mainly configures the
   * ajax call with default values like type, headers or dataType for an api call.
   * This function is for internal usage only.
   * @param  {Object} options
   *     Possible keys:
   *      - {string} path: This should be the Pocket API
   *                       endpoint to call. For example providing the path
   *                       "/get" would result in a call to getpocket.com/v3/get
   *      - {Object|undefined} data: Gets passed on to the jQuery ajax
   *                                 call as data parameter
   *      - {function(Object data, XMLHttpRequest xhr) | undefined} success:
   *                        A function to be called if the request succeeds.
   *      - {function(Error errorThrown,  XMLHttpRequest xhr) | undefined} error:
   *                       A function to be called if the request fails.
   * @return {Boolean} Returns Boolean whether the api call started sucessfully
   *
   */
  function apiRequest(options, useBFF = false) {
    let baseAPIUrl;
    let oAuthConsumerKey;

    if (!useBFF) {
      baseAPIUrl = `https://${Services.prefs.getCharPref(
        "extensions.pocket.api"
      )}/v3`;

      oAuthConsumerKey = Services.prefs.getCharPref(
        "extensions.pocket.oAuthConsumerKey"
      );
    } else {
      baseAPIUrl = `https://${lazy.NimbusFeatures.saveToPocket.getVariable(
        "bffApi"
      )}/desktop/v1`;

      oAuthConsumerKey = lazy.NimbusFeatures.saveToPocket.getVariable(
        "oAuthConsumerKeyBff"
      );
    }

    if (typeof options === "undefined" || typeof options.path === "undefined") {
      return false;
    }

    var url = baseAPIUrl + options.path;
    var data = options.data || {};
    data.locale_lang = Services.locale.appLocaleAsBCP47;
    data.consumer_key = oAuthConsumerKey;

    var request = new XMLHttpRequest();

    if (!useBFF) {
      request.open("POST", url, true);
    } else {
      request.open("GET", url, true);
    }

    request.onreadystatechange = function (e) {
      if (request.readyState == 4) {
        // "done" is a completed XHR regardless of success/error:
        if (options.done) {
          options.done();
        }

        if (request.status === 200) {
          // There could still be an error if the response is no valid json
          // or does not have status = 1
          var response = parseJSON(request.response);

          // BFF doesn't return an appended `status` code in the returned data
          if (options.success && response && (response.status == 1 || useBFF)) {
            options.success(response, request);
            return;
          }
        }

        // Handle error case
        if (options.error) {
          // In case the user did revoke the access token or it's not
          // valid anymore clear the user data
          if (request.status === 401) {
            clearUserData();
          }

          // Handle error message
          var errorMessage;
          if (request.status !== 200) {
            errorMessage =
              request.getResponseHeader("X-Error") || request.statusText;
            errorMessage = JSON.parse('"' + errorMessage + '"');
          }
          var error = { message: errorMessage };
          options.error(error, request);
        }
      }
    };

    // Set headers
    request.setRequestHeader(
      "Content-Type",
      "application/x-www-form-urlencoded; charset=UTF-8"
    );
    request.setRequestHeader("X-Accept", " application/json");

    if (useBFF) {
      let cookies = getCookiesFromPocket();
      let serializedCookies = ``;

      for (const key in cookies) {
        serializedCookies += `${key}=${cookies[key]}; `;
      }

      serializedCookies = serializedCookies.substring(
        0,
        serializedCookies.length - 2
      );

      request.setRequestHeader("Cookie", serializedCookies);
      request.setRequestHeader("consumer_key", oAuthConsumerKey);
    }

    // Serialize and Fire off the request
    var str = [];
    for (var p in data) {
      if (data.hasOwnProperty(p)) {
        str.push(encodeURIComponent(p) + "=" + encodeURIComponent(data[p]));
      }
    }

    request.send(str.join("&"));

    return true;
  }

  /**
   * Cleans all settings for the previously logged in user
   */
  function clearUserData() {
    // Clear stored information
    setSetting("premium_status", undefined);
    setSetting("latestSince", undefined);
    setSetting("tags", undefined);
    // An old pref that is no longer used,
    // but the user data may still exist on some profiles.
    // So best to clean it up just in case.
    // Can probably remove this line in the future.
    setSetting("usedTags", undefined);

    setSetting("fsv1", undefined);

    _clearRecentSavesCache();
  }

  /**
   * Add a new link to Pocket
   * @param {string} url     URL of the link
   * @param {Object | undefined} options Can provide a string-based title, a
   *                                     `success` callback and an `error` callback.
   * @return {Boolean} Returns Boolean whether the api call started sucessfully
   */
  function addLink(url, options) {
    var since = getSetting("latestSince");
    var accessToken = getAccessToken();

    var sendData = {
      access_token: accessToken,
      url,
      since: since ? since : 0,
    };

    if (options.title) {
      sendData.title = options.title;
    }

    return apiRequest({
      path: "/firefox/save",
      data: sendData,
      success(data) {
        // Update premium status, tags and since
        var tags = data.tags;
        if (typeof tags !== "undefined" && Array.isArray(tags)) {
          // If a tagslist is in the response replace the tags
          setSetting("tags", JSON.stringify(data.tags));
        }

        // Update premium status
        var premiumStatus = data.premium_status;
        if (typeof premiumStatus !== "undefined") {
          // If a premium_status is in the response replace the premium_status
          setSetting("premium_status", premiumStatus);
        }

        // Save since value for further requests
        setSetting("latestSince", data.since);

        // Define variant for ho2
        if (data.flags) {
          var showHo2 =
            Services.locale.appLocaleAsBCP47 === "en-US"
              ? data.flags.show_ffx_mobile_prompt
              : "control";
          setSetting("test.ho2", showHo2);
        }
        data.ho2 = getSetting("test.ho2");

        _expireRecentSavesCache();
        if (options.success) {
          options.success.apply(options, Array.apply(null, arguments));
        }
      },
      error: options.error,
    });
  }

  /**
   * Get a preview for saved URL
   * @param {string} url     URL of the link
   * @param {Object | undefined} options Can provide a `success` callback and an `error` callback.
   * @return {Boolean} Returns Boolean whether the api call started sucessfully
   */
  function getArticleInfo(url, options) {
    return apiRequest({
      path: "/getItemPreview",
      data: {
        access_token: getAccessToken(),
        url,
      },
      success(data) {
        if (options.success) {
          options.success.apply(options, Array.apply(null, arguments));
        }
      },
      error: options.error,
      done: options.done,
    });
  }

  /**
   * Request a email for mobile apps
   * @param {Object | undefined} options Can provide a `success` callback and an `error` callback.
   * @return {Boolean} Returns Boolean whether the api call started sucessfully
   */
  function getMobileDownload(options) {
    return apiRequest({
      path: "/firefox/get-app",
      data: {
        access_token: getAccessToken(),
      },
      success(data) {
        if (options.success) {
          options.success.apply(options, Array.apply(null, arguments));
        }
      },
      error: options.error,
    });
  }

  /**
   * Delete an item identified by item id from the users list
   * @param  {string} itemId  The id from the item we want to remove
   * @param  {Object | undefined} options Can provide an actionInfo object with
   *                                      further data to send to the API. Can
   *                                      have success and error callbacks
   * @return {Boolean} Returns Boolean whether the api call started sucessfully
   */
  function deleteItem(itemId, options) {
    var action = {
      action: "delete",
      item_id: itemId,
    };
    return sendAction(action, options);
  }

  /**
   * Archive an item identified by item id from the users list
   * @param  {string} itemId  The id from the item we want to archive
   * @param  {Object | undefined} options Can provide an actionInfo object with
   *                                      further data to send to the API. Can
   *                                      have success and error callbacks
   * @return {Boolean} Returns Boolean whether the api call started sucessfully
   */
  function archiveItem(itemId, options) {
    var action = {
      action: "archive",
      item_id: itemId,
    };
    return sendAction(action, options);
  }

  /**
   * General function to send all kinds of actions like adding of links or
   * removing of items via the API
   * @param  {Object}  action  Action object
   * @param  {Object | undefined}  options Can provide an actionInfo object
   *                                       with further data to send to the
   *                                       API. Can have success and error
   *                                       callbacks
   * @return {Boolean} Returns Boolean whether the api call started sucessfully
   */
  function sendAction(action, options) {
    // Options can have an 'actionInfo' object. This actionInfo object gets
    // passed through to the action object that will be send to the API endpoint
    if (typeof options.actionInfo !== "undefined") {
      action = extend(action, options.actionInfo);
    }
    return sendActions([action], options);
  }

  /**
   * General function to send all kinds of actions like adding of links or
   * removing of items via the API
   * @param  {Array} actions Array of action objects
   * @param  {Object | undefined} options Can have success and error callbacks
   * @return {Boolean} Returns Boolean whether the api call started sucessfully
   */
  function sendActions(actions, options) {
    return apiRequest({
      path: "/send",
      data: {
        access_token: getAccessToken(),
        actions: JSON.stringify(actions),
      },
      success: options.success,
      error: options.error,
    });
  }

  /**
   * Handling Tags
   */

  /**
   * Add tags to the item identified by the url. Also updates the used tags
   * list
   * @param {string} itemId  The item identifier by item id
   * @param {Array}  tags    Tags adding to the item
   * @param {Object | undefined} options Can provide an actionInfo object with
   *                                     further data to send to the API. Can
   *                                     have success and error callbacks
   * @return {Boolean} Returns Boolean whether the api call started sucessfully
   */
  function addTagsToItem(itemId, tags, options) {
    return addTags({ item_id: itemId }, tags, options);
  }

  /**
   * Add tags to the item identified by the url. Also updates the used tags
   * list
   * @param {string} url     The item identifier by url
   * @param {Array}  tags    Tags adding to the item
   * @param {Object} options Can provide an actionInfo object with further
   *                         data to send to the API. Can have success and error
   *                         callbacks
   * @return {Boolean} Returns Boolean whether the api call started sucessfully
   */
  function addTagsToURL(url, tags, options) {
    return addTags({ url }, tags, options);
  }

  /**
   * Helper function to execute the add tags api call. Will be used from addTagsToURL
   * and addTagsToItem but not exposed outside
   * @param {string} actionPart Specific action part to add to action
   * @param {Array}  tags       Tags adding to the item
   * @param {Object | undefined} options Can provide an actionInfo object with
   *                                     further data to send to the API. Can
   *                                     have success and error callbacks
   * @return {Boolean} Returns Boolean whether the api call started sucessfully
   */
  function addTags(actionPart, tags, options) {
    if (tags.length) {
      addRecentTags(tags);
    }

    // Tags add action
    var action = {
      action: "tags_add",
      tags,
    };
    action = extend(action, actionPart);

    // Execute the action
    return sendAction(action, options);
  }

  /**
   * Return all cached tags and used tags.
   */
  function getTags() {
    var tagsFromSettings = function () {
      var tagsJSON = getSetting("tags");
      if (typeof tagsJSON !== "undefined") {
        return JSON.parse(tagsJSON);
      }
      return [];
    };

    return {
      tags: tagsFromSettings(),
    };
  }

  /**
   * Return all recent tags.
   */
  function getRecentTags() {
    var tagsFromSettings = function () {
      var tagsJSON = getSetting("recentTags");

      if (typeof tagsJSON !== "undefined") {
        let parsedTags;

        try {
          parsedTags = JSON.parse(tagsJSON);
        } catch {
          parsedTags = [];
        }

        return parsedTags;
      }

      return [];
    };

    return {
      recentTags: tagsFromSettings(),
    };
  }

  /**
   * Store recently used tags.
   * @param {Array}  tags   Newly used tags to store
   */
  function addRecentTags(tags) {
    var newRecentTags = tags || [];
    var cachedRecentTags = getRecentTags()?.recentTags;
    var mergedRecentTags = [];

    cachedRecentTags.forEach(tag => {
      if (!newRecentTags.includes(tag)) {
        mergedRecentTags.push(tag);
      }
    });

    mergedRecentTags = [...newRecentTags, ...mergedRecentTags];

    // update recent tags pref to store
    setSetting("recentTags", JSON.stringify(mergedRecentTags));
  }

  /**
   * Fetch suggested tags for a given item id
   * @param  {string} itemId Item id of
   * @param  {Object | undefined} options Can provide an actionInfo object
   *                                      with further data to send to the API.
   *                                      Can have success and error callbacks
   * @return {Boolean} Returns Boolean whether the api call started sucessfully
   */
  function getSuggestedTagsForItem(itemId, options) {
    return getSuggestedTags({ item_id: itemId }, options);
  }

  /**
   * Fetch suggested tags for a given URL
   * @param {string} url (required) The item identifier by url
   * @param {Object} options Can provide an actionInfo object with further
   *                         data to send to the API. Can have success and error
   *                         callbacks
   * @return {Boolean} Returns Boolean whether the api call started sucessfully
   */
  function getSuggestedTagsForURL(url, options) {
    return getSuggestedTags({ url }, options);
  }

  /**
   * Helper function to get suggested tags
   * @return {Boolean} Returns Boolean whether the api call started sucessfully
   */
  function getSuggestedTags(data, options) {
    data = data || {};
    options = options || {};

    data.access_token = getAccessToken();

    return apiRequest({
      path: "/getSuggestedTags",
      data,
      success: options.success,
      error: options.error,
    });
  }

  /**
   * Helper function to get a user's pocket stories
   * @return {Boolean} Returns Boolean whether the api call started sucessfully
   */
  function retrieve(data = {}, options = {}) {
    const requestData = Object.assign({}, data, {
      access_token: getAccessToken(),
    });

    const useBFF =
      lazy.NimbusFeatures.saveToPocket.getVariable("bffRecentSaves");

    return apiRequest(
      {
        path: useBFF ? `/recent-saves?count=${data.count}` : `/firefox/get`,
        data: requestData,
        success: options.success,
        error: options.error,
      },
      useBFF
    ); // Use BFF
  }

  async function _getRecentSavesCache() {
    const db = await getDatabase();
    return db.objectStore(STORE_NAME, "readonly").get("recentSaves");
  }
  async function _setRecentSavesCache(data) {
    const db = await getDatabase();
    db.objectStore(STORE_NAME, "readwrite").put(data, "recentSaves");
  }
  // Clears the cache time, so the next get forces an update.
  async function _expireRecentSavesCache() {
    const cache = await _getRecentSavesCache();
    _setRecentSavesCache({
      ...cache,
      lastUpdated: 0,
    });
  }
  // Clears the cache, for when a new user logs in.
  async function _clearRecentSavesCache() {
    const db = await getDatabase();
    db.objectStore(STORE_NAME, "readwrite").delete("recentSaves");
  }

  async function getRecentSavesCache() {
    // Get cache
    const cache = await _getRecentSavesCache();
    // Check age
    if (
      cache?.lastUpdated &&
      Date.now() - cache.lastUpdated < RECENT_SAVES_UPDATE_TIME
    ) {
      // Return cache if it's not too old.
      return cache.list;
    }
    return null;
  }

  async function getRecentSaves(options = {}) {
    pktApi.retrieve(
      { count: 4 },
      {
        success(data) {
          const useBFF =
            lazy.NimbusFeatures.saveToPocket.getVariable("bffRecentSaves");

          // Don't try to parse bad or missing data
          if (
            useBFF &&
            (typeof data !== `object` || typeof data?.data !== `object`)
          ) {
            return;
          }

          try {
            let list = useBFF ? [] : data.list;

            if (useBFF) {
              // Transform BFF list item schema to existing api schema
              data.data.forEach((item, index) => {
                list[index] = {
                  item_id: item.id,
                  id: item.id, // This can probably be deprecated when the old API is
                  resolved_url: item.resolvedUrl,
                  given_url: item.givenUrl,
                  resolved_title: item.title,
                  excerpt: item.excerpt,
                  word_count: item.wordCount,
                  time_to_read: item.timeToRead,
                  top_image_url: item.topImageUrl,
                };
              });
            } else {
              // We want these to show up in the same order as they saved,
              // so we need to do some work and sort.
              list = Object.values(list)
                .map(item => ({
                  ...item,
                  id: parseInt(item.item_id || item.resolved_id, 10),
                  time_added: parseInt(item.time_added),
                }))
                .sort((a, b) => b.time_added - a.time_added);
            }

            // Cache results
            const results = {
              lastUpdated: Date.now(),
              list,
            };

            _setRecentSavesCache(results);
            options.success?.(results.list);
          } catch {
            // If parsing fails, just leave existing recent saves cache intact
          }
        },
        error(error) {
          options.error?.(error);
        },
      }
    );
  }

  /**
   * Public functions
   */
  return {
    isUserLoggedIn,
    clearUserData,
    addLink,
    deleteItem,
    archiveItem,
    addTagsToItem,
    addTagsToURL,
    getTags,
    getRecentTags,
    isPremiumUser,
    getSuggestedTagsForItem,
    getSuggestedTagsForURL,
    retrieve,
    getRecentSavesCache,
    getRecentSaves,
    getArticleInfo,
    getMobileDownload,
  };
})();
