/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
let contentWindow;
let enablePersistentHttpsCaching;
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

    contentWindow.ThumbnailStorage.enablePersistentHttpsCaching =
        enablePersistentHttpsCaching;
  });

  showTabView(function() {
    contentWindow = TabView.getContentWindow();
    test1();
  });
}


function test1() {
  // page with cache-control: no-store, should not save thumbnail
  HttpRequestObserver.cacheControlValue = "no-store";
  newTab.linkedBrowser.loadURI("http://www.example.com/browser/browser/base/content/test/tabview/dummy_page.html");

  afterAllTabsLoaded(function() {
    let tabItem = newTab._tabViewTabItem;

    ok(!contentWindow.ThumbnailStorage._shouldSaveThumbnail(newTab), 
       "Should not save the thumbnail for tab");

    whenDeniedToCacheImageData(tabItem, test2);
    tabItem.save(true);
    HttpRequestObserver.cacheControlValue = null;
  });
}

function test2() {
  // page with cache-control: private, should save thumbnail
  HttpRequestObserver.cacheControlValue = "private";

  newTab.linkedBrowser.loadURI("http://www.example.com/");
  afterAllTabsLoaded(function() {
    let tabItem = newTab._tabViewTabItem;

    ok(contentWindow.ThumbnailStorage._shouldSaveThumbnail(newTab), 
       "Should save the thumbnail for tab");

    whenSavedCachedImageData(tabItem, test3);
    tabItem.save(true);
  });
}

function test3() {
  // page with cache-control: private with https caching enabled, should save thumbnail
  HttpRequestObserver.cacheControlValue = "private";

  enablePersistentHttpsCaching =
    contentWindow.ThumbnailStorage.enablePersistentHttpsCaching;
  contentWindow.ThumbnailStorage.enablePersistentHttpsCaching = true;

  newTab.linkedBrowser.loadURI("https://example.com/browser/browser/base/content/test/tabview/dummy_page.html");
  afterAllTabsLoaded(function() {
    let tabItem = newTab._tabViewTabItem;

    ok(contentWindow.ThumbnailStorage._shouldSaveThumbnail(newTab),
       "Should save the thumbnail for tab");

    whenSavedCachedImageData(tabItem, test4);
    tabItem.save(true);
  });
}

function test4() {
  // page with cache-control: public with https caching disabled, should save thumbnail
  HttpRequestObserver.cacheControlValue = "public";

  contentWindow.ThumbnailStorage.enablePersistentHttpsCaching = false;

  newTab.linkedBrowser.loadURI("https://example.com/browser/browser/base/content/test/tabview/");
  afterAllTabsLoaded(function() {
    let tabItem = newTab._tabViewTabItem;

    ok(contentWindow.ThumbnailStorage._shouldSaveThumbnail(newTab),
       "Should save the thumbnail for tab");

    whenSavedCachedImageData(tabItem, test5);
    tabItem.save(true);
  });
}

function test5() {
  // page with cache-control: private with https caching disabled, should not save thumbnail
  HttpRequestObserver.cacheControlValue = "private";
 
  newTab.linkedBrowser.loadURI("https://example.com/");
  afterAllTabsLoaded(function() {
    let tabItem = newTab._tabViewTabItem;

    ok(!contentWindow.ThumbnailStorage._shouldSaveThumbnail(newTab),
       "Should not the thumbnail for tab");

    whenDeniedToCacheImageData(tabItem, function () {
      hideTabView(function () {
        gBrowser.removeTab(gBrowser.tabs[1]);
        finish();
      });
    });
    tabItem.save(true);
  });
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
