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

var pktApi = (function() {

    /**
     * Configuration
     */

    // Base url for all api calls
    // TODO: This is a dev server and will be changed before launch
    var pocketAPIhost = Services.prefs.getCharPref("browser.pocket.hostname");

    // Base url for all api calls
    var baseAPIUrl = "https://" + pocketAPIhost + "/v3";


    /**
     * Auth keys for the API requests
     */
    var oAuthConsumerKey = Services.prefs.getCharPref("browser.pocket.oAuthConsumerKey");

	/**
	 *
	 */
	var prefBranch = Services.prefs.getBranch("browser.pocket.settings.");

    /**
     * Helper
     */

    var extend = function(out) {
        out = out || {};

        for (var i = 1; i < arguments.length; i++) {
            if (!arguments[i])
                continue;

            for (var key in arguments[i]) {
                if (arguments[i].hasOwnProperty(key))
                    out[key] = arguments[i][key];
                }
            }
        return out;
    }

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
     	
		if (!prefBranch.prefHasUserValue(key))
			return;
		
		return prefBranch.getComplexValue(key, Components.interfaces.nsISupportsString).data;
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
     	
     	if (!value)
     		prefBranch.clearUserPref(key);
     	else
     	{
     		// We use complexValue as tags can have utf-8 characters in them
     		var str = Components.classes["@mozilla.org/supports-string;1"].createInstance(Components.interfaces.nsISupportsString);
			str.data = value;
     		prefBranch.setComplexValue(key, Components.interfaces.nsISupportsString, str);
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
    
        var cookieManager = Cc["@mozilla.org/cookiemanager;1"].getService(Ci.nsICookieManager2);
        var pocketCookies = cookieManager.getCookiesFromHost(pocketAPIhost);
        var cookies = {};
        while (pocketCookies.hasMoreElements()) {
            var cookie = pocketCookies.getNext().QueryInterface(Ci.nsICookie2);
            cookies[cookie.name] = cookie.value;
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
        if (typeof pocketCookies['ftv1'] === "undefined") {
            return undefined;
        }

        // Check if a new user logged in in the meantime and clearUserData if so
        var sessionId = pocketCookies['fsv1'];
        var lastSessionId = getSetting('fsv1');
        if (sessionId !== lastSessionId) {
            clearUserData();
            setSetting("fsv1", sessionId);
        }

        // Return access token
        return pocketCookies['ftv1'];
    }

    /**
     * Returns users logged in status
     * @return {Boolean} Users logged in status
     */
    function isUserLoggedIn() {
        return (typeof getAccessToken() !== "undefined");
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
    function apiRequest(options) {
        if ((typeof options === "undefined") || (typeof options.path === "undefined")) {
            return false;
        }

        var url = baseAPIUrl + options.path;
        var data = options.data || {};
        data.locale_lang = window.navigator.language;
        data.consumer_key = oAuthConsumerKey;

        var request = Components.classes["@mozilla.org/xmlextras/xmlhttprequest;1"].createInstance(Components.interfaces.nsIXMLHttpRequest);
        request.open("POST", url, true);
		request.onreadystatechange = function(e){
			if (request.readyState == 4) {
				if (request.status === 200) {
                    if (options.success) {
                        options.success(JSON.parse(request.response), request);
                    }
                    return;
                }

                // TODO: Better error handling
                if (options.error) {
                    // Check to handle Pocket error
                    var errorMessage = request.getResponseHeader("X-Error");
                    if (typeof errorMessage !== "undefined") {
                        options.error(new Error(errorMessage), request);
                        return;
                    }

                    // Handle other error
                    options.error(new Error(request.statusText), request);
                }
			}
		};

		// TODO - do we want to pass a special user agent?
		//request.setRequestHeader("User-Agent" , 'Pocket Firefox ' + this.APP.v);

		// Set headers
		request.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded; charset=UTF-8');
		request.setRequestHeader('X-Accept',' application/json');

        // Serialize and Fire off the request
		var str = [];
		for(var p in data) {
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
        setSetting("usedTags", undefined);

    }

    /**
     * Add a new link to Pocket
     * @param {string} url     URL of the link
     * @param {Object | undefined} options Can provide an title and have a
     *                                     success and error callbacks
     * @return {Boolean} Returns Boolean whether the api call started sucessfully
     */
    function addLink(url, options) {

        var since = getSetting('latestSince');
        var accessToken = getAccessToken();

        var sendData = {
            access_token: accessToken,
            url: url,
            since: since ? since : 0
        };

        var title = options.title;
        if (title !== "undefined") {
            sendData.title = title;
        };

        return apiRequest({
            path: "/firefox/save",
            data: sendData,
            success: function(data) {

                // Update premium status, tags and since
                var tags = data.tags;
                if ((typeof tags !== "undefined") && Array.isArray(tags)) {
                    // If a tagslist is in the response replace the tags
                    setSetting('tags', JSON.stringify(data.tags));
                }

                // Update premium status
                var premiumStatus = data.premium_status;
                if (typeof premiumStatus !== "undefined") {
                    // If a premium_status is in the response replace the premium_status
                    setSetting("premium_status", premiumStatus);
                }

                // Save since value for further requests
                setSetting('latestSince', data.since);

                if (options.success) {
                    options.success.apply(options, Array.apply(null, arguments));
                }
            },
            error: options.error
        });

        return sendAction(action, options);
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
            item_id: itemId
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
        if (typeof options.actionInfo !== 'undefined') {
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
                actions: JSON.stringify(actions)
            },
            success: options.success,
            error: options.error
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
        return addTags({item_id: itemId}, tags, options);
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
        return addTags({url: url}, tags, options);
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
        // Tags add action
        var action = {
            action: "tags_add",
            tags: tags
        };
        action = extend(action, actionPart);

        // Backup the success callback as we need it later
        var finalSuccessCallback = options.success;

        // Switch the success callback
        options.success = function(data) {

            // Update used tags
            var usedTagsJSON = getSetting("usedTags");
            var usedTags = usedTagsJSON ? JSON.parse(usedTagsJSON) : {};

            // Check for each tag if it's already in the used tags
            for (var i = 0; i < tags.length; i++) {
                var tagToSave = tags[i].trim();
                var newUsedTagObject = {
                    "tag": tagToSave,
                    "timestamp": new Date()
                };
                usedTags[tagToSave] = newUsedTagObject;
            }
            setSetting("usedTags", JSON.stringify(usedTags));

            // Let the callback know that we are finished
            if (finalSuccessCallback) {
                finalSuccessCallback(data);
            }
        };

        // Execute the action
        return sendAction(action, options);
    }

    /**
     * Get all cached tags and used tags within the callback
     * @param {function(Array, Array, Boolean)} callback
     *                           Function with tags and used tags as parameter.
     */
    function getTags(callback) {

        var tagsFromSettings = function() {
            var tagsJSON = getSetting("tags");
            if (typeof tagsJSON !== "undefined") {
                return JSON.parse(tagsJSON)
            }
            return [];
        }

        var sortedUsedTagsFromSettings = function() {
            // Get and Sort used tags
            var usedTags = [];

            var usedTagsJSON = getSetting("usedTags");
            if (typeof usedTagsJSON !== "undefined") {
                var usedTagsObject = JSON.parse(usedTagsJSON);
                var usedTagsObjectArray = [];
                for (var tagKey in usedTagsObject) {
                    usedTagsObjectArray.push(usedTagsObject[tagKey]);
                }

                // Sort usedTagsObjectArray based on timestamp
                usedTagsObjectArray.sort(function(a, b) {
                    a = new Date(a.timestamp);
                    b = new Date(b.timestamp);
                    return a < b ? -1 : a > b ? 1 : 0;
                });

                // Get all keys tags
                for (var j = 0; j < usedTagsObjectArray.length; j++) {
                    usedTags.push(usedTagsObjectArray[j].tag);
                }

                // Reverse to set the last recent used tags to the front
                usedTags.reverse();
            }

            return usedTags;
        }

        if (callback) {
            var tags = tagsFromSettings();
            var usedTags = sortedUsedTagsFromSettings();
            callback(tags, usedTags);
        }
    }

    /**
     * Helper method to check if a user is premium or not
     */
    function isPremiumUser() {
        return getSetting('premium_status') == 1;
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
        return getSuggestedTags({item_id: itemId}, options);
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
        return getSuggestedTags({url: url}, options);
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
            data: data,
            success: options.success,
            error: options.error
        });
    }

    /**
     * Public functions
     */
    return {
        isUserLoggedIn : isUserLoggedIn,
        clearUserData: clearUserData,
        addLink: addLink,
        deleteItem: deleteItem,
        addTagsToItem: addTagsToItem,
        addTagsToURL: addTagsToURL,
        getTags: getTags,
        isPremiumUser: isPremiumUser,
        getSuggestedTagsForItem: getSuggestedTagsForItem,
        getSuggestedTagsForURL: getSuggestedTagsForURL
    };
}());