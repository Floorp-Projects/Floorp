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
 *   Niels Provos <niels@google.com> (original author)d

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

// A warden that knows how to register lists with a listmanager and keep them
// updated if necessary.  The ListWarden also provides a simple interface to
// check if a URL is evil or not.  Specialized wardens like the PhishingWarden
// inherit from it.
//
// Classes that inherit from ListWarden are responsible for calling
// enableTableUpdates or disableTableUpdates.  This usually entails
// registering prefObservers and calling enable or disable in the base
// class as appropriate.
//

/**
 * Abtracts the checking of user/browser actions for signs of
 * phishing. 
 *
 * @constructor
 */
function PROT_ListWarden() {
  this.debugZone = "listwarden";
  var listManager = Cc["@mozilla.org/url-classifier/listmanager;1"]
                      .getService(Ci.nsIUrlListManager);
  this.listManager_ = listManager;

  // If we add support for changing local data providers, we need to add a
  // pref observer that sets the update url accordingly.
  this.listManager_.setUpdateUrl(gDataProvider.getUpdateURL());

  // Once we register tables, their respective names will be listed here.
  this.blackTables_ = [];
  this.whiteTables_ = [];
}

/**
 * Tell the ListManger to keep all of our tables updated
 */

PROT_ListWarden.prototype.enableBlacklistTableUpdates = function() {
  for (var i = 0; i < this.blackTables_.length; ++i) {
    this.listManager_.enableUpdate(this.blackTables_[i]);
  }
}

/**
 * Tell the ListManager to stop updating our tables
 */

PROT_ListWarden.prototype.disableBlacklistTableUpdates = function() {
  for (var i = 0; i < this.blackTables_.length; ++i) {
    this.listManager_.disableUpdate(this.blackTables_[i]);
  }
}

/**
 * Tell the ListManager to update whitelist tables.  They may be enabled even
 * when other updates aren't, for performance reasons.
 */
PROT_ListWarden.prototype.enableWhitelistTableUpdates = function() {
  for (var i = 0; i < this.whiteTables_.length; ++i) {
    this.listManager_.enableUpdate(this.whiteTables_[i]);
  }
}

/**
 * Tell the ListManager to stop updating whitelist tables.
 */
PROT_ListWarden.prototype.disableWhitelistTableUpdates = function() {
  for (var i = 0; i < this.whiteTables_.length; ++i) {
    this.listManager_.disableUpdate(this.whiteTables_[i]);
  }
}

/**
 * Register a new black list table with the list manager
 * @param tableName - name of the table to register
 * @returns true if the table could be registered, false otherwise
 */

PROT_ListWarden.prototype.registerBlackTable = function(tableName) {
  var result = this.listManager_.registerTable(tableName, false);
  if (result) {
    this.blackTables_.push(tableName);
  }
  return result;
}

/**
 * Register a new white list table with the list manager
 * @param tableName - name of the table to register
 * @returns true if the table could be registered, false otherwise
 */

PROT_ListWarden.prototype.registerWhiteTable = function(tableName) {
  var result = this.listManager_.registerTable(tableName, false);
  if (result) {
    this.whiteTables_.push(tableName);
  }
  return result;
}

/**
 * Internal method that looks up a url in both the white and black lists.
 *
 * If there is conflict, the white list has precedence over the black list.
 *
 * This is tricky because all database queries are asynchronous.  So we need
 * to chain together our checks against white and black tables.  We use
 * MultiTableQuerier (see below) to manage this.
 *
 * @param url URL to look up
 * @param evilCallback Function if the url is evil, we call this function.
 */
PROT_ListWarden.prototype.isEvilURL_ = function(url, evilCallback) {
  (new MultiTableQuerier(url,
                         this.whiteTables_, 
                         this.blackTables_,
                         evilCallback)).run();
}

/**
 * This class helps us query multiple tables even though each table check
 * is asynchronous.  It provides callbacks for each listManager lookup
 * and decides whether we need to continue querying or not.  After
 * instantiating the method, use run() to invoke.
 *
 * @param url String The url to check
 * @param whiteTables Array of strings with each white table name
 * @param blackTables Array of strings with each black table name
 * @param evilCallback Function to call if it is an evil url
 */
function MultiTableQuerier(url, whiteTables, blackTables, evilCallback) {
  this.debugZone = "multitablequerier";
  this.url_ = url;

  this.whiteTables_ = whiteTables;
  this.blackTables_ = blackTables;
  this.whiteIdx_ = 0;
  this.blackIdx_ = 0;

  this.evilCallback_ = evilCallback;
  this.listManager_ = Cc["@mozilla.org/url-classifier/listmanager;1"]
                      .getService(Ci.nsIUrlListManager);
}

/**
 * We first query the white tables in succession.  If any contain
 * the url, we stop.  If none contain the url, we query the black tables
 * in succession.  If any contain the url, we call evilCallback and
 * stop.  If none of the black tables contain the url, then we just stop
 * (i.e., it's not black url).
 */
MultiTableQuerier.prototype.run = function() {
  var whiteTable = this.whiteTables_[this.whiteIdx_];
  var blackTable = this.blackTables_[this.blackIdx_];
  if (whiteTable) {
    //G_Debug(this, "Looking in whitetable: " + whiteTable);
    ++this.whiteIdx_;
    this.listManager_.safeExists(whiteTable, this.url_,
                                 BindToObject(this.whiteTableCallback_,
                                              this));
  } else if (blackTable) {
    //G_Debug(this, "Looking in blacktable: " + blackTable);
    ++this.blackIdx_;
    this.listManager_.safeExists(blackTable, this.url_,
                                 BindToObject(this.blackTableCallback_,
                                              this));
  } else {
    // No tables left to check, so we quit.
    G_Debug(this, "Not found in any tables: " + this.url_);

    // Break circular ref to callback.
    this.evilCallback_ = null;
  }
}

/**
 * After checking a white table, we return here.  If the url is found,
 * we can stop.  Otherwise, we call run again.
 */
MultiTableQuerier.prototype.whiteTableCallback_ = function(isFound) {
  //G_Debug(this, "whiteTableCallback_: " + isFound);
  if (!isFound)
    this.run();
  else
    G_Debug(this, "Found in whitelist: " + this.url_)
}

/**
 * After checking a black table, we return here.  If the url is found,
 * we can call the evilCallback and stop.  Otherwise, we call run again.
 */
MultiTableQuerier.prototype.blackTableCallback_ = function(isFound) {
  //G_Debug(this, "blackTableCallback_: " + isFound);
  if (!isFound) {
    this.run();
  } else {
    // In the blacklist, must be an evil url.
    G_Debug(this, "Found in blacklist: " + this.url_)
    this.evilCallback_();
  }
}
