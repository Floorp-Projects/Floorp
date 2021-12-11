/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

// Stop updating jumplists after some idle time.
const IDLE_TIMEOUT_SECONDS = 5 * 60;

// Prefs
const PREF_TASKBAR_BRANCH = "browser.taskbar.lists.";
const PREF_TASKBAR_ENABLED = "enabled";
const PREF_TASKBAR_ITEMCOUNT = "maxListItemCount";
const PREF_TASKBAR_FREQUENT = "frequent.enabled";
const PREF_TASKBAR_RECENT = "recent.enabled";
const PREF_TASKBAR_TASKS = "tasks.enabled";
const PREF_TASKBAR_REFRESH = "refreshInSeconds";

// Hash keys for pendingStatements.
const LIST_TYPE = {
  FREQUENT: 0,
  RECENT: 1,
};

/**
 * Exports
 */

var EXPORTED_SYMBOLS = ["WinTaskbarJumpList"];

/**
 * Smart getters
 */

XPCOMUtils.defineLazyGetter(this, "_prefs", function() {
  return Services.prefs.getBranch(PREF_TASKBAR_BRANCH);
});

XPCOMUtils.defineLazyGetter(this, "_stringBundle", function() {
  return Services.strings.createBundle(
    "chrome://browser/locale/taskbar.properties"
  );
});

XPCOMUtils.defineLazyServiceGetter(
  this,
  "_idle",
  "@mozilla.org/widget/useridleservice;1",
  "nsIUserIdleService"
);
XPCOMUtils.defineLazyServiceGetter(
  this,
  "_taskbarService",
  "@mozilla.org/windows-taskbar;1",
  "nsIWinTaskbar"
);

ChromeUtils.defineModuleGetter(
  this,
  "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);

/**
 * Global functions
 */

function _getString(name) {
  return _stringBundle.GetStringFromName(name);
}

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
  // Open new tab
  {
    get title() {
      return _getString("taskbar.tasks.newTab.label");
    },
    get description() {
      return _getString("taskbar.tasks.newTab.description");
    },
    args: "-new-tab about:blank",
    iconIndex: 3, // New window icon
    open: true,
    close: true, // The jump list already has an app launch icon, but
    // we don't always update the list on shutdown.
    // Thus true for consistency.
  },

  // Open new window
  {
    get title() {
      return _getString("taskbar.tasks.newWindow.label");
    },
    get description() {
      return _getString("taskbar.tasks.newWindow.description");
    },
    args: "-browser",
    iconIndex: 2, // New tab icon
    open: true,
    close: true, // No point, but we don't always update the list on
    // shutdown. Thus true for consistency.
  },
];

// Open new private window
let privateWindowTask = {
  get title() {
    return _getString("taskbar.tasks.newPrivateWindow.label");
  },
  get description() {
    return _getString("taskbar.tasks.newPrivateWindow.description");
  },
  args: "-private-window",
  iconIndex: 4, // Private browsing mode icon
  open: true,
  close: true, // No point, but we don't always update the list on
  // shutdown. Thus true for consistency.
};

// Implementation

var WinTaskbarJumpList = {
  _builder: null,
  _tasks: null,
  _shuttingDown: false,

  /**
   * Startup, shutdown, and update
   */

  startup: function WTBJL_startup() {
    // exit if this isn't win7 or higher.
    if (!this._initTaskbar()) {
      return;
    }

    // Store our task list config data
    this._tasks = tasksCfg;

    if (PrivateBrowsingUtils.enabled) {
      tasksCfg.push(privateWindowTask);
    }

    // retrieve taskbar related prefs.
    this._refreshPrefs();

    // observer for private browsing and our prefs branch
    this._initObs();

    // jump list refresh timer
    this._updateTimer();
  },

  update: function WTBJL_update() {
    // are we disabled via prefs? don't do anything!
    if (!this._enabled) {
      return;
    }

    // do what we came here to do, update the taskbar jumplist
    this._buildList();
  },

  _shutdown: function WTBJL__shutdown() {
    this._shuttingDown = true;
    this._free();
  },

  /**
   * List building
   *
   * @note Async builders must add their mozIStoragePendingStatement to
   *       _pendingStatements object, using a different LIST_TYPE entry for
   *       each statement. Once finished they must remove it and call
   *       commitBuild().  When there will be no more _pendingStatements,
   *       commitBuild() will commit for real.
   */

  _pendingStatements: {},
  _hasPendingStatements: function WTBJL__hasPendingStatements() {
    return !!Object.keys(this._pendingStatements).length;
  },

  async _buildList() {
    if (this._hasPendingStatements()) {
      // We were requested to update the list while another update was in
      // progress, this could happen at shutdown, idle or privatebrowsing.
      // Abort the current list building.
      for (let listType in this._pendingStatements) {
        this._pendingStatements[listType].cancel();
        delete this._pendingStatements[listType];
      }
      this._builder.abortListBuild();
    }

    // anything to build?
    if (!this._showFrequent && !this._showRecent && !this._showTasks) {
      // don't leave the last list hanging on the taskbar.
      this._deleteActiveJumpList();
      return;
    }

    await this._startBuild();

    if (this._showTasks) {
      this._buildTasks();
    }

    // Space for frequent items takes priority over recent.
    if (this._showFrequent) {
      this._buildFrequent();
    }

    if (this._showRecent) {
      this._buildRecent();
    }

    this._commitBuild();
  },

  /**
   * Taskbar api wrappers
   */

  async _startBuild() {
    this._builder.abortListBuild();
    let URIsToRemove = await this._builder.initListBuild();
    if (URIsToRemove.length) {
      // Prior to building, delete removed items from history.
      this._clearHistory(URIsToRemove);
    }
  },

  _commitBuild: function WTBJL__commitBuild() {
    if (this._hasPendingStatements()) {
      return;
    }

    this._builder.commitListBuild(succeed => {
      if (!succeed) {
        this._builder.abortListBuild();
      }
    });
  },

  _buildTasks: function WTBJL__buildTasks() {
    var items = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
    this._tasks.forEach(function(task) {
      if (
        (this._shuttingDown && !task.close) ||
        (!this._shuttingDown && !task.open)
      ) {
        return;
      }
      var item = this._getHandlerAppItem(
        task.title,
        task.description,
        task.args,
        task.iconIndex,
        null
      );
      items.appendElement(item);
    }, this);

    if (items.length) {
      this._builder.addListToBuild(
        this._builder.JUMPLIST_CATEGORY_TASKS,
        items
      );
    }
  },

  _buildCustom: function WTBJL__buildCustom(title, items) {
    if (items.length) {
      this._builder.addListToBuild(
        this._builder.JUMPLIST_CATEGORY_CUSTOMLIST,
        items,
        title
      );
    }
  },

  _buildFrequent: function WTBJL__buildFrequent() {
    // Windows supports default frequent and recent lists,
    // but those depend on internal windows visit tracking
    // which we don't populate. So we build our own custom
    // frequent and recent lists using our nav history data.

    var items = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
    // track frequent items so that we don't add them to
    // the recent list.
    this._frequentHashList = [];

    this._pendingStatements[LIST_TYPE.FREQUENT] = this._getHistoryResults(
      Ci.nsINavHistoryQueryOptions.SORT_BY_VISITCOUNT_DESCENDING,
      this._maxItemCount,
      function(aResult) {
        if (!aResult) {
          delete this._pendingStatements[LIST_TYPE.FREQUENT];
          // The are no more results, build the list.
          this._buildCustom(_getString("taskbar.frequent.label"), items);
          this._commitBuild();
          return;
        }

        let title = aResult.title || aResult.uri;
        let faviconPageUri = Services.io.newURI(aResult.uri);
        let shortcut = this._getHandlerAppItem(
          title,
          title,
          aResult.uri,
          1,
          faviconPageUri
        );
        items.appendElement(shortcut);
        this._frequentHashList.push(aResult.uri);
      },
      this
    );
  },

  _buildRecent: function WTBJL__buildRecent() {
    var items = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
    // Frequent items will be skipped, so we select a double amount of
    // entries and stop fetching results at _maxItemCount.
    var count = 0;

    this._pendingStatements[LIST_TYPE.RECENT] = this._getHistoryResults(
      Ci.nsINavHistoryQueryOptions.SORT_BY_DATE_DESCENDING,
      this._maxItemCount * 2,
      function(aResult) {
        if (!aResult) {
          // The are no more results, build the list.
          this._buildCustom(_getString("taskbar.recent.label"), items);
          delete this._pendingStatements[LIST_TYPE.RECENT];
          this._commitBuild();
          return;
        }

        if (count >= this._maxItemCount) {
          return;
        }

        // Do not add items to recent that have already been added to frequent.
        if (
          this._frequentHashList &&
          this._frequentHashList.includes(aResult.uri)
        ) {
          return;
        }

        let title = aResult.title || aResult.uri;
        let faviconPageUri = Services.io.newURI(aResult.uri);
        let shortcut = this._getHandlerAppItem(
          title,
          title,
          aResult.uri,
          1,
          faviconPageUri
        );
        items.appendElement(shortcut);
        count++;
      },
      this
    );
  },

  _deleteActiveJumpList: function WTBJL__deleteAJL() {
    this._builder.deleteActiveList();
  },

  /**
   * Jump list item creation helpers
   */

  _getHandlerAppItem: function WTBJL__getHandlerAppItem(
    name,
    description,
    args,
    iconIndex,
    faviconPageUri
  ) {
    var file = Services.dirsvc.get("XREExeF", Ci.nsIFile);

    var handlerApp = Cc[
      "@mozilla.org/uriloader/local-handler-app;1"
    ].createInstance(Ci.nsILocalHandlerApp);
    handlerApp.executable = file;
    // handlers default to the leaf name if a name is not specified
    if (name && name.length) {
      handlerApp.name = name;
    }
    handlerApp.detailedDescription = description;
    handlerApp.appendParameter(args);

    var item = Cc["@mozilla.org/windows-jumplistshortcut;1"].createInstance(
      Ci.nsIJumpListShortcut
    );
    item.app = handlerApp;
    item.iconIndex = iconIndex;
    item.faviconPageUri = faviconPageUri;
    return item;
  },

  _getSeparatorItem: function WTBJL__getSeparatorItem() {
    var item = Cc["@mozilla.org/windows-jumplistseparator;1"].createInstance(
      Ci.nsIJumpListSeparator
    );
    return item;
  },

  /**
   * Nav history helpers
   */

  _getHistoryResults: function WTBLJL__getHistoryResults(
    aSortingMode,
    aLimit,
    aCallback,
    aScope
  ) {
    var options = PlacesUtils.history.getNewQueryOptions();
    options.maxResults = aLimit;
    options.sortingMode = aSortingMode;
    var query = PlacesUtils.history.getNewQuery();

    // Return the pending statement to the caller, to allow cancelation.
    return PlacesUtils.history.asyncExecuteLegacyQuery(query, options, {
      handleResult(aResultSet) {
        for (let row; (row = aResultSet.getNextRow()); ) {
          try {
            aCallback.call(aScope, {
              uri: row.getResultByIndex(1),
              title: row.getResultByIndex(2),
            });
          } catch (e) {}
        }
      },
      handleError(aError) {
        Cu.reportError(
          "Async execution error (" + aError.result + "): " + aError.message
        );
      },
      handleCompletion(aReason) {
        aCallback.call(WinTaskbarJumpList, null);
      },
    });
  },

  _clearHistory: function WTBJL__clearHistory(uriSpecsToRemove) {
    let URIsToRemove = uriSpecsToRemove
      .map(spec => {
        try {
          // in case we get a bad uri
          return Services.io.newURI(spec);
        } catch (e) {
          return null;
        }
      })
      .filter(uri => !!uri);

    if (URIsToRemove.length) {
      PlacesUtils.history.remove(URIsToRemove).catch(Cu.reportError);
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
    if (!this._builder || !this._builder.available) {
      return false;
    }

    return true;
  },

  _initObs: function WTBJL__initObs() {
    // If the browser is closed while in private browsing mode, the "exit"
    // notification is fired on quit-application-granted.
    // History cleanup can happen at profile-change-teardown.
    Services.obs.addObserver(this, "profile-before-change");
    Services.obs.addObserver(this, "browser:purge-session-history");
    _prefs.addObserver("", this);
    this._placesObserver = new PlacesWeakCallbackWrapper(
      this.update.bind(this)
    );
    PlacesUtils.observers.addListener(
      ["history-cleared"],
      this._placesObserver
    );
  },

  _freeObs: function WTBJL__freeObs() {
    Services.obs.removeObserver(this, "profile-before-change");
    Services.obs.removeObserver(this, "browser:purge-session-history");
    _prefs.removeObserver("", this);
    if (this._placesObserver) {
      PlacesUtils.observers.removeListener(
        ["history-cleared"],
        this._placesObserver
      );
    }
  },

  _updateTimer: function WTBJL__updateTimer() {
    if (this._enabled && !this._shuttingDown && !this._timer) {
      this._timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
      this._timer.initWithCallback(
        this,
        _prefs.getIntPref(PREF_TASKBAR_REFRESH) * 1000,
        this._timer.TYPE_REPEATING_SLACK
      );
    } else if ((!this._enabled || this._shuttingDown) && this._timer) {
      this._timer.cancel();
      delete this._timer;
    }
  },

  _hasIdleObserver: false,
  _updateIdleObserver: function WTBJL__updateIdleObserver() {
    if (this._enabled && !this._shuttingDown && !this._hasIdleObserver) {
      _idle.addIdleObserver(this, IDLE_TIMEOUT_SECONDS);
      this._hasIdleObserver = true;
    } else if (
      (!this._enabled || this._shuttingDown) &&
      this._hasIdleObserver
    ) {
      _idle.removeIdleObserver(this, IDLE_TIMEOUT_SECONDS);
      this._hasIdleObserver = false;
    }
  },

  _free: function WTBJL__free() {
    this._freeObs();
    this._updateTimer();
    this._updateIdleObserver();
    delete this._builder;
  },

  notify: function WTBJL_notify(aTimer) {
    // Add idle observer on the first notification so it doesn't hit startup.
    this._updateIdleObserver();
    Services.tm.idleDispatchToMainThread(() => {
      this.update();
    });
  },

  observe: function WTBJL_observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "nsPref:changed":
        if (this._enabled && !_prefs.getBoolPref(PREF_TASKBAR_ENABLED)) {
          this._deleteActiveJumpList();
        }
        this._refreshPrefs();
        this._updateTimer();
        this._updateIdleObserver();
        Services.tm.idleDispatchToMainThread(() => {
          this.update();
        });
        break;

      case "profile-before-change":
        this._shutdown();
        break;

      case "browser:purge-session-history":
        this.update();
        break;
      case "idle":
        if (this._timer) {
          this._timer.cancel();
          delete this._timer;
        }
        break;

      case "active":
        this._updateTimer();
        break;
    }
  },
};
