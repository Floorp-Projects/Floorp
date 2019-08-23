/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.defineModuleGetter(
  this,
  "AppConstants",
  "resource://gre/modules/AppConstants.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "SessionStore",
  "resource:///modules/sessionstore/SessionStore.jsm"
);

var gStateObject;
var gTreeData;

// Page initialization

window.onload = function() {
  let toggleTabs = document.getElementById("tabsToggle");
  if (toggleTabs) {
    let treeContainer = document.querySelector(".tree-container");

    let toggleHiddenTabs = () => {
      toggleTabs.classList.toggle("show-tabs");
      treeContainer.classList.toggle("expanded");
    };
    toggleTabs.onclick = toggleHiddenTabs;
  }

  // pages used by this script may have a link that needs to be updated to
  // the in-product link.
  let anchor = document.getElementById("linkMoreTroubleshooting");
  if (anchor) {
    let baseURL = Services.urlFormatter.formatURLPref("app.support.baseURL");
    anchor.setAttribute("href", baseURL + "troubleshooting");
  }

  // wire up click handlers for the radio buttons if they exist.
  for (let radioId of ["radioRestoreAll", "radioRestoreChoose"]) {
    let button = document.getElementById(radioId);
    if (button) {
      button.addEventListener("click", updateTabListVisibility);
    }
  }

  var tabListTree = document.getElementById("tabList");
  tabListTree.addEventListener("click", onListClick);
  tabListTree.addEventListener("keydown", onListKeyDown);

  var errorCancelButton = document.getElementById("errorCancel");
  // aboutSessionRestore.js is included aboutSessionRestore.xhtml
  // and aboutWelcomeBack.xhtml, but the latter does not have an
  // errorCancel button.
  if (errorCancelButton) {
    errorCancelButton.addEventListener("command", startNewSession);
  }

  var errorTryAgainButton = document.getElementById("errorTryAgain");
  errorTryAgainButton.addEventListener("command", restoreSession);

  // the crashed session state is kept inside a textbox so that SessionStore picks it up
  // (for when the tab is closed or the session crashes right again)
  var sessionData = document.getElementById("sessionData");
  if (!sessionData.value) {
    errorTryAgainButton.disabled = true;
    return;
  }

  gStateObject = JSON.parse(sessionData.value);

  // make sure the data is tracked to be restored in case of a subsequent crash
  var event = document.createEvent("UIEvents");
  event.initUIEvent("input", true, true, window, 0);
  sessionData.dispatchEvent(event);

  initTreeView();

  errorTryAgainButton.focus();
};

function isTreeViewVisible() {
  let tabList = document.querySelector(".tree-container");
  return tabList.hasAttribute("available");
}

async function initTreeView() {
  // If we aren't visible we initialize as we are made visible (and it's OK
  // to initialize multiple times)
  if (!isTreeViewVisible()) {
    return;
  }
  var tabList = document.getElementById("tabList");
  let l10nIds = [];
  for (
    let labelIndex = 0;
    labelIndex < gStateObject.windows.length;
    labelIndex++
  ) {
    l10nIds.push({
      id: "restore-page-window-label",
      args: { windowNumber: labelIndex + 1 },
    });
  }
  let winLabels = await document.l10n.formatValues(l10nIds);
  gTreeData = [];
  gStateObject.windows.forEach(function(aWinData, aIx) {
    var winState = {
      label: winLabels[aIx],
      open: true,
      checked: true,
      ix: aIx,
    };
    winState.tabs = aWinData.tabs.map(function(aTabData) {
      var entry = aTabData.entries[aTabData.index - 1] || {
        url: "about:blank",
      };
      var iconURL = aTabData.image || null;
      // don't initiate a connection just to fetch a favicon (see bug 462863)
      if (/^https?:/.test(iconURL)) {
        iconURL = "moz-anno:favicon:" + iconURL;
      }
      return {
        label: entry.title || entry.url,
        checked: true,
        src: iconURL,
        parent: winState,
      };
    });
    gTreeData.push(winState);
    for (let tab of winState.tabs) {
      gTreeData.push(tab);
    }
  }, this);

  tabList.view = treeView;
  tabList.view.selection.select(0);
}

// User actions
function updateTabListVisibility() {
  let tabList = document.querySelector(".tree-container");
  let container = document.querySelector(".container");
  if (document.getElementById("radioRestoreChoose").checked) {
    tabList.setAttribute("available", "true");
    container.classList.add("restore-chosen");
  } else {
    tabList.removeAttribute("available");
    container.classList.remove("restore-chosen");
  }
  initTreeView();
}

function restoreSession() {
  Services.obs.notifyObservers(null, "sessionstore-initiating-manual-restore");
  document.getElementById("errorTryAgain").disabled = true;

  if (isTreeViewVisible()) {
    if (!gTreeData.some(aItem => aItem.checked)) {
      // This should only be possible when we have no "cancel" button, and thus
      // the "Restore session" button always remains enabled.  In that case and
      // when nothing is selected, we just want a new session.
      startNewSession();
      return;
    }

    // remove all unselected tabs from the state before restoring it
    var ix = gStateObject.windows.length - 1;
    for (var t = gTreeData.length - 1; t >= 0; t--) {
      if (treeView.isContainer(t)) {
        if (gTreeData[t].checked === 0) {
          // this window will be restored partially
          gStateObject.windows[ix].tabs = gStateObject.windows[ix].tabs.filter(
            (aTabData, aIx) => gTreeData[t].tabs[aIx].checked
          );
        } else if (!gTreeData[t].checked) {
          // this window won't be restored at all
          gStateObject.windows.splice(ix, 1);
        }
        ix--;
      }
    }
  }
  var stateString = JSON.stringify(gStateObject);

  var top = getBrowserWindow();

  // if there's only this page open, reuse the window for restoring the session
  if (top.gBrowser.tabs.length == 1) {
    SessionStore.setWindowState(top, stateString, true);
    return;
  }

  // restore the session into a new window and close the current tab
  var newWindow = top.openDialog(
    top.location,
    "_blank",
    "chrome,dialog=no,all"
  );

  Services.obs.addObserver(function observe(win, topic) {
    if (win != newWindow) {
      return;
    }

    Services.obs.removeObserver(observe, topic);
    SessionStore.setWindowState(newWindow, stateString, true);

    let tabbrowser = top.gBrowser;
    let browser = window.docShell.chromeEventHandler;
    let tab = tabbrowser.getTabForBrowser(browser);
    tabbrowser.removeTab(tab);
  }, "browser-delayed-startup-finished");
}

function startNewSession() {
  if (Services.prefs.getIntPref("browser.startup.page") == 0) {
    getBrowserWindow().gBrowser.loadURI("about:blank", {
      triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal(
        {}
      ),
    });
  } else {
    getBrowserWindow().BrowserHome();
  }
}

function onListClick(aEvent) {
  // don't react to right-clicks
  if (aEvent.button == 2) {
    return;
  }

  var cell = treeView.treeBox.getCellAt(aEvent.clientX, aEvent.clientY);
  if (cell.col) {
    // Restore this specific tab in the same window for middle/double/accel clicking
    // on a tab's title.
    let accelKey =
      AppConstants.platform == "macosx" ? aEvent.metaKey : aEvent.ctrlKey;
    if (
      (aEvent.button == 1 ||
        (aEvent.button == 0 && aEvent.detail == 2) ||
        accelKey) &&
      cell.col.id == "title" &&
      !treeView.isContainer(cell.row)
    ) {
      restoreSingleTab(cell.row, aEvent.shiftKey);
      aEvent.stopPropagation();
    } else if (cell.col.id == "restore") {
      toggleRowChecked(cell.row);
    }
  }
}

function onListKeyDown(aEvent) {
  switch (aEvent.keyCode) {
    case KeyEvent.DOM_VK_SPACE:
      toggleRowChecked(document.getElementById("tabList").currentIndex);
      // Prevent page from scrolling on the space key.
      aEvent.preventDefault();
      break;
    case KeyEvent.DOM_VK_RETURN:
      var ix = document.getElementById("tabList").currentIndex;
      if (aEvent.ctrlKey && !treeView.isContainer(ix)) {
        restoreSingleTab(ix, aEvent.shiftKey);
      }
      break;
  }
}

// Helper functions

function getBrowserWindow() {
  return window.docShell.rootTreeItem.domWindow;
}

function toggleRowChecked(aIx) {
  function isChecked(aItem) {
    return aItem.checked;
  }

  var item = gTreeData[aIx];
  item.checked = !item.checked;
  treeView.treeBox.invalidateRow(aIx);

  if (treeView.isContainer(aIx)) {
    // (un)check all tabs of this window as well
    for (let tab of item.tabs) {
      tab.checked = item.checked;
      treeView.treeBox.invalidateRow(gTreeData.indexOf(tab));
    }
  } else {
    // Update the window's checkmark as well (0 means "partially checked").
    let state = false;
    if (item.parent.tabs.every(isChecked)) {
      state = true;
    } else if (item.parent.tabs.some(isChecked)) {
      state = 0;
    }
    item.parent.checked = state;

    treeView.treeBox.invalidateRow(gTreeData.indexOf(item.parent));
  }

  // we only disable the button when there's no cancel button.
  if (document.getElementById("errorCancel")) {
    document.getElementById("errorTryAgain").disabled = !gTreeData.some(
      isChecked
    );
  }
}

function restoreSingleTab(aIx, aShifted) {
  var tabbrowser = getBrowserWindow().gBrowser;
  var newTab = tabbrowser.addWebTab();
  var item = gTreeData[aIx];

  var tabState =
    gStateObject.windows[item.parent.ix].tabs[
      aIx - gTreeData.indexOf(item.parent) - 1
    ];
  // ensure tab would be visible on the tabstrip.
  tabState.hidden = false;
  SessionStore.setTabState(newTab, JSON.stringify(tabState));

  // respect the preference as to whether to select the tab (the Shift key inverses)
  if (
    Services.prefs.getBoolPref("browser.tabs.loadInBackground") != !aShifted
  ) {
    tabbrowser.selectedTab = newTab;
  }
}

// Tree controller

var treeView = {
  treeBox: null,
  selection: null,

  get rowCount() {
    return gTreeData.length;
  },
  setTree(treeBox) {
    this.treeBox = treeBox;
  },
  getCellText(idx, column) {
    return gTreeData[idx].label;
  },
  isContainer(idx) {
    return "open" in gTreeData[idx];
  },
  getCellValue(idx, column) {
    return gTreeData[idx].checked;
  },
  isContainerOpen(idx) {
    return gTreeData[idx].open;
  },
  isContainerEmpty(idx) {
    return false;
  },
  isSeparator(idx) {
    return false;
  },
  isSorted() {
    return false;
  },
  isEditable(idx, column) {
    return false;
  },
  canDrop(idx, orientation, dt) {
    return false;
  },
  getLevel(idx) {
    return this.isContainer(idx) ? 0 : 1;
  },

  getParentIndex(idx) {
    if (!this.isContainer(idx)) {
      for (var t = idx - 1; t >= 0; t--) {
        if (this.isContainer(t)) {
          return t;
        }
      }
    }
    return -1;
  },

  hasNextSibling(idx, after) {
    var thisLevel = this.getLevel(idx);
    for (var t = after + 1; t < gTreeData.length; t++) {
      if (this.getLevel(t) <= thisLevel) {
        return this.getLevel(t) == thisLevel;
      }
    }
    return false;
  },

  toggleOpenState(idx) {
    if (!this.isContainer(idx)) {
      return;
    }
    var item = gTreeData[idx];
    if (item.open) {
      // remove this window's tab rows from the view
      var thisLevel = this.getLevel(idx);
      /* eslint-disable no-empty */
      for (
        var t = idx + 1;
        t < gTreeData.length && this.getLevel(t) > thisLevel;
        t++
      ) {}
      /* eslint-disable no-empty */
      var deletecount = t - idx - 1;
      gTreeData.splice(idx + 1, deletecount);
      this.treeBox.rowCountChanged(idx + 1, -deletecount);
    } else {
      // add this window's tab rows to the view
      var toinsert = gTreeData[idx].tabs;
      for (var i = 0; i < toinsert.length; i++) {
        gTreeData.splice(idx + i + 1, 0, toinsert[i]);
      }
      this.treeBox.rowCountChanged(idx + 1, toinsert.length);
    }
    item.open = !item.open;
    this.treeBox.invalidateRow(idx);
  },

  getCellProperties(idx, column) {
    if (
      column.id == "restore" &&
      this.isContainer(idx) &&
      gTreeData[idx].checked === 0
    ) {
      return "partial";
    }
    if (column.id == "title") {
      return this.getImageSrc(idx, column) ? "icon" : "noicon";
    }

    return "";
  },

  getRowProperties(idx) {
    var winState = gTreeData[idx].parent || gTreeData[idx];
    if (winState.ix % 2 != 0) {
      return "alternate";
    }

    return "";
  },

  getImageSrc(idx, column) {
    if (column.id == "title") {
      return gTreeData[idx].src || null;
    }
    return null;
  },

  cycleHeader(column) {},
  cycleCell(idx, column) {},
  selectionChanged() {},
  getColumnProperties(column) {
    return "";
  },
};
