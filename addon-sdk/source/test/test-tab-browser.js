/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

module.metadata = {
  'engines': {
    'Firefox': '*'
  }
};

var timer = require("sdk/timers");
var { Cc, Ci } = require("chrome");

function onBrowserLoad(callback, event) {
  if (event.target && event.target.defaultView == this) {
    this.removeEventListener("load", onBrowserLoad, true);
    let browsers = this.document.getElementsByTagName("tabbrowser");
    try {
      timer.setTimeout(function (window) {
        callback(window, browsers[0]);
      }, 10, this);
    } catch (e) { console.exception(e); }
  }
}
// Utility function to open a new browser window.
function openBrowserWindow(callback, url) {
  let ww = Cc["@mozilla.org/embedcomp/window-watcher;1"].
           getService(Ci.nsIWindowWatcher);
  let urlString = Cc["@mozilla.org/supports-string;1"].
                  createInstance(Ci.nsISupportsString);
  urlString.data = url;
  let window = ww.openWindow(null, "chrome://browser/content/browser.xul",
                             "_blank", "chrome,all,dialog=no", urlString);
  if (callback)
    window.addEventListener("load", onBrowserLoad.bind(window, callback), true);

  return window;
}

// Helper for calling code at window close
function closeBrowserWindow(window, callback) {
  timer.setTimeout(function() {
    window.addEventListener("unload", function onUnload() {
      window.removeEventListener("unload", onUnload, false);
      callback();
    }, false);
    window.close();
  }, 0);
}

// Helper for opening two windows at once
function openTwoWindows(callback) {
  openBrowserWindow(function (window1) {
    openBrowserWindow(function (window2) {
      callback(window1, window2);
    });
  });
}

// Helper for closing two windows at once
function closeTwoWindows(window1, window2, callback) {
  closeBrowserWindow(window1, function() {
    closeBrowserWindow(window2, callback);
  });
}

exports.testAddTab = function(assert, done) {
  openBrowserWindow(function(window, browser) {
    const tabBrowser = require("sdk/deprecated/tab-browser");

    let cache = [];
    let windowUtils = require("sdk/deprecated/window-utils");
    new windowUtils.WindowTracker({
      onTrack: function(win) {
        cache.push(win);
      },
      onUntrack: function(win) {
        cache.splice(cache.indexOf(win), 1)
      }
    });
    let startWindowCount = cache.length;

    // Test 1: add a tab
    let firstUrl = "data:text/html;charset=utf-8,one";
    tabBrowser.addTab(firstUrl, {
      onLoad: function(e) {
        let win1 = cache[startWindowCount - 1];
        assert.equal(win1.content.location, firstUrl, "URL of new tab in first window matches");

        // Test 2: add a tab in a new window
        let secondUrl = "data:text/html;charset=utf-8,two";
        tabBrowser.addTab(secondUrl, {
          inNewWindow: true,
          onLoad: function(e) {
            assert.equal(cache.length, startWindowCount + 1, "a new window was opened");
            let win2 = cache[startWindowCount];
            let gBrowser = win2.gBrowser;
            gBrowser.addEventListener("DOMContentLoaded", function onLoad(e) {
              gBrowser.removeEventListener("DOMContentLoaded", onLoad, false);
              assert.equal(win2.content.location, secondUrl, "URL of new tab in the new window matches");

              closeBrowserWindow(win2, function() {
                closeBrowserWindow(win1, done);
              });
            }, false);
          }
        });
      }
    });
  });
};

exports.testTrackerWithDelegate = function(assert, done) {
  const tabBrowser = require("sdk/deprecated/tab-browser");

  var delegate = {
    state: "initializing",
    onTrack: function onTrack(browser) {
      if (this.state == "initializing") {
        this.state = "waiting for browser window to open";
      }
      else if (this.state == "waiting for browser window to open") {
        this.state = "waiting for browser window to close";
        timer.setTimeout(function() {
          closeBrowserWindow(browser.ownerDocument.defaultView, function() {
            assert.equal(delegate.state, "deinitializing");
            tb.unload();
            done();
          });
        }, 0);
      }
      else
        assert.fail("invalid state");
    },
    onUntrack: function onUntrack(browser) {
      if (this.state == "waiting for browser window to close") {
        assert.pass("proper state in onUntrack");
        this.state = "deinitializing";
      }
      else if (this.state != "deinitializing")
        assert.fail("invalid state");
    }
  };
  var tb = new tabBrowser.Tracker(delegate);

  delegate.state = "waiting for browser window to open";

  openBrowserWindow();
};

exports.testWhenContentLoaded = function(assert, done) {
  const tabBrowser = require("sdk/deprecated/tab-browser");

  var tracker = tabBrowser.whenContentLoaded(
    function(window) {
      var item = window.document.getElementById("foo");
      assert.equal(item.textContent, "bar",
                       "whenContentLoaded() works.");
      tracker.unload();
      closeBrowserWindow(activeWindow(), done);
    });

  openBrowserWindow(function(browserWindow, browser) {
    var html = '<div id="foo">bar</div>';
    browser.addTab("data:text/html;charset=utf-8," + html);
  });
};

exports.testTrackerWithoutDelegate = function(assert, done) {
  const tabBrowser = require("sdk/deprecated/tab-browser");

  openBrowserWindow(function(browserWindow, browser) {
    var tb = new tabBrowser.Tracker();

    if (tb.length == 0)
      test.fail("expect at least one tab browser to exist.");

    for (var i = 0; i < tb.length; i++)
      assert.equal(tb.get(i).nodeName, "tabbrowser",
                       "get() method and length prop should work");
    for (var b in tb)
      assert.equal(b.nodeName, "tabbrowser",
                       "iterator should work");

    var matches = [b for (b in tb)
                           if (b == browser)];
    assert.equal(matches.length, 1,
                     "New browser should be in tracker.");
    tb.unload();

    closeBrowserWindow(browserWindow, done);
  });
};

exports.testTabTracker = function(assert, done) {
  const tabBrowser = require("sdk/deprecated/tab-browser");

  openBrowserWindow(function(browserWindow, browser) {
    var delegate = {
      tracked: 0,
      onTrack: function(tab) {
        this.tracked++;
      },
      onUntrack: function(tab) {
        this.tracked--;
      }
    };

    let tabTracker = tabBrowser.TabTracker(delegate);

    let tracked = delegate.tracked;
    let url1 = "data:text/html;charset=utf-8,1";
    let url2 = "data:text/html;charset=utf-8,2";
    let url3 = "data:text/html;charset=utf-8,3";
    let tabCount = 0;

    function tabLoadListener(e) {
      let loadedURL = e.target.defaultView.location;
      if (loadedURL == url1)
        tabCount++;
      else if (loadedURL == url2)
        tabCount++;
      else if (loadedURL == url3)
        tabCount++;

      if (tabCount == 3) {
        assert.equal(delegate.tracked, tracked + 3, "delegate tracked tabs matched count");
        tabTracker.unload();
        closeBrowserWindow(browserWindow, function() {
          timer.setTimeout(done, 0);
        });
      }
    }

    tabBrowser.addTab(url1, {
      onLoad: tabLoadListener
    });
    tabBrowser.addTab(url2, {
      onLoad: tabLoadListener
    });
    tabBrowser.addTab(url3, {
      onLoad: tabLoadListener
    });
  });
};

exports.testActiveTab = function(assert, done) {
  openBrowserWindow(function(browserWindow, browser) {
    const tabBrowser = require("sdk/deprecated/tab-browser");
    const TabModule = require("sdk/deprecated/tab-browser").TabModule;
    let tm = new TabModule(browserWindow);
    assert.equal(tm.length, 1);
    let url1 = "data:text/html;charset=utf-8,foo";
    let url2 = "data:text/html;charset=utf-8,bar";

    function tabURL(tab) tab.ownerDocument.defaultView.content.location.toString()

    tabBrowser.addTab(url1, {
      onLoad: function(e) {
        // make sure we're running in the right window.
        assert.equal(tabBrowser.activeTab.ownerDocument.defaultView, browserWindow, "active window matches");
        browserWindow.focus();

        assert.equal(tabURL(tabBrowser.activeTab), url1, "url1 matches");
        let tabIndex = browser.getBrowserIndexForDocument(e.target);
        let tabAtIndex = browser.tabContainer.getItemAtIndex(tabIndex);
        assert.equal(tabAtIndex, tabBrowser.activeTab, "activeTab element matches");

        tabBrowser.addTab(url2, {
          inBackground: true,
          onLoad: function() {
            assert.equal(tabURL(tabBrowser.activeTab), url1, "url1 still matches");
            let tabAtIndex = browser.tabContainer.getItemAtIndex(tabIndex);
            assert.equal(tabAtIndex, tabBrowser.activeTab, "activeTab element matches");
            closeBrowserWindow(browserWindow, done);
          }
        });
      }
    });
  });
};

// TabModule tests
exports.testEventsAndLengthStayInModule = function(assert, done) {
  let TabModule = require("sdk/deprecated/tab-browser").TabModule;

  openTwoWindows(function(window1, window2) {
    let tm1 = new TabModule(window1);
    let tm2 = new TabModule(window2);

    let counter1 = 0, counter2 = 0;
    let counterTabs = 0;

    function onOpenListener() {
      ++counterTabs;
      if (counterTabs < 5)
        return;
      assert.equal(counter1, 2, "Correct number of events fired from window 1");
      assert.equal(counter2, 3, "Correct number of events fired from window 2");
      assert.equal(counterTabs, 5, "Correct number of events fired from all windows");
      assert.equal(tm1.length, 3, "Correct number of tabs in window 1");
      assert.equal(tm2.length, 4, "Correct number of tabs in window 2");
      closeTwoWindows(window1, window2, done);
    }

    tm1.onOpen = function() ++counter1 && onOpenListener();
    tm2.onOpen = function() ++counter2 && onOpenListener();

    let url = "data:text/html;charset=utf-8,default";
    tm1.open(url);
    tm1.open(url);

    tm2.open(url);
    tm2.open(url);
    tm2.open(url);
  });
}

exports.testTabModuleActiveTab_getterAndSetter = function(assert, done) {
  let TabModule = require("sdk/deprecated/tab-browser").TabModule;

  openTwoWindows(function(window1, window2) {
    let tm1 = new TabModule(window1);
    let tm2 = new TabModule(window2);

    // First open two tabs per window
    tm1.open({
      url: "data:text/html;charset=utf-8,<title>window1,tab1</title>",
      onOpen: function(tab1) {
        tm1.open({
          url: "data:text/html;charset=utf-8,<title>window1,tab2</title>",
          onOpen: function (tab2) {
            tm2.open({
              url: "data:text/html;charset=utf-8,<title>window2,tab1</title>",
              onOpen: function (tab3) {
                tm2.open({
                  url: "data:text/html;charset=utf-8,<title>window2,tab2</title>",
                  onOpen: function(tab4) {
                    onTabsOpened(tab1, tab2, tab3, tab4);
                  }
                });
              }
            });
          }
        });
      }
    });

    // Then try to activate tabs, but wait for all of them to be activated after
    // being opened
    function onTabsOpened(tab1, tab2, tab3, tab4) {
      assert.equal(tm1.activeTab.title, "window1,tab2",
                       "Correct active tab on window 1");
      assert.equal(tm2.activeTab.title, "window2,tab2",
                       "Correct active tab on window 2");

      tm1.onActivate = function onActivate() {
        tm1.onActivate.remove(onActivate);
        timer.setTimeout(function() {
          assert.equal(tm1.activeTab.title, "window1,tab1",
                           "activeTab setter works (window 1)");
          assert.equal(tm2.activeTab.title, "window2,tab2",
                           "activeTab is ignored with tabs from another window");
          closeTwoWindows(window1, window2, done);
        }, 1000);
      }

      tm1.activeTab = tab1;
      // Setting activeTab from another window should have no effect:
      tm1.activeTab = tab4;
    }

  });
}

// test tabs iterator
exports.testTabModuleTabsIterator = function(assert, done) {
  let TabModule = require("sdk/deprecated/tab-browser").TabModule;

  openBrowserWindow(function(window) {
    let tm1 = new TabModule(window);
    let url = "data:text/html;charset=utf-8,default";
    tm1.open(url);
    tm1.open(url);
    tm1.open({
      url: url,
      onOpen: function(tab) {
        let count = 0;
        for each (let t in tm1) count++;
        assert.equal(count, 4, "iterated tab count matches");
        assert.equal(count, tm1.length, "length tab count matches");
        closeBrowserWindow(window, done);
      }
    });
  });
};

// inNewWindow parameter is ignored on single-window modules
exports.testTabModuleCantOpenInNewWindow = function(assert, done) {
  let TabModule = require("sdk/deprecated/tab-browser").TabModule;

  openBrowserWindow(function(window) {
    let tm = new TabModule(window);
    let url = "data:text/html;charset=utf-8,default";
    tm.open({
      url: url,
      inNewWindow: true,
      onOpen: function() {
        assert.equal(tm.length, 2, "Tab was open on same window");
        closeBrowserWindow(window, done);
      }
    });
  });
};

// Test that having two modules attached to the same
// window won't duplicate events fired on each module
exports.testModuleListenersDontInteract = function(assert, done) {
  let TabModule = require("sdk/deprecated/tab-browser").TabModule;
  let onOpenTab;
  openBrowserWindow(function(window) {
    let tm1 = new TabModule(window);
    let tm2 = new TabModule(window);

    let url = "data:text/html;charset=utf-8,foo";
    let eventCount = 0, eventModule1 = 0, eventModule2 = 0;


    let listener1 = function() {
      // this should be called twice: when tab is open and when
      // the url location is changed
      eventCount++;
      eventModule1++;
      check();
    }
    tm1.onReady = listener1;

    tm2.open({
      url: "about:blank",
      onOpen: function(tab) {
        onOpenTab = tab;
        // add listener via property assignment
        tab.onReady = listener2;

        // add listener via collection add
        tab.onReady.add(listener3);

        tab.location = url;

      }
    });
    
    function listener2 () {
      eventCount++;
      eventModule2++;
      check();
    }
    
    function listener3 () {
      eventCount++;
      eventModule2++;
      check();
    }

    function check () {
      if (eventCount !== 4) return;
      assert.equal(eventModule1, 2,
        "Correct number of events on module 1");
      assert.equal(eventModule2, 2,
        "Correct number of events on module 2");

      tm1.onReady.remove(listener1);
      onOpenTab.onReady.remove(listener2);
      onOpenTab.onReady.remove(listener3);
      closeBrowserWindow(window, done);
    }
  });
};

/******************* helpers *********************/

// Helper for getting the active window
function activeWindow() {
  return Cc["@mozilla.org/appshell/window-mediator;1"].
         getService(Ci.nsIWindowMediator).
         getMostRecentWindow("navigator:browser");
}

require('sdk/test').run(exports);
