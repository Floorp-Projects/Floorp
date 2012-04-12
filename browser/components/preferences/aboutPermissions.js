/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is about:permissions code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Margaret Leibovic <margaret.leibovic@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

let Ci = Components.interfaces;
let Cc = Components.classes;
let Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/PluralForm.jsm");
Cu.import("resource://gre/modules/DownloadUtils.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");

let gFaviconService = Cc["@mozilla.org/browser/favicon-service;1"].
                      getService(Ci.nsIFaviconService);

let gPlacesDatabase = Cc["@mozilla.org/browser/nav-history-service;1"].
                      getService(Ci.nsPIPlacesDatabase).
                      DBConnection.
                      clone(true);

let gSitesStmt = gPlacesDatabase.createAsyncStatement(
                  "SELECT get_unreversed_host(rev_host) AS host " +
                  "FROM moz_places " +
                  "WHERE rev_host > '.' " +
                  "AND visit_count > 0 " +
                  "GROUP BY rev_host " +
                  "ORDER BY MAX(frecency) DESC " +
                  "LIMIT :limit");

let gVisitStmt = gPlacesDatabase.createAsyncStatement(
                  "SELECT SUM(visit_count) AS count " +
                  "FROM moz_places " +
                  "WHERE rev_host = :rev_host");

/**
 * Permission types that should be tested with testExactPermission, as opposed
 * to testPermission. This is based on what consumers use to test these permissions.
 */
let TEST_EXACT_PERM_TYPES = ["geo"];

/**
 * Site object represents a single site, uniquely identified by a host.
 */
function Site(host) {
  this.host = host;
  this.listitem = null;

  this.httpURI = NetUtil.newURI("http://" + this.host);
  this.httpsURI = NetUtil.newURI("https://" + this.host);
}

Site.prototype = {
  /**
   * Gets the favicon to use for the site. The callback only gets called if
   * a favicon is found for either the http URI or the https URI.
   *
   * @param aCallback
   *        A callback function that takes a favicon image URL as a parameter.
   */
  getFavicon: function Site_getFavicon(aCallback) {
    function invokeCallback(aFaviconURI) {
      try {
        // Use getFaviconLinkForIcon to get image data from the database instead
        // of using the favicon URI to fetch image data over the network.
        aCallback(gFaviconService.getFaviconLinkForIcon(aFaviconURI).spec);
      } catch (e) {
        Cu.reportError("AboutPermissions: " + e);
      }
    }

    // Try to find favicon for both URIs, but always prefer the https favicon.
    gFaviconService.getFaviconURLForPage(this.httpsURI, function (aURI) {
      if (aURI) {
        invokeCallback(aURI);
      } else {
        gFaviconService.getFaviconURLForPage(this.httpURI, function (aURI) {
          if (aURI) {
            invokeCallback(aURI);
          }
        });
      }
    }.bind(this));
  },

  /**
   * Gets the number of history visits for the site.
   *
   * @param aCallback
   *        A function that takes the visit count (a number) as a parameter.
   */
  getVisitCount: function Site_getVisitCount(aCallback) {
    let rev_host = this.host.split("").reverse().join("") + ".";
    gVisitStmt.params.rev_host = rev_host;
    gVisitStmt.executeAsync({
      handleResult: function(aResults) {
        let row = aResults.getNextRow();
        let count = row.getResultByName("count") || 0;
        try {
          aCallback(count);
        } catch (e) {
          Cu.reportError("AboutPermissions: " + e);
        }
      },
      handleError: function(aError) {
        Cu.reportError("AboutPermissions: " + aError);
      },
      handleCompletion: function(aReason) {
      }
    });
  },

  /**
   * Gets the permission value stored for a specified permission type.
   *
   * @param aType
   *        The permission type string stored in permission manager.
   *        e.g. "cookie", "geo", "indexedDB", "popup", "image"
   * @param aResultObj
   *        An object that stores the permission value set for aType.
   *
   * @return A boolean indicating whether or not a permission is set.
   */
  getPermission: function Site_getPermission(aType, aResultObj) {
    // Password saving isn't a nsIPermissionManager permission type, so handle
    // it seperately.
    if (aType == "password") {
      aResultObj.value =  this.loginSavingEnabled ?
                          Ci.nsIPermissionManager.ALLOW_ACTION :
                          Ci.nsIPermissionManager.DENY_ACTION;
      return true;
    }

    let permissionValue;
    if (TEST_EXACT_PERM_TYPES.indexOf(aType) == -1) {
      permissionValue = Services.perms.testPermission(this.httpURI, aType);
    } else {
      permissionValue = Services.perms.testExactPermission(this.httpURI, aType);
    }
    aResultObj.value = permissionValue;

    return permissionValue != Ci.nsIPermissionManager.UNKNOWN_ACTION;
  },

  /**
   * Sets a permission for the site given a permission type and value.
   *
   * @param aType
   *        The permission type string stored in permission manager.
   *        e.g. "cookie", "geo", "indexedDB", "popup", "image"
   * @param aPerm
   *        The permission value to set for the permission type. This should
   *        be one of the constants defined in nsIPermissionManager.
   */
  setPermission: function Site_setPermission(aType, aPerm) {
    // Password saving isn't a nsIPermissionManager permission type, so handle
    // it seperately.
    if (aType == "password") {
      this.loginSavingEnabled = aPerm == Ci.nsIPermissionManager.ALLOW_ACTION;
      return;
    }

    // Using httpURI is kind of bogus, but the permission manager stores the
    // permission for the host, so the right thing happens in the end.
    Services.perms.add(this.httpURI, aType, aPerm);
  },

  /**
   * Clears a user-set permission value for the site given a permission type.
   *
   * @param aType
   *        The permission type string stored in permission manager.
   *        e.g. "cookie", "geo", "indexedDB", "popup", "image"
   */
  clearPermission: function Site_clearPermission(aType) {
    Services.perms.remove(this.host, aType);
  },

  /**
   * Gets cookies stored for the site. This does not return cookies stored
   * for the base domain, only the exact hostname stored for the site.
   *
   * @return An array of the cookies set for the site.
   */
  get cookies() {
    let cookies = [];
    let enumerator = Services.cookies.getCookiesFromHost(this.host);
    while (enumerator.hasMoreElements()) {
      let cookie = enumerator.getNext().QueryInterface(Ci.nsICookie2);
      // getCookiesFromHost returns cookies for base domain, but we only want
      // the cookies for the exact domain.
      if (cookie.rawHost == this.host) {
        cookies.push(cookie);
      }
    }
    return cookies;
  },

  /**
   * Removes a set of specific cookies from the browser.
   */
  clearCookies: function Site_clearCookies() {
    this.cookies.forEach(function(aCookie) {
      Services.cookies.remove(aCookie.host, aCookie.name, aCookie.path, false);
    });
  },

  /**
   * Gets logins stored for the site.
   *
   * @return An array of the logins stored for the site.
   */
  get logins() {
    let httpLogins = Services.logins.findLogins({}, this.httpURI.prePath, "", "");
    let httpsLogins = Services.logins.findLogins({}, this.httpsURI.prePath, "", "");
    return httpLogins.concat(httpsLogins);
  },

  get loginSavingEnabled() {
    // Only say that login saving is blocked if it is blocked for both http and https.
    return Services.logins.getLoginSavingEnabled(this.httpURI.prePath) &&
           Services.logins.getLoginSavingEnabled(this.httpsURI.prePath);
  },

  set loginSavingEnabled(isEnabled) {
    Services.logins.setLoginSavingEnabled(this.httpURI.prePath, isEnabled);
    Services.logins.setLoginSavingEnabled(this.httpsURI.prePath, isEnabled);
  },

  /**
   * Removes all data from the browser corresponding to the site.
   */
  forgetSite: function Site_forgetSite() {
    let pb = Cc["@mozilla.org/privatebrowsing;1"].
             getService(Ci.nsIPrivateBrowsingService);
    pb.removeDataFromDomain(this.host);
  }
}

/**
 * PermissionDefaults object keeps track of default permissions for sites based
 * on global preferences.
 *
 * Inspired by pageinfo/permissions.js
 */
let PermissionDefaults = {
  UNKNOWN: Ci.nsIPermissionManager.UNKNOWN_ACTION, // 0
  ALLOW: Ci.nsIPermissionManager.ALLOW_ACTION, // 1
  DENY: Ci.nsIPermissionManager.DENY_ACTION, // 2
  SESSION: Ci.nsICookiePermission.ACCESS_SESSION, // 8

  get password() {
    if (Services.prefs.getBoolPref("signon.rememberSignons")) {
      return this.ALLOW;
    }
    return this.DENY;
  },
  set password(aValue) {
    let value = (aValue != this.DENY);
    Services.prefs.setBoolPref("signon.rememberSignons", value);
  },

  // For use with network.cookie.* prefs.
  COOKIE_ACCEPT: 0,
  COOKIE_DENY: 2,
  COOKIE_NORMAL: 0,
  COOKIE_SESSION: 2,

  get cookie() {
    if (Services.prefs.getIntPref("network.cookie.cookieBehavior") == this.COOKIE_DENY) {
      return this.DENY;
    }

    if (Services.prefs.getIntPref("network.cookie.lifetimePolicy") == this.COOKIE_SESSION) {
      return this.SESSION;
    }
    return this.ALLOW;
  },
  set cookie(aValue) {
    let value = (aValue == this.DENY) ? this.COOKIE_DENY : this.COOKIE_ACCEPT;
    Services.prefs.setIntPref("network.cookie.cookieBehavior", value);

    let lifetimeValue = aValue == this.SESSION ? this.COOKIE_SESSION :
                                                 this.COOKIE_NORMAL;
    Services.prefs.setIntPref("network.cookie.lifetimePolicy", lifetimeValue);
  },

  get geo() {
    if (!Services.prefs.getBoolPref("geo.enabled")) {
      return this.DENY;
    }
    // We always ask for permission to share location with a specific site, so
    // there is no global ALLOW.
    return this.UNKNOWN;
  },
  set geo(aValue) {
    let value = (aValue != this.DENY);
    Services.prefs.setBoolPref("geo.enabled", value);
  },

  get indexedDB() {
    if (!Services.prefs.getBoolPref("dom.indexedDB.enabled")) {
      return this.DENY;
    }
    // We always ask for permission to enable indexedDB storage for a specific
    // site, so there is no global ALLOW.
    return this.UNKNOWN;
  },
  set indexedDB(aValue) {
    let value = (aValue != this.DENY);
    Services.prefs.setBoolPref("dom.indexedDB.enabled", value);
  },

  get popup() {
    if (Services.prefs.getBoolPref("dom.disable_open_during_load")) {
      return this.DENY;
    }
    return this.ALLOW;
  },
  set popup(aValue) {
    let value = (aValue == this.DENY);
    Services.prefs.setBoolPref("dom.disable_open_during_load", value);
  }
}

/**
 * AboutPermissions manages the about:permissions page.
 */
let AboutPermissions = {
  /**
   * Number of sites to return from the places database.
   */  
  PLACES_SITES_LIMIT: 50,

  /**
   * When adding sites to the dom sites-list, divide workload into intervals.
   */
  LIST_BUILD_CHUNK: 5, // interval size
  LIST_BUILD_DELAY: 100, // delay between intervals

  /**
   * Stores a mapping of host strings to Site objects.
   */
  _sites: {},

  sitesList: null,
  _selectedSite: null,

  /**
   * For testing, track initializations so we can send notifications
   */
  _initPlacesDone: false,
  _initServicesDone: false,

  /**
   * This reflects the permissions that we expose in the UI. These correspond
   * to permission type strings in the permission manager, PermissionDefaults,
   * and element ids in aboutPermissions.xul.
   *
   * Potential future additions: "sts/use", "sts/subd"
   */
  _supportedPermissions: ["password", "cookie", "geo", "indexedDB", "popup"],

  /**
   * Permissions that don't have a global "Allow" option.
   */
  _noGlobalAllow: ["geo", "indexedDB"],

  _stringBundle: Services.strings.
                 createBundle("chrome://browser/locale/preferences/aboutPermissions.properties"),

  /**
   * Called on page load.
   */
  init: function() {
    this.sitesList = document.getElementById("sites-list");

    this.getSitesFromPlaces();

    this.enumerateServicesGenerator = this.getEnumerateServicesGenerator();
    setTimeout(this.enumerateServicesDriver.bind(this), this.LIST_BUILD_DELAY);

    // Attach observers in case data changes while the page is open.
    Services.prefs.addObserver("signon.rememberSignons", this, false);
    Services.prefs.addObserver("network.cookie.", this, false);
    Services.prefs.addObserver("geo.enabled", this, false);
    Services.prefs.addObserver("dom.indexedDB.enabled", this, false);
    Services.prefs.addObserver("dom.disable_open_during_load", this, false);

    Services.obs.addObserver(this, "perm-changed", false);
    Services.obs.addObserver(this, "passwordmgr-storage-changed", false);
    Services.obs.addObserver(this, "cookie-changed", false);
    Services.obs.addObserver(this, "browser:purge-domain-data", false);
    
    this._observersInitialized = true;
    Services.obs.notifyObservers(null, "browser-permissions-preinit", null);
  },

  /**
   * Called on page unload.
   */
  cleanUp: function() {
    if (this._observersInitialized) {
      Services.prefs.removeObserver("signon.rememberSignons", this, false);
      Services.prefs.removeObserver("network.cookie.", this, false);
      Services.prefs.removeObserver("geo.enabled", this, false);
      Services.prefs.removeObserver("dom.indexedDB.enabled", this, false);
      Services.prefs.removeObserver("dom.disable_open_during_load", this, false);

      Services.obs.removeObserver(this, "perm-changed", false);
      Services.obs.removeObserver(this, "passwordmgr-storage-changed", false);
      Services.obs.removeObserver(this, "cookie-changed", false);
      Services.obs.removeObserver(this, "browser:purge-domain-data", false);
    }

    gSitesStmt.finalize();
    gVisitStmt.finalize();
    gPlacesDatabase.asyncClose(null);
  },

  observe: function (aSubject, aTopic, aData) {
    switch(aTopic) {
      case "perm-changed":
        // Permissions changes only affect individual sites.
        if (!this._selectedSite) {
          break;
        }
        // aSubject is null when nsIPermisionManager::removeAll() is called.
        if (!aSubject) {
          this._supportedPermissions.forEach(function(aType){
            this.updatePermission(aType);
          }, this);
          break;
        }
        let permission = aSubject.QueryInterface(Ci.nsIPermission);
        // We can't compare selectedSite.host and permission.host here because
        // we need to handle the case where a parent domain was changed in a
        // way that affects the subdomain.
        if (this._supportedPermissions.indexOf(permission.type) != -1) {
          this.updatePermission(permission.type);
        }
        break;
      case "nsPref:changed":
        this._supportedPermissions.forEach(function(aType){
          this.updatePermission(aType);
        }, this);
        break;
      case "passwordmgr-storage-changed":
        this.updatePermission("password");
        if (this._selectedSite) {
          this.updatePasswordsCount();
        }
        break;
      case "cookie-changed":
        if (this._selectedSite) {
          this.updateCookiesCount();
        }
        break;
      case "browser:purge-domain-data":
        this.deleteFromSitesList(aData);
        break;
    }
  },

  /**
   * Creates Site objects for the top-frecency sites in the places database and stores
   * them in _sites. The number of sites created is controlled by PLACES_SITES_LIMIT.
   */
  getSitesFromPlaces: function() {
    gSitesStmt.params.limit = this.PLACES_SITES_LIMIT;
    gSitesStmt.executeAsync({
      handleResult: function(aResults) {
        AboutPermissions.startSitesListBatch();
        let row;
        while (row = aResults.getNextRow()) {
          let host = row.getResultByName("host");
          AboutPermissions.addHost(host);
        }
        AboutPermissions.endSitesListBatch();
      },
      handleError: function(aError) {
        Cu.reportError("AboutPermissions: " + aError);
      },
      handleCompletion: function(aReason) {
        // Notify oberservers for testing purposes.
        AboutPermissions._initPlacesDone = true;
        if (AboutPermissions._initServicesDone) {
          Services.obs.notifyObservers(null, "browser-permissions-initialized", null);
        }
      }
    });
  },

  /**
   * Drives getEnumerateServicesGenerator to work in intervals.
   */
  enumerateServicesDriver: function() {
    if (this.enumerateServicesGenerator.next()) {
      // Build top sitesList items faster so that the list never seems sparse
      let delay = Math.min(this.sitesList.itemCount * 5, this.LIST_BUILD_DELAY);
      setTimeout(this.enumerateServicesDriver.bind(this), delay);
    } else {
      this.enumerateServicesGenerator.close();
      this._initServicesDone = true;
      if (this._initPlacesDone) {
        Services.obs.notifyObservers(null, "browser-permissions-initialized", null);
      }
    }
  },

  /**
   * Finds sites that have non-default permissions and creates Site objects for
   * them if they are not already stored in _sites.
   */
  getEnumerateServicesGenerator: function() {
    let itemCnt = 1;

    let logins = Services.logins.getAllLogins();
    logins.forEach(function(aLogin) {
      if (itemCnt % this.LIST_BUILD_CHUNK == 0) {
        yield true;
      }
      try {
        // aLogin.hostname is a string in origin URL format (e.g. "http://foo.com")
        let uri = NetUtil.newURI(aLogin.hostname);
        this.addHost(uri.host);
      } catch (e) {
        // newURI will throw for add-ons logins stored in chrome:// URIs 
      }
      itemCnt++;
    }, this);

    let disabledHosts = Services.logins.getAllDisabledHosts();
    disabledHosts.forEach(function(aHostname) {
      if (itemCnt % this.LIST_BUILD_CHUNK == 0) {
        yield true;
      }
      try {
        // aHostname is a string in origin URL format (e.g. "http://foo.com")
        let uri = NetUtil.newURI(aHostname);
        this.addHost(uri.host);
      } catch (e) {
        // newURI will throw for add-ons logins stored in chrome:// URIs 
      }
      itemCnt++;
    }, this);

    let (enumerator = Services.perms.enumerator) {
      while (enumerator.hasMoreElements()) {
        if (itemCnt % this.LIST_BUILD_CHUNK == 0) {
          yield true;
        }
        let permission = enumerator.getNext().QueryInterface(Ci.nsIPermission);
        // Only include sites with exceptions set for supported permission types.
        if (this._supportedPermissions.indexOf(permission.type) != -1) {
          this.addHost(permission.host);
        }
        itemCnt++;
      }
    }

    yield false;
  },

  /**
   * Creates a new Site and adds it to _sites if it's not already there.
   *
   * @param aHost
   *        A host string.
   */
  addHost: function(aHost) {
    if (aHost in this._sites) {
      return;
    }
    let site = new Site(aHost);
    this._sites[aHost] = site;
    this.addToSitesList(site);
  },

  /**
   * Populates sites-list richlistbox with data from Site object.
   *
   * @param aSite
   *        A Site object.
   */
  addToSitesList: function(aSite) {
    let item = document.createElement("richlistitem");
    item.setAttribute("class", "site");
    item.setAttribute("value", aSite.host);

    aSite.getFavicon(function(aURL) {
      item.setAttribute("favicon", aURL);
    });
    aSite.listitem = item;

    // Make sure to only display relevant items when list is filtered
    let filterValue = document.getElementById("sites-filter").value.toLowerCase();
    item.collapsed = aSite.host.toLowerCase().indexOf(filterValue) == -1;

    (this._listFragment || this.sitesList).appendChild(item);
  },

  startSitesListBatch: function () {
    if (!this._listFragment)
      this._listFragment = document.createDocumentFragment();
  },

  endSitesListBatch: function () {
    if (this._listFragment) {
      this.sitesList.appendChild(this._listFragment);
      this._listFragment = null;
    }
  },

  /**
   * Hides sites in richlistbox based on search text in sites-filter textbox.
   */
  filterSitesList: function() {
    let siteItems = this.sitesList.children;
    let filterValue = document.getElementById("sites-filter").value.toLowerCase();

    if (filterValue == "") {
      for (let i = 0; i < siteItems.length; i++) {
        siteItems[i].collapsed = false;
      }
      return;
    }

    for (let i = 0; i < siteItems.length; i++) {
      let siteValue = siteItems[i].value.toLowerCase();
      siteItems[i].collapsed = siteValue.indexOf(filterValue) == -1;
    }
  },

  /**
   * Removes all evidence of the selected site. The "forget this site" observer
   * will call deleteFromSitesList to update the UI.
   */
  forgetSite: function() {
    this._selectedSite.forgetSite();
  },

  /**
   * Deletes sites for a host and all of its sub-domains. Removes these sites
   * from _sites and removes their corresponding elements from the DOM.
   *
   * @param aHost
   *        The host string corresponding to the site to delete.
   */
  deleteFromSitesList: function(aHost) {
    for each (let site in this._sites) {
      if (site.host.hasRootDomain(aHost)) {
        if (site == this._selectedSite) {
          // Replace site-specific interface with "All Sites" interface.
          this.sitesList.selectedItem = document.getElementById("all-sites-item");
        }

        this.sitesList.removeChild(site.listitem);
        delete this._sites[site.host];
      }
    }
  },

  /**
   * Shows interface for managing site-specific permissions.
   */
  onSitesListSelect: function(event) {
    if (event.target.selectedItem.id == "all-sites-item") {
      // Clear the header label value from the previously selected site.
      document.getElementById("site-label").value = "";
      this.manageDefaultPermissions();
      return;
    }

    let host = event.target.value;
    let site = this._selectedSite = this._sites[host];
    document.getElementById("site-label").value = host;
    document.getElementById("header-deck").selectedPanel =
      document.getElementById("site-header");

    this.updateVisitCount();
    this.updatePermissionsBox();
  },

  /**
   * Shows interface for managing default permissions. This corresponds to
   * the "All Sites" list item.
   */
  manageDefaultPermissions: function() {
    this._selectedSite = null;

    document.getElementById("header-deck").selectedPanel =
      document.getElementById("defaults-header");

    this.updatePermissionsBox();
  },

  /**
   * Updates permissions interface based on selected site.
   */
  updatePermissionsBox: function() {
    this._supportedPermissions.forEach(function(aType){
      this.updatePermission(aType);
    }, this);

    this.updatePasswordsCount();
    this.updateCookiesCount();
  },

  /**
   * Sets menulist for a given permission to the correct state, based on the
   * stored permission.
   *
   * @param aType
   *        The permission type string stored in permission manager.
   *        e.g. "cookie", "geo", "indexedDB", "popup", "image"
   */
  updatePermission: function(aType) {
    let allowItem = document.getElementById(aType + "-" + PermissionDefaults.ALLOW);
    allowItem.hidden = !this._selectedSite &&
                       this._noGlobalAllow.indexOf(aType) != -1;

    let permissionMenulist = document.getElementById(aType + "-menulist");
    let permissionValue;    
    if (!this._selectedSite) {
      // If there is no selected site, we are updating the default permissions interface.
      permissionValue = PermissionDefaults[aType];
    } else {
      let result = {};
      permissionValue = this._selectedSite.getPermission(aType, result) ?
                        result.value : PermissionDefaults[aType];
    }

    permissionMenulist.selectedItem = document.getElementById(aType + "-" + permissionValue);
  },

  onPermissionCommand: function(event) {
    let permissionType = event.currentTarget.getAttribute("type");
    let permissionValue = event.target.value;

    if (!this._selectedSite) {
      // If there is no selected site, we are setting the default permission.
      PermissionDefaults[permissionType] = permissionValue;
    } else {
      this._selectedSite.setPermission(permissionType, permissionValue);
    }
  },

  updateVisitCount: function() {
    this._selectedSite.getVisitCount(function(aCount) {
      let visitForm = AboutPermissions._stringBundle.GetStringFromName("visitCount");
      let visitLabel = PluralForm.get(aCount, visitForm)
                                  .replace("#1", aCount);
      document.getElementById("site-visit-count").value = visitLabel;
    });  
  },

  updatePasswordsCount: function() {
    if (!this._selectedSite) {
      document.getElementById("passwords-count").hidden = true;
      document.getElementById("passwords-manage-all-button").hidden = false;
      return;
    }

    let passwordsCount = this._selectedSite.logins.length;
    let passwordsForm = this._stringBundle.GetStringFromName("passwordsCount");
    let passwordsLabel = PluralForm.get(passwordsCount, passwordsForm)
                                   .replace("#1", passwordsCount);

    document.getElementById("passwords-label").value = passwordsLabel;
    document.getElementById("passwords-manage-button").disabled = (passwordsCount < 1);
    document.getElementById("passwords-manage-all-button").hidden = true;
    document.getElementById("passwords-count").hidden = false;
  },

  /**
   * Opens password manager dialog.
   */
  managePasswords: function() {
    let selectedHost = "";
    if (this._selectedSite) {
      selectedHost = this._selectedSite.host;
    }

    let win = Services.wm.getMostRecentWindow("Toolkit:PasswordManager");
    if (win) {
      win.setFilter(selectedHost);
      win.focus();
    } else {
      window.openDialog("chrome://passwordmgr/content/passwordManager.xul",
                        "Toolkit:PasswordManager", "", {filterString : selectedHost});
    }
  },

  updateCookiesCount: function() {
    if (!this._selectedSite) {
      document.getElementById("cookies-count").hidden = true;
      document.getElementById("cookies-clear-all-button").hidden = false;
      document.getElementById("cookies-manage-all-button").hidden = false;
      return;
    }

    let cookiesCount = this._selectedSite.cookies.length;
    let cookiesForm = this._stringBundle.GetStringFromName("cookiesCount");
    let cookiesLabel = PluralForm.get(cookiesCount, cookiesForm)
                                 .replace("#1", cookiesCount);

    document.getElementById("cookies-label").value = cookiesLabel;
    document.getElementById("cookies-clear-button").disabled = (cookiesCount < 1);
    document.getElementById("cookies-manage-button").disabled = (cookiesCount < 1);
    document.getElementById("cookies-clear-all-button").hidden = true;
    document.getElementById("cookies-manage-all-button").hidden = true;
    document.getElementById("cookies-count").hidden = false;
  },

  /**
   * Clears cookies for the selected site.
   */
  clearCookies: function() {
    if (!this._selectedSite) {
      return;
    }
    let site = this._selectedSite;
    site.clearCookies(site.cookies);
    this.updateCookiesCount();
  },

  /**
   * Opens cookie manager dialog.
   */
  manageCookies: function() {
    let selectedHost = "";
    if (this._selectedSite) {
      selectedHost = this._selectedSite.host;
    }

    let win = Services.wm.getMostRecentWindow("Browser:Cookies");
    if (win) {
      win.gCookiesWindow.setFilter(selectedHost);
      win.focus();
    } else {
      window.openDialog("chrome://browser/content/preferences/cookies.xul",
                        "Browser:Cookies", "", {filterString : selectedHost});
    }
  }
}

// See nsPrivateBrowsingService.js
String.prototype.hasRootDomain = function hasRootDomain(aDomain) {
  let index = this.indexOf(aDomain);
  if (index == -1)
    return false;

  if (this == aDomain)
    return true;

  let prevChar = this[index - 1];
  return (index == (this.length - aDomain.length)) &&
         (prevChar == "." || prevChar == "/");
}
