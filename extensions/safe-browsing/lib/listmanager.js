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
 * The Original Code is Google Safe Browsing.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Niels Provos <niels@google.com> (original author)
 *   Fritz Schneider <fritz@google.com>
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


// A class that manages lists, namely white and black lists for
// phishing or malware protection. The ListManager knows how to fetch,
// update, and store lists, and knows the "kind" of list each is (is
// it a whitelist? a blacklist? etc). However it doesn't know how the
// lists are serialized or deserialized (the wireformat classes know
// this) nor the specific format of each list. For example, the list
// could be a map of domains to "1" if the domain is phishy. Or it
// could be a map of hosts to regular expressions to match, who knows?
// Answer: the trtable knows. List are serialized/deserialized by the
// wireformat reader from/to trtables, and queried by the listmanager.
//
// There is a single listmanager for the whole application.
//
// The listmanager is used only in privacy mode; in advanced protection
// mode a remote server is queried.
//
// How to add a new table:
// 1) get it up on the server
// 2) add it to tablesKnown
// 3) if it is not a known table type (trtable.js), add an implementation
//    for it in trtable.js
// 4) add a check for it in the phishwarden's isXY() method, for example
//    isBlackURL()
//
// TODO: obviously the way this works could use a lot of improvement. In
//       particular adding a list should just be a matter of adding
//       its name to the listmanager and an implementation to trtable
//       (or not if a talbe of that type exists). The format and semantics
//       of the list comprise its name, so the listmanager should easily
//       be able to figure out what to do with what list (i.e., no
//       need for step 4).
// TODO reading, writing, and whatnot code should be cleaned up
// TODO read/write asynchronously
// TODO more comprehensive update tests, for example add unittest check 
//      that the listmanagers tables are properly written on updates


/**
 * A ListManager keeps track of black and white lists and knows
 * how to update them.
 *
 * @param threadQueue A global thread queue that we use to schedule our
 *        work so that we do not take up too much time at once.
 *
 * @param opt_testing Boolean indicating that we're testing, so we shouldn't
 *                    automatically act according to preference settings
 *
 * @constructor
 */
function PROT_ListManager(threadQueue, opt_testing) {
  this.debugZone = "listmanager";
  if (opt_testing)
    this.debugZone += "testing";
  G_debugService.enableZone(this.debugZone);
  this.threadQueue_ = threadQueue;
  this.testing_ = !!opt_testing;
  this.appDir_ = null;                 // appDir is set via a method call
  this.rpcPending_ = false;
  this.readingData_ = false;

  // We don't want to start checking against our lists until we've
  // read them. But then again, we don't want to queue URLs to check
  // forever.  So if we haven't successfully read our lists after a
  // certain amount of time, just pretend.

  this.dataImportedAtLeastOnce_ = false;
  var self = this;
  new G_Alarm(function() { self.dataImportedAtLeastOnce_ = true; }, 60 * 1000);

  this.updateserverURL_ = PROT_globalStore.getUpdateserverURL();

  // The lists we know about and the parses we can use to read
  // them. Default all to the earlies possible version (1.-1); this
  // version will get updated when successfully read from disk or
  // fetch updates.
  this.tablesKnown = {
    "goog-white-domain": new PROT_VersionParser("goog-white-domain", 1, -1),
    "goog-black-url" : new PROT_VersionParser("goog-black-url", 1, -1),
    "goog-black-enchash": new PROT_VersionParser("goog-black-enchash", 1, -1),
    "goog-white-url": new PROT_VersionParser("goog-white-url", 1, -1),

    // A major version of zero means local, so don't ask for updates    
    "test1-foo-domain" : new PROT_VersionParser("test1-foo-domain", 0, -1),
    "test2-foo-domain" : new PROT_VersionParser("test2-foo-domain", 0, -1),
  };
  
  this.tablesData = {};

  // Use this to query preferences
  this.prefs_ = new G_Preferences();
}

/**
 * Start managing the lists we know about. We don't do this automatically
 * when the listmanager is instantiated because their profile directory
 * (where we store the lists) might not be available.
 */
PROT_ListManager.prototype.maybeStartManagingUpdates = function() {
  if (this.testing_)
    return;

  if (!this.registeredPrefObservers_) {
    // Get notifications when the advanced protection preference changes
    var checkRemotePrefName = PROT_globalStore.getServerCheckEnabledPrefName();
    var checkRemotePrefObserver = BindToObject(this.onCheckRemotePrefChanged,
                                               this);
    this.prefs_.addObserver(checkRemotePrefName, checkRemotePrefObserver);
    
    // Get notifications when the phishwarden is en/disabled; we want to 
    // start/stop managing the lists accordingly.
    var phishWardenPrefName = PROT_globalStore.getPhishWardenEnabledPrefName();
    var phishWardenPrefObserver = 
      BindToObject(this.onPhishWardenEnabledPrefChanged, this);
    this.prefs_.addObserver(phishWardenPrefName, phishWardenPrefObserver);

    this.registeredPrefObservers_ = true;
  }

  // The preferences might already have been initialized, so let's check 
  // to see if we should be actually updating.
  this.maybeToggleUpdateChecking();
}

/**
 * When a preference (either advanced features or the phishwarden
 * enabled) changes, we might have to start or stop asking for updates. 
 * 
 * This is a little tricky; we start or stop management only when we
 * have complete information we can use to determine whether we
 * should.  It could be the case that one pref or the other isn't set
 * yet (e.g., they haven't opted in/out of advanced protection). So do
 * nothing unless we have both pref values -- we get notifications for
 * both, so eventually we will start correctly (at the very latest
 * when maybeStartManagingUpdates() is explicitly called).
 */ 
PROT_ListManager.prototype.maybeToggleUpdateChecking = function() {
  if (this.testing_)
    return;

  var checkRemotePrefName = PROT_globalStore.getServerCheckEnabledPrefName();
  this.checkRemote_ = this.prefs_.getPref(checkRemotePrefName, null);

  var phishWardenPrefName = PROT_globalStore.getPhishWardenEnabledPrefName();
  var phishWardenEnabled = this.prefs_.getPref(phishWardenPrefName, null);

  G_Debug(this, "Maybe toggling update checking. " +
          "Check remote? " + this.checkRemote_ + " " + 
          "Warden enabled? " + phishWardenEnabled);

  // Only toggle if we have complete information
  if (phishWardenEnabled === true && this.checkRemote_ === false) {
    G_Debug(this, "Starting managing lists");
    new G_Alarm(BindToObject(this.readDataFiles, this), 0);
    new G_Alarm(BindToObject(this.checkForUpdates, this), 3000);
    this.startUpdateChecker();
  } else if (phishWardenEnabled === false || this.checkRemote_ === true) {
    G_Debug(this, "Stopping managing lists (if currently active)");
    this.stopUpdateChecker();                    // Cancel pending updates
  }
}

/**
 * Deal with a user changing the pref that says whether we phishing
 * protection is enabled. If we're in privacy mode and the warden is
 * disabled, we want to stop asking for updates.
 *
 * @param prefName Name of the pref holding the value indicating whether
 *                 the phish warden is enabled.
 */
PROT_ListManager.prototype.onPhishWardenEnabledPrefChanged = function(
    prefName) {

  var phishWardenEnabled = this.prefs_.getPref(prefName, null);

  G_Debug(this, "PhishWardenEnabled Pref Changed: " + prefName + " = " + 
          phishWardenEnabled);
  this.maybeToggleUpdateChecking();
}

/**
 * Deal with a user changing the pref that says whether we should check
 * the remote server.
 *
 * @param prefName Name of the pref holding the value indicating whether
 *                 we should check remote server
 */
PROT_ListManager.prototype.onCheckRemotePrefChanged = function(prefName) {

  var checkRemotePrefName = PROT_globalStore.getServerCheckEnabledPrefName();
  this.checkRemote_ = this.prefs_.getPref(checkRemotePrefName, null);

  G_Debug(this, "CheckRemote Pref Changed: " + prefName + " = " + 
          this.checkRemote_);

  this.maybeToggleUpdateChecking();
}

/**
 * Start periodic checks for updates. Idempotent.
 */
PROT_ListManager.prototype.startUpdateChecker = function() {
  this.stopUpdateChecker();
  
  // Schedule a check for updates every so often
  var sixtyMinutes = 60 * 60 * 1000;
  this.updateChecker_ = new G_Alarm(BindToObject(this.checkForUpdates, this), 
                                    sixtyMinutes, true /* repeat */);
}

/**
 * Stop checking for updates. Idempotent.
 */
PROT_ListManager.prototype.stopUpdateChecker = function() {
  if (this.updateChecker_) {
    this.updateChecker_.cancel();
    this.updateChecker_ = null;
  }
}

/**
 * Set the directory in which we should serialize data
 *
 * @param appDir An nsIFile pointing to our directory (should exist)
 */
PROT_ListManager.prototype.setAppDir = function(appDir) {
  this.appDir_ = appDir;
}

/**
 * Clears the specified table
 * @param table Name of the table that we want to consult
 * @returns true if the table exists, false otherwise
 */
PROT_ListManager.prototype.clearList = function(table) {
  if (!this.tablesKnown[table])
    return false;

  this.tablesData[table] = new PROT_TRTable(this.tablesKnown[table].type);
  return true;
}

/**
 * Provides an exception free way to look up the data in a table. We
 * use this because at certain points our tables might not be loaded,
 * and querying them could throw.
 *
 * @param table Name of the table that we want to consult
 * @param key Key for table lookup
 * @returns false or the value in the table corresponding to key.
 *          If the table name does not exist, we return false, too.
 */
PROT_ListManager.prototype.safeLookup = function(table, key) {
  var result = false;
  try {
    var map = this.tablesData[table];
    result = map.find(key);
  } catch(e) {
    result = false;
    G_Debug(this, "Safelookup masked failure: " + e);
  }

  return result;
}

/**
 * Provides an exception free way to insert data into a table.
 * @param table Name of the table that we want to consult
 * @param key Key for table insert
 * @param value Value for table insert
 * @returns true if the value could be inserted, false otherwise
 */
PROT_ListManager.prototype.safeInsert = function(table, key, value) {
  if (!this.tablesKnown[table]) {
    G_Debug(this, "Unknown table: " + table);
    return false;
  }
  if (!this.tablesData[table])
    this.tablesData[table] = new PROT_TRTable(table);
  try {
    this.tablesData[table].insert(key, value);
  } catch (e) {
    G_Debug(this, "Cannot insert key " + key + " value " + value);
    G_Debug(this, e);
    return false;
  }

  return true;
}

/**
 * Provides an exception free way to remove data from a table.
 * @param table Name of the table that we want to consult
 * @param key Key for table erase
 * @returns true if the value could be removed, false otherwise
 */
PROT_ListManager.prototype.safeErase = function(table, key) {
  if (!this.tablesKnown[table]) {
    G_Debug(this, "Unknown table: " + table);
    return false;
  }

  if (!this.tablesData[table])
    return false;

  return this.tablesData[table].erase(key);
}

/**
 * Reads all data files from storage
 *
 * @returns true if we started reading data from disk, false otherwise.
 */
PROT_ListManager.prototype.readDataFiles = function() {
  if (this.rpcPending_) {
    G_Debug(this, "Cannot read data files while an update RPC is pending");
    return false;
  }

  this.readingData_ = true;

  // Now we need to read all of our nice data files. We just concatenate
  // them together and treat it exactly like a response from the update
  // server.
  var files = [];
  for (var type in this.tablesKnown) {
    var filename = type + ".sst";
    var file = this.appDir_.clone();
    file.append(filename);
    if (file.exists() && file.isFile() && file.isReadable()) {
      G_Debug(this, "Found saved data for: " + type);
      files.push(file);
    } else {
      G_Debug(this, "Failed to find saved data for: " + type);
    }
  }

  // TODO: Should probably break this up on a thread
  var data = "";
  for (var i = 0; i < files.length; ++i) {
    G_Debug(this, "Trying to read: " + files[i].path);
    var gFile = new G_FileReader(files[i]);
    data += gFile.read() + "\n";
    gFile.close();
  }

  var wfr = new PROT_WireFormatReader(this.threadQueue_, this.tablesData);
  wfr.deserialize(data, BindToObject(this.dataFromDisk, this));
  return true;
}

/**
 * A callback that is executed when we have read our table data from
 * disk.
 *
 * @param tablesKnown An array that maps table name to current version
 * @param tablesData An array that maps a table name to a Map which
 *        contains key value pairs.
 */
PROT_ListManager.prototype.dataFromDisk = function(tablesKnown, tablesData) {
  G_Debug(this, "Called dataFromDisk");

  this.importData_(tablesKnown, tablesData);

  this.readingData_ = false;

  return true;
}

/**
 * Prepares a URL to fetch upates from. Format is a squence of 
 * type:major:minor, fields
 * 
 * @param url The base URL to which query parameters are appended; assumes
 *            already has a trailing ?
 * @returns the URL that we should request the table update from.
 */
PROT_ListManager.prototype.getRequestURL_ = function(url) {
  url += "version=";
  var firstElement = true;
  for(var type in this.tablesKnown) {
    // All tables with a major of 0 are internal tables that we never
    // update remotely.
    if (this.tablesKnown[type].major == 0)
      continue;
    if (!firstElement) {
      url += ","
    } else {
      firstElement = false;
    }
    url += type + ":" + this.tablesKnown[type].toUrl();
  }

  G_Debug(this, "Returning: " + url);
  return url;
}

/**
 * Updates our internal tables from the update server
 *
 * @returns true when a new request was scheduled, false if an old request
 *          was still pending.
 */
PROT_ListManager.prototype.checkForUpdates = function() {
  if (this.rpcPending_) {
    G_Debug(this, 'checkForUpdates: old callback is still pending...');
    return false;
  }

  if (this.readingData_) {
    G_Debug(this, 'checkForUpdate: still reading data from disk...');

    // Reschedule the update to happen in a little while
    new G_Alarm(BindToObject(this.checkForUpdates, this), 500);
    return false;
  }

  G_Debug(this, 'checkForUpdates: scheduling request..');
  this.rpcPending_ = true;
  this.xmlFetcher_ = new PROT_XMLFetcher();
  this.xmlFetcher_.get(this.getRequestURL_(this.updateserverURL_),
                       BindToObject(this.rpcDone, this));
  return true;
}

/**
 * A callback that is executed when the XMLHTTP request is finished
 *
 * @param data String containing the returned data
 */
PROT_ListManager.prototype.rpcDone = function(data) {
  G_Debug(this, "Called rpcDone");
  /* Runs in a thread and executes the callback when ready */
  var wfr = new PROT_WireFormatReader(this.threadQueue_, this.tablesData);
  wfr.deserialize(data, BindToObject(this.dataReady, this));

  this.xmlFetcher_ = null;
}

/**
 * @returns Boolean indicating if it has read data from somewhere (e.g.,
 *          disk)
 */
PROT_ListManager.prototype.hasData = function() {
  return !!this.dataImportedAtLeastOnce_;
}

/**
 * We've deserialized tables from disk or the update server, now let's
 * swap them into the tables we're currently using.
 * 
 * @param tablesKnown An array that maps table name to current version
 * @param tablesData An array that maps a table name to a Map which
 *        contains key value pairs.
 * @returns Array of strings holding the names of tables we updated
 */
PROT_ListManager.prototype.importData_ = function(tablesKnown, tablesData) {
  this.dataImportedAtLeastOnce_ = true;

  var changes = [];

  // If our data has changed, update it
  if (tablesKnown && tablesData) {
    // Update our tables with the new data
    for (var name in tablesKnown) {
      if (this.tablesKnown[name] != tablesKnown[name]) {
        changes.push(name);
        this.tablesKnown[name] = tablesKnown[name];
        this.tablesData[name] = tablesData[name];
      }
    }
  }

  return changes;
}

/**
 * A callback that is executed when we have updated the tables from
 * the server. We are provided with the new table versions and the
 * corresponding data.
 *
 * @param tablesKnown An array that maps table name to current version
 * @param tablesData An array that maps a table name to a Map which
 *        contains key value pairs.
 */
PROT_ListManager.prototype.dataReady = function(tablesKnown, tablesData) {
  G_Debug(this, "Called dataReady");

  // First, replace the current tables we're using
  var changes = this.importData_(tablesKnown, tablesData);

  // Then serialize the new tables to disk
  if (changes.length) {
    G_Debug(this, "Commiting " + changes.length + " changed tables to disk.");
    for (var i = 0; i < changes.length; i++) 
      this.storeTable(changes[i], 
                      this.tablesData[changes[i]], 
                      this.tablesKnown[changes[i]]);
  }

  this.rpcPending_ = false;            // todo maybe can do away cuz asynch
  G_Debug(this, "Done writing data to disk.");
}

/**
 * Serialize a table to disk.
 *
 * @param tableName String holding the name of the table to serialize
 * 
 * @param opt_table Reference to the Map holding the table (if omitted,
 *                  we look the table up)
 *
 * @param opt_parser Reference to the versionparser for this table (if
 *                   omitted we look the table up)
 */
PROT_ListManager.prototype.storeTable = function(tableName, 
                                                 opt_table, 
                                                 opt_parser) {

  var table = opt_table ? opt_table : this.tablesData[tableName];
  var parser = opt_parser ? opt_parser : this.tablesKnown[tableName];
 
  if (!table || ! parser) 
    G_Error(this, "Tried to serialize a non-existent table: " + tableName);

  var wfw = new PROT_WireFormatWriter(this.threadQueue_);
  wfw.serialize(table, parser, BindToObject(this.writeDataFile, 
                                            this, 
                                            tableName));
}

/**
 * Takes a serialized table and writes it into our application directory.
 * 
 * @param tableName String containing the name of the table, used to
 *                  create the filename
 *
 * @param tableData Serialized version of the table
 */
PROT_ListManager.prototype.writeDataFile = function(tableName, tableData) {
  var filename = tableName + ".sst";

  G_Debug(this, "Serializing to " + filename);

  try {
    var tmpFile = G_File.createUniqueTempFile(filename);
    var tmpFileWriter = new G_FileWriter(tmpFile);
    
    tmpFileWriter.write(tableData);
    tmpFileWriter.close();

  } catch(e) {
    G_Debug(this, e);
    G_Error(this, "Couldn't write to temp file: " + filename);
  }

  // Now overwrite!
  try {
    tmpFile.moveTo(this.appDir_, filename);
  } catch(e) {
    G_Debug(this, e);
    G_Error(this, "Couldn't overwrite existing table: " + 
            this.appDir_.path + filename);
    tmpFile.remove(false /* not recursive */);
  }
  G_Debug(this, "Serializing to " + filename + " finished.");
}


function TEST_PROT_ListManager() {
  if (G_GDEBUG) {
    var z = "listmanager UNITTEST";
    G_debugService.enableZone(z);
    G_Debug(z, "Starting");

    var tempDir = G_File.createUniqueTempDir();

    var listManager = new PROT_ListManager(new TH_ThreadQueue(), 
                                           true /*testing*/);
    listManager.setAppDir(tempDir);

    var data = "";

    var set1Name = "test1-foo-domain";
    data += "[" + set1Name + " 1.2]\n";
    var set1 = {};
    for (var i = 0; i < 10; i++) {
      set1["http://" + i + ".com"] = 1;
      data += "+" + i + ".com\t1\n";
    }

    var set2Name = "test2-foo-domain";
    // TODO must have blank line
    data += "\n[" + set2Name + " 1.7]\n";
    var set2 = {};
    for (var i = 0; i < 5; i++) {
      set2["http://" + i + ".com"] = 1;
      data += "+" + i + ".com\t1\n";
    }

    function deserialized(tablesKnown, tablesData) {
      listManager.dataReady(tablesKnown, tablesData);

      var file = tempDir.clone();
      file.append(set1Name + ".sst");
      G_Assert(z, file.exists() && file.isFile() && file.isReadable(), 
               "Failed to write out: " + file.path);
      
      file = tempDir.clone();
      file.append(set2Name + ".sst");
      G_Assert(z, file.exists() && file.isFile() && file.isReadable(), 
               "Failed to write out: " + file.path);
      
      listManager = new PROT_ListManager(new TH_ThreadQueue(), 
                                         true /*testing*/);
      listManager.setAppDir(tempDir);
      listManager.readDataFiles();
      
      for (var prop in set1)
        G_Assert(z, 
                 listManager.tablesData[set1Name].find(prop) == 1, 
                 "Couldn't find member " + prop + "of set1 from disk.");
      
      for (var prop in set2)
        G_Assert(z, 
                 listManager.tablesData[set2Name].find(prop) == 1, 
                 "Couldn't find member " + prop + "of set2 from disk.");

      tempDir.remove(true);
      
      G_Debug(z, "PASSED");
    };
  }


  var wfr = new PROT_WireFormatReader(new TH_ThreadQueue, 
                                      listManager.tablesData);
  wfr.deserialize(data, deserialized);
}

