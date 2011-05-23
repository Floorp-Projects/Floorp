/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function createEmptyGroupItem(contentWindow, width, height, padding, animate) {
  let pageBounds = contentWindow.Items.getPageBounds();
  pageBounds.inset(padding, padding);

  let box = new contentWindow.Rect(pageBounds);
  box.width = width;
  box.height = height;

  let emptyGroupItem =
    new contentWindow.GroupItem([], { bounds: box, immediately: !animate });

  return emptyGroupItem;
}

// ----------
function createGroupItemWithTabs(win, width, height, padding, urls, animate) {
  let contentWindow = win.TabView.getContentWindow();
  let groupItemCount = contentWindow.GroupItems.groupItems.length;
  // create empty group item
  let groupItem = createEmptyGroupItem(contentWindow, width, height, padding, animate);
  ok(groupItem.isEmpty(), "This group is empty");
  is(contentWindow.GroupItems.groupItems.length, ++groupItemCount,
     "The number of groups is increased by 1");
  // add blank items
  contentWindow.UI.setActive(groupItem);
  let t = 0;
  urls.forEach( function(url) {
    let newItem = win.gBrowser.loadOneTab(url)._tabViewTabItem;
    ok(newItem.container, "Created element "+t+":"+newItem.container);
    ++t;
  });
  return groupItem;
}

// ----------
function createGroupItemWithBlankTabs(win, width, height, padding, numNewTabs, animate) {
  let urls = [];
  while(numNewTabs--)
    urls.push("about:blank");
  return createGroupItemWithTabs(win, width, height, padding, urls, animate);
}

// ----------
function closeGroupItem(groupItem, callback) {
  groupItem.addSubscriber(groupItem, "groupHidden", function() {
    groupItem.removeSubscriber(groupItem, "groupHidden");
    groupItem.addSubscriber(groupItem, "close", function() {
      groupItem.removeSubscriber(groupItem, "close");
      callback();
    });
    groupItem.closeHidden();
  });
  groupItem.closeAll();
}

// ----------
function afterAllTabItemsUpdated(callback, win) {
  win = win || window;
  let tabItems = win.document.getElementById("tab-view").contentWindow.TabItems;

  for (let a = 0; a < win.gBrowser.tabs.length; a++) {
    let tabItem = win.gBrowser.tabs[a]._tabViewTabItem;
    if (tabItem)
      tabItems._update(win.gBrowser.tabs[a]);
  }
  callback();
}

// ---------
function newWindowWithTabView(shownCallback, loadCallback, width, height) {
  let winWidth = width || 800;
  let winHeight = height || 800;
  let win = window.openDialog(getBrowserURL(), "_blank",
                              "chrome,all,dialog=no,height=" + winHeight +
                              ",width=" + winWidth);
  let onLoad = function() {
    win.removeEventListener("load", onLoad, false);
    if (typeof loadCallback == "function")
      loadCallback(win);

    let onShown = function() {
      win.removeEventListener("tabviewshown", onShown, false);
      shownCallback(win);
    };
    win.addEventListener("tabviewshown", onShown, false);
    win.TabView.toggle();
  }
  win.addEventListener("load", onLoad, false);
}

// ----------
function afterAllTabsLoaded(callback, win) {
  const TAB_STATE_NEEDS_RESTORE = 1;

  win = win || window;

  let stillToLoad = 0;
  let restoreHiddenTabs = Services.prefs.getBoolPref(
                          "browser.sessionstore.restore_hidden_tabs");

  function onLoad() {
    this.removeEventListener("load", onLoad, true);
    stillToLoad--;
    if (!stillToLoad)
      callback();
  }

  for (let a = 0; a < win.gBrowser.tabs.length; a++) {
    let tab = win.gBrowser.tabs[a];
    let browser = tab.linkedBrowser;

    let isRestorable = !(tab.hidden && !restoreHiddenTabs &&
                         browser.__SS_restoreState &&
                         browser.__SS_restoreState == TAB_STATE_NEEDS_RESTORE);

    if (isRestorable && browser.contentDocument.readyState != "complete" ||
        browser.webProgress.isLoadingDocument) {
      stillToLoad++;
      browser.addEventListener("load", onLoad, true);
    }
  }

  if (!stillToLoad)
    executeSoon(callback);
}

// ----------
function showTabView(callback, win) {
  win = win || window;

  if (win.TabView.isVisible()) {
    callback();
    return;
  }

  whenTabViewIsShown(callback, win);
  win.TabView.show();
}

// ----------
function hideTabView(callback, win) {
  win = win || window;

  if (!win.TabView.isVisible()) {
    callback();
    return;
  }

  whenTabViewIsHidden(callback, win);
  win.TabView.hide();
}

// ----------
function whenTabViewIsHidden(callback, win) {
  win = win || window;

  if (!win.TabView.isVisible()) {
    callback();
    return;
  }

  win.addEventListener('tabviewhidden', function () {
    win.removeEventListener('tabviewhidden', arguments.callee, false);
    callback();
  }, false);
}

// ----------
function whenTabViewIsShown(callback, win) {
  win = win || window;

  if (win.TabView.isVisible()) {
    callback();
    return;
  }

  win.addEventListener('tabviewshown', function () {
    win.removeEventListener('tabviewshown', arguments.callee, false);
    callback();
  }, false);
}

// ----------
function showSearch(callback, win) {
  win = win || window;

  let contentWindow = win.document.getElementById("tab-view").contentWindow;
  if (contentWindow.isSearchEnabled()) {
    callback();
    return;
  }

  whenSearchIsEnabled(callback, win);
  contentWindow.performSearch();
}

// ----------
function hideSearch(callback, win) {
  win = win || window;

  let contentWindow = win.document.getElementById("tab-view").contentWindow;
  if (!contentWindow.isSearchEnabled()) {
    callback();
    return;
  }

  whenSearchIsDisabled(callback, win);
  contentWindow.hideSearch();
}

// ----------
function whenSearchIsEnabled(callback, win) {
  win = win || window;

  let contentWindow = win.document.getElementById("tab-view").contentWindow;
  if (contentWindow.isSearchEnabled()) {
    callback();
    return;
  }

  contentWindow.addEventListener("tabviewsearchenabled", function () {
    contentWindow.removeEventListener("tabviewsearchenabled", arguments.callee, false);
    callback();
  }, false);
}

// ----------
function whenSearchIsDisabled(callback, win) {
  win = win || window;

  let contentWindow = win.document.getElementById("tab-view").contentWindow;
  if (!contentWindow.isSearchEnabled()) {
    callback();
    return;
  }

  contentWindow.addEventListener("tabviewsearchdisabled", function () {
    contentWindow.removeEventListener("tabviewsearchdisabled", arguments.callee, false);
    callback();
  }, false);
}


// ----------
function hideGroupItem(groupItem, callback) {
  if (groupItem.hidden) {
    callback();
    return;
  }

  groupItem.addSubscriber(groupItem, "groupHidden", function () {
    groupItem.removeSubscriber(groupItem, "groupHidden");
    callback();
  });
  groupItem.closeAll();
}

// ----------
function unhideGroupItem(groupItem, callback) {
  if (!groupItem.hidden) {
    callback();
    return;
  }

  groupItem.addSubscriber(groupItem, "groupShown", function () {
    groupItem.removeSubscriber(groupItem, "groupShown");
    callback();
  });
  groupItem._unhide();
}

// ----------
function whenWindowLoaded(win, callback) {
  win.addEventListener("load", function onLoad() {
    win.removeEventListener("load", onLoad, false);
    executeSoon(callback);
  }, false);
}

// ----------
function whenWindowStateReady(win, callback) {
  win.addEventListener("SSWindowStateReady", function onReady() {
    win.removeEventListener("SSWindowStateReady", onReady, false);
    executeSoon(callback);
  }, false);
}

// ----------
function newWindowWithState(state, callback) {
  let opts = "chrome,all,dialog=no,height=800,width=800";
  let win = window.openDialog(getBrowserURL(), "_blank", opts);

  whenWindowLoaded(win, function () {
    ss.setWindowState(win, JSON.stringify(state), true);
  });

  whenWindowStateReady(win, function () {
    afterAllTabsLoaded(function () callback(win), win);
  });
}
