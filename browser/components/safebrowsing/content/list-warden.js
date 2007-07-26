# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Google Safe Browsing.
#
# The Initial Developer of the Original Code is Google Inc.
# Portions created by the Initial Developer are Copyright (C) 2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Niels Provos <niels@google.com> (original author)d

#   Fritz Schneider <fritz@google.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

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

  // Once we register tables, their respective names will be listed here.
  this.blackTables_ = [];
  this.whiteTables_ = [];
}

PROT_ListWarden.IN_BLACKLIST = 0
PROT_ListWarden.IN_WHITELIST = 1
PROT_ListWarden.NOT_FOUND = 2

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
 * Method that looks up a url on the whitelist.
 *
 * @param url The URL to check
 * @param callback Function with a single param:
 *       PROT_ListWarden.IN_BLACKLIST, PROT_ListWarden.IN_WHITELIST,
 *       or PROT_ListWarden.NOT_FOUND
 */
PROT_ListWarden.prototype.isWhiteURL = function(url, callback) {
  (new MultiTableQuerier(url,
                         this.whiteTables_,
                         [] /* no blacklists */,
                         callback)).run();
}

/**
 * Method that looks up a url in both the white and black lists.
 *
 * If there is conflict, the white list has precedence over the black list.
 *
 * This is tricky because all database queries are asynchronous.  So we need
 * to chain together our checks against white and black tables.  We use
 * MultiTableQuerier (see below) to manage this.
 *
 * @param url URL to look up
 * @param callback Function with a single param:
 *       PROT_ListWarden.IN_BLACKLIST, PROT_ListWarden.IN_WHITELIST,
 *       or PROT_ListWarden.NOT_FOUND
 */
PROT_ListWarden.prototype.isEvilURL = function(url, callback) {
  (new MultiTableQuerier(url,
                         this.whiteTables_,
                         this.blackTables_,
                         callback)).run();
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
 * @param callback Function to call with result 
 *       PROT_ListWarden.IN_BLACKLIST, PROT_ListWarden.IN_WHITELIST,
 *       or PROT_ListWarden.NOT_FOUND
 */
function MultiTableQuerier(url, whiteTables, blackTables, callback) {
  this.debugZone = "multitablequerier";
  this.url_ = url;

  this.whiteTables_ = {};
  for (var i = 0; i < whiteTables.length; i++) {
    this.whiteTables_[whiteTables[i]] = true;
  }

  this.blackTables_ = {};
  for (var i = 0; i < blackTables.length; i++) {
    this.blackTables_[blackTables[i]] = true;
  }

  this.callback_ = callback;
  this.listManager_ = Cc["@mozilla.org/url-classifier/listmanager;1"]
                      .getService(Ci.nsIUrlListManager);
}

MultiTableQuerier.prototype.run = function() {
  /* ask the dbservice for all the tables to which this URL belongs */
  this.listManager_.safeLookup(this.url_,
                               BindToObject(this.lookupCallback_, this));
}

MultiTableQuerier.prototype.lookupCallback_ = function(result) {
  if (result == "") {
    this.callback_(PROT_ListWarden.NOT_FOUND);
    return;
  }

  var tableNames = result.split(",");

  /* Check the whitelists */
  for (var i = 0; i < tableNames.length; i++) {
    if (tableNames[i] && this.whiteTables_[tableNames[i]]) {
      this.callback_(PROT_ListWarden.IN_WHITELIST);
      return;
    }
  }

  /* Check the blacklists */
  for (var i = 0; i < tableNames.length; i++) {
    if (tableNames[i] && this.blackTables_[tableNames[i]]) {
      this.callback_(PROT_ListWarden.IN_BLACKLIST);
      return;
    }
  }

  /* Not in any lists we know about */
  this.callback_(PROT_ListWarden.NOT_FOUND);
}
