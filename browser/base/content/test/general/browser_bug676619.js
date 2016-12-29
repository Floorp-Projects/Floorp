function test() {
  requestLongerTimeout(3);
  waitForExplicitFinish();

  var isHTTPS = false;

  function loadListener() {
    function testLocation(link, url, next) {
      new TabOpenListener(url, function() {
        gBrowser.removeTab(this.tab);
      }, function() {
        next();
      });

      ContentTask.spawn(gBrowser.selectedBrowser, link, contentLink => {
        content.document.getElementById(contentLink).click();
      });
    }

    function testLink(link, name, next) {
      addWindowListener("chrome://mozapps/content/downloads/unknownContentType.xul", function(win) {
        ContentTask.spawn(gBrowser.selectedBrowser, null, () => {
          Assert.equal(content.document.getElementById("unload-flag").textContent,
            "Okay", "beforeunload shouldn't have fired");
        }).then(() => {
          is(win.document.getElementById("location").value, name, "file name should match");
          win.close();
          next();
        });
      });

      ContentTask.spawn(gBrowser.selectedBrowser, link, contentLink => {
        content.document.getElementById(contentLink).click();
      });
    }

    testLink("link1", "test.txt",
      testLink.bind(null, "link2", "video.ogg",
        testLink.bind(null, "link3", "just some video",
          testLink.bind(null, "link4", "with-target.txt",
            testLink.bind(null, "link5", "javascript.txt",
              testLink.bind(null, "link6", "test.blob",
                testLocation.bind(null, "link7", "http://example.com/",
                  function() {
                    if (isHTTPS) {
                      finish();
                    } else {
                      // same test again with https:
                      isHTTPS = true;
                      gBrowser.loadURI("https://example.com:443/browser/browser/base/content/test/general/download_page.html");
                    }
                  })))))));

  }

  BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(() => {
    loadListener();
    BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(loadListener);
  });

  gBrowser.loadURI("http://mochi.test:8888/browser/browser/base/content/test/general/download_page.html");
}


function addWindowListener(aURL, aCallback) {
  Services.wm.addListener({
    onOpenWindow(aXULWindow) {
      info("window opened, waiting for focus");
      Services.wm.removeListener(this);

      var domwindow = aXULWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                                .getInterface(Ci.nsIDOMWindow);
      waitForFocus(function() {
        is(domwindow.document.location.href, aURL, "should have seen the right window open");
        aCallback(domwindow);
      }, domwindow);
    },
    onCloseWindow(aXULWindow) { },
    onWindowTitleChange(aXULWindow, aNewTitle) { }
  });
}

// This listens for the next opened tab and checks it is of the right url.
// opencallback is called when the new tab is fully loaded
// closecallback is called when the tab is closed
function TabOpenListener(url, opencallback, closecallback) {
  this.url = url;
  this.opencallback = opencallback;
  this.closecallback = closecallback;

  gBrowser.tabContainer.addEventListener("TabOpen", this, false);
}

TabOpenListener.prototype = {
  url: null,
  opencallback: null,
  closecallback: null,
  tab: null,
  browser: null,

  handleEvent(event) {
    if (event.type == "TabOpen") {
      gBrowser.tabContainer.removeEventListener("TabOpen", this, false);
      this.tab = event.originalTarget;
      this.browser = this.tab.linkedBrowser;
      BrowserTestUtils.browserLoaded(this.browser, false, this.url).then(() => {
        this.tab.addEventListener("TabClose", this, false);
        var url = this.browser.currentURI.spec;
        is(url, this.url, "Should have opened the correct tab");
        this.opencallback();
      });
    } else if (event.type == "TabClose") {
      if (event.originalTarget != this.tab)
        return;
      this.tab.removeEventListener("TabClose", this, false);
      this.opencallback = null;
      this.tab = null;
      this.browser = null;
      // Let the window close complete
      executeSoon(this.closecallback);
      this.closecallback = null;
    }
  }
};
