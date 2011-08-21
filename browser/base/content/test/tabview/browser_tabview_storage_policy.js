/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const PREF_DISK_CACHE_SSL = "browser.cache.disk_cache_ssl";

let contentWindow;
let newTab;

function test() {
  waitForExplicitFinish();

  newTab = gBrowser.addTab();

  HttpRequestObserver.register();

  registerCleanupFunction(function () {
    HttpRequestObserver.unregister();
    if (gBrowser.tabs[1])
      gBrowser.removeTab(gBrowser.tabs[1]);
    hideTabView();

    Services.prefs.clearUserPref(PREF_DISK_CACHE_SSL);
  });

  showTabView(function() {
    contentWindow = TabView.getContentWindow();
    test1();
  });
}


function test1() {
  // page with cache-control: no-store, should not save thumbnail
  HttpRequestObserver.cacheControlValue = "no-store";

  whenStorageDenied(newTab, function () {
    let tabItem = newTab._tabViewTabItem;

    ok(!contentWindow.StoragePolicy.canStoreThumbnailForTab(newTab), 
       "Should not save the thumbnail for tab");

    whenDeniedToCacheImageData(tabItem, test2);
    tabItem.save(true);
    HttpRequestObserver.cacheControlValue = null;
  });

  newTab.linkedBrowser.loadURI("http://www.example.com/browser/browser/base/content/test/tabview/dummy_page.html");
}

function test2() {
  // page with cache-control: private, should save thumbnail
  HttpRequestObserver.cacheControlValue = "private";

  newTab.linkedBrowser.loadURI("http://www.example.com/");
  afterAllTabsLoaded(function() {
    let tabItem = newTab._tabViewTabItem;

    ok(contentWindow.StoragePolicy.canStoreThumbnailForTab(newTab), 
       "Should save the thumbnail for tab");

    whenSavedCachedImageData(tabItem, test3);
    tabItem.save(true);
  });
}

function test3() {
  // page with cache-control: private with https caching enabled, should save thumbnail
  HttpRequestObserver.cacheControlValue = "private";

  Services.prefs.setBoolPref(PREF_DISK_CACHE_SSL, true);

  newTab.linkedBrowser.loadURI("https://example.com/browser/browser/base/content/test/tabview/dummy_page.html");
  afterAllTabsLoaded(function() {
    let tabItem = newTab._tabViewTabItem;

    ok(contentWindow.StoragePolicy.canStoreThumbnailForTab(newTab),
       "Should save the thumbnail for tab");

    whenSavedCachedImageData(tabItem, test4);
    tabItem.save(true);
  });
}

function test4() {
  // page with cache-control: public with https caching disabled, should save thumbnail
  HttpRequestObserver.cacheControlValue = "public";

  Services.prefs.setBoolPref(PREF_DISK_CACHE_SSL, false);

  newTab.linkedBrowser.loadURI("https://example.com/browser/browser/base/content/test/tabview/");
  afterAllTabsLoaded(function() {
    let tabItem = newTab._tabViewTabItem;

    ok(contentWindow.StoragePolicy.canStoreThumbnailForTab(newTab),
       "Should save the thumbnail for tab");

    whenSavedCachedImageData(tabItem, test5);
    tabItem.save(true);
  });
}

function test5() {
  // page with cache-control: private with https caching disabled, should not save thumbnail
  HttpRequestObserver.cacheControlValue = "private";

  whenStorageDenied(newTab, function () {
    let tabItem = newTab._tabViewTabItem;

    ok(!contentWindow.StoragePolicy.canStoreThumbnailForTab(newTab),
       "Should not save the thumbnail for tab");

    whenDeniedToCacheImageData(tabItem, function () {
      hideTabView(function () {
        gBrowser.removeTab(gBrowser.tabs[1]);
        finish();
      });
    });
    tabItem.save(true);
  });

  newTab.linkedBrowser.loadURI("https://example.com/");
}

let HttpRequestObserver = {
  cacheControlValue: null,

  observe: function(subject, topic, data) {
    if (topic == "http-on-examine-response" && this.cacheControlValue) {
      let httpChannel = subject.QueryInterface(Ci.nsIHttpChannel);
      httpChannel.setResponseHeader("Cache-Control", this.cacheControlValue, false);
    }
  },

  register: function() {
    Services.obs.addObserver(this, "http-on-examine-response", false);
  },

  unregister: function() {
    Services.obs.removeObserver(this, "http-on-examine-response");
  }
};

function whenSavedCachedImageData(tabItem, callback) {
  tabItem.addSubscriber("savedCachedImageData", function onSaved() {
    tabItem.removeSubscriber("savedCachedImageData", onSaved);
    callback();
  });
}

function whenDeniedToCacheImageData(tabItem, callback) {
  tabItem.addSubscriber("deniedToCacheImageData", function onDenied() {
    tabItem.removeSubscriber("deniedToCacheImageData", onDenied);
    callback();
  });
}

function whenStorageDenied(tab, callback) {
  let mm = tab.linkedBrowser.messageManager;

  mm.addMessageListener("Panorama:StoragePolicy:denied", function onDenied() {
    mm.removeMessageListener("Panorama:StoragePolicy:denied", onDenied);
    executeSoon(callback);
  });
}
