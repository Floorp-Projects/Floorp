/* -*- Mode: js; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2 et
 * This Source Code is subject to the terms of the Mozilla Public License
 * version 2.0 (the "License"). You can obtain a copy of the License at
 * http://mozilla.org/MPL/2.0/. */

/*
 * Migrates from a Firefox profile in a lossy manner in order to clean up a user's profile. Data
 * is only migrated where the benefits outweigh the potential problems caused by importing
 * undesired/invalid configurations from the source profile.
 */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;
const MIGRATOR = Ci.nsIBrowserProfileMigrator;

const LOCAL_FILE_CID = "@mozilla.org/file/local;1";

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/PlacesUtils.jsm");
Components.utils.import("resource://gre/modules/NetUtil.jsm");
Components.utils.import("resource://gre/modules/FileUtils.jsm");

function FirefoxProfileMigrator()
{
  // profD is not available when the migrator is run during startup so use ProfDS.
  this._paths.currentProfile = FileUtils.getDir("ProfDS", []);
}

FirefoxProfileMigrator.prototype = {
  _paths: {
    bookmarks : null,
    cookies : null,
    currentProfile: null,    // The currently running (destination) profile.
    encryptionKey: null,
    history : null,
    passwords: null,
  },

  _homepageURL : null,
  _replaceBookmarks : false,
  _sourceProfile: null,
  _profilesCache: null,

  /**
   * Notify to observers to start migration
   *
   * @param   aType
   *          notification type such as MIGRATOR.BOOKMARKS
   */
  _notifyStart : function Firefox_notifyStart(aType)
  {
    Services.obs.notifyObservers(null, "Migration:ItemBeforeMigrate", aType);
    this._pendingCount++;
  },

  /**
   * Notify observers that a migration error occured with an item
   *
   * @param   aType
   *          notification type such as MIGRATOR.BOOKMARKS
   */
  _notifyError : function Firefox_notifyError(aType)
  {
    Services.obs.notifyObservers(null, "Migration:ItemError", aType);
  },

  /**
   * Notify to observers to finish migration for item
   * If all items are finished, it sends migration end notification.
   *
   * @param   aType
   *          notification type such as MIGRATOR.BOOKMARKS
   */
  _notifyCompleted : function Firefox_notifyIfCompleted(aType)
  {
    Services.obs.notifyObservers(null, "Migration:ItemAfterMigrate", aType);
    if (--this._pendingCount == 0) {
      // All items are migrated, so we have to send end notification.
      Services.obs.notifyObservers(null, "Migration:Ended", null);
    }
  },

  /**
   * The directory used for bookmark backups
   * @return  directory of the bookmark backups
   */
  get _bookmarks_backup_folder()
  {
    let bookmarksBackupRelativePath = PlacesUtils.backups.profileRelativeFolderPath;
    let bookmarksBackupDir = this._sourceProfile.clone().QueryInterface(Ci.nsILocalFile);
    bookmarksBackupDir.appendRelativePath(bookmarksBackupRelativePath);
    return bookmarksBackupDir;
  },

  /**
   * Migrating bookmark items
   */
  _migrateBookmarks : function Firefox_migrateBookmarks()
  {
    this._notifyStart(MIGRATOR.BOOKMARKS);

    try {
      let srcBackupsDir = this._bookmarks_backup_folder;
      let backupFolder = this._paths.currentProfile.clone();
      backupFolder.append(srcBackupsDir.leafName);
      if (!backupFolder.exists())
        srcBackupsDir.copyTo(this._paths.currentProfile, null);
    } catch (e) {
      Cu.reportError(e);
      // Don't notify about backup migration errors since actual bookmarks are
      // migrated with history.
    } finally {
      this._notifyCompleted(MIGRATOR.BOOKMARKS);
    }
  },

  /**
   * Migrating history
   */
  _migrateHistory : function Firefox_migrateHistory()
  {
    this._notifyStart(MIGRATOR.HISTORY);

    try {
      // access sqlite3 database of history
      let file = Cc[LOCAL_FILE_CID].createInstance(Ci.nsILocalFile);
      file.initWithPath(this._paths.history);
      file.copyTo(this._paths.currentProfile, null);
    } catch (e) {
      Cu.reportError(e);
      this._notifyError(MIGRATOR.HISTORY);
    } finally {
      this._notifyCompleted(MIGRATOR.HISTORY);
    }
  },

  /**
   * Migrating cookies
   *
   * @note    Cookie permissions are not migrated since they may be inadvertently set leading to
   *          website login problems.
   */
  _migrateCookies : function Firefox_migrateCookies()
  {
    this._notifyStart(MIGRATOR.COOKIES);

    try {
      // Access sqlite3 database of cookies
      let file = Cc[LOCAL_FILE_CID].createInstance(Ci.nsILocalFile);
      file.initWithPath(this._paths.cookies);
      file.copyTo(this._paths.currentProfile, null);
    } catch (e) {
      Cu.reportError(e);
      this._notifyError(MIGRATOR.COOKIES);
    } finally {
      this._notifyCompleted(MIGRATOR.COOKIES);
    }
  },

  /**
   * Migrating passwords
   */
  _migratePasswords : function Firefox_migratePasswords()
  {
    this._notifyStart(MIGRATOR.PASSWORDS);

    try {
      // Access sqlite3 database of passwords
      let file = Cc[LOCAL_FILE_CID].createInstance(Ci.nsILocalFile);
      file.initWithPath(this._paths.passwords);
      file.copyTo(this._paths.currentProfile, null);

      let encryptionKey = Cc[LOCAL_FILE_CID].createInstance(Ci.nsILocalFile);
      encryptionKey.initWithPath(this._paths.encryptionKey);
      encryptionKey.copyTo(this._paths.currentProfile, null);
    } catch (e) {
      Cu.reportError(e);
      this._notifyError(MIGRATOR.PASSWORDS);
    } finally {
      this._notifyCompleted(MIGRATOR.PASSWORDS);
    }
  },


  /**
   * nsIBrowserProfileMigrator interface implementation
   */

  /**
   * Let's migrate all items
   *
   * @param   aItems
   *          list of data items to migrate.
   * @param   aStartup
   *          non-null if called during startup.
   * @param   aProfile
   *          profile directory path to migrate from
   */
  migrate : function Firefox_migrate(aItems, aStartup, aProfile)
  {
    if (aStartup) {
      aStartup.doStartup();
      this._replaceBookmarks = true;
    }

    Services.obs.notifyObservers(null, "Migration:Started", null);

    // Reset pending count.  If this count becomes 0, "Migration:Ended"
    // notification is sent
    this._pendingCount = 1;

    if (aItems & MIGRATOR.HISTORY)
      this._migrateHistory();

    if (aItems & MIGRATOR.COOKIES)
      this._migrateCookies();

    if (aItems & MIGRATOR.BOOKMARKS)
      this._migrateBookmarks();

    if (aItems & MIGRATOR.PASSWORDS)
      this._migratePasswords();

    if (--this._pendingCount == 0) {
      // When async imports are immediately completed unfortunately,
      // this will be called.
      // Usually, this notification is sent by _notifyCompleted()
      Services.obs.notifyObservers(null, "Migration:Ended", null);
    }
  },

  /**
   * return supported migration types
   *
   * @param   aProfile
   *          the profile path to migrate from
   * @param   aDoingStartup
   *          non-null if called during startup.
   * @return  supported migration types
   *
   * @todo    Bug 715315 - make sure source databases are not in-use
   */
  getMigrateData: function Firefox_getMigrateData(aProfile, aDoingStartup)
  {
    this._sourceProfile = Cc[LOCAL_FILE_CID].createInstance(Ci.nsILocalFile);
    this._sourceProfile.initWithPath(aProfile);

    let result = 0;
    if (!this._sourceProfile.exists() || !this._sourceProfile.isReadable()) {
      Cu.reportError("source profile directory doesn't exist or is not readable");
      return result;
    }

    // Migration initiated from the UI is not supported.
    if (!aDoingStartup)
      return result;

    // Bookmark backups in JSON format
    try {
      let file = this._bookmarks_backup_folder;
      if (file.exists()) {
        this._paths.bookmarks = file.path;
        result += MIGRATOR.BOOKMARKS;
      }
    } catch (e) {
      Cu.reportError(e);
    }

    // Bookmarks, history and favicons
    try {
      let file = this._sourceProfile.clone();
      file.append("places.sqlite");
      if (file.exists()) {
        this._paths.history = file.path;
        result += MIGRATOR.HISTORY;
        result |= MIGRATOR.BOOKMARKS;
      }
    } catch (e) {
      Cu.reportError(e);
    }

    // Cookies
    try {
      let file = this._sourceProfile.clone();
      file.append("cookies.sqlite");
      if (file.exists()) {
        this._paths.cookies = file.path;
        result += MIGRATOR.COOKIES;
      }
    } catch (e) {
      Cu.reportError(e);
    }

    // Passwords & encryption key
    try {
      let passwords = this._sourceProfile.clone();
      passwords.append("signons.sqlite");
      let encryptionKey = this._sourceProfile.clone();
      encryptionKey.append("key3.db");
      if (passwords.exists() && encryptionKey.exists()) {
        this._paths.passwords = passwords.path;
        this._paths.encryptionKey = encryptionKey.path;
        result += MIGRATOR.PASSWORDS;
      }
    } catch (e) {
      Cu.reportError(e);
    }

    return result;
  },

  /**
   * Whether we support migration of Firefox
   *
   * @return true if supported
   */
  get sourceExists()
  {
    let userData = Services.dirsvc.get("DefProfRt", Ci.nsIFile).path;
    let result = 0;
    try {
      let userDataDir = Cc[LOCAL_FILE_CID].createInstance(Ci.nsILocalFile);
      userDataDir.initWithPath(userData);
      if (!userDataDir.exists() || !userDataDir.isReadable())
        return false;

      let profiles = this.sourceProfiles;
      if (profiles.length < 1)
        return false;
      // Check that we can get data from at least one profile since profile selection has not
      // happened yet.
      for (let i = 0; i < profiles.length; i++) {
        result = this.getMigrateData(profiles.queryElementAt(i, Ci.nsISupportsString), true);
        if (result)
          break;
      }
    } catch (e) {
      Cu.reportError(e);
    }
    return result > 0;
  },

  get sourceHasMultipleProfiles()
  {
    return this.sourceProfiles.length > 1;
  },

  get sourceProfiles()
  {
    try
    {
      if (!this._profilesCache)
      {
        this._profilesCache = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
        let profileService = Cc["@mozilla.org/toolkit/profile-service;1"]
                               .getService(Ci.nsIToolkitProfileService);
        // Only allow migrating from the default (selected) profile since this will be the only one
        // returned by the toolkit profile service after bug 214675 has landed.
        var profile = profileService.selectedProfile;
        if (profile.rootDir.path === this._paths.currentProfile.path)
          return null;

        let str = Cc["@mozilla.org/supports-string;1"].createInstance(Ci.nsISupportsString);
        str.data = profile.rootDir.path;
        this._profilesCache.appendElement(str, false);
      }
    } catch (e) {
      Cu.reportError("Error detecting Firefox profiles: " + e);
    }
    return this._profilesCache;
  },

  /**
   * Return home page URL
   *
   * @return  home page URL
   *
   * @todo    Bug 715348 will migrate preferences such as the homepage
   */
  get sourceHomePageURL()
  {
    try  {
      if (this._homepageURL)
        return this._homepageURL;
    } catch (e) {
      Cu.reportError(e);
    }
    return "";
  },

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIBrowserProfileMigrator
  ]),

  classDescription: "Firefox Profile Migrator",
  contractID: "@mozilla.org/profile/migrator;1?app=browser&type=firefox",
  classID: Components.ID("{91185366-ba97-4438-acba-48deaca63386}")
};

const NSGetFactory = XPCOMUtils.generateNSGetFactory([FirefoxProfileMigrator]);
