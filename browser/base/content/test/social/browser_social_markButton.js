/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let prefName = "social.enabled",
    gFinishCB;

function test() {
  waitForExplicitFinish();

  // Need to load a http/https/ftp/ftps page for the social mark button to appear
  let tab = gBrowser.selectedTab = gBrowser.addTab("https://test1.example.com", {skipAnimation: true});
  tab.linkedBrowser.addEventListener("load", function tabLoad(event) {
    tab.linkedBrowser.removeEventListener("load", tabLoad, true);
    executeSoon(tabLoaded);
  }, true);

  registerCleanupFunction(function() {
    Services.prefs.clearUserPref(prefName);
    gBrowser.removeTab(tab);
  });
}

function tabLoaded() {
  ok(Social, "Social module loaded");

  let manifest = { // normal provider
    name: "provider 1",
    origin: "https://example.com",
    sidebarURL: "https://example.com/browser/browser/base/content/test/social/social_sidebar.html",
    workerURL: "https://example.com/browser/browser/base/content/test/social/social_worker.js",
    iconURL: "https://example.com/browser/browser/base/content/test/moz.png"
  };
  runSocialTestWithProvider(manifest, function (finishcb) {
    gFinishCB = finishcb;
    testInitial();
  });
}

function testInitial(finishcb) {
  ok(Social.provider, "Social provider is active");
  ok(Social.provider.enabled, "Social provider is enabled");
  let port = Social.provider.getWorkerPort();
  ok(port, "Social provider has a port to its FrameWorker");
  port.close();

  let markButton = SocialMark.button;
  ok(markButton, "mark button exists");

  // ensure the worker initialization and handshakes are all done and we
  // have a profile and the worker has sent a page-mark-config msg.
  waitForCondition(function() Social.provider.pageMarkInfo != null, function() {
    is(markButton.hasAttribute("marked"), false, "SocialMark button should not have 'marked' attribute before mark button is clicked");
    // Check the strings from our worker actually ended up on the button.
    is(markButton.getAttribute("tooltiptext"), "Mark this page", "check tooltip text is correct");
    // Check the relative URL was resolved correctly (note this image has offsets of zero...)
    is(markButton.style.listStyleImage, 'url("https://example.com/browser/browser/base/content/test/social/social_mark_image.png")', "check image url is correct");

    // Test the mark button command handler
    SocialMark.togglePageMark(function() {
      is(markButton.hasAttribute("marked"), true, "mark button should have 'marked' attribute after mark button is clicked");
      is(markButton.getAttribute("tooltiptext"), "Unmark this page", "check tooltip text is correct");
      // Check the URL and offsets were applied correctly
      is(markButton.style.listStyleImage, 'url("https://example.com/browser/browser/base/content/test/social/social_mark_image.png")', "check image url is correct");
      SocialMark.togglePageMark(function() {
        is(markButton.hasAttribute("marked"), false, "mark button should not be marked");
        executeSoon(function() {
          testStillMarkedIn2Tabs();
        });
      });
    });
  }, "provider didn't provide page-mark-config");
}

function testStillMarkedIn2Tabs() {
  let toMark = "http://test2.example.com";
  let markUri = Services.io.newURI(toMark, null, null);
  let markButton = SocialMark.button;
  let initialTab = gBrowser.selectedTab;
  info("initialTab has loaded " + gBrowser.currentURI.spec);
  is(markButton.hasAttribute("marked"), false, "SocialMark button should not have 'marked' for the initial tab");

  addTab(toMark, function(tab1) {
    addTab(toMark, function(tab2) {
      // should start without either page being marked.
      is(markButton.hasAttribute("marked"), false, "SocialMark button should not have 'marked' before we've done anything");
      Social.isURIMarked(markUri, function(marked) {
        ok(!marked, "page is unmarked in annotations");
        markButton.click();
        waitForCondition(function() markButton.hasAttribute("marked"), function() {
          Social.isURIMarked(markUri, function(marked) {
            ok(marked, "page is marked in annotations");
            // and switching to the first tab (with the same URL) should still reflect marked.
            selectBrowserTab(tab1, function() {
              is(markButton.hasAttribute("marked"), true, "SocialMark button should reflect the marked state");
              // wait for tabselect
              selectBrowserTab(initialTab, function() {
                waitForCondition(function() !markButton.hasAttribute("marked"), function() {
                  gBrowser.selectedTab = tab1;
        
                  SocialMark.togglePageMark(function() {
                    Social.isURIMarked(gBrowser.currentURI, function(marked) {
                      ok(!marked, "page is unmarked in annotations");
                      is(markButton.hasAttribute("marked"), false, "mark button should not be marked");
                      gBrowser.removeTab(tab1);
                      gBrowser.removeTab(tab2);
                      executeSoon(testStillMarkedAfterReopen);
                    });
                  });
                }, "button has been unmarked");
              });
            });
          });
        }, "button has been marked");
      });
    });
  });
}

function testStillMarkedAfterReopen() {
  let toMark = "http://test2.example.com";
  let markButton = SocialMark.button;

  is(markButton.hasAttribute("marked"), false, "Reopen: SocialMark button should not have 'marked' for the initial tab");
  addTab(toMark, function(tab) {
    SocialMark.togglePageMark(function() {
      is(markButton.hasAttribute("marked"), true, "SocialMark button should reflect the marked state");
      gBrowser.removeTab(tab);
      // should be on the initial unmarked tab now.
      waitForCondition(function() !markButton.hasAttribute("marked"), function() {
        // now open the same URL - should be back to Marked.
        addTab(toMark, function(tab) {
          is(markButton.hasAttribute("marked"), true, "New tab to previously marked URL should reflect marked state");
          SocialMark.togglePageMark(function() {
            gBrowser.removeTab(tab);
            executeSoon(testOnlyMarkCertainUrlsTabSwitch);
          });
        });
      }, "button is now unmarked");
    });
  });
}

function testOnlyMarkCertainUrlsTabSwitch() {
  let toMark = "http://test2.example.com";
  let notSharable = "about:blank";
  let markButton = SocialMark.button;
  addTab(toMark, function(tab) {
    ok(!markButton.hidden, "SocialMark button not hidden for http url");
    addTab(notSharable, function(tab2) {
      ok(markButton.disabled, "SocialMark button disabled for about:blank");
      selectBrowserTab(tab, function() {
        ok(!markButton.disabled, "SocialMark button re-shown when switching back to http: url");
        selectBrowserTab(tab2, function() {
          ok(markButton.disabled, "SocialMark button re-hidden when switching back to about:blank");
          gBrowser.removeTab(tab);
          gBrowser.removeTab(tab2);
          executeSoon(testOnlyMarkCertainUrlsSameTab);
        });
      });
    });
  });
}

function testOnlyMarkCertainUrlsSameTab() {
  let toMark = "http://test2.example.com";
  let notSharable = "about:blank";
  let markButton = SocialMark.button;
  addTab(toMark, function(tab) {
    ok(!markButton.disabled, "SocialMark button not disabled for http url");
    loadIntoTab(tab, notSharable, function() {
      ok(markButton.disabled, "SocialMark button disabled for about:blank");
      loadIntoTab(tab, toMark, function() {
        ok(!markButton.disabled, "SocialMark button re-enabled http url");
        gBrowser.removeTab(tab);
        executeSoon(testDisable);
      });
    });
  });
}

function testDisable() {
  let markButton = SocialMark.button;
  Services.prefs.setBoolPref(prefName, false);
  is(markButton.hidden, true, "SocialMark button should be hidden when pref is disabled");
  gFinishCB();
}
