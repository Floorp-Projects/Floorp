/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

const kMainKey = "Software\\Microsoft\\Internet Explorer\\Main";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource:///modules/MigrationUtils.jsm");
Cu.import("resource:///modules/MSMigrationUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "WindowsRegistry",
                                  "resource://gre/modules/WindowsRegistry.jsm");

////////////////////////////////////////////////////////////////////////////////
//// Resources


function History() {
}

History.prototype = {
  type: MigrationUtils.resourceTypes.HISTORY,

  get exists() true,

  __typedURLs: null,
  get _typedURLs() {
    if (!this.__typedURLs) {
      // The list of typed URLs is a sort of annotation stored in the registry.
      // Currently, IE stores 25 entries and this value is not configurable,
      // but we just keep reading up to the first non-existing entry to support
      // possible future bumps of this limit.
      this.__typedURLs = {};
      let registry = Cc["@mozilla.org/windows-registry-key;1"].
                     createInstance(Ci.nsIWindowsRegKey);
      try {
        registry.open(Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
                      "Software\\Microsoft\\Internet Explorer\\TypedURLs",
                      Ci.nsIWindowsRegKey.ACCESS_READ);
        for (let entry = 1; registry.hasValue("url" + entry); entry++) {
          let url = registry.readStringValue("url" + entry);
          this.__typedURLs[url] = true;
        }
      } catch (ex) {
      } finally {
        registry.close();
      }
    }
    return this.__typedURLs;
  },

  migrate: function H_migrate(aCallback) {
    let places = [];
    let historyEnumerator = Cc["@mozilla.org/profile/migrator/iehistoryenumerator;1"].
                            createInstance(Ci.nsISimpleEnumerator);
    while (historyEnumerator.hasMoreElements()) {
      let entry = historyEnumerator.getNext().QueryInterface(Ci.nsIPropertyBag2);
      let uri = entry.get("uri").QueryInterface(Ci.nsIURI);
      // MSIE stores some types of URLs in its history that we don't handle,
      // like HTMLHelp and others.  Since we don't properly map handling for
      // all of them we just avoid importing them.
      if (["http", "https", "ftp", "file"].indexOf(uri.scheme) == -1) {
        continue;
      }

      let title = entry.get("title");
      // Embed visits have no title and don't need to be imported.
      if (title.length == 0) {
        continue;
      }

      // The typed urls are already fixed-up, so we can use them for comparison.
      let transitionType = this._typedURLs[uri.spec] ?
                             Ci.nsINavHistoryService.TRANSITION_TYPED :
                             Ci.nsINavHistoryService.TRANSITION_LINK;
      // use the current date if we have no visits for this entry
      let lastVisitTime = entry.get("time") || Date.now();

      places.push(
        { uri: uri,
          title: title,
          visits: [{ transitionType: transitionType,
                     visitDate: lastVisitTime }]
        }
      );
    }

    // Check whether there is any history to import.
    if (places.length == 0) {
      aCallback(true);
      return;
    }

    PlacesUtils.asyncHistory.updatePlaces(places, {
      _success: false,
      handleResult: function() {
        // Importing any entry is considered a successful import.
        this._success = true;
      },
      handleError: function() {},
      handleCompletion: function() {
        aCallback(this._success);
      }
    });
  }
};

function Settings() {
}

Settings.prototype = {
  type: MigrationUtils.resourceTypes.SETTINGS,

  get exists() true,

  migrate: function S_migrate(aCallback) {
    // Converts from yes/no to a boolean.
    function yesNoToBoolean(v) v == "yes";

    // Converts source format like "en-us,ar-kw;q=0.7,ar-om;q=0.3" into
    // destination format like "en-us, ar-kw, ar-om".
    // Final string is sorted by quality (q=) param.
    function parseAcceptLanguageList(v) {
      return v.match(/([a-z]{1,8}(-[a-z]{1,8})?)\s*(;\s*q\s*=\s*(1|0\.[0-9]+))?/gi)
              .sort(function (a , b) {
                let qA = parseFloat(a.split(";q=")[1]) || 1.0;
                let qB = parseFloat(b.split(";q=")[1]) || 1.0;
                return qA < qB ? 1 : qA == qB ? 0 : -1;
              })
              .map(function(a) a.split(";")[0]);
    }

    // For reference on some of the available IE Registry settings:
    //  * http://msdn.microsoft.com/en-us/library/cc980058%28v=prot.13%29.aspx
    //  * http://msdn.microsoft.com/en-us/library/cc980059%28v=prot.13%29.aspx

    // Note that only settings exposed in our UI should be migrated.

    this._set("Software\\Microsoft\\Internet Explorer\\International",
              "AcceptLanguage",
              "intl.accept_languages",
              parseAcceptLanguageList);
    // TODO (bug 745853): For now, only x-western font is translated.
    this._set("Software\\Microsoft\\Internet Explorer\\International\\Scripts\\3",
              "IEFixedFontName",
              "font.name.monospace.x-western");
    this._set(kMainKey,
              "Use FormSuggest",
              "browser.formfill.enable",
              yesNoToBoolean);
    this._set(kMainKey,
              "FormSuggest Passwords",
              "signon.rememberSignons",
              yesNoToBoolean);
    this._set(kMainKey,
              "Anchor Underline",
              "browser.underline_anchors",
              yesNoToBoolean);
    this._set(kMainKey,
              "Display Inline Images",
              "permissions.default.image",
              function (v) yesNoToBoolean(v) ? 1 : 2);
    this._set(kMainKey,
              "Move System Caret",
              "accessibility.browsewithcaret",
              yesNoToBoolean);
    this._set("Software\\Microsoft\\Internet Explorer\\Settings",
              "Always Use My Colors",
              "browser.display.document_color_use",
              function (v) !Boolean(v) ? 0 : 2);
    this._set("Software\\Microsoft\\Internet Explorer\\Settings",
              "Always Use My Font Face",
              "browser.display.use_document_fonts",
              function (v) !Boolean(v));
    this._set(kMainKey,
              "SmoothScroll",
              "general.smoothScroll",
              Boolean);
    this._set("Software\\Microsoft\\Internet Explorer\\TabbedBrowsing\\",
              "WarnOnClose",
              "browser.tabs.warnOnClose",
              Boolean);
    this._set("Software\\Microsoft\\Internet Explorer\\TabbedBrowsing\\",
              "OpenInForeground",
              "browser.tabs.loadInBackground",
              function (v) !Boolean(v));

    aCallback(true);
  },

  /**
   * Reads a setting from the Registry and stores the converted result into
   * the appropriate Firefox preference.
   * 
   * @param aPath
   *        Registry path under HKCU.
   * @param aKey
   *        Name of the key.
   * @param aPref
   *        Firefox preference.
   * @param [optional] aTransformFn
   *        Conversion function from the Registry format to the pref format.
   */
  _set: function S__set(aPath, aKey, aPref, aTransformFn) {
    let value = WindowsRegistry.readRegKey(Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
                                           aPath, aKey);
    // Don't import settings that have never been flipped.
    if (value === undefined)
      return;

    if (aTransformFn)
      value = aTransformFn(value);

    switch (typeof(value)) {
      case "string":
        Services.prefs.setCharPref(aPref, value);
        break;
      case "number":
        Services.prefs.setIntPref(aPref, value);
        break;
      case "boolean":
        Services.prefs.setBoolPref(aPref, value);
        break;
      default:
        throw new Error("Unexpected value type: " + typeof(value));
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
//// Migrator

function IEProfileMigrator()
{
}

IEProfileMigrator.prototype = Object.create(MigratorPrototype);

IEProfileMigrator.prototype.getResources = function IE_getResources() {
  let resources = [
    MSMigrationUtils.getBookmarksMigrator()
  , new History()
  , MSMigrationUtils.getCookiesMigrator()
  , new Settings()
  ];
  return [r for each (r in resources) if (r.exists)];
};

Object.defineProperty(IEProfileMigrator.prototype, "sourceHomePageURL", {
  get: function IE_get_sourceHomePageURL() {
    let defaultStartPage = WindowsRegistry.readRegKey(Ci.nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE,
                                                      kMainKey, "Default_Page_URL");
    let startPage = WindowsRegistry.readRegKey(Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
                                               kMainKey, "Start Page");
    // If the user didn't customize the Start Page, he is still on the default
    // page, that may be considered the equivalent of our about:home.  There's
    // no reason to retain it, since it is heavily targeted to IE.
    let homepage = startPage != defaultStartPage ? startPage : "";

    // IE7+ supports secondary home pages located in a REG_MULTI_SZ key.  These
    // are in addition to the Start Page, and no empty entries are possible,
    // thus a Start Page is always defined if any of these exists, though it
    // may be the default one.
    let secondaryPages = WindowsRegistry.readRegKey(Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
                                                    kMainKey, "Secondary Start Pages");
    if (secondaryPages) {
      if (homepage)
        secondaryPages.unshift(homepage);
      homepage = secondaryPages.join("|");
    }

    return homepage;
  }
});

IEProfileMigrator.prototype.classDescription = "IE Profile Migrator";
IEProfileMigrator.prototype.contractID = "@mozilla.org/profile/migrator;1?app=browser&type=ie";
IEProfileMigrator.prototype.classID = Components.ID("{3d2532e3-4932-4774-b7ba-968f5899d3a4}");

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([IEProfileMigrator]);
