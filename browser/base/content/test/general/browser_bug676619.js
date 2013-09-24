function test () {
  waitForExplicitFinish();

  var isHTTPS = false;

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function () {
    if (isHTTPS) {
      gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);
    }
    let doc = gBrowser.contentDocument;


    function testLocation(link, url, next) {
      var tabOpenListener = new TabOpenListener(url, function () {
          gBrowser.removeTab(this.tab);
      }, function () {
        next();
      });

      doc.getElementById(link).click();
    }

    function testLink(link, name, next) {
        addWindowListener("chrome://mozapps/content/downloads/unknownContentType.xul", function (win) {
            is(doc.getElementById("unload-flag").textContent, "Okay", "beforeunload shouldn't have fired");
            is(win.document.getElementById("location").value, name, "file name should match");
            win.close();
            next();
        });

        doc.getElementById(link).click();
    }

    testLink("link1", "test.txt",
      testLink.bind(null, "link2", "video.ogg",
        testLink.bind(null, "link3", "just some video",
          testLink.bind(null, "link4", "with-target.txt",
            testLink.bind(null, "link5", "javascript.txt",
              testLink.bind(null, "link6", "test.blob",
                testLocation.bind(null, "link7", "http://example.com/",
                  function () {
                    if (isHTTPS) {
                      gBrowser.removeCurrentTab();
                      finish();
                    } else {
                      // same test again with https:
                      isHTTPS = true;
                      content.location = "https://example.com:443/browser/browser/base/content/test/general/download_page.html";
                    }
                  })))))));

  }, true);

  content.location = "http://mochi.test:8888/browser/browser/base/content/test/general/download_page.html";
}


function addWindowListener(aURL, aCallback) {
  Services.wm.addListener({
    onOpenWindow: function(aXULWindow) {
      info("window opened, waiting for focus");
      Services.wm.removeListener(this);

      var domwindow = aXULWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                                .getInterface(Ci.nsIDOMWindow);
      waitForFocus(function() {
        is(domwindow.document.location.href, aURL, "should have seen the right window open");
        aCallback(domwindow);
      }, domwindow);
    },
    onCloseWindow: function(aXULWindow) { },
    onWindowTitleChange: function(aXULWindow, aNewTitle) { }
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

  handleEvent: function(event) {
    if (event.type == "TabOpen") {
      gBrowser.tabContainer.removeEventListener("TabOpen", this, false);
      this.tab = event.originalTarget;
      this.browser = this.tab.linkedBrowser;
      gBrowser.addEventListener("pageshow", this, false);
    } else if (event.type == "pageshow") {
      if (event.target.location.href != this.url)
        return;
      gBrowser.removeEventListener("pageshow", this, false);
      this.tab.addEventListener("TabClose", this, false);
      var url = this.browser.contentDocument.location.href;
      is(url, this.url, "Should have opened the correct tab");
      this.opencallback(this.tab, this.browser.contentWindow);
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
