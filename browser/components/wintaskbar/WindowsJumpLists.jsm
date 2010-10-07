/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jim Mathies <jmathies@mozilla.com>
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

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

/**
 * Constants
 */

const Cc = Components.classes;
const Ci = Components.interfaces;

// Prefs
const PREF_TASKBAR_BRANCH    = "browser.taskbar.lists.";
const PREF_TASKBAR_ENABLED   = "enabled";
const PREF_TASKBAR_ITEMCOUNT = "maxListItemCount";
const PREF_TASKBAR_FREQUENT  = "frequent.enabled";
const PREF_TASKBAR_RECENT    = "recent.enabled";
const PREF_TASKBAR_TASKS     = "tasks.enabled";
const PREF_TASKBAR_REFRESH   = "refreshInSeconds";

/**
 * Exports
 */

let EXPORTED_SYMBOLS = [
  "WinTaskbarJumpList",
];

/**
 * Smart getters
 */

XPCOMUtils.defineLazyGetter(this, "_prefs", function() {
  return Cc["@mozilla.org/preferences-service;1"]
           .getService(Ci.nsIPrefService)
           .getBranch(PREF_TASKBAR_BRANCH)
           .QueryInterface(Ci.nsIPrefBranch2);
});

XPCOMUtils.defineLazyGetter(this, "_stringBundle", function() {
  return Cc["@mozilla.org/intl/stringbundle;1"]
           .getService(Ci.nsIStringBundleService)
           .createBundle("chrome://browser/locale/taskbar.properties");
});

XPCOMUtils.defineLazyServiceGetter(this, "_taskbarService",
                                   "@mozilla.org/windows-taskbar;1",
                                   "nsIWinTaskbar");

XPCOMUtils.defineLazyServiceGetter(this, "_navHistoryService",
                                   "@mozilla.org/browser/nav-history-service;1",
                                   "nsINavHistoryService");

XPCOMUtils.defineLazyServiceGetter(this, "_observerService",
                                   "@mozilla.org/observer-service;1",
                                   "nsIObserverService");

XPCOMUtils.defineLazyServiceGetter(this, "_directoryService",
                                   "@mozilla.org/file/directory_service;1",
                                   "nsIProperties");

XPCOMUtils.defineLazyServiceGetter(this, "_ioService",
                                   "@mozilla.org/network/io-service;1",
                                   "nsIIOService");

XPCOMUtils.defineLazyServiceGetter(this, "_winShellService",
                                   "@mozilla.org/browser/shell-service;1",
                                   "nsIWindowsShellService");

XPCOMUtils.defineLazyServiceGetter(this, "_privateBrowsingSvc",
                                   "@mozilla.org/privatebrowsing;1",
                                   "nsIPrivateBrowsingService");

/**
 * Global functions
 */

function _getString(name) {
  return _stringBundle.GetStringFromName(name);
}

/////////////////////////////////////////////////////////////////////////////
// Task list configuration data object.

var tasksCfg = [
  /**
   * Task configuration options: title, description, args, iconIndex, open, close.
   *
   * title       - Task title displayed in the list. (strings in the table are temp fillers.)
   * description - Tooltip description on the list item.
   * args        - Command line args to invoke the task.
   * iconIndex   - Optional win icon index into the main application for the
   *               list item.
   * open        - Boolean indicates if the command should be visible after the browser opens.
   * close       - Boolean indicates if the command should be visible after the browser closes.
   */
  // Open new window
  {
    get title()       _getString("taskbar.tasks.newTab.label"),
    get description() _getString("taskbar.tasks.newTab.description"),
    args:             "-new-tab about:blank",
    iconIndex:        0, // Fx app icon
    open:             true,
    close:            false, // The jump list already has an app launch icon
  },

  // Open new tab
  {
    get title()       _getString("taskbar.tasks.newWindow.label"),
    get description() _getString("taskbar.tasks.newWindow.description"),
    args:             "-browser",
    iconIndex:        0, // Fx app icon
    open:             true,
    close:            false, // no point
  },

  // Toggle the Private Browsing mode
  {
    get title() {
      if (_privateBrowsingSvc.privateBrowsingEnabled)
        return _getString("taskbar.tasks.exitPrivacyMode.label");
      else
        return _getString("taskbar.tasks.enterPrivacyMode.label");
    },
    get description() {
      if (_privateBrowsingSvc.privateBrowsingEnabled)
        return _getString("taskbar.tasks.exitPrivacyMode.description");
      else
        return _getString("taskbar.tasks.enterPrivacyMode.description");
    },
    args:             "-private-toggle",
    iconIndex:        0, // Fx app icon
    open:             true,
    close:            true,
  },
];

/////////////////////////////////////////////////////////////////////////////
// Implementation

var WinTaskbarJumpList =
{
  _builder: null,
  _tasks: null,
  _shuttingDown: false,

  /**
   * Startup, shutdown, and update
   */ 

  startup: function WTBJL_startup() {
    // exit if this isn't win7 or higher.
    if (!this._initTaskbar())
      return;

    // Win shell shortcut maintenance. If we've gone through an update,
    // this will update any pinned taskbar shortcuts. Not specific to
    // jump lists, but this was a convienent place to call it. 
    this._shortcutMaintenance();

    // Store our task list config data
    this._tasks = tasksCfg;

    // retrieve taskbar related prefs.
    this._refreshPrefs();
    
    // observer for private browsing and our prefs branch
    this._initObs();

    // jump list refresh timer
    this._initTimer();
  },

  update: function WTBJL_update() {
    // are we disabled via prefs? don't do anything!
    if (!this._enabled)
      return;

    // do what we came here to do, update the taskbar jumplist
    this._buildList();
  },

  _shutdown: function WTBJL__shutdown() {
    this._shuttingDown = true;
    this.update();
    this._free();
  },

  _shortcutMaintenance: function WTBJL__maintenace() {
    _winShellService.shortcutMaintenance();
  },

  /**
   * List building
   */ 

  _buildList: function WTBJL__buildList() {
    // anything to build?
    if (!this._showFrequent && !this._showRecent && !this._showTasks) {
      // don't leave the last list hanging on the taskbar.
      this._deleteActiveJumpList();
      return;
    }

    if (!this._startBuild())
      return;

    if (this._showTasks)
      this._buildTasks();

    // Space for frequent items takes priority over recent.
    if (this._showFrequent)
      this._buildFrequent();

    if (this._showRecent)
      this._buildRecent();

    this._commitBuild();
  },

  /**
   * Taskbar api wrappers
   */ 

  _startBuild: function WTBJL__startBuild() {
    var removedItems = Cc["@mozilla.org/array;1"].
                       createInstance(Ci.nsIMutableArray);
    this._builder.abortListBuild();
    if (this._builder.initListBuild(removedItems)) { 
      // Prior to building, delete removed items from history.
      this._clearHistory(removedItems);
      return true;
    }
    return false;
  },

  _commitBuild: function WTBJL__commitBuild() {
    if (!this._builder.commitListBuild())
      this._builder.abortListBuild();
  },

  _buildTasks: function WTBJL__buildTasks() {
    var items = Cc["@mozilla.org/array;1"].
                createInstance(Ci.nsIMutableArray);
    this._tasks.forEach(function (task) {
      if ((this._shuttingDown && !task.close) || (!this._shuttingDown && !task.open))
        return;
      var item = this._getHandlerAppItem(task.title, task.description,
                                         task.args, task.iconIndex);
      items.appendElement(item, false);
    }, this);
    
    if (items.length > 0)
      this._builder.addListToBuild(this._builder.JUMPLIST_CATEGORY_TASKS, items);
  },

  _buildCustom: function WTBJL__buildCustom(title, items) {
    if (items.length > 0)
      this._builder.addListToBuild(this._builder.JUMPLIST_CATEGORY_CUSTOMLIST, items, title);
  },

  _buildFrequent: function WTBJL__buildFrequent() {
    // Windows supports default frequent and recent lists,
    // but those depend on internal windows visit tracking
    // which we don't populate. So we build our own custom
    // frequent and recent lists using our nav history data.

    var items = Cc["@mozilla.org/array;1"].
                createInstance(Ci.nsIMutableArray);
    var list = this._getNavFrequent(this._maxItemCount);

    if (!list || list.length == 0)
      return;

    // track frequent items so that we don't add them to
    // the recent list.
    this._frequentHashList = [];

    list.forEach(function (entry) {
      let shortcut = this._getHandlerAppItem(entry.title, entry.title, entry.uri, 1);
      items.appendElement(shortcut, false);
      this._frequentHashList.push(entry.uri);
    }, this);
    this._buildCustom(_getString("taskbar.frequent.label"), items);
  },

  _buildRecent: function WTBJL__buildRecent() {
    var items = Cc["@mozilla.org/array;1"].
                createInstance(Ci.nsIMutableArray);
    var list = this._getNavRecent(this._maxItemCount*2);
    
    if (!list || list.length == 0)
      return;

    let count = 0;
    for (let idx = 0; idx < list.length; idx++) {
      if (count >= this._maxItemCount)
        break;
      let entry = list[idx];
      // do not add items to recent that have already been added
      // to frequent.
      if (this._frequentHashList &&
          this._frequentHashList.indexOf(entry.uri) != -1)
        continue;
      let shortcut = this._getHandlerAppItem(entry.title, entry.title, entry.uri, 1);
      items.appendElement(shortcut, false);
      count++;
    }
    this._buildCustom(_getString("taskbar.recent.label"), items);
  },

  _deleteActiveJumpList: function WTBJL__deleteAJL() {
    this._builder.deleteActiveList();
  },

  /**
   * Jump list item creation helpers
   */

  _getHandlerAppItem: function WTBJL__getHandlerAppItem(name, description, args, icon) {
    var file = _directoryService.get("XCurProcD", Ci.nsILocalFile);

    // XXX where can we grab this from in the build? Do we need to?
    file.append("firefox.exe");

    var handlerApp = Cc["@mozilla.org/uriloader/local-handler-app;1"].
                     createInstance(Ci.nsILocalHandlerApp);
    handlerApp.executable = file;
    // handlers default to the leaf name if a name is not specified
    if (name && name.length != 0)
      handlerApp.name = name;
    handlerApp.detailedDescription = description;
    handlerApp.appendParameter(args);

    var item = Cc["@mozilla.org/windows-jumplistshortcut;1"].
               createInstance(Ci.nsIJumpListShortcut);
    item.app = handlerApp;
    item.iconIndex = icon;
    return item;
  },

  _getSeparatorItem: function WTBJL__getSeparatorItem() {
    var item = Cc["@mozilla.org/windows-jumplistseparator;1"].
               createInstance(Ci.nsIJumpListSeparator);
    return item;
  },

  /**
   * Nav history helpers
   */ 

  _getNavFrequent: function WTBJL__getNavFrequent(depth) {
    var options = _navHistoryService.getNewQueryOptions();
    var query = _navHistoryService.getNewQuery();
    
    query.beginTimeReference = query.TIME_RELATIVE_NOW;
    query.beginTime = -24 * 30 * 60 * 60 * 1000000; // one month
    query.endTimeReference = query.TIME_RELATIVE_NOW;

    options.maxResults = depth;
    options.queryType = options.QUERY_TYPE_HISTORY;
    options.sortingMode = options.SORT_BY_VISITCOUNT_DESCENDING;
    options.resultType = options.RESULT_TYPE_URI;

    var result = _navHistoryService.executeQuery(query, options);

    var list = [];

    var rootNode = result.root;
    rootNode.containerOpen = true;

    for (let idx = 0; idx < rootNode.childCount; idx++) {
      let node = rootNode.getChild(idx);
      list.push({uri: node.uri, title: node.title});
    }
    rootNode.containerOpen = false;

    return list;
  },
  
  _getNavRecent: function WTBJL__getNavRecent(depth) {
    var options = _navHistoryService.getNewQueryOptions();
    var query = _navHistoryService.getNewQuery();
    
    query.beginTimeReference = query.TIME_RELATIVE_NOW;
    query.beginTime = -48 * 60 * 60 * 1000000; // two days
    query.endTimeReference = query.TIME_RELATIVE_NOW;

    options.maxResults = depth;
    options.queryType = options.QUERY_TYPE_HISTORY;
    options.sortingMode = options.SORT_BY_LASTMODIFIED_DESCENDING;
    options.resultType = options.RESULT_TYPE_URI;

    var result = _navHistoryService.executeQuery(query, options);

    var list = [];

    var rootNode = result.root;
    rootNode.containerOpen = true;

    for (var idx = 0; idx < rootNode.childCount; idx++) {
      var node = rootNode.getChild(idx);
      list.push({uri: node.uri, title: node.title});
    }
    rootNode.containerOpen = false;

    return list;
  },
  
  _clearHistory: function WTBJL__clearHistory(items) {
    if (!items)
      return;
    var enum = items.enumerate();
    while (enum.hasMoreElements()) {
      let oldItem = enum.getNext().QueryInterface(Ci.nsIJumpListShortcut);
      if (oldItem) {
        try { // in case we get a bad uri
          let uriSpec = oldItem.app.getParameter(0);
          _navHistoryService.QueryInterface(Ci.nsIBrowserHistory).removePage(
            _ioService.newURI(uriSpec, null, null));
        } catch (err) { }
      }
    }
  },

  /**
   * Prefs utilities
   */ 

  _refreshPrefs: function WTBJL__refreshPrefs() {
    this._enabled = _prefs.getBoolPref(PREF_TASKBAR_ENABLED);
    this._showFrequent = _prefs.getBoolPref(PREF_TASKBAR_FREQUENT);
    this._showRecent = _prefs.getBoolPref(PREF_TASKBAR_RECENT);
    this._showTasks = _prefs.getBoolPref(PREF_TASKBAR_TASKS);
    this._maxItemCount = _prefs.getIntPref(PREF_TASKBAR_ITEMCOUNT);
  },

  /**
   * Init and shutdown utilities
   */ 

  _initTaskbar: function WTBJL__initTaskbar() {
    this._builder = _taskbarService.createJumpListBuilder();
    if (!this._builder || !this._builder.available)
      return false;

    return true;
  },

  _initObs: function WTBJL__initObs() {
    _observerService.addObserver(this, "private-browsing", false);
    _observerService.addObserver(this, "quit-application-granted", false);
    _observerService.addObserver(this, "browser:purge-session-history", false);
    _prefs.addObserver("", this, false);
  },
 
  _freeObs: function WTBJL__freeObs() {
    _observerService.removeObserver(this, "private-browsing");
    _observerService.removeObserver(this, "quit-application-granted");
    _observerService.removeObserver(this, "browser:purge-session-history");
    _prefs.removeObserver("", this);
  },

  _initTimer: function WTBJL__initTimer(aTimer) {
    this._timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    this._timer.initWithCallback(this,
                                 _prefs.getIntPref(PREF_TASKBAR_REFRESH)*1000,
                                 this._timer.TYPE_REPEATING_SLACK);
  },

  _free: function WTBJL__free() {
    this._freeObs();
    delete this._builder;
    delete this._timer;
  },

  /**
   * Notification handlers
   */

  notify: function WTBJL_notify(aTimer) {
    this.update();
  },

  observe: function WTBJL_observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "nsPref:changed":
        if (this._enabled == true && !_prefs.getBoolPref(PREF_TASKBAR_ENABLED))
          this._deleteActiveJumpList();
        this._refreshPrefs();
        this.update();
      break;

      case "quit-application-granted":
        this._shutdown();
      break;

      case "browser:purge-session-history":
        this.update();
      break;

      case "private-browsing":
        this.update();
      break;
    }
  },
};
